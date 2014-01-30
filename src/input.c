/* $Id: input.c,v 1.38 2003-09-04 19:25:38 martin Exp $ */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#if defined (UNIX)
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#if defined (WIN32)
#include <windows.h>
#include <winbase.h> // PeekNamedPipe
#include <wincon.h> // GetNumberOfConsoleInputEvents, GetConsoleMode
#endif

#include "chess.h"
#include "input.h"
#include "logger.h"
#include "chessio.h"
#include "movegen.h"
#include "execute.h" /* verify move */
#include "init.h" /* setup_board */
#include "helpers.h" /* command help */
#include "book.h"
#include "test.h" /* bogomips */
#include "mstimer.h"
#include "analyse.h"
#include "version.h"
#include "transref.h" /* tt_clear */

#define INPUT_MAXSIZE 128

/* detailed error conditions if reading a command failed */
#define G2_NO_ERROR 0 
#define G2_UNKNOWN_CMD 1 
#define G2_AMBIGUOUS_CMD 2 
#define G2_NOTLEGALNOW_CMD 4 
#define G2_NUMPARAM_CMD 8 /* number of parameters wrong */
#define G2_NOTIMPLEMENTED_CMD 16
#define G2_EMPTY_CMD 32
#define G2_FAILED_CMD 64

static int command_error_reason;

/* array of all known commands.
   Must match constants defined in input.h
 */

#define MAX_COMMANDS 58 /* members in cmds[] */

char * cmds[] =
{
  "unknown",
  "o-o",
  "o-o-o",
  "bd",
  "list",
  "undo",
  "edit",
  "switch",
  "white",
  "depth",
  "post",
  "save",
  "random",
  "quit",
  "material",
  "easy",
  "hash",
  "reverse",
  "book",
  "remove",
  "force",
  "both",
  "black",
  "clock",
  "hint",
  "get",
  "new",
  "go",
  "help",
  "nopost",
  "otim",
  "time",
  "level",
  "ponder",
  "setboard",
  "hard",
  "xboard",
  "bogomips",
  "bench",
  "analyze",
  "exit",
  "?",
  ".",
  "ping", 
  "bk",
  "name", 
  "rating",
  "computer",
  "ics",
  "pause",
  "resume",
  "fritz",
  "draw",
  "result",
  "protover",
  "accepted",
  "rejected",
  "reset"
};


static int input_pending = 0;

static int command_match(const char *);
static int read_command(char *);
static int parse_move(char *,move_t *);

static char static_inbuf[INPUT_MAXSIZE]; 

/*
 * Gullydeckel command handling:
 *
 * A command is any input which was identified 
 * to be a move by the move parsers. Internally, each 
 * command code has a prefix.
 * Gullydeckel understands three related types
 * of commands: 
 * 1. Gnuchess commands (prefix GNU) which are an
 * attempt to be compatible with the classic GNU chess program.
 * 2. XBoard commands (prefix XB). XB commands are all commands 
 * be sent from XBoard (or WinBoard) to the chess engine. Many
 * of them are there because of GNU chess, but XBoard now actively
 * defines some useful extensions.
 * 3. Gullydeckel specific commands.
 *
 * When a command is not recognized, Gully tries to print
 * error messages as expected by XBoard. At least if Gully runs
 * in Xboard mode (checked by XBOARD_ON) macro.
 */

static int
read_command(char * inbuf)
{
  int command;

  assert(inbuf);

  command_error_reason = G2_NO_ERROR;

  if ((command = command_match(inbuf)) == 0)
    return UNKNOWN_COMMAND;

  return command;
}

/* 
 *  Look whether the given command matches a command exactly.
 *  later do substring matching.
 */

static int 
command_match(const char * input_command)
{
  int i;
  char command[INPUT_MAXSIZE];

  assert(input_command);

  /* use only first token of input buffer */
  command[0] = 0;
  sscanf(input_command, "%s", &command[0]);
  if (strlen(command) == 0) {
    command_error_reason = G2_EMPTY_CMD;
    return 0;
  }

#if 0
  log_msg("command_match: Found command: %s.\n", command);
#endif

  /* exact matches */
  for ( i = 1 ; i < MAX_COMMANDS ; i++)
    if (!strcmp(cmds[i], command)) return i;
  

  /* substrings */
  for ( i = 1 ; i < MAX_COMMANDS ; i++)
    if (!strncmp(cmds[i], command, strlen(command))) {
      int j = i+1;

      while (j < MAX_COMMANDS) {
	if (!strncmp(cmds[j], command, strlen(command))) {
	  if (!XBOARD_ON)
	    fprintf(stderr, "Ambiguous command."
		    "(%s matches at least %s and %s)\n",
		    command, cmds[i], cmds[j]);
	  command_error_reason = G2_AMBIGUOUS_CMD;
	  return 0;
	}
	j++;
      }

      return i;
    }

  command_error_reason = G2_UNKNOWN_CMD;
  return 0;
}
     
/* 
 *  read a move in long algebraic notation as it comes from xboard:
 *  e2e4 e7e8q e1g1.
 *  also recognizes o-o 0-0 O-O etc 
 *  return 1 (and fills m) on success, 0 on failure.
 */

