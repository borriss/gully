/* $Id: init.c,v 1.43 2011-03-05 20:45:07 martin Exp $ */

#include <stdio.h>
#include <stdlib.h> /* calloc */
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "chess.h"
#include "pvalues.h"
#include "plist.h"
#include "board.h"
#include "movegen.h"
#include "init.h"
#include "helpers.h"
#include "logger.h"
#include "evaluate.h" /* get_material_score() */
#include "mstimer.h"
#include "hash.h"
#include "repeat.h"
#include "chessio.h" /* piece_name */
#include "transref.h" /* clear tt table */
#include "history.h" /* clear killers */
#include "version.h"

#ifndef NULL
#define NULL ((void*) 0)
#endif

/* space for testpositions */
static struct epd_buf_tag {
  char fen_buf[128]; /*  forsythe-edwards like position description */
  char turn; /* either 'b'  or 'w' */
  char castling[5]; /* contains string like 'KQkq' or just '-' */
  char ep_square[3]; /* contains the ep square 'e3' or just '-' */
  char bm[TESTPOS_SOL_LENGTH]; /* best move list - e4;Qxg7+\0 */
  char am[TESTPOS_SOL_LENGTH]; /* avoid move list - e4;Qxg7+\0 */
  char id[TESTPOS_ID_LENGTH]; /* description of position */
} epd_buf;
  
/* forward */
int setup_from_fen_string(char* szFen,const char t,const char * castling,
		      const char * ep_sq);
int setup_from_epd(char *buf);

static int validate_epd_buf(char * buf);

static void
reset_board_and_plist(void)
{
  int i;

  /* reset offsets */
  MaxWhitePiece = WPIECE_START_INDEX, MaxWhitePawn = WPAWN_START_INDEX,
    MaxBlackPiece = BPIECE_START_INDEX, MaxBlackPawn = BPAWN_START_INDEX;
  
  for (i = 0; i < 56; i++) PList[i] = NO_PIECE;
  for (i = 0; i < 127; i++) BOARD[i] = BOARD_NO_ENTRY;
}

void 
reset_gamestats(void)
{
  memset((void*)&gamestat, 0, sizeof(struct gamestat_tag));
}

void 
reset_test_stats(void)
{
  memset((void*)&test_stat, 0, sizeof(struct test_stats_tag));
}

/* 
 * Only called by main when the program is freshly started.
 * Not on NEW. 
 */
void 
reset_gameoptions(void)
{
  memset((void*) &gameopt, 0, sizeof(struct gameoptions_tag));

  gameopt.maxdepth = MAX_SEARCH_DEPTH >> 1;
  gameopt.test = CMD_TEST_NONE;
  gameopt.testfile[0] = '\0';
  gameopt.transref_size = DEFAULT_TT_BITS;
  gameopt.options = O_TRANSREF_BIT | O_KILLER_BIT | O_POST_BIT 
    | O_PONDER_BIT | O_NULL_BIT | O_BOOK_BIT;
}

#define NALLOC 80

/* sets up info about the current game and possibly 
 * deletes the old game struct 
 */
int
init_the_game(struct the_game_tag **game)
{
  struct the_game_tag *g;

  /* well, ISO C allows free(NULL), but lets play it safe */
  if (*game != NULL) {
    if((*game)->history != NULL)
      free((*game)->history);
    free(*game);
  }

  g = malloc(sizeof(struct the_game_tag));
  if (g == NULL) return 0; 

  /* room for 20 moves in history, gets extended if necessary */
  g->size = NALLOC;
  g->current_move = 0;
  g->last_in_book = 0;
  g->timeinfo = &game_time; /* link */
  g->history = calloc(g->size, sizeof(game_history_t));

  if (g->history == NULL) {
    free(g);
    return 0;
  }

  g->opponent_name[0] = '\0';
  strcpy(g->my_name, "Gullydeckel ");
  strcat(g->my_name, VERSION);
  g->my_elo = g->opponent_elo = 2000;

  *game = g;
  return 1;
}


