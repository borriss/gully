/* $Id: execute.c,v 1.25 2003-04-11 16:51:11 martin Exp $ */

#include <assert.h>
#include <stdio.h>

#include "chess.h"
#include "board.h"
#include "movegen.h"
#include "attacks.h"
#include "execute.h"
#include "pvalues.h"
#include "logger.h"
#include "chessio.h"
#include "helpers.h"
#include "hash.h"
#include "init.h" /* extend game history */
#include "repeat.h"

/* attack (incheck) macro */
/* in ? : operator context there is unary operand conversion of
   move_flags[x].white_king_square to int. So cast it back here. */
#define break_if_now_in_check()  if(attacks(turn^32,                    \
	(square_t) ((turn==WHITE) ?	                                \
	(move_flags[ply+1].white_king_square) :			        \
	(move_flags[ply+1].black_king_square))))		       	\
			return 0;

/* color-sensitive attack macro */
#define break_if_black_attacks(sq)  if(attacks(BLACK,(sq)))	\
			return 0;

#define break_if_white_attacks(sq)  if(attacks(WHITE,(sq)))	\
			return 0;

#define simple_move(from,to)	{		\
BOARD[to]=BOARD[from];				\
BOARD[from]=BOARD_NO_ENTRY;			\
PL_NEW_SQ(*BOARD[to],(to));			\
}

#define simple_unmove(from,to)	{		\
BOARD[from]=BOARD[to];				\
BOARD[to]=BOARD_NO_ENTRY;			\
PL_NEW_SQ(*BOARD[from],(from));			\
}


/* forward decl */
int adjust_castling_flags(int ply, const int from, const int to,
			  const int flags);
int rebuild_rep_list(struct the_game_tag *g, int turn);

