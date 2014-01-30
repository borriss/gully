/* $Id: order.c,v 1.3 2011-03-05 20:45:08 martin Exp $ */

#include <stdio.h>
#include <assert.h>

#include "chess.h"
#include "logger.h"
#include "movegen.h"
#include "attacks.h" /* see */
#include "order.h"
#include "history.h"
#include "execute.h"
#include "quies.h"

#define KILLER_BONUS 80
#define TRANSREF_BONUS 2000 /* should be largest bonus */

/*
  move ordering function to be used in regular search.
  It is called at maximum once per search() call.

  Awards bonus for 
  a) being move from the transposition table
  b) be a winning capture
  c) be a killer
  d) even capture
  e) rest ... normally no bonus.
 
  moves are in move_array[index..end_index].

  If tt_from_to is != 0 it will be tried as first key.

  n is the search depth remaining, e.g. killers are only taken care
  of in basic search.

  */

int
order_moves(int index, int end_index, int tt_from_to, int n)
{
  int i;
  
  if(n && KILLERS_ON)
    /* reset killers for NEXT ply */
    reset_killer_use_count(current_ply+1);

  for (i = index; i < end_index; i++) {
    /* TT move lookup */
    if(tt_from_to && move_array[i].from_to == tt_from_to) {
      /* do whatever needs to be done for the most important move 
	 in the search... */
      move_array[i].key = TRANSREF_BONUS;
      /* should swap moves */
      tt_from_to = 0;
      continue;
    }
    /* give bonus to captures and XXX promotions  */
    if(move_array[i].cap_pro) {
      /* for now, just take the see score + some bonus (
       * a capture is an active move after all).
       */
      move_array[i].key = see(turn, &move_array[i]) + 30;
      continue;
    }
	  
      /* finally, reward killers */
    if(n && KILLERS_ON &&
       ((move_array[i].from_to == Killer[current_ply][0].from_to) ||
	(move_array[i].from_to == Killer[current_ply][1].from_to))) {
      move_array[i].key = KILLER_BONUS;
      continue;
    }
    /* here one could do things like centralisation etc. */
  }

  if(tt_from_to) { 
    /* we should have found a killer move */
    printf("warning: TT move not recognized"
	   "(ply %d maxindex:%d 0x%02x-0x%02x\n",
	   current_ply,end_index,GET_FROM(tt_from_to),GET_TO(tt_from_to));
    log_msg("order: no tt move (ply %d 0x%02x-0x%02x\n",
	    current_ply, GET_FROM(tt_from_to),GET_TO(tt_from_to));
  }

  return 0;
}

/*
 * move ordering based on quiescence search.
 * Done close at the root ply only.
 * 
 * If we have a transref move (tt_from_to != 0) , we use it.  
 * The rest of the moves gets a key based on calling quies()
 * 
 * Ignore killers.
 *
 * Should be enough to order them once (not for all iterations anew)
 */


int
order_root_moves(int index, int end_index, int tt_from_to, int n)
{
  int i;
  
  for (i = index; i < end_index; i++) {

#if 0
    if(current_ply <= 1) {
      printf("ply: %d: ", current_ply);
      fprint_move(stdout, &move_array[i]);
    }
#endif

    if(tt_from_to && move_array[i].from_to == tt_from_to) {
      move_array[i].key = TRANSREF_BONUS;
      /* mark as used  */
      tt_from_to = 0;
      continue;
    }

    if(n && KILLERS_ON &&
       ((move_array[i].from_to == Killer[current_ply][0].from_to) ||
	(move_array[i].from_to == Killer[current_ply][1].from_to))) {
      move_array[i].key = KILLER_BONUS;
      continue;
    }

#if 0
    if (make_move(&move_array[i], current_ply)) {
      turn = (turn == WHITE) ? BLACK : WHITE;
      current_ply++;
      /* do this with a full window, needs testing */
      move_array[i].key = -quies(-INFINITY, INFINITY, end_index);
      current_ply--;
      turn = (turn == WHITE) ? BLACK : WHITE;
      undo_move(&move_array[i], current_ply);


#if 0
      if (current_ply <= 1)
	fprintf(stdout, "Score: %d\n", move_array[i].key);
#endif
    }
    else undo_move(&move_array[i], current_ply);
#endif
  }

  /* sanity */
  if(tt_from_to) { 
    /* we should have found a killer move */
    printf("warning: TT move not recognized"
	   "(ply %d maxindex:%d 0x%02x-0x%02x\n",
	   current_ply,end_index,GET_FROM(tt_from_to),GET_TO(tt_from_to));
    log_msg("order: no tt move (ply %d 0x%02x-0x%02x\n",
	    current_ply, GET_FROM(tt_from_to),GET_TO(tt_from_to));
  }

  return 0;
}