int
extend_the_game(struct the_game_tag **game)
{  
  assert(*game != NULL);
  
  log_msg("extending the_game\n");

  (*game)->history = realloc((*game)->history,
			     ((*game)->size + NALLOC) 
			     * sizeof(game_history_t));
  if((*game)->history == NULL) {
    err_msg("realloc failed");
    return 0;
  }
 
  (*game)->size += NALLOC;
  return 1;
}


int
setup_board(char * epd_buf)
{
  clear_move_flags();
  reset_board_and_plist();
  /* special handling for fritz / chessbase, see reset command */
  if(!FRITZ_ON) tt_clear();
  if(!FRITZ_ON) ph_clear();
  reset_killers();
  clear_move_list(0, MAX_MOVE_ARRAY);
  clear_pv(0);

  current_ply = 0;
  max_pos_score = PAWNVALUE;

  reset_gamestats();
  reset_rep_heads();
  init_game_time();

  if (epd_buf == NULL) {
    char default_buf[128];
    
    strcpy(default_buf,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR"
	   " w KQkq - bm e4; id start;"); 
    if (!setup_from_epd(default_buf))
      err_quit("SetUpBoard failed\n");
  }
  else
    if (!setup_from_epd(epd_buf))
      err_quit("SetUpBoard failed\n");

  /* initialize material score */
  assert(current_ply == 0 && (turn == WHITE || turn == BLACK));
  get_material_score(&move_flags[current_ply].w_material,
		     &move_flags[current_ply].b_material);

  /* init extension counters */
  move_flags[current_ply].extension_count = 0;

  /* initialize hash value */
  generate_hash_value(&move_flags[0].hash);

  /* initialize pawn hash value */
  generate_pawn_hash_value(&move_flags[0].phash);

  /* initialize repetition list */
  if (turn == WHITE)
    *repetition_head_w++ = move_flags[0].hash;
  else
    *repetition_head_b++ = move_flags[0].hash;
	      
  /* sanity */
  assert(!(move_flags[0].white_king_square & 0x88) &&
	 !(move_flags[0].black_king_square & 0x88));
  assert((move_flags[0].e_p_square & 0x88) == 0);
  
  return 1;
}


int
get_next_position_from_file(FILE * testfile)
{
  char buf[512]; /* buffer to hold a single position */

  /* read until we got a more or less valid epd buffer */
  do {
    if (fgets(buf,511,testfile) == NULL) return 0;
  } while(validate_epd_buf(buf) <= 0);

  memset((void*) &epd_buf, 0, sizeof(epd_buf));
  setup_board(buf);

  return 1;
}

/* 
 * check the current line, so we don't try to setup empty
 * boards or commented out lines.
 *
 * returns 1 if we think the buffer is worth a try.
 * return 0 on comments (lines started with #) or
 * empty lines.
 */

static int 
validate_epd_buf(char * buf)
{
  /* test for minimum size */
  if(strlen(buf) < 10) { 
    log_msg("init.c:validate_epd_buf:skipped buffer with length %d.",
	    strlen(buf));
    return 0;
  }
  /* allow comments (#) */
  if(buf[0] == '#') return 0;
  
  /* assumed to be valid */
  return 1;
}

/*
  takes buffer in the form as a line read from an epd file and sets up
  the board and all flags.
  Firstly, it tokenizes the buffer into the structure epd_buf.
  For actual board setup, it uses setup_from_fen.

  It expects the following: (example)
  rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - (mandatory)
  and now optionally one or two integers:
  1.irreversible_plies 2. total_move_count
  after that optionally epd commands completed with semicolon:
  a) bm { list of best moves }; move_list separated by spaces.
  a) am { list of moves to avoid}; move_list separated by spaces.
  c) id { position description string };
  
  returns 1 on success,
  returns 0 if we found a problem
  */