int
make_move(register const move_t * m, int ply)
{
  square_t from=GET_FROM(m->from_to);
  square_t to=GET_TO(m->from_to);

#if 0
  fprintf(stdout, "ply %d: from %02x to %02x\n", ply, from, to); 
#endif

  assert(m != NULL);
  assert(BOARD[from] != BOARD_NO_ENTRY);
  assert(GET_PIECE(*BOARD[from]) != NO_PIECE);

  /* copy flags */
  /* XXX just_deleted_entry is not reset for the next ply */
  move_flags[ply+1] = move_flags[ply];
  
  move_flags[ply+1].reverse_cnt++;
  /* reset e_p_square, note hash update */
  if (move_flags[ply+1].e_p_square) {
    update_hash_epsq(&move_flags[ply+1].hash, 
		     move_flags[ply+1].e_p_square);
    move_flags[ply+1].e_p_square = 0;
  }

#ifndef NDEBUG
  current_line[ply] = (move_t *) m;
#endif  

  if (!m->special) { /* normal moves */ 
    if (m->cap_pro) { 	/* capture */ 
      assert(GET_PRO(m->cap_pro) == 0);
      assert(BOARD[to] != BOARD_NO_ENTRY);
      assert(BOARD[from] != BOARD_NO_ENTRY);
      
      *BOARD[to] = NO_PIECE;
      move_flags[ply].just_deleted_entry = BOARD[to];
      move_flags[ply+1].reverse_cnt = 0;
      
      update_material(GET_CAP(m->cap_pro));
    }
    else {	/* no capture, *very* normal move... */
      assert(BOARD[to] == BOARD_NO_ENTRY);
      assert(BOARD[from] != BOARD_NO_ENTRY);
    }

    simple_move(from, to);

    /* update king square if necessary! */
    if (GET_PIECE(*BOARD[to]) == KING) {
      if (turn == WHITE)
	move_flags[ply+1].white_king_square = to;
      else
	move_flags[ply+1].black_king_square = to;
    }
    else if (GET_PIECE(*BOARD[to]) == PAWN)
      move_flags[ply+1].reverse_cnt = 0;

      break_if_now_in_check();

      /* basic hash key update */
      update_hash(&move_flags[ply+1].hash , m );

      /* pawn hash update */
      if (GET_PIECE(*BOARD[to]) == PAWN)
	update_pawn_hash(&move_flags[ply+1].phash , m );
      else
	/* other piece could have taken a pawn */
	if (m->cap_pro && (GET_CAP(m->cap_pro) == PAWN))
	  remove_pawn_hash(&move_flags[ply+1].phash , m );

      /* also updates castling flags in hash, if necessary */
      if (move_flags[ply].castling_flags && 
	 adjust_castling_flags(ply, from, to,
			       move_flags[ply].castling_flags)) {
	/* only called if something has changed */
	update_hash_cflags(&move_flags[ply+1].hash,
			   move_flags[ply].castling_flags,
			   move_flags[ply+1].castling_flags);
	move_flags[ply+1].reverse_cnt = 0;
      }
      
      return 1;
  }
  /* special moves */
  
  /* all special moves are irreversible */
  move_flags[ply+1].reverse_cnt = 0;
  
  switch (m->special) {
    /*
     * pawn went two squares forward
     * hardcode PAWN if you want
     * set e_p_square
     */
  case DOUBLE_ADVANCE:
    simple_move(from,to);
    break_if_now_in_check();
    /* tricky */
    move_flags[ply+1].e_p_square = from ^ 0x30;

    /* hash update: double_advance changes ep_flags */
    update_hash(&move_flags[ply+1].hash , m);
    update_hash_epsq(&move_flags[ply+1].hash, 
		     (const unsigned char) (from ^ 0x30));

    /* pawn hash update */
    update_pawn_hash(&move_flags[ply+1].phash , m );
    return 1;
  case EN_PASSANT: {
    square_t cap_sq = (turn == WHITE) ? to + DOWN : to + UP;

    assert(m->cap_pro == PAWN);
    assert(BOARD[to] == BOARD_NO_ENTRY);
    assert(BOARD[from] != BOARD_NO_ENTRY);
    assert((cap_sq & 0x88) == 0 && GET_PIECE(*BOARD[cap_sq]) == PAWN);
    
    *BOARD[cap_sq] = NO_PIECE;
    move_flags[ply].just_deleted_entry = BOARD[cap_sq];
    BOARD[cap_sq] = BOARD_NO_ENTRY;

    simple_move(from,to);
    break_if_now_in_check();
    
    update_material(PAWN);

    /* hash update: changes neither castling nor sets ep_square again 
     * uses special function due to specific capture characteristics
     */
    update_hash_ep_move(&move_flags[ply+1].hash, m);

    /* pawn hash update */
    update_pawn_hash(&move_flags[ply+1].phash , m );
    return 1;
  }
  case CASTLING:
    /* adjust castling flags, extra check for legality
     * ( was square the king crossed attacked?)
     *  also check if we were in check before!
     */
    if (turn == WHITE) {
      assert(from == 0x04 && (to == 0x06 || to == 0x02));
      if (to == 0x06) {	/* 0-0 */
	simple_move(7,5);
	simple_move(from,to);
	break_if_black_attacks(4);
	break_if_black_attacks(5);
	break_if_black_attacks(6);
	move_flags[ply+1].white_king_square = 6;
      }
      else {
	simple_move(0,3);
	simple_move(from,to);
	break_if_black_attacks(4);
	break_if_black_attacks(3);
	break_if_black_attacks(2);
	move_flags[ply+1].white_king_square = 2;
      }

      move_flags[ply+1].castling_flags &= ~WHITE_CASTLING;
      
      /* hash update: specific function required */
      update_hash_castling(&move_flags[ply+1].hash, m);
      update_hash_cflags(&move_flags[ply+1].hash,
			 move_flags[ply].castling_flags,
			 move_flags[ply+1].castling_flags);
      return 1;
    }
    else {
      assert(from==0x74 && (to==0x76 || to==0x72));

      if (to == 0x76) {	/* 0-0 */
	simple_move(0x77,0x75);
	simple_move(from,to);
	break_if_white_attacks(0x74);
	break_if_white_attacks(0x75);
	break_if_white_attacks(0x76);
	move_flags[ply+1].black_king_square=0x76;
      }
      else {
	simple_move(0x70,0x73);
	simple_move(from,to);
	break_if_white_attacks(0x74);
	break_if_white_attacks(0x73);
	break_if_white_attacks(0x72);
	move_flags[ply+1].black_king_square=0x72;
      }

      move_flags[ply+1].castling_flags &= ~BLACK_CASTLING;

      /* hash update: specific function required */
      update_hash_castling(&move_flags[ply+1].hash, m);
      update_hash_cflags(&move_flags[ply+1].hash,
			 move_flags[ply].castling_flags,
			 move_flags[ply+1].castling_flags);
      return 1;
    }
    assert(0);
  case PROMOTION:	
    /*
     * update material
     * update plist - Max{White|Black}Piece
     */
    assert(GET_PRO(m->cap_pro));
    assert(BOARD[from] && GET_PIECE(*BOARD[from]) == PAWN);
    
    /* capture? */
    if (GET_CAP(m->cap_pro)) {
      assert(BOARD[to] != BOARD_NO_ENTRY);
      
      *BOARD[to] = NO_PIECE;
      move_flags[ply].just_deleted_entry = BOARD[to];
	  
      /* special case: have we spoiled castling 
       * by capturing a rook at original square?
       */
      if (move_flags[ply].castling_flags) {
	if (turn == WHITE) {
	  if ((move_flags[ply+1].castling_flags & BLACK_SHORT) &&
	     to == 0x77)
	    move_flags[ply+1].castling_flags &= ~BLACK_SHORT;
	  else
	    if ((move_flags[ply+1].castling_flags & BLACK_LONG) &&
	       to == 0x70)
	      move_flags[ply+1].castling_flags &= ~BLACK_LONG;
	}
	else {
	  if ((move_flags[ply+1].castling_flags & WHITE_SHORT) &&
	     to == 0x07) move_flags[ply+1].castling_flags &= ~WHITE_SHORT;
	  else if ((move_flags[ply+1].castling_flags & WHITE_LONG) &&
		  to == 0) move_flags[ply+1].castling_flags &= ~WHITE_LONG;
	}
	/* reflect this castling flags change in hash */
	if (move_flags[ply+1].castling_flags != 
	   move_flags[ply].castling_flags)
	  update_hash_cflags(&move_flags[ply+1].hash, 
			     move_flags[ply].castling_flags,
			     move_flags[ply+1].castling_flags);

	    } /* castling flag update */

	} /* promoted with capture */

      /* save the entry where this pawn was in plist for undo */
      move_flags[ply].last_promoted = BOARD[from];
      *BOARD[from] = NO_PIECE;
      BOARD[from] = BOARD_NO_ENTRY;

      BOARD[to] = (turn == WHITE) ?
	PList+MaxWhitePiece : PList+MaxBlackPiece;
      *BOARD[to] = MAKE_PL_ENTRY(GET_PRO(m->cap_pro),to);
      if (turn == WHITE)
	MaxWhitePiece++;
      else
	MaxBlackPiece++;

      break_if_now_in_check();      

      update_material_on_promotion(GET_PRO(m->cap_pro), 
				   GET_CAP(m->cap_pro));

      /* finally, update hash key */
      update_hash_prom(&move_flags[ply+1].hash, m);
	
      /* pawn hash update */
      update_pawn_hash(&move_flags[ply+1].phash , m );
      return 1;
      
    case HASHMOVE:
      log_quit("trying to execute hashmove");
    default:
      printf("warning: make_move oops\n");
      assert(0);
    }

  assert(0);
  return 1;
}