static int
parse_move(char * inbuf,move_t *m)
{
  int len;

  assert(m);
  assert(inbuf);

  /* in ICS mode, xboard sends a dummy "a3" to tell the chess program 
   * that the position to edit will be black's move.
   * This kludge catches it. In the long term, this parser should detect 
   * any abbreviated moves.
   */
  if (!strncmp(inbuf, "a3", 2) && strlen(inbuf) == 3) {
    m->from_to = FROM_TO(0x10,0x20);
    return 1;
  }

  if ((len = strlen(inbuf)) < 4)
    return 0;

  /* detect castling moves 
     XXX wrong if parsing o-o while pondering (e.g. vs. gnu)
     kludge in ponder().
  */
  if (!strncmp(inbuf,"o-o-o",5) || !strncmp(inbuf,"0-0-0",5) ||
      !strncmp(inbuf,"O-O-O",5)) {
    m->from_to = (turn == WHITE) ? (FROM_TO(0x04, 0x02)) : 
      (FROM_TO(0x74, 0x72));
    m->special = CASTLING;
    return 1;
  }

  if (!strncmp(inbuf, "o-o", 3) || !strncmp(inbuf, "0-0", 3) ||
      !strncmp(inbuf, "O-O", 3)) {
    m->from_to = (turn == WHITE) ? (FROM_TO(0x04, 0x06)) : 
      (FROM_TO(0x74, 0x76));
    m->special = CASTLING;
    return 1;
  }


  if (inbuf[0] < 'a' || inbuf[0] > 'h' || inbuf[1] < '1' || inbuf[1] > '8'
      || inbuf[2] < 'a' || inbuf[2] > 'h' || inbuf[3] < '1' || inbuf[3] > '8')
    return 0;

  m->from_to = FROM_TO(((inbuf[1] - '1') << 4) + inbuf[0] - 'a',
		       ((inbuf[3] - '1') << 4) + inbuf[2] - 'a');

  m->cap_pro = 0;
  m->special = NORMAL_MOVE;
  
  if (isalpha(inbuf[4])) {
    switch (tolower(inbuf[4])) {
    case 'q':
    case 'd':
      m->cap_pro = CAP_PRO(0, QUEEN); break;
    case 'n':
    case 's':
      m->cap_pro = CAP_PRO(0, KNIGHT); break;
    case 'b':
    case 'l':
      m->cap_pro = CAP_PRO(0, BISHOP); break;
    case 'r':
    case 't':
      m->cap_pro = CAP_PRO(0, ROOK); break;
    default:
      return 0;
    }
    m->special = PROMOTION;
  }

  return 1;
}

/* 
 * Parse a move in reduced algebraic format. Returns 1 on success.
 * Users will probably prefer this; ICS sends always in the long 
 * format.
 */

