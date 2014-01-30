/* $Id: main.c,v 1.54 2007-01-13 14:06:05 martin Exp $ */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h> /* time */

#ifdef UNIX
#include <syslog.h> /* LOG_* constants */
#endif


#include "chess.h"
#include "chessio.h"
#include "test.h"
#include "init.h"
#include "logger.h"
#include "readopt.h"
#include "input.h"
#include "hash.h"
#include "transref.h"
#include "iterate.h"
#include "execute.h"
#include "repeat.h" /* draw_by_repetition */
#include "book.h"
#include "helpers.h"
#include "mstimer.h"

#ifdef UNIX
/* see logger.{c|h} 
 * See also syslog.conf(5) on how to configure the local
 * syslogd.
 */ 
int use_syslog = 0; 
#endif

static int opponent_move(move_t *);
static int computer_move(int force);
static int play(void);

static void
sig_int(int dummy_arg)
{
  signal(SIGINT,sig_int);
}

int
main(int argc, char** argv)
{
  time_t t;

  print_version(1);

#ifdef UNIX
  /* if (use_syslog) open syslog logging facility 
   * otherwise use dump to log file
   */
  log_open("Gullydeckel2", LOG_PERROR, LOG_USER);
#else /* win32 */
  log_open("Gullydeckel2", 0, 0);
#endif

  /* handle SIGINT */
  if (signal(SIGINT, sig_int) == SIG_ERR)
    log_msg("Can't establish sigint handler");

  /* force line buffered output (necessary at least for for WIN32 */
  if(setvbuf(stdout, NULL, _IONBF, 0) != 0)
    log_msg("Could not set stdout buffering mode.\n");
  
  /* read command line */
  reset_gameoptions();
  if (read_options(argc,argv) == -1)
    err_msg("\nI couldn't fully understand the options.\n"
	    "Type \"quit\" and try \"%s --help\".\n",argv[0]);

  init_hash();

  if (gameopt.transref_size) {
    if ((init_transref_table(gameopt.transref_size)) == -1)
      return -1;
  } else log_msg("main.c: Skipping initialization of main hash table.\n");
    

  if ((init_pawn_table(0)) == -1)
    return -1;

  print_version(2);

  /* test file read skips interaction */
  if (gameopt.test != CMD_TEST_NONE) {
    /* facade for bench cmd and several tests */
    do_test();
    return 0;
  }

  /* setup game history */
  if (!setup_board(NULL))
    err_quit("setup board");
  if (init_the_game(&the_game) == 0)
    err_quit("init_the_game");

  /* opening book */
  if (book_file == NULL) {
    if (BOOK_ON) {
      if (!book_open(DEFAULT_BOOK, &book_file)) {
	err_msg("No default book file: %s", DEFAULT_BOOK);
	RESET_OPTION(O_BOOK_BIT);
      }
      else {
	log_msg("Using default opening book %s.\n", DEFAULT_BOOK);
      }
    }
    else book_file = NULL;
  }
  /* init random number generator */
  time(&t);
  srand(t);

  play();

  log_msg("Never reached!\n");
  quit(); 
  return 0;
}

static int
computer_move(int force)
{
  int score = 0;

  if (!force) {
    reset_gamestats();
    timestamp();
    
    if (!book()) score = iterate(gameopt.maxdepth, SEARCHING, NULL);
  }
  
  global_search_state = IDLING;

  if (!make_root_move(the_game, &principal_variation[0][0])) {
    /* use info provided by iterate_stats */
    if (iterate_stats.max_depth_reached) 
      switch (iterate_stats.reason) {
      case DRAW:
	fprintf(stdout,"1/2-1/2 {Stalemate}\n");
	break;
      case MATE: /* computer mated */
	fprintf(stdout,"%s {%s mates}\n",
		(turn == WHITE) ? "0-1" : "1-0",
		(turn == WHITE) ? "Black" : "White");

	break;
      default:
	break;
      }
    
    /* otherwise there should be a move */
    else err_quit("couldn't exec root move");
    
    return 0;
  }

  fprint_board(stdout);      
  fprint_computer_move(stdout, &principal_variation[0][0]);

  if (draw_by_repetition(&move_flags[0].hash)) {
    fprintf(stdout, "1/2-1/2 {Draw by repetition}\n");
    fflush(stdout);
    return 0;
  }

  /* computer mates */
  if (score == (-MATE-1)) {
    fprintf(stdout,"%s {%s mates}\n",
	    (turn == WHITE) ? "0-1" : "1-0",
	    (turn == WHITE) ? "Black" : "White");
    fflush(stdout);
    return 0;
  }
  
  return 1;
}

static int
opponent_move(move_t *m)
{
  if (!make_root_move(the_game,m))
    return 0;

  if (draw_by_repetition(&move_flags[0].hash)) {
    fprintf(stdout, "1/2-1/2 {Draw by repetition}\n");
    fflush(stdout);
  }

  return 1;
}

/*
 * Main game playing loop.
 * 
 * 1) Reads user input (commands and moves)  
 * 2) Triggers move verification and opponent move execution
 * 3) Triggers search
 * 4) Triggers pondering
 *
 *
 * XXX: Rewrite this logic because we do have computer_color
 * available now. This lets us do things like start pondering
 * if we switch sides; or implement the "both" command.
 * 
 */
static int
play()
{
  int input;

  do {
    input = fread_input(stdin, &user_move);
    
    if (input == GNU_CMD_GO)
      goto comp_move_mark;

#if 1
    if(input == XB_CMD_ANALYSIS_EXIT) 
      err_quit("main.c: play() unexpected input.\n");
#endif

    assert(user_move.from_to != 0);
    if (!opponent_move(&user_move))
      err_quit("cannot make user move\n");
      
    if (FORCE_ON) continue;

  comp_move_mark:
    if (computer_color != turn) {
      log_msg("computer_color = %d, but turn = %d?\n",
	      computer_color, turn);
      continue;
    }
    computer_move(0);
  ponder_mark:
    if (PONDER_ON)
      switch (ponder()) {
      case P_USER_MOVED:
	if (!opponent_move(&user_move))
	  err_quit("cannot make user move\n");
	goto comp_move_mark;
      case  P_USE_PMOVE:
	computer_move(1);
	goto ponder_mark;
      case P_GO:
	goto comp_move_mark;
      case P_WAIT:
      default:
	prompt();
	continue;
      }
  } while(1);

  return 0;
}