/* undoes a move.
 * (plist and board are affected by this,
 * flags are not, since we fall back to move_flags[old_ply])
 */

int 
undo_move(register const move_t * m, int ply)
{
  square_t from =  GET_FROM(m->from_to);
  square_t to	=	GET_TO(m->from_to);

  assert(m != NULL);
  assert(BOARD[from] == BOARD_NO_ENTRY);

#ifndef NDEBUG
  current_line[ply]=NULL;
#endif  

  if (m->special) {	/* undo special move */
    switch(m->special) {
    case DOUBLE_ADVANCE:
      simple_unmove(from,to);
      break;
    case PROMOTION:
      assert(GET_PRO(m->cap_pro));
      assert(BOARD[to] && GET_PIECE(*BOARD[to]) == GET_PRO(m->cap_pro));
      
      /* undo capturing promotion */
      if (GET_CAP(m->cap_pro)) {
	*BOARD[to] = NO_PIECE;
	BOARD[to] = move_flags[ply].just_deleted_entry;
	assert(BOARD[to]);
	*BOARD[to] = MAKE_PL_ENTRY(GET_CAP(m->cap_pro),to);
      }
      else { /* normal promotion */
	*BOARD[to] = NO_PIECE;
	BOARD[to] = BOARD_NO_ENTRY;
      }
      
      BOARD[from] = move_flags[ply].last_promoted;
      *BOARD[from] = MAKE_PL_ENTRY(PAWN,from);
      
      if (turn == WHITE) --MaxWhitePiece;
      else --MaxBlackPiece;
	  
      break;
    case EN_PASSANT: {
      square_t cap_sq = (turn == WHITE) ? to + DOWN : to + UP;
      
      assert(GET_CAP(m->cap_pro) == PAWN);
      
      simple_unmove(from,to);
      BOARD[cap_sq] = move_flags[ply].just_deleted_entry;
      *BOARD[cap_sq] = MAKE_PL_ENTRY(PAWN,cap_sq);	    
      break;
    }
    case CASTLING:
      simple_unmove(from,to);
      /* undo rook move */
      switch(to) {
      case 0x06: simple_unmove(7,5); break;
      case 0x76: simple_unmove(0x77,0x75);break;
      case 0x02: simple_unmove(0,3); break;
      case 0x72: simple_unmove(0x70,0x73); break;
      default: assert(0);
      }
      break;
    default:
      log_msg("Undo_move: couldn't undo special move\n");
      assert(0);
    }
  }
  else { /* normal move undo */
    BOARD[from] = BOARD[to];
    PL_NEW_SQ(*BOARD[from],from);
    
    if (m->cap_pro) {
      /* restore pl_entry - use undo info stored in make_move */
      assert(move_flags[ply].just_deleted_entry
	     != (plistentry_t *)FLAGS_INIT);

      BOARD[to] = move_flags[ply].just_deleted_entry;
      *BOARD[to] = MAKE_PL_ENTRY(GET_CAP(m->cap_pro), to);
    }
    else { /* no capture, *very* normal move... */
      BOARD[to] = BOARD_NO_ENTRY;
    }
  }

  return 1;
}