int
parse_abbreviated(char * inbuf,move_t * m)
{
  piece_t piece = PAWN, promo = 0;
  int i,j,found,state,f_rank,t_rank,f_file,t_file;
  const char piece_symbols[6] = { ' ','K','Q','R','B','N'};
  char move_buf[INPUT_MAXSIZE],*in,*out,token;

  assert(inbuf);

  /* allow abreviated input only if we are idle since we rely on
     the correct board position. Don't allow it during search or
     pondering.
  */
  if (!IS_IDLE)
    return 0;

  f_rank = t_rank = G2_NO_RANK;
  f_file = t_file = G2_NO_FILE;
  /* strip x,=,-,+,# */
  in = inbuf; out = move_buf;
  while (in[0] != '\0')
    if (in[0] == 'x' || in[0] == '=' || in[0] == '+' || in[0] == '-' ||
	in[0] == '#' || in[0] == '\n' || in[0] == ' ')
      in++;
    else
      *out++ = *in++;
  *out = 0;


  /* handle special case of castling - note that hyphens are 
     removed */

  if (!strncmp(move_buf,"ooo",3) || !strncmp(move_buf,"000",3) ||
      !strncmp(move_buf,"OOO",3)) {
    f_file = 0x04;
    t_file = 0x02;
    f_rank = t_rank = (turn == WHITE) ? 0x00 : 0x07;
    piece = KING;
    goto final_state;
  }

  if (!strncmp(move_buf,"oo",2) || !strncmp(move_buf,"00",2) ||
      !strncmp(move_buf,"OO",2)) {
    f_file = 0x04;
    t_file = 0x06;
    f_rank = t_rank = (turn == WHITE) ? 0x00 : 0x07;
    piece = KING;
    goto final_state;
  }



  /* Use a simple state machine to read a short move. 
   * It accepts: e4 ; e4e5 ; ed5 ; ba8Q (and more) for pawn moves; and
   * Nd2, Nbd2, N1d2, Nb1d2 for piece moves.
   * To avoid ambiguity (bc5 meaning b4xc5 or Bxc5?)
   * pieces are required to be in capitals.
   */

  out = move_buf;

  /* fall into start state */

  /* state 1 - starting state; [file(2) | piece(5)] */
  state = 1;
  token = *out++;
  for (i = 1; i < 6; i++)
    if (token == piece_symbols[i]) {
      piece = i;
      goto state5;
    }
  /* file? */
  if (token > 'h' || token < 'a')
    goto error_state;

  t_file = token - 'a';
  goto state2;
 state2: /* prev: 1 ; [file(3) | rank(4)] */
  state = 2; token = *out++;
  /* rank? */
  if (token >= '1' && token <= '8') {
    t_rank = token - '1';
    goto state4;
  }

  /* file? */
  if (token >= 'a' && token <= 'h') {
    f_file = t_file;
    t_file = token - 'a';
    goto state3;
  }
  
  goto error_state;

 state3: /* prev: 2,4; [rank(10)] */
  state = 3; token = *out++;
  /* rank? */
  if (token >= '1' && token <= '8') {
    t_rank = token - '1';
    goto state10;
  }
  goto error_state;

 state4: /* prev: 2; [ file(3) | eps (E) | piece (E)] */
  state = 4;
  token = *out++;


  /* file? */
  if (token >= 'a' && token <= 'h') {
    f_file = t_file;
    f_rank = t_rank;
    t_file = token - 'a';
    goto state3;
  }
  
  if (iscntrl(token))
    goto final_state;
  for (i = 1; i < 6; i++)
    if (token == piece_symbols[i]) {
      promo = i;
      goto final_state;
    }
  goto error_state;
 state5: /* prev: start; [ rank(8) | file(6) ] */
  state = 5;
  token = *out++;
  /* rank? */
  if (token >= '1' && token <= '8') {
    f_rank = token - '1';
    goto state8;
  }

  /* file? */
  if (token >= 'a' && token <= 'h') {
    t_file = token - 'a';
    goto state6;
  }
  
  goto error_state;
 state6: /* prev: 5 ; [ rank(7) | file (9) ] */
  state = 6;
  token = *out++;
  /* rank? */
  if (token >= '1' && token <= '8') {
    t_rank = token - '1';
    goto state7;
  }

  /* file? */
  if (token >= 'a' && token <= 'h') {
    f_file = t_file;
    t_file = token - 'a';
    goto state9;
  }
  
  goto error_state;
 state7: /* prev: 6 ; [ eps(E) | file(9) ] */
  state = 7;
  token = *out;

  if (iscntrl(token))
    goto final_state;
  /* file? */
  if (token >= 'a' && token <= 'h') {
    f_file = t_file;
    f_rank = t_rank;
    t_file = token - 'a';
    out++;
    goto state9;
  }
  
  goto error_state;
 state8: /* prev: 5 ; [file(9)] */
  state = 8;
  token = *out++;

  /* file? */
  if (token >= 'a' && token <= 'h') {
    t_file = token - 'a';
    goto state9;
  }
   
  goto error_state;
 state9: /* prev: 6,7,8 ; [ rank(E) ] */
  state = 9;
  token = *out;

  /* rank? */
  if (token >= '1' && token <= '8') {
    t_rank = token - '1';
    goto final_state;
  }
  goto error_state;
 state10: /* prev: 3; [ eps (E) | piece (E)] */
  state = 10;
  token = *out++;

  if (iscntrl(token))
    goto final_state;
  for (i = 1; i < 6; i++)
    if (token == piece_symbols[i]) {
      promo = i;
      goto final_state;
    }
  goto error_state;
 final_state:
  state = 100;
  
  /* end state machine */
  
  if ((t_file + (t_rank << 4)) & 0x88) {
    err_msg("parse error: illegal square");
    return 0;
  }

  i = generate_moves(turn,0);
  found = 0;
  for (j = 0; j < i; j++) {
    if (GET_TO(move_array[j].from_to) == t_file + (t_rank << 4)) {
      /* optional info must fit */
      if (piece != GET_PIECE(*BOARD[GET_FROM(move_array[j].from_to)]))
	continue;

      if (f_file != G2_NO_FILE &&
	  ((GET_FROM(move_array[j].from_to) & 0x0f) != f_file))
	continue;
      if (f_rank != G2_NO_RANK &&
	  ((GET_FROM(move_array[j].from_to) >> 4) != f_rank))
	continue;

      if (promo && promo != GET_PRO(move_array[j].cap_pro))
	continue;

      /* check whether one can actually make this move */
      if (make_move(&move_array[j],0)) {
	found++;
	*m = move_array[j];
      }
      undo_move(&move_array[j],0);
    }
  }
  clear_move_list(0, i);

  if (found == 1)
    return 1;
  
  if (found > 1)
    printf("Ambiguous move\n");
  
  /* fall through */
 error_state:
  return 0;
}

/* 
 * Wrapper for reading abbreviated moves.
 * used for book building. It currently
 * accepts everything except the ICS move form (g1f3).
 * Moves are checked for legality. On success, 1 is returned,
 * otherwise 0.
 */

int
read_move(char *buf,move_t *m)
{
  return parse_abbreviated(buf, m);
}



/* 
 *  wrapper for reading commands and moves 
 */