int
setup_from_epd(char *buf)
{
  int i,state = 100;
  const char delimiters[] = " ;\n";
  char *token;
  int irr_plies,move_cnt;

  if(!buf) return 0;

  irr_plies = move_cnt = 0;

  /* sscanf will extract the constant parts of buf (fen,turn,castling,ep) */
  i = sscanf(buf,"%s %c %s %s",
	     epd_buf.fen_buf, &epd_buf.turn, epd_buf.castling, 
	     epd_buf.ep_square);
  if(i != 4) {
    err_msg("warning: sscanf has wrong number of arguments (%d)",i);
    if(i <= 3) {
      log_msg("Assuming no ep_sq.\n");
      strcpy(epd_buf.ep_square,"-");
    }
    if(i <= 2) {
      log_msg("Assuming no castling rights.\n");
      strcpy(epd_buf.castling,"-");
    }
    if(i <= 1) {
      log_msg("Assuming white to move.\n");
      epd_buf.turn= 'w';
    }
    if(!i) {
      err_msg("setup_from_epd - empty string? %s\n", buf);
      return 0;
    }
  }
  
  strcpy(epd_buf.bm, "none");
  strcpy(epd_buf.am, "none");
  strcpy(epd_buf.id, "none");
  
  /* tokenize remaining buffer - first skip over the mandatory fields */
  token = strtok(buf, delimiters);
  
  if((token == NULL) || !i) {
    err_msg("empty buf? %s\n",buf);
    state = 0;
  }
  else {
    while(--i) {
      if(!state)
	break;
      token = strtok(NULL, delimiters);
      if(token == NULL)
	state = 0;
    }
  }

  while(state &&  ((token = strtok(NULL, delimiters)) != NULL)) {
    unsigned long j;
    
    if(!strcmp(token,"bm")) {
      state = 1;
      token = strtok(NULL,";");
      if(token)
	strncpy(epd_buf.bm, token, TESTPOS_SOL_LENGTH);
      continue;
    }
    if(!strcmp(token,"am")) {
      state = 1;
      token = strtok(NULL,";");
      if(token)
	strncpy(epd_buf.am, token, TESTPOS_SOL_LENGTH);
      continue;
    }
    if(!strcmp(token,"id")) {
      state = 2;
      token = strtok(NULL,";");
      if(token)
	strncpy(epd_buf.id, token, TESTPOS_ID_LENGTH);
      continue;
    }
    /* token could also be irreversible_plies number or
       total move count */
    j = strtoul(token, (char**) NULL, 10);
    if(state == 100) { /* original state, j means irr_plies */
      irr_plies = MIN(j, 99);
      /* workaround bug with repeat_head underflow */
      if(irr_plies)
	err_msg("bug: cannot use the plies_since_conversion number yet\n");
      irr_plies = 0;
      state = 99; /* read irr_plies state */
      continue;
    }
    if(state == 99) { /* j means total move count */
      move_cnt = j;
      state = 98;
      continue;
    }
  }

  /* setup board */
  if(!setup_from_fen_string(epd_buf.fen_buf, epd_buf.turn,
			    epd_buf.castling, epd_buf.ep_square))
    err_quit("setup_from_fen");

  /* moves since last irreversible move - XXX should take info from
     epd file */
  move_flags[0].reverse_cnt = irr_plies;

  /* provide best moves, avoid moves and id of test position in gamestat 
     structure */
  strncpy(gamestat.testpos_sol, epd_buf.bm, TESTPOS_SOL_LENGTH-1);
  strncpy(gamestat.testpos_avoid, epd_buf.am, TESTPOS_SOL_LENGTH-1);
  strncpy(gamestat.testpos_id, epd_buf.id, TESTPOS_ID_LENGTH-1);

return 1;
}


/*
  takes an forsyth-edwards string (without castling flags, turn, etc.
  and sets up the board and the piecelist accordingly.
  Used by setup_from_epd. Doesn't make much sense alone.
 */