/*
 * "making a null move" is merely a question of hash updates.
 */

int
make_null_move(int ply)
{

  move_flags[ply+1] = move_flags[ply];
  move_flags[ply+1].reverse_cnt++;
  if (move_flags[ply+1].e_p_square) {
    update_hash_epsq(&move_flags[ply+1].hash, 
		     move_flags[ply+1].e_p_square);
    move_flags[ply+1].e_p_square = 0;
  }
  
  hash_change_turn(&move_flags[ply+1].hash);
  return 1;
}


/* castling can be spoiled by:
 * a) moving our king
 * b) moving a rook for which side castling was still allowed
 * c) capturing an enemy rook (and castling was still allowed
 *  for this side)
 *
 * return: 1 if castling flags have changed, 0 otherwise.
*/

int
adjust_castling_flags(int ply, const int from, const int to, const int flags)
{
  int changed = 0; /* flag */
  
  if (turn==WHITE) {
    if (flags & WHITE_CASTLING) {
      /* king move? */
      if (from == 0x04) {
	move_flags[ply+1].castling_flags &= ~WHITE_CASTLING;
	return 1;
      }
      /* rook move? - note Rh1xh8, spoiling whites and blacks rights!
       * so don't return too early
       */
      if (from == 0x07) {
	if (flags & WHITE_SHORT) {
	  move_flags[ply+1].castling_flags &= ~WHITE_SHORT;
	  changed++;
	}
      }
      else if (from == 0x00 && flags & WHITE_LONG) {
	move_flags[ply+1].castling_flags &= ~WHITE_LONG;
	changed++;
      }
    }
      /* check for capturing enemy rooks */
    if (flags & BLACK_CASTLING) {
      if (to == 0x77 && flags & BLACK_SHORT) {
	move_flags[ply+1].castling_flags &= ~BLACK_SHORT;	  
	return 1;
      }
      if (to == 0x70 && flags & BLACK_LONG) {
	move_flags[ply+1].castling_flags &= ~BLACK_LONG;
	return 1;
      }
    }
    return changed;
  }
  /* black */
  assert(turn == BLACK);

  if (flags & BLACK_CASTLING) {
    /* king move? */
    if (from == 0x74) {
      move_flags[ply+1].castling_flags &= ~BLACK_CASTLING;
      return 1;
    }
    /* rook move? - note Rh8xh1, spoiling whites and blacks rights!
     * so don't return too early
     */
    if (from == 0x77) {
      if (flags & BLACK_SHORT) {
	move_flags[ply+1].castling_flags &= ~BLACK_SHORT;
	changed++;
      }
    }
    else if (from == 0x70 && flags & BLACK_LONG) {
      move_flags[ply+1].castling_flags &= ~BLACK_LONG;
      changed++;
    }
  }
  /* check for capturing enemy rooks */
  if (flags & WHITE_CASTLING) {
    if (to == 0x07 && flags & WHITE_SHORT) {
      move_flags[ply+1].castling_flags &= ~WHITE_SHORT;
      return 1;
    }
    if (to == 0 && flags & WHITE_LONG) {
      move_flags[ply+1].castling_flags &= ~WHITE_LONG;
      return 1;
    }
  }
  return changed;
}