int 
fread_input(FILE * in, move_t *m)
{
  assert(m);
  m->from_to = 0;

#if 1
    if(input_pending) 
      log_msg("input.c: (blocking) fread_input is about to eat input without"
	      " reducing input_pending (%d chars), static_inbuf == %s\n",
	      input_pending, static_inbuf);
#endif

  /* Any input remaining will be eaten here. */
  input_pending = 0;

  for (;;) {
    prompt();

    if (fgets(static_inbuf, INPUT_MAXSIZE, in) == NULL) {
      if (feof(in)) {
	err_msg("EOF encountered while reading input."); 
	clearerr(in);
      }
      else 
	/* this REALLY happens in fast computer matches */
	perror("gully2:input.c:fread_input:fgets");
      continue; 
    }

#if 0
    log_msg("Received %d chars input: %s", strlen(static_inbuf), 
	    static_inbuf);
#endif
    /* 
     * move verification already done inside parse_abbreviated.
     * on success, sets m to full qualified move
     */
    if (parse_abbreviated(&static_inbuf[0], m)) {
      if(! XBOARD_ON) {
	fprint_move(stdout, m);
	fprintf(stdout, " \n"); 
      }
      break; 
    }

    /* 
     * parse_move is not as sophisticated, we have to check
     * legality afterwards.
     * on success, m is set to fully qualified move.
     */
    if (parse_move(&static_inbuf[0],m)) {
      if (!verify_move(m, 0, turn, 0)) {
	fprintf(stderr, "Illegal move: ");
	fprint_move(stderr, m);
	fprintf(stderr, "\n");
      }
      else {
	if(! XBOARD_ON) {
	  fprint_move(stdout, m);
	  fprintf(stdout, " \n"); 
	}
	break;
      }
    }
    else { /* its no valid move, so its assumed to be a command */
      int i;

      i = read_command(&static_inbuf[0]);

      switch (do_command(i, NULL)) {
      case EX_CMD_BUSY:
	saved_command = i;
	return 0;
      case EX_CMD_FAILURE:
	log_msg("input.c: do_command() failed for command %d.\n", i);
	break;
      default:
      /* Stay in command loop except if we see GO or analysis exit. */
      if (i == GNU_CMD_GO || i == XB_CMD_ANALYSIS_EXIT)
	return i;
      break;
      }
    }
  }
  return 0;
}  


/*
 * check_input is the main reason for portability problems.
 * use select() (requires some changes in the logic) or
 * work around the problem as in Crafty.
 *
 * input_pending contains the number of characters waiting to be 
 * processed. It is decremented or reset by actual input
 * handling in handle_{ponder|analyis|search}_input() (but not in
 * blocking fread_input()  ).
 * 
 * See engine-intf document by Tim Mann. I am not trying to do this
 * politically correct (ioctl for *nix and the crafty way for
 * Win32 (see utility.c)).)
 */

int 
check_input()
{
#if defined (UNIX)
  if (input_pending) return input_pending;
  ioctl(STDIN_FILENO, FIONREAD, &input_pending);
  return input_pending;

#elif defined (WIN32)
  /* Win32 input handling "inspired by" Crafty */
  
  int success;
  static int init = 0, pipe;
  static HANDLE inh;
  
  /* try to find out whether we are hooked up to a pipe or to a terminal. */
  if (!init) {
    DWORD dw; 
    init = 1;
    inh = GetStdHandle(STD_INPUT_HANDLE);
    pipe = !GetConsoleMode(inh, &dw);
    if (!pipe) {
      if(SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT|ENABLE_WINDOW_INPUT)) 
	 == 0) err_msg("input.c: SetConsoleMode failed.\n");
      FlushConsoleInputBuffer(inh);
      log_msg("Win32 - console input.");
    }
    else { 
      /* FORCE at this point standard input to be line buffered. 
       * For a discussion a buffering issues with stdio:
       * W.R. Stevens "Advanced Programming in the Unix environment".
       */
      if (setvbuf(stdin, NULL, _IONBF, 0) == 0)
	log_msg("Win32: pipe input. Forced *unbuffered* input.\n");
      else log_msg("Win32: Could not set stdin to *unbuffered*.\n");
    }
  }
  
  if (input_pending) return input_pending;
  
  if (pipe) 
    success = PeekNamedPipe(inh, NULL, 0, NULL, &input_pending, NULL);
  else {
    INPUT_RECORD irInBuf[128];
    DWORD cNumRead;
    unsigned i;

    success = GetNumberOfConsoleInputEvents(inh, &input_pending);
    /* for console apps, check if we do get windowing or mouse events here */
    if(success && input_pending) {
      if(PeekConsoleInput(inh, irInBuf, 128, &cNumRead) == 0)
	err_msg("input.c; PeekConsoleInput failed\n");
      /* now, check the input record */
      for(i = 0; i < cNumRead; i++) 
	if(irInBuf[i].EventType != KEY_EVENT) {
	  log_msg("eventbuf[%d] has a non-key event, ignored.\n", i); 
	  input_pending--;
	}						
    } /* input_pending != 0 */
  } /* console handling */
  
  if (!success) {
    int error_code = GetLastError();
    
    /* XXX hardcoded until this is proven */
    if(pipe && error_code == 109) 
      err_quit("WIN32 - pipe is broken, exiting now.\n");

    err_msg("WIN32 check_input: code %d.\n", error_code);
    input_pending = 0; 
  }
  
  return input_pending;

#else
  err_quit("Pondering not supported for this platform.\n");
  return 0; /* never reached */
#endif
}


/* 
 * reacts to command. cmd_buf is the input buffer again to allow for 
 * parsing command parameters (as in level, otim, time). If cmd_buf 
 * is NULL, it will be set to the file-local input buffer which is 
 * normally used for reading
 * the command.
 */

