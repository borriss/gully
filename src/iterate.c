/* $Id: iterate.c,v 1.21 2007-01-15 20:35:45 martin Exp $ */

/* controls the search */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "chess.h"
#include "chessio.h"
#include "mstimer.h"
#include "test.h"
#include "search.h"
#include "history.h" /* pv_2_killer */
#include "iterate.h"
#include "execute.h"
#include "input.h" /* do_command */
#include "movegen.h"
#include "logger.h"
#include "helpers.h" /* phase */
#include "init.h" /* reset_game_stats */

struct iterate_stats_tag iterate_stats;

int
iterate(int depth,enum global_search_state_tag g_state,
	struct pos_solve_stat_tag *pss)
{
  int i = 1,score = 0,last_score = 0;
  move_t saved_move;
  enum local_search_state_tag { 
    REGULAR_SEARCH, FAIL_HIGH_SEARCH, FAIL_LOW_SEARCH } local_search_state; 

  iterate_stats.max_depth_reached = 0;

  global_search_state = g_state;
  abort_search = 0;

  /* only for search */
  if (IS_SEARCHING) allocate_time(0);
    
  /* this is a good moment to do some pre-evaluation,
     such as seeing which game phase we are in etc. */
  
  phase();

  while(i <= depth) {
    local_search_state = REGULAR_SEARCH;
    
    if(KILLERS_ON && !TRANSREF_ON)
      pv_2_killer();
    
    score = search(last_score - WINDOW,last_score + WINDOW,i,0);
    if(!abort_search) {
      if(score <= last_score - WINDOW) {
	fpost(stdout, i, FAIL_LOW, score,
	      (float) time_diff(get_time(), game_time.timestamp));
	local_search_state = FAIL_LOW_SEARCH;
	memcpy((void *) &saved_move, &principal_variation[0][0],
	       sizeof(saved_move));
	score = search( -INFINITY,last_score - WINDOW,i,0);
      }
      else if(score >= last_score + WINDOW) {
	fpost(stdout, i, FAIL_HIGH, score,
	      (float) time_diff(get_time(), game_time.timestamp));
	local_search_state = FAIL_HIGH_SEARCH;
	score = search( last_score + WINDOW,INFINITY,i,0);
      }
      if(abort_search)
	goto abort;

      fpost(stdout, i, PLY_COMPLETE, score,
	    (float) time_diff(get_time(), game_time.timestamp));
      last_score = score;

      /* test related stuff */
      if(pss != NULL) {
	test_stat.ply_total++;
	update_pss(pss, i-1, &principal_variation[0][0],
		   (float) time_diff(get_time(), game_time.timestamp));
      }
      
    }

    else {
    abort:

      switch(local_search_state) {
      case FAIL_LOW_SEARCH:
	printf("using saved move instead\n");
	memcpy((void*) &principal_variation[0][0],
	       (void*) &saved_move, sizeof(saved_move));
	memset((void*)&principal_variation[0][1], 0,
	       sizeof(move_t));
	score = last_score;
	break;
      case FAIL_HIGH_SEARCH:
	score = MAX(score, last_score);
	break;
      case REGULAR_SEARCH:
	score = last_score;
	break;
      default:
	assert(0);
      }

      /* there may be a command waiting to be executed.
       * see main.c for comments. (Like a MOVE NOW + 
       * { UNDO | NEW} sequence.
       *
       * XXX should probably be handled in main
       */
      if(saved_command) {
	log_msg("Found saved command at end of iterate: (%d)\n", 
		saved_command);
      }

      /* tell result */ 
      fpost(stdout, i, INTERRUPTED, score,
	    (float) time_diff(get_time(), game_time.timestamp));
      
      break;
    }
    
    i++;
  }

  /* check for reaching the maximum search depth. This means the either
     mate, stalemate, draw (by repetition or not sufficient material)
  */
  if(i > depth) {
    iterate_stats.max_depth_reached = 1;
    if(score > (-MATE - 250)) 
      iterate_stats.reason = -MATE;
    else if (score < (MATE + 250)) 
      iterate_stats.reason = MATE;
    else iterate_stats.reason = DRAW;
    log_msg("iterate: reached max search depth, reason == %d.\n",
	    iterate_stats.reason); 
  }

  return score;
}


int 
ponder(void)
{
  int score;
  
  ponder_move = principal_variation[0][1];
  saved_command = 0;
  user_move.from_to = 0;

  /* try to find move to ponder on */
  if(ponder_move.from_to == 0)
    return 0;

#if 0
  printf("pondering on:");
  fprint_move(stdout,&ponder_move);
  printf("\n");
#endif

  if(!make_root_move(the_game,&ponder_move))
    return 0;

  reset_gamestats();
  timestamp();

  score = iterate(gameopt.maxdepth, PONDERING, NULL);
  
  /* returns on many a input */
#if 0
  printf("pondered for %.2f (cmd:%d) (m_from_to %d)\n",
	 time_diff(get_time(),game_time.timestamp),
	 saved_command,user_move.from_to);
#endif

  if(IS_PONDERING) {
    unmake_root_move(the_game,NULL);
    global_search_state = IDLING;
    /* exec saved commands, if any. */
    if(saved_command)  {
      do_command(saved_command, NULL);
      /* handle special cases */
      switch(saved_command) {
      case GNU_CMD_GO:
	if(user_move.from_to)
	  err_msg("warning: both move and go received during pondering.\n");
	saved_command = 0;
	return P_GO;
      case GNU_CMD_NEW: /* no need to use ponder move */
	saved_command = 0;
	return P_WAIT;
      default:
	break;
	  }
      /* reset the command, no matter if it successfully executed */
      saved_command = 0;
    }

    /* In addition to a possibly received command, we got a
     * move. This move is not the move we have pondered about.
     */
    if(user_move.from_to) {
      /* kludge: o-o will have been parsed with wrong assumptions
	 on turn.
	 (see move parser) 
      */
      if(user_move.special == CASTLING) {
	switch(GET_TO(user_move.from_to)) {
	case 0x02: user_move.from_to = FROM_TO(0x74,0x72); break;
	case 0x72: user_move.from_to = FROM_TO(0x04,0x02); break;
	case 0x06: user_move.from_to = FROM_TO(0x74,0x76); break;
	case 0x76: user_move.from_to = FROM_TO(0x04,0x06); break;
	default: err_msg("0-0 correction failed");
	}
      }
	
      if(verify_move(&user_move, 0, turn, 0)) /* user move is valid */
	return P_USER_MOVED;
      else {
	/* Illegal move */
	fprintf(stderr, "Illegal move: ");
	fprint_move(stderr, &user_move);
	fprintf(stderr, " (while pondering)\n");
	  return P_WAIT;
      }
    } 
  }
  else 
    /* state switched to search; use the result of pondering */
    return P_USE_PMOVE;
  
  return 0;
}
