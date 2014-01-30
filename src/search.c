/* $Id: search.c,v 1.37 2011-03-05 20:45:08 martin Exp $ */

#include <assert.h>

#include "chess.h"
#include "search.h"
#include "quies.h"
#include "movegen.h"
#include "logger.h"
#include "chessio.h"
#include "attacks.h"
#include "execute.h"
#include "evaluate.h"
#include "helpers.h"
#include "mstimer.h"
#include "history.h"
#include "transref.h"
#include "repeat.h"
#include "input.h"
#include "order.h"
#include "analyse.h"

#define NULL_DEPTH_REDUCTION 2
/* so many plies away from the leafs we do a full ordering */
#define FULL_ORDER_DEPTH 2

/* Ordering threshold is the number of legal moves executed before 
   re-ordering the move_array via pick() is skipped completely.
   After having seen that many moves, take the remaining moves as they
   come in.
   */
#define ORDERING_THRESHOLD 6 

/* globals */
static int last_ply_null = 0;

int
search(const int alpha, const int beta, int n, const int index)
{
  int legal_found = 0, k, value, new_index, old_n = n;
  int tt_from_to = 0, height, flag, best_move_index;
  int best = alpha;

  best_move_index = index;

  assert(n >= 0);

  if (abort_search) return 0;

  gamestat.search_nps++; 

  /* check for input or time every NPS/3 nodes depending on
     NPS magic number in mstimer.h.
   */
  if (++game_time.next_check >  NODES_BETWEEN_TIME_CHECK) {
    assert(global_search_state == SEARCHING || 
	   global_search_state == PONDERING ||
	   global_search_state == ANALYZING);

    game_time.next_check = 0;
    
    /* different things in different context... */
    switch(global_search_state) {
    case SEARCHING:
      /* may set abort_search flag */
      abort_search = time_check(); 
      /* check for user input in search mode to implement move now 
       * command. (may set abort_search)
       */
      if (check_input()) handle_search_input(stdin); 
      break;
    case PONDERING:
      if (check_input()) 
	handle_ponder_input(stdin, &user_move, &ponder_move);
      /* may switch to search state if we hit on the correct move */
      if (IS_SEARCHING) {
	allocate_time(1);
	abort_search += time_check();
      }
      break;
    case ANALYZING:
      /* accepted are legal moves, undo, new, setboard, edit, exit, . , 
       * bk and hint.
       * In all cases except for . analysis is stopped and restarted (except
       * for exit). Information for the bk command should be cached in 
       * analyse().
       */
      if (check_input()) handle_analysis_input(stdin, &user_move);
      break;
    default:
      log_quit("search: unknown state.\n");
      break;
    }
    
    /* abort search (due to exceeded search time or some command) */
    if (abort_search) return 0;
  }

  /* repetition_check */
  if (repetition_check(current_ply, &move_flags[current_ply].hash)) {
    if (alpha < 0 && 0 < beta) 
      cut_pv();

    return REPETITION_DRAW;
  }

  /* transref table lookup */
  if (tt_retrieve(&move_flags[current_ply].hash, &tt_from_to,
		  &value, &height, &flag) == TT_RT_FOUND) {

#if 0
    /* debug */
    if(!current_ply) {
      char buf[3], buf1[3];
      printf("TT: %s-%s flag = %d value= %d height= %d n = %d [%d:%d]\n", 
	     square_name(GET_FROM(tt_from_to),buf),
	     square_name(GET_TO(tt_from_to),buf1), 
	     flag, value, height, n, alpha, beta);
    }
#endif

    if (height >= n) /* sufficient depth makes score valuable */ {
      /* bounds updates are risky */
      switch(flag) {
      case LOWER_BOUND: /* fail high */
	if (value >= beta) return beta;
	break;
      case UPPER_BOUND: /* fail low */
	if (value <= alpha) return alpha;
	break;
      case EXACT_VALUE:
	if (value >= beta) return beta;
	update_pv_hash(tt_from_to);
	return value;
      }
    }
  }

  /* check detection/extension - has to be done before null */
  if (n != 0) 
    if (attacks(turn^32,(square_t) ((turn==WHITE) ? 
	       (move_flags[current_ply].white_king_square) : 	
	       (move_flags[current_ply].black_king_square))) 
       == NOT_ATTACKED)
      n--;

  /* null move */
  
  /* we are using a recursive nullmove with R (depth reduction) == 2 */
  if (!last_ply_null) {
    if (n > NULL_DEPTH_REDUCTION && (old_n > n) && NULL_ON 
       && current_ply) {
      last_ply_null++;
      /* just like a regular make_move */ 
      make_null_move(current_ply);
      turn = (turn == WHITE) ? BLACK : WHITE;
      current_ply++;
      
      value = -search(-beta, -best, n - NULL_DEPTH_REDUCTION, index);
      
      current_ply--;
      turn = (turn == WHITE) ? BLACK : WHITE;
      
      /* KISS : look for cutoffs only, bounds updates proved again
	 problematic, since we might store the move best_move_index
	 points to initially */
      if (value >= beta)
	return beta;
    }
  }
  else
    last_ply_null = 0;
  
  
  new_index = generate_moves(turn, index);

  gamestat.moves_generated_in_search += (new_index - index);
  
  /* 
   * Move ordering.
   * Every candidate move has a key associated with it, the bigger
   * the better. High keys are awarded for being in the killer table,
   * being a capture, being in the history table.
   * Note that value might be altered by null move in the meantime.
   * 
   * Also try using hash move in fail-low situations (TT_MOVE not useful) 
   * if we are at the root 
   */

  if(n >= FULL_ORDER_DEPTH)
    order_root_moves(index, new_index, tt_from_to, n);
  else {
    if(!(flag && TT_MOVE_USEFUL) && current_ply) tt_from_to = 0;
    order_moves(index, new_index, tt_from_to, n);

  }

  assert(current_ply < MAX_SEARCH_DEPTH - 1);
    
  /* execute moves */
  for (k = index; k < new_index; k++) {
    gamestat.moves_looked_at_in_search++;
    
    /* 
     * does the move ordering by putting move with highest key 
     * into slot k
     */

    /* XXX experiment with doing this only near the leafs */
    if ((n >= FULL_ORDER_DEPTH) || (legal_found < ORDERING_THRESHOLD))
	pick(k,new_index);

      /* debug */
#if 0
      if(!current_ply) {
	printf("Searching at root %d [%d..%d]: ", k, index, new_index);
	fprint_move(stdout, &move_array[k]);
	fprintf(stdout, "key = %d\n", move_array[k].key); 
      }
#endif


      if (make_move(&move_array[k], current_ply)) {
	legal_found++;

	/* experimental: update analysis stats when in ply 0 */
	if (!current_ply && IS_ANALYZING)
	  update_analysis_stats(legal_found, k, old_n);
	
	turn = (turn == WHITE) ? BLACK : WHITE;
	current_ply++;
	if (n) {
	  assert( n > 0 );
	  value = -search(-beta, -best, n, new_index);
	}
	else {
	  value = -quies(-beta, -best, new_index);
	}
	current_ply--;
	turn = (turn == WHITE) ? BLACK : WHITE;
	
	undo_move(&move_array[k], current_ply);

	if (value > best) {
	  if (value >= beta) {
	    if (!current_ply) {
	      if (!abort_search) {
		clear_pv(1);
		update_pv(&move_array[k]);
	      }
	    }
	    if (KILLERS_ON && n) update_killers(move_array[k].from_to);
	    
	    tt_store(&move_flags[current_ply].hash,
		     move_array[k].from_to, beta, 
		     old_n, LOWER_BOUND);
	    clear_move_list(index, new_index);
	    return beta;
	  } /* fail high */

	  best = value;
	  best_move_index = k;
	  
	    if (!abort_search) {
	      update_pv(&move_array[k]);
	      if (!current_ply)
		fpost(stdout, old_n, UPDATE, value,
		      (float) time_diff(get_time(), game_time.timestamp));
	    }
	}
      } /* if (make_move()) */
      
      else undo_move(&move_array[k], current_ply); 
  }

  /* transpos store, don´t store if mate */
  if (legal_found) {
    if (best == alpha) {
      /* for ply 0, store the old pv move who has failed low to have it
	 re-searched first. Other plies, we don't have a move (XXX true -?)
      */
      if(!current_ply) {
	tt_store(&move_flags[current_ply].hash,
	       principal_variation[0][0].from_to, best, old_n, UPPER_BOUND);
      }
      else
	tt_store(&move_flags[current_ply].hash,
		 0, best, old_n, UPPER_BOUND);
    }

    else {
      assert(alpha < best && best < beta);
      tt_store(&move_flags[current_ply].hash,
	       move_array[best_move_index].from_to,
	       best, old_n, EXACT_VALUE);	
    }
  }

  clear_move_list(index, new_index);

  /* mate / stalemate stuff */
  if (!legal_found) {
    if (attacks(turn^32,
		(square_t) ((turn==WHITE) ? 
			    (move_flags[current_ply].white_king_square) : 
			    (move_flags[current_ply].black_king_square)))) {
      cut_pv();
      return MATE + current_ply;
    }
    else {
      cut_pv();
      return DRAW;
    }
  }
  
  return best;
}

/* XXX pointer implementation faster */

#define LOW_KEY -20000 /* smaller than a captured king */

void
pick(int start_index, int end_index)
{
  int i = start_index, best_index = start_index, best_key = LOW_KEY;

  while (i < end_index) {
    if (move_array[i].key > best_key) {
      best_key = move_array[i].key;
      best_index = i;
    }
    i++;
  }

  assert(best_key != LOW_KEY);

  /* 
     here: if the best key is 0, history bonus should be added to the
     remaining moves (best key == 0 means that all killers and captures 
     have been tried already).
     */

  /* swap best move with first move */
  if (best_index != start_index) {
    move_t tmp = move_array[start_index];
    
    move_array[start_index] = move_array[best_index];
    move_array[best_index] = tmp;
  }
  
}