int 
do_command(int command, char * cmd_buf)
{
  int errorflag = 0;

  if (cmd_buf == NULL)
    cmd_buf = static_inbuf;

  /* Gullydeckel command switch.
   * Some commands can always be handled,
   * many commands are only allowed if the engine is idle.
   * In this case, EX_CMD_BUSY is returned, this command is saved 
   * (look for saved_command variable), search, analysis or pondering
   * will be finished asap and the saved command will then be handled.
   */

  switch (command) {
  case GNU_CMD_HASH:
    if (! IS_IDLE) return EX_CMD_BUSY;
    /* initial hash size cannot be changed */
    if (gameopt.transref_size == 0) {
      log_msg("do_command: ignoring %s command.\n",
	      cmds[GNU_CMD_HASH]);
      errorflag = 1;
      command_error_reason = G2_NOTLEGALNOW_CMD;
      break;
    }
    TOGGLE_OPTION(O_TRANSREF_BIT);
    log_msg("do_command: toggled transref bit, now %s.\n",
	    (TRANSREF_ON) ? "ON" : "OFF");
    break;
  case GNU_CMD_O_O:
  case GNU_CMD_O_O_O:
    /* should be handled by move parser */
    errorflag = 1;
    command_error_reason = G2_NOTLEGALNOW_CMD;
    break;
  case GNU_CMD_BD:
    if (! IS_IDLE) return EX_CMD_BUSY;
    fprint_board(stdout);
    break;
  case GNU_CMD_LIST:
    fprint_game(stdout, the_game);
    break;
  case GNU_CMD_UNDO:
    if (! IS_IDLE) return EX_CMD_BUSY;
    unmake_root_move(the_game, NULL);
    break;
  case GNU_CMD_EDIT: 
    {
      char fen_buf[128];
      /* xboard sends level command _before_ edit 
       * setup board kills time settings.
       */
      struct game_time_tag saved_game_time; 

      if (! IS_IDLE) return EX_CMD_BUSY;
	
      log_msg("do_command: entered edit mode.\n");

      saved_game_time = game_time;
      edit_board(fen_buf);
      if (!setup_board(fen_buf)) {
	err_msg("Setting up chessboard failed: %s\n", fen_buf);
	errorflag = 1;
	command_error_reason  = G2_FAILED_CMD;
	break;
      }

      /* XXX: almost goes wrong if xboard wants to play with fixed time 
       * per move.  
       * setup_board should not delete time info. 
       * Currently, init_game_time does not reset time_per_move, so it
       * works. Needs fix anyhow.
       */
      if (saved_game_time.use_game_time)
	game_time = saved_game_time;
	
      break;
    }
  case GNU_CMD_SWITCH :
    computer_color = (computer_color == WHITE) ? BLACK : WHITE;
    fprintf(stdout, "computer is now playing %s.\n",
	    (computer_color) ? "BLACK" : "WHITE");
    break;    
  case GNU_CMD_WHITE :
    if (computer_color == WHITE) break;
    if (! IS_IDLE) return EX_CMD_BUSY;
    computer_color = WHITE;
    log_msg("Gullydeckel is now playing WHITE.\n");
    break;
  case GNU_CMD_DEPTH :
    sscanf(cmd_buf, "%*s %d", &gameopt.maxdepth);    
    log_msg("DEPTH set to: %d\n");
    break;    
  case GNU_CMD_POST:
    SET_OPTION(O_POST_BIT);
    break;
  case GNU_CMD_NOPOST:
    RESET_OPTION(O_POST_BIT);
    break;
  case GNU_CMD_SAVE :
  case GNU_CMD_RANDOM :
    errorflag = 1;
    command_error_reason = G2_NOTIMPLEMENTED_CMD;
    break;    
  case GNU_CMD_QUIT :
    if (! IS_IDLE) { 
      log_msg("Will exit after finishing search...\n");
      return EX_CMD_BUSY;
    }
    quit();
    break;
  case GULLY_CMD_PONDER :
    if (IS_PONDERING) return EX_CMD_BUSY;
    else {
      char option_buf[64];

      option_buf[0]='\0';
      if (! IS_IDLE) return EX_CMD_BUSY;
      sscanf(cmd_buf,"%*s %s", option_buf);
      /* default action: */
      if (strlen(option_buf) < 1) {
	TOGGLE_OPTION(O_PONDER_BIT);
	break; 
      }
      
      /* optional argument to ponder is a move (only in fritz mode. 
	 For now, simply log the request and check it against cb8 */
      log_msg("Received ponder command with argument: %s.\n",
	      option_buf);
    }
    break;
  case GNU_CMD_EASY:
    if (IS_PONDERING) return EX_CMD_BUSY;
    RESET_OPTION(O_PONDER_BIT);
    break;
  case GNU_CMD_HARD:
    if (IS_PONDERING) break;
    SET_OPTION(O_PONDER_BIT);
    break;
  case GNU_CMD_MATERIAL :
  case GNU_CMD_REVERSE :
    errorflag = 1;
    command_error_reason = G2_NOTIMPLEMENTED_CMD;
    break;  
  case GNU_CMD_BOOK: {
    char option_buf[255];

    option_buf[0]='\0';
    if (! IS_IDLE) return EX_CMD_BUSY;
    sscanf(cmd_buf,"%*s %s",option_buf);
    if (strlen(option_buf) < 1)
      strcpy(option_buf,"status");
    if (!strcmp(option_buf,"admin"))
      book_up(DEFAULT_BOOK);
    else if (!strcmp(option_buf,"on")) {
      if (book_file) SET_OPTION(O_BOOK_BIT);
      else fprintf(stdout, "No book loaded.\n");
    }
    else if (!strcmp(option_buf, "off"))
      RESET_OPTION(O_BOOK_BIT);
    else if (!strcmp(option_buf, "status")) {
      book_status();
    }
    else {
      if (book_open(option_buf, &book_file) != 0) {
	SET_OPTION(O_BOOK_BIT);
	fprintf(stdout, "Using book %s.\n", option_buf);
      }
      else
	RESET_OPTION(O_BOOK_BIT);
    }
    break;
  }
  case GNU_CMD_REMOVE :
    if (! IS_IDLE) return EX_CMD_BUSY;
    log_msg("Undoing two plies.\n");
    unmake_root_move(the_game, NULL);
    unmake_root_move(the_game, NULL);
    break;
  case GNU_CMD_FORCE :
    if (! IS_IDLE) return EX_CMD_BUSY;
    SET_OPTION(O_FORCE_BIT);
    break;
  case GNU_CMD_BOTH :
    errorflag = 1;
    command_error_reason = G2_NOTIMPLEMENTED_CMD;
    break;  
  case GNU_CMD_BLACK:
    if (computer_color == BLACK) break;
    if (! IS_IDLE) return EX_CMD_BUSY;
    computer_color = BLACK;
    log_msg("Gullydeckel is now playing BLACK.\n");
    break;
  case GNU_CMD_CLOCK :
  case GNU_CMD_HINT :
  case GNU_CMD_GET :
    errorflag = 1;
    command_error_reason = G2_NOTIMPLEMENTED_CMD;
    break;  
  case GNU_CMD_NEW :
    if (! IS_IDLE) return EX_CMD_BUSY;
    if (!setup_board(NULL)) {
      errorflag = 1;
      command_error_reason = G2_FAILED_CMD;
    }
    else if (init_the_game(&the_game) == 0)
      err_quit("init_the_game");
    /* 
     * in games vs. humans, xboard puts us in
     * force mode and starts a new game. I explicitly
     * reset the force bit on "new".
     */
    RESET_OPTION(O_FORCE_BIT);
    /* reset maximum search depth */
    gameopt.maxdepth = MAX_SEARCH_DEPTH/2;
    log_msg("NEW game.\n");
    break;
  case XB_CMD_XBOARD :
    if (! IS_IDLE) return EX_CMD_BUSY;
    /* Could be a toggle switch? */
    SET_OPTION(O_XBOARD_BIT); 
    SET_OPTION(O_POST_BIT);
    fprintf(stdout, "\n");
    break;
  case GNU_CMD_GO : 
    /* go means that the computer is on move in current position */
    if (IS_PONDERING) return EX_CMD_BUSY;

    if(IS_SEARCHING || IS_ANALYZING) {
      errorflag = 1;
      command_error_reason = G2_NOTLEGALNOW_CMD;
      break;
    }

    if (computer_color != turn) {
      log_msg("setting computer_color to %s.\n",
	      (turn) ? "BLACK" : "WHITE");
      computer_color = turn;
    }

    if (FORCE_ON) {
      log_msg("Leaving force mode.\n");
      RESET_OPTION(O_FORCE_BIT);
    }
    break;
  case GNU_CMD_HELP:
    /* XXX this is only generic help 
       list available commands and offer specific help
    */
    command_help(0);
    break;
  case XB_CMD_OTIM : 
    /* opponents time. We can use this to consider draws, resignation
       and the like */
    sscanf(cmd_buf, "%*s %d", &game_time.o_left);
    break;
  case XB_CMD_TIME :
    sscanf(cmd_buf, "%*s %d", &game_time.left);
    break;
  case XB_CMD_LEVEL :
    /* XXX supports only ICS sudden death controls */
    if (cmd_buf != NULL) {
      sscanf(cmd_buf,"%*s %d %d %d\n",
	     &game_time.moves,
	     &game_time.total,
	     &game_time.increment);
      log_msg("Set up %d %d match.\n",
	      game_time.total,game_time.increment);
      game_time.total *= 60;
      game_time.left = game_time.total * 100;
      /* enable sophisticated time management */
      game_time.use_game_time = 1;
    }
    else  {
      errorflag = 1;
      command_error_reason = G2_NUMPARAM_CMD;
    }
    break;
  case XB_CMD_SETBOARD:
    {
      char epd_buf[128];
      char * epd_start;

      /* XXX I don't understand why game_time needs a backup here? */
      struct game_time_tag saved_game_time; 	

      if (! IS_IDLE) return EX_CMD_BUSY;

      saved_game_time = game_time;
      sscanf(cmd_buf,"%*s %s\n",epd_buf);
      epd_start = strstr(cmd_buf,epd_buf);
      if (epd_start == NULL || !setup_board(epd_start)) {
	errorflag = 1;
	command_error_reason = G2_FAILED_CMD;
	break;
      }
      fprint_board(stdout);
      if (saved_game_time.use_game_time)
	game_time = saved_game_time;
      break;
    }
  case GULLY_CMD_BENCH:
    if(! IS_IDLE) {
      errorflag = 1;
      command_error_reason = G2_NOTLEGALNOW_CMD;
    }
    gameopt.test = CMD_TEST_BENCH;
    do_test();
    break;
  case GULLY_CMD_BOGOMIPS:
    bogomips();
    break;      
  case XB_CMD_ANALYZE:
    if(IS_ANALYZING) {
      errorflag = 1;
      command_error_reason = G2_NOTLEGALNOW_CMD;
    }
    analyse();
    /* Any input remaining will be eaten by the next call to fread_input(). */
    input_pending = 0;
    break;
  case XB_CMD_ANALYSIS_EXIT:
    if(! IS_ANALYZING) {
      errorflag = 1;
      command_error_reason = G2_NOTLEGALNOW_CMD;
    }
    analyse_exit();
    break;
  case XB_CMD_MOVENOW:
    /* only legal if engine is searching */
    if (!IS_SEARCHING) { 
      errorflag = 1;
      command_error_reason = G2_NOTLEGALNOW_CMD;
    }
    else { 
      abort_search = 1;
      log_msg("move NOW - Stopping search.\n");
    }
    break;
  case XB_CMD_ANALYSIS_STATUS:
    if (!IS_ANALYZING) { 
      errorflag = 1;
      command_error_reason = G2_NOTLEGALNOW_CMD;
    }
    else post_analysis_update();
    break;
  case XB_CMD_PING:
    {
      int ping_no;
      
      sscanf(cmd_buf, "%*s %d", &ping_no);
      log_msg("ping-pong %d\n", ping_no);
      fprintf(stdout, "pong %d\n", ping_no);
      break;
    }
  case XB_CMD_BK: 
    /* show available book if moves, if any */
    book_status();
    break;
  case XB_CMD_NAME: /* opponents name */
    log_msg(cmd_buf);
    sscanf(cmd_buf, "%*s %s", the_game->opponent_name);
    break;
  case XB_CMD_RATING:
    log_msg(cmd_buf);
    sscanf(cmd_buf, "%*s %d %d", &the_game->my_elo, &the_game->opponent_elo);
    break;
  case XB_CMD_COMPUTER:
  case XB_CMD_ICS:
  case XB_CMD_PAUSE:
  case XB_CMD_RESUME:
    errorflag = 1;
    command_error_reason = G2_NOTIMPLEMENTED_CMD;
    break;
  case GULLY_CMD_FRITZ:
    SET_OPTION(O_FRITZ_BIT); 
    SET_OPTION(O_XBOARD_BIT);    
    SET_OPTION(O_POST_BIT);    
    break;
  case XB_CMD_DRAW:
    log_msg("Got draw offer. Auto-declined!\n");
    break;
  case XB_CMD_RESULT:
    /* stop any activity, its over now. Make sure that we stay idle. */
    log_msg("Game ended. %s (%d) - %s (%d): %s", 
	    computer_color ? the_game->opponent_name : the_game->my_name,
	    computer_color ? the_game->opponent_elo : the_game->my_elo, 
	    computer_color ? the_game->my_name : the_game->opponent_name,
	    computer_color ? the_game->my_elo : the_game->opponent_elo,
	    cmd_buf);
    
    if(! IS_PONDERING) RESET_OPTION(O_PONDER_BIT);
    
    /* no saved command handling neccessary, so abort search from here */
    if(IS_SEARCHING || IS_PONDERING) abort_search = 1;

    break;
  case XB_CMD_PROTOVER:
    {
      int prot_version; 
      /* satisfy different protocol versions here */
      sscanf(cmd_buf, "%*s %d", &prot_version);
      log_msg("Xboard protocol version %d.\n", prot_version);
      if(prot_version >= 2) { /* XXX experimental */
	fprintf(stdout, "feature done=0 ping=1 setboard=1 name=1\n");
	fprintf(stdout, "feature myname=\"%s\" done=1\n", the_game->my_name);
      }      
    }
    break;
  case XB_CMD_ACCEPTED:
  case XB_CMD_REJECTED:
    log_msg("No action taken: %s", cmd_buf);
    break;
  case GULLY_CMD_RESET:
    /* ignore if not fritz */
    if(! FRITZ_ON) {
      errorflag = 1;
      command_error_reason = G2_NOTLEGALNOW_CMD;
      break;
    }
    /* clear hash tables */
    tt_clear();
    ph_clear();
  default: 
    /* keep errorflags set by command parser */
    if (command_error_reason == G2_NO_ERROR) 
      command_error_reason = G2_UNKNOWN_CMD;
    errorflag = 1;
    break;
  }

  /* setting stdout mode etc. does not work with Win32 */
  fflush(stdout);

  if (errorflag) {
    switch(command_error_reason) {
    case G2_EMPTY_CMD:
      break;
    case G2_AMBIGUOUS_CMD:
      err_msg("Error (ambiguous command): %s", cmd_buf);
      break;
    case G2_UNKNOWN_CMD:
      err_msg("Error (unknown command): %s", cmd_buf);
      break;
    case G2_NOTLEGALNOW_CMD:
      err_msg("Error (command not legal now): %s\n", 
	      cmds[command]);
      break;
    case G2_NUMPARAM_CMD:
      err_msg("Error (number of parameters wrong): %s", 
	      cmd_buf);
      break;
    case G2_FAILED_CMD:
      err_msg("Error (executing command failed): %s", cmd_buf);
      break;
    case G2_NOTIMPLEMENTED_CMD:
      err_msg("Error (command not implemented): %s\n", 
	      cmds[command]);
      break;
    case G2_NO_ERROR:
      err_msg("Error (no reason available): %s", cmd_buf);
      break;
    default:
      err_msg("Error (generic error - sorry.).\n");
    }
    return EX_CMD_FAILURE;
  }
  return EX_CMD_SUCCESS;  
}