int
make_root_move(struct the_game_tag *g,move_t * m)
{
  assert(g != NULL && g->current_move < g->size);
  
  if (!verify_move(m, current_ply, turn, 0)) return 0;

  if (!make_move(m, 0))
    err_quit("Illegal move");
  
  g->history[g->current_move].m = *m;
  g->history[g->current_move].flags = move_flags[0];
  g->history[g->current_move].reversible = 
    (unsigned char) move_flags[1].reverse_cnt;
  
  move_flags[0] = move_flags[1];
  turn = (turn == WHITE) ? BLACK : WHITE;
  
  if (move_flags[0].reverse_cnt == 0) 
    reset_rep_heads();
  
  if (turn == WHITE)
    *repetition_head_w++ = move_flags[0].hash;
  else
    *repetition_head_b++ = move_flags[0].hash;

  g->current_move++;
  if (g->current_move == g->size)
    extend_the_game(&g);
  
  rep_show_offset();
  
  return 1;
}

/* 
 * needed for takeback function and pondering 
 */
int
unmake_root_move(struct the_game_tag *g,move_t * mp)
{
  move_t m;
  assert(g != NULL);

  if (g->current_move < 1) {
    err_msg("can't go back further!");
    return 0;
  }

  g->current_move--;
  move_flags[0] = g->history[g->current_move].flags;
  
  turn = (turn == WHITE) ? BLACK : WHITE;
  
  if (mp == NULL || mp->from_to  == 0)
    m = g->history[g->current_move].m;
  else
    m = *mp;

  assert(m.from_to && g->history[g->current_move].m.from_to == m.from_to);

  if (!undo_move(&m,0)) 
    err_quit("couldn't undo root move\n");

  if (g->history[g->current_move].reversible == 0)
    /* rebuild rep list from game history */
    rebuild_rep_list(g,turn);
  else {
    if (turn == BLACK) repetition_head_w--;
    else repetition_head_b--;
  }
  
  rep_show_offset();
  
  return 1;  
}

int 
rebuild_rep_list(struct the_game_tag *g, int wtm)
{
  int j,i = g->current_move;

  j = i;
  reset_rep_heads();

  /* look for previous non-reversible move */
  while (--j > 0 && g->history[j].reversible);
  if (++j <= 0) return 0;

  if (((i-j) & 1) == 0) wtm = (wtm == WHITE) ? BLACK : WHITE;

  /* rebuild w and b lists */
  while (i >= j) {
    if (wtm == BLACK) 
      *repetition_head_w++ = g->history[j].flags.hash;
    else
      *repetition_head_b++ = g->history[j].flags.hash;
    j++;
    wtm = (wtm == WHITE) ? BLACK : WHITE;
  }

  return 1;
}

/*
 *  Verifies if move m is a strictly legal move.
 * Intended for checking the PV move before executing and
 * checking the move fed into the program.
 *
 * It also clears ambiguity by the hash move:
 * (hash move has HASHMOVE set as special and may be
 * ambiguous in case of underpromotion).
 * This function changes:
 *  - move_array starting from index
 *  - move_flags[ply+1]
 *  - *m itself 
 */
int 
verify_move(move_t *m, int ply, int turn, int index)
{
  int k,new_index,found = 0;

  assert(ply < MAX_SEARCH_DEPTH);

  new_index = generate_moves(turn,index);

  for (k = index; k < new_index && !found ; k++)
    if (move_array[k].from_to == m->from_to) {
      if (m->special == PROMOTION && m->cap_pro && 
	  GET_PRO(m->cap_pro) != GET_PRO(move_array[k].cap_pro)) {
#ifndef NDEBUG
	log_msg("vrfy: other pro intended?\n");
#endif
	continue;
      }
      if (make_move(&move_array[k],ply)) {
	m->special = move_array[k].special;
	m->cap_pro = move_array[k].cap_pro;
	found = 1;
      }
      undo_move(&move_array[k],ply);
    }

  clear_move_list(index,new_index);
  return found;
}