int
setup_from_fen_string(char* szFen,const char t,const char * castling,
		      const char * ep_sq)
{
  int nSquare = 112, i = 0;
  plistentry_t *PListPtr = NULL;

  while(szFen[i]) {
    char cChar = szFen[i];
    
    switch (cChar) {
    case '1':nSquare+=1;break;
    case '2':nSquare+=2;break;
    case '3':nSquare+=3;break;
    case '4':nSquare+=4;break;
    case '5':nSquare+=5;break;
    case '6':nSquare+=6;break;
    case '7':nSquare+=7;break;
    case '8':nSquare+=8;break;
    case '/':nSquare-=24;break;
    case 'R': PListPtr=PList+MaxWhitePiece;
      BOARD[nSquare]=PListPtr;
      *PListPtr=MAKE_PL_ENTRY(ROOK,nSquare);
      nSquare++; MaxWhitePiece++;
      break;
    case 'B': PListPtr=PList+MaxWhitePiece;
      BOARD[nSquare]=PListPtr;
      *PListPtr=MAKE_PL_ENTRY(BISHOP,nSquare);
      nSquare++; MaxWhitePiece++;
      break;
    case 'N': PListPtr=PList+MaxWhitePiece;
      BOARD[nSquare]=PListPtr;
      *PListPtr=MAKE_PL_ENTRY(KNIGHT,nSquare);
      nSquare++; MaxWhitePiece++;
      break;
    case 'Q': PListPtr=PList+MaxWhitePiece;
      BOARD[nSquare]=PListPtr;
      *PListPtr=MAKE_PL_ENTRY(QUEEN,nSquare);
      nSquare++; MaxWhitePiece++;
      break;
    case 'P': PListPtr=PList+MaxWhitePawn;
      BOARD[nSquare]=PListPtr;
      *PListPtr=MAKE_PL_ENTRY(PAWN,nSquare);
      nSquare++; MaxWhitePawn++;
      break;
    case  'K':PListPtr=PList+MaxWhitePiece;
      BOARD[nSquare]=PListPtr;
      *PListPtr=MAKE_PL_ENTRY(KING,nSquare);
      move_flags[0].white_king_square=nSquare;
      nSquare++; MaxWhitePiece++;
      break;
    case 'r': PListPtr=PList+MaxBlackPiece;
      BOARD[nSquare]=PListPtr;
      *PListPtr=MAKE_PL_ENTRY(ROOK,nSquare);
      nSquare++; MaxBlackPiece++;
      break;
    case 'b': PListPtr=PList+MaxBlackPiece;
      BOARD[nSquare]=PListPtr;
      *PListPtr=MAKE_PL_ENTRY(BISHOP,nSquare);
      nSquare++; MaxBlackPiece++;
      break;
    case 'n': PListPtr=PList+MaxBlackPiece;
      BOARD[nSquare]=PListPtr;
      *PListPtr=MAKE_PL_ENTRY(KNIGHT,nSquare);
      nSquare++; MaxBlackPiece++;
      break;
    case 'q': PListPtr=PList+MaxBlackPiece;
      BOARD[nSquare]=PListPtr;
      *PListPtr=MAKE_PL_ENTRY(QUEEN,nSquare);
      nSquare++; MaxBlackPiece++;
      break;
    case 'p': PListPtr=PList+MaxBlackPawn;
      BOARD[nSquare]=PListPtr;
      *PListPtr=MAKE_PL_ENTRY(PAWN,nSquare);
      nSquare++; MaxBlackPawn++;
      break;
    case  'k':PListPtr=PList+MaxBlackPiece;
      BOARD[nSquare]=PListPtr;
      *PListPtr=MAKE_PL_ENTRY(KING,nSquare);
      move_flags[0].black_king_square=nSquare;
      nSquare++; MaxBlackPiece++;
      break;
    default:
      printf("char: %c unexpected\n",cChar);
      return 0;
    }
    i++;
  }
  /* setup turn */
  assert(t == 'b' || t == 'w');

  if(t == 'b') turn = BLACK;
  else turn = WHITE;

  /* setup color computer plays */
  computer_color = (turn == WHITE) ? BLACK : WHITE;

  /* castling flags */
  i=0;
  move_flags[0].castling_flags = 0; 

  while(castling[i]) {
    switch(castling[i]) {
    case 'K': move_flags[0].castling_flags|=WHITE_SHORT;
      break;
    case 'Q': move_flags[0].castling_flags|=WHITE_LONG;
      break;
    case 'k': move_flags[0].castling_flags|=BLACK_SHORT;
      break;
    case 'q': move_flags[0].castling_flags|=BLACK_LONG;
      break;
    case '-': 
      break;
    default: 
      err_msg("castling flag - unexpected char:(%c)",
	      castling[i]);
      break;
    }
    ++i;
  }

  /* ep_square */
  if(ep_sq[0] != '-') {
    move_flags[0].e_p_square = 16*(epd_buf.ep_square[0] - 'a') +
      ep_sq[1] - '1';
    if(((move_flags[0].e_p_square >> 4) != 0x05  && 
	(move_flags[0].e_p_square >> 4) != 0x02) ||
       (move_flags[0].e_p_square & 0x88)) {
      err_msg("correcting invalid ep_sq: 0x%x\n",
	      move_flags[0].e_p_square);
      move_flags[0].e_p_square = 0;
    }
  }
  else
    move_flags[0].e_p_square = 0;

return 1;
}