/* 
 * handle_ponder_input is called from search() when we are in
 * PONDER-mode and input is available.
 *
 * Uses a static input buffer since input may pile up there and
 * is often not handled immediately.
 *
 * There have been bugs around 2.13pl6 and earlier when (saved) 
 * commands were overwriting each other. We are now more strict
 * and return immediately if this function has set abort_search 
 * earlier.
 */

int
handle_ponder_input(FILE *in, move_t *m, move_t *p)
{
  int num_chars; 
  assert(m && p);

  if (abort_search) return 0;

  /* clear user move */
  m->from_to = 0; 

  /* 
   * read the number of pending input characters into 
   * the input buffer.
   * 
   * Written in great verbosity, to make sure we log those 
   * rare but nasty interrupted syscall conditions.
   * Try to restart the fgets call then.
   */

 DO_FGETS:
  if (fgets(static_inbuf, INPUT_MAXSIZE, in) == NULL) {
    if (feof(in)) {
      /* this means we did not get the number of pending chars right */
      log_ret("EOF encountered while reading ponder input?"); 
      clearerr(in);
      abort_search = 1;
      return 0;
    }
    else { 
      perror("gully2:input.c:handle_ponder_input:fgets");
      log_ret("handle_ponder_input: fgets: %s. Retrying...", strerror(errno));
      goto DO_FGETS; /* oh well */
    }
  }

  num_chars = strlen(static_inbuf);
  /* number of chars  left for next call, if any. */
  input_pending -= num_chars;
  
  if (parse_move(&static_inbuf[0],m)) {
    /* 
     * There is no easy way to verify this move; so just take it 
     * as it is. It will be verified later (in ponder()).
     *
     * Compare this move to the move we are pondering on.
     * if equal, switch to searching state, else abort pondering. 
     */
    
    if (m->from_to == p->from_to)
      /* XXX take care of promotions here */
      global_search_state = SEARCHING;
    else {
      /* We got a move we did not ponder at, stop pondering. */
      abort_search = 1;
      return 0;
    }
  }
  else {
    int i;
    i = read_command(&static_inbuf[0]);
    
    if (do_command(i, NULL) == EX_CMD_BUSY) {
      /* 
       * The  command will be handled when idle.
       * Saved_command is reset after executing
       * this command in idle mode. 
       */ 
      saved_command = i;
      abort_search = 1;
      return 0;
    }
    /* otherwise, we have successfully handled it WHILE pondering */
  }
  
  return 1;
}

