/* $Id: quies.c,v 1.12 2007-01-15 20:35:45 martin Exp $ */

#include <assert.h>
#include <stdio.h>

#include "logger.h"
#include "compile.h"
#include "chess.h"
#include "movegen.h"
#include "chessio.h"
#include "attacks.h"
#include "execute.h"
#include "evaluate.h"
#include "helpers.h"
#include "mstimer.h"
#include "search.h"
#include "quies.h"

int
quies(int alpha,int beta,int index)
{
  int value,k,new_index;
  int best = alpha;
  int fix_val;

  gamestat.quies_nps++; 

  /* count quies node for triggering time_check */
  game_time.next_check++; 

#ifdef QUIES_CHECK
  if(attacks(turn^32,(turn == WHITE) ? 
	     (move_flags[current_ply].white_king_square) : 	
	     (move_flags[current_ply].black_king_square)))
    return search(alpha,beta,0,index);
#endif

  /* fix an evaluation */
  fix_val = value = evaluate(alpha, beta);

  if( value > alpha) {
    if (value >= beta) {
      return beta;
    }
      
    cut_pv();
    
    best = value;
  }
  else 
    /* crude avoidance of unjustified rejection of winning captures
       later on */
    fix_val = (fix_val + max_pos_score < alpha) ? 
      (fix_val + max_pos_score) : fix_val;

  /* generate captures and investigate promising ones */
  new_index = generate_captures(turn, index);

  for(k = index ; k < new_index; k++) {
    int see_score;
    assert(move_array[k].cap_pro);

    /* 
       look only at winners which (in addition) 
       may be able to pull score above alpha.
       
       XXX could be interesting to try to prune w/o
       calling see, e.g. based on cap_pro info.
       also, shortcuts due to "winner anyway" might
       be possible.
    */

    see_score = see(turn,&move_array[k]);
    if (see_score <= 0) continue;
    
    if (see_score + fix_val <= alpha) continue;
    /*
      There is no move ordering attempted. Every capture which
      comes as far as to this point, is tried immediately.
      The assumption is that there aren't usually many good captures
      to order for.
    */
    
    if(make_move(&move_array[k],current_ply)) {
      assert(current_ply < MAX_SEARCH_DEPTH-1);
      
      turn = (turn == WHITE) ? BLACK : WHITE;
      current_ply++;
      
      value= -quies(-beta,-best,new_index);
      
      current_ply--;
      turn= (turn == WHITE) ? BLACK : WHITE;
      
      undo_move(&move_array[k],current_ply);

      if(value > best) {
	if(value >= beta) {
	  clear_move_list(index,new_index);
	  return beta;
	}

	update_pv(&move_array[k]);
	best = value;
      }
    }
    else /* move illegal */
      undo_move(&move_array[k],current_ply);
    }
  
  clear_move_list(index,new_index);
  
  return best;  
}