/* 
 * handles xboards edit command:
 * reads piece positions in the form: Ke1,Pb5 etc.
 * 'c' changes the side, '.' ends setup.
 * typically xboard sends after edit like this:
 * 1. 'white' (meaning the first piece is a white one)
 * 2. '#' (unknown semantics)
 * 3. 'Ke1' 'Nb1' and so on.
 * 4. 'c' (change side)
 * 5. 'Ke8', 'Bc8' and so on.
 * 6. '.' we are done here.
 * Note!
 * turn is determined before edit is sent! Xboard probably sends a 'new'
 * followed by 'force' and 'a2a3' to force blacks turn in this position!
 *
 * This function then transforms this into an fen string and uses 
 * setup_board()
 */

int
edit_board(char *fen_buf)
{
#define INBUFSIZE 128 
  char board[128]; /* for consistency use 128 square board */
  char inbuf[INBUFSIZE];
  int col,piece,square,cflags;

  /* clear Board first */
  memset((void*)board,0,128);
  col = WHITE;

  printf("edit mode: Nc3 , c (change side), . (end editing) are valid cmds\n");

  for (;;) 
    {
      fgets(inbuf,INBUFSIZE,stdin);

      /* paranoia */
      if(!strncmp(inbuf,"black",5))
	{
	  col = BLACK;
	  continue;
	}

      /* extreme paranoia */
      if(!strncmp(inbuf,"white",5))
	{
	  col = WHITE;
	  continue;
	}

      if(!strncmp(inbuf,".",1))
	break;

      if(!isalpha(inbuf[0]))
	{
	  printf("ignoring: %c\n",inbuf[0]);
	  continue;
	}

      switch(toupper(inbuf[0]))
	{
	case 'K': piece = KING; break;
	case 'Q': piece = QUEEN; break;
	case 'R': piece = ROOK; break;
	case 'B': piece = BISHOP; break;
	case 'N': piece = KNIGHT; break;
	case 'P': piece = PAWN; break;
	case 'C': col = (col == WHITE) ? BLACK : WHITE;
	  continue;
	default:
	  printf("unrecognized: %c, returning.\n",inbuf[0]);
	  return 0;
	}
      
      if(inbuf[1]<'a' || inbuf[1]>'h' || inbuf[2]<'1' || inbuf[2]>'8')
	{
	  printf("unexpected: inbuf[1]: %c inbuf[2]: %c\n",
		 inbuf[1],inbuf[2]);
	  return 0;
	}
      else
	  square = ((inbuf[2]-'1') << 4) + inbuf[1]-'a';


      assert((square & 0x88) == 0);

      board[square] = piece | col;
    }

  /* use kludge about castling availability (ep is not known either) */
  cflags = 0;
  if(board[0x04] == (KING | WHITE))
    {
      if(board[0x07] == (WHITE | ROOK))
	cflags |= WHITE_SHORT;
      if(board[0x00] == (WHITE | ROOK))
	cflags |= WHITE_LONG;
    }
  if(board[0x74] == (KING | BLACK))
    {
      if(board[0x77] == (BLACK | ROOK))
	cflags |= BLACK_SHORT;
      if(board[0x70] == (BLACK | ROOK))
	cflags |= BLACK_LONG;
    }

  /* now build fen-string */
  board128_2_fen(board,fen_buf,turn,cflags,0,0,0);

  return 1;
}