/* 
 * handle_search_input is called from search() when we are in
 * SEARCH-mode and input is available.
 *
 * Uses a static input buffer since input may pile up there and
 * is often not handled immediately.
 *
 * This is a simplified version of handle_ponder_input().
 */

int
handle_search_input(FILE *in)
{
  int i, num_chars; 

  if (abort_search) return 0;

  /* 
   * read the number of pending input characters into 
   * the input buffer.
   * 
   * written in great verbosity, to make we log those 
   * rare but nasty interrupted syscall conditions.
   * We try to restart the fgets call then.
   */

 DO_FGETS:
  if (fgets(static_inbuf, INPUT_MAXSIZE, in) == NULL) {
    if (feof(in)) {
      /* this means we did not get the number of pending chars right */
      log_ret("EOF encountered while reading search input?"); 
      clearerr(in);
      abort_search = 1;
      return 0;
    }
    else { 
      perror("gully2:input.c:handle_search_input:fgets");
      log_ret("handle_search_input: fgets: %s. Retrying...", strerror(errno));
      goto DO_FGETS; /* oh well */
    }
  }

  num_chars = strlen(static_inbuf);
  /* number of chars  left for next call, if any. */
  input_pending -= num_chars;
  
  i = read_command(&static_inbuf[0]);    
  if (do_command(i, NULL) == EX_CMD_BUSY) {
    /* 
     * The  command will be handled after search has finished.
     * Saved_command is reset after executing
     * this command in idle mode. 
     */ 
    saved_command = i;
    abort_search = 1;
    return 0;
  }

  log_msg("Handled command %d (%s) while searching.\n", i, static_inbuf);
  return 1;
}

/* 
 * handle_analysis_input is called from search() when we are in
 * ANALYSIS mode and input is available.
 */

int
handle_analysis_input(FILE *in, move_t *m)
{
  int num_chars; 
  assert(m);

  if (abort_search) return 0;

  /* clear user move */
  m->from_to = 0; 

  /* 
   * read the number of pending input characters into 
   * the input buffer.
   * 
   * Written in great verbosity, to make sure we log those 
   * rare but nasty interrupted syscall conditions.
   * Try to restart the fgets call then.
   */

 DO_FGETS:
  if (fgets(static_inbuf, INPUT_MAXSIZE, in) == NULL) {
    if (feof(in)) {
      /* this means we did not get the number of pending chars right */
      log_ret("EOF encountered while reading analysis input?"); 
      clearerr(in);
      abort_search = 1;
      return 0;
    }
    else { 
      perror("gully2:input.c:handle_analysis_input:fgets");
      log_ret("handle_analysis_input: fgets: %s. Retrying...", 
	      strerror(errno));
      goto DO_FGETS; /* oh well */
    }
  }

  num_chars = strlen(static_inbuf);
  /* number of chars  left for next call, if any. */
  input_pending -= num_chars;
  
  if (parse_move(&static_inbuf[0], m)) {
    /* 
     * There is no easy way to verify this move; so just take it 
     * as it is. It will be verified in analyse()).
     *
     * In any case, current analysis is aborted and restarted 
     * with the position after the move we got here.
     */
    abort_search = 1;
    return 0;
  }
  else {
    int i;
    i = read_command(&static_inbuf[0]);
    
    /* There are only few commands which can be handled while analysing
     * ("." and "bk" ). 
     * For the rest, this analysis is aborted.
     */

    if (do_command(i, NULL) == EX_CMD_BUSY) {

      saved_command = i;
      abort_search = 1;
      return 0;
    }
    /* otherwise, we have successfully handled it WHILE analysing. */
  }
  
  return 1;
}