/*
 * something about FEN and EPD (from a rgcc message of Steven Edwards)
 * FEN = Forsyth Edwards Notation. 
 * FEN has always 6 fields. Example (starting position)
 * rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
 * that is: position turn castling_flags ep_square 
 *          plies_since_last_capture_or_pawn_move full_move_number
 *
 * EPD has always the first 4 fields of FEN + optional operations
 * (such as best move "bm" or "id", separated by semicolon.
 *
 * This function fills fen_buf and uses the information provided
 * in the other fields.
 */
int
board128_2_fen(const char * board128,char *fen_buf,const int turn,
	       const int castling_flags, const int ep_sq,
	       const int no_irreversible, const int move_no)
{
  int i,free_cnt,row,col;
  char tmp[256],*pos,piece;

  assert(fen_buf);
  row = 7; pos = tmp;

  while(row >= 0)
    {
      free_cnt = 0;
      for(col = 0; col < 8; col++)
	{
	  i = (row << 4) + col;
	  assert((i & 0x88) == 0);

	  if(!board128[i])
	    ++free_cnt;
	  else
	    {
	      if(free_cnt)
		{
		  *pos++ = free_cnt + 0x30;
		  free_cnt = 0;
		}
	      piece = piece_name(board128[i] & 0x0f);
	      if((board128[i] & BLACK) == 0)
		piece = toupper(piece);
	      *pos++ = piece;
	    }
	}

      if(free_cnt)
	  *pos++ = free_cnt + 0x30;
      
      if(row)
	*pos++ = '/';

      --row;
    }

  *pos++ = ' ';

  /* 2nd field: turn */
  if(turn == WHITE)
    *pos++ = 'w';
  else
    *pos++ = 'b';

  *pos++ = ' ';

  /* 3rd field: Castling availability */
  if(!castling_flags)
     *pos++ = '-';
  else
    {
      if(castling_flags & WHITE_SHORT) *pos++ = 'K';
      if(castling_flags & WHITE_LONG) *pos++ = 'Q';
      if(castling_flags & BLACK_SHORT) *pos++ = 'k';
      if(castling_flags & BLACK_LONG) *pos++ = 'q';
    }

  *pos++ = ' ';
  
  /* 4: ep_square */
  if(!ep_sq)
    *pos++ = '-';
  else
    {
      *pos++ = (ep_sq & 0x0f) + 'a';
      *pos++ = (ep_sq >> 4) + '1';
    }

  *pos++ = ' ';

  /* 5: plies since last irreversible move */

  *pos++ = no_irreversible + 0x30;
  *pos++ = ' ';

  /* 6: total move count */
  *pos++ = move_no + 0x30;
  *pos++ = ' ';

  *pos++ = 0;
  printf("tmp: %s\n",tmp);

  strcpy(fen_buf,tmp);

  return 1;
}
