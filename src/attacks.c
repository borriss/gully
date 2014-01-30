/* $Id: attacks.c,v 1.13 2003-03-02 13:52:50 martin Exp $ */

#include <assert.h>
#include <string.h> /* memset */

#include "chess.h"
#include "board.h"
#include "movegen.h" /* GET_CAP and GET_PRO macros */
#include "attacks.h"
#include "logger.h"
#include "chessio.h"

/* gives the exclusive bit for each piecetype. relies on
   the definition in chess.h!!
*/
#define K 1
#define Q 6 /* QUEEN IS ROOK + BISHOP */
#define R 2
#define B 4
#define N 16

#if 0
#define SEE_DEBUG
#endif

/* only pieces are in the matrix, no pawns */
static int PieceBits[6] = { 0, K, Q, R, B, N };

/* piece values used for SEE (e.g., KNIGHT == BISHOP) */
static int see_piece_value[7] = { 0, 15000, 900, 500, 300, 300 , 100};

/* uses feature of 16*8 board - their relation is uniquely
   identified by their difference.
   128 is added to the difference of two squares to find the
   table entry. Thus, the table is symmetric since it doesn't
   contain pawns.
*/

static int SqRel[256] = {
  0	,0	,0	,0	,0	,0	,0	,0	,0	,B	,0	,0	,0	,0	,0	,0	,
  R	,0	,0	,0	,0	,0	,0	,B	,0	,0	,B	,0	,0	,0	,0	,0	,
  R	,0	,0	,0	,0	,0	,B	,0	,0	,0	,0	,B	,0	,0	,0	,0	,
  R	,0	,0	,0	,0	,B	,0	,0	,0	,0	,0	,0	,B	,0	,0	,0	,
  R	,0	,0	,0	,B	,0	,0	,0	,0	,0	,0	,0	,0	,B	,0	,0	,
  R	,0	,0	,B	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,B	,N	,
  R	,N	,B	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,N	,K|B,
  K|R	,K|B,N	,0	,0	,0	,0	,0	,0	,R	,R	,R	,R	,R	,R	,K|R,
  0	,K|R,R	,R	,R	,R	,R	,R	,0	,0	,0	,0	,0	,0	,N	,K|B,
  K|R	,K|B,N	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,B	,N	,
  R	,N	,B	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,B	,0	,0	,
  R	,0	,0	,B	,0	,0	,0	,0	,0	,0	,0	,0	,B	,0	,0	,0	,
  R	,0	,0	,0	,B	,0	,0	,0	,0	,0	,0	,B	,0	,0	,0	,0	,
  R	,0	,0	,0	,0	,B	,0	,0	,0	,0	,B	,0	,0	,0	,0	,0	,
  R	,0	,0	,0	,0	,0	,B	,0	,0	,B	,0	,0	,0	,0	,0	,0	,
  R	,0	,0	,0	,0	,0	,0	,B	,0	,0	,0	,0	,0	,0	,0	,0
};

/*
if we find that two squares are related according to matrix SqRel,
we use another matrix to look up the direction of the move required
('to walk the vector') if it is a sliding piece (R,B,Q)
*/
     
     static int vector[256] = {
       0	,0	,0	,0	,0	,0	,0	,0	,0	,UP_RIGHT	,0	,0	,0	,0	,0	,0	,
       UP	,0	,0	,0	,0	,0	,0	,UP_LEFT	,0	,0	,UP_RIGHT	,0	,0	,0	,0	,0	,
       UP	,0	,0	,0	,0	,0	,UP_LEFT	,0	,0	,0	,0	,UP_RIGHT	,0	,0	,0	,0	,
       UP	,0	,0	,0	,0	,UP_LEFT	,0	,0	,0	,0	,0	,0	,UP_RIGHT	,0	,0	,0	,
       UP	,0	,0	,0	,UP_LEFT	,0	,0	,0	,0	,0	,0	,0	,0	,UP_RIGHT	,0	,0	,
       UP	,0	,0	,UP_LEFT	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,UP_RIGHT	,0	,
       UP	,0	,UP_LEFT	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,UP_RIGHT,
       UP	,UP_LEFT,0	,0	,0	,0	,0	,0	,0	,RIGHT	,RIGHT	,RIGHT	,RIGHT	,RIGHT	,RIGHT	,RIGHT,
       0	,LEFT,LEFT	,LEFT	,LEFT	,LEFT	,LEFT	,LEFT	,0	,0	,0	,0	,0	,0	,0	,DOWN_RIGHT,
       DOWN	,DOWN_LEFT	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,DOWN_RIGHT	,0	,
       DOWN	,0	,DOWN_LEFT	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,DOWN_RIGHT	,0	,0	,
       DOWN	,0	,0	,DOWN_LEFT	,0	,0	,0	,0	,0	,0	,0	,0	,DOWN_RIGHT	,0	,0	,0	,
       DOWN	,0	,0	,0	,DOWN_LEFT	,0	,0	,0	,0	,0	,0	,DOWN_RIGHT	,0	,0	,0	,0	,
       DOWN	,0	,0	,0	,0	,DOWN_LEFT	,0	,0	,0	,0	,DOWN_RIGHT	,0	,0	,0	,0	,0	,
       DOWN	,0	,0	,0	,0	,0	,DOWN_LEFT	,0	,0	,DOWN_RIGHT	,0	,0	,0	,0	,0	,0	,
       DOWN	,0	,0	,0	,0	,0	,0	,DOWN_LEFT	,0	,0	,0	,0	,0	,0	,0	,0
     };

/* data structure for static exchange evaluator */
static struct see_array_tag {
  piece_t direct[2][16];
  piece_t indirect[2][16];
  int direct_counter[2];
  int indirect_counter[2];
} see_array;

/*  macro for inserting piece/pawn into see_array */
#define LINK_SEE_DIRECT(col,piece)				\
{								\
  int k = (col == WHITE) ? 0 : 1;				\
  see_array.direct[k][see_array.direct_counter[k]++] = (piece);	\
}

#define LINK_SEE_INDIRECT(col,piece)					\
{									\
  int k = (col == WHITE) ? 0 : 1;					\
  see_array.indirect[k][see_array.indirect_counter[k]++] = (piece);	\
}

/* forward decl */
int fill_see_array(int color, const square_t sq);
void reset_see_array(void);

/* does color ctm attack square sq?
* We search our piece list for this and check SqRel for a possible
* attack. If it is a sliding piece, we walk the vector.
* Also, pawns are treated as special case.
*/

int
attacks(int ctm,square_t sq)
{
  int i;
  register plistentry_t *PListPtr, *StopPtr;

  if(ctm==WHITE)
    {
      PListPtr=PList; StopPtr=PList+MaxWhitePiece;
    }
  else
    {
      PListPtr=PList+BPIECE_START_INDEX; StopPtr=PList+MaxBlackPiece;
    }

  /* check for each piece SqRel Matrix*/
  assert(PListPtr != StopPtr);
  do
    {
      if(*PListPtr != NO_PIECE)
	if(SqRel[GET_SQUARE(*PListPtr)+128-sq]
	   & PieceBits[GET_PIECE(*PListPtr)])
	  {
	    /* for king and knight - we are done */
	    if((GET_PIECE(*PListPtr) == KNIGHT) ||
	       (GET_PIECE(*PListPtr) == KING))
	      return ATTACKED;
	    else /* scan vector */
	      {
		int j,next=GET_SQUARE(*PListPtr);

		j=vector[next+128-sq];
		for(;;)
		  {
		    next+=j;
		    if(next==sq) return ATTACKED;
		    
		    assert((next & 0x88)==0);
		    if(BOARD[next] != 0) break;
		  }
	      } /* scan vector */
	  } /* SqRel set for this piece */
    }
  while(++PListPtr != StopPtr);

  /* special code for pawns */
  if(ctm==WHITE)
    {
      if((((i=sq+DOWN_LEFT) & 0x88) == 0)  && BOARD[i] != BOARD_NO_ENTRY
	 && (GET_PIECE(*BOARD[i]) == PAWN)
	 && !(PLIST_OFFSET(BOARD[i]) & BLACK))
	return ATTACKED;
      if((((i=sq+DOWN_RIGHT) & 0x88) == 0)  && BOARD[i] != BOARD_NO_ENTRY
	 && (GET_PIECE(*BOARD[i]) == PAWN)
	 && !(PLIST_OFFSET(BOARD[i]) & BLACK))
	return ATTACKED;
    }
  else
    {
      assert(ctm==BLACK);
      if((((i=sq+UP_LEFT) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY
	 && (GET_PIECE(*BOARD[i]) == PAWN)
	 && (PLIST_OFFSET(BOARD[i]) & BLACK))
	return ATTACKED;
      if((((i=sq+UP_RIGHT) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY
	 && (GET_PIECE(*BOARD[i]) == PAWN)
	 && (PLIST_OFFSET(BOARD[i]) & BLACK))
	return ATTACKED;
    }
  return NOT_ATTACKED;
}

/* 
   static exchange evaluator.
   m is the capture which is evaluated. Must be called before
   the capture is actually made on the board (new!).
   Evaluates whether this capture will be a 'winner' or 'trade'.
   To find this, all pieces attacking the battlefield square are
   put into arrays saying whether the piece directly or indirectly
   attacks the square.
   Using this data, the capture is evaluated.
   Winners, losers and trades are distinguished.
   */

#define SEE_REMOVE_ATT { 			\
  *BOARD[a_from] = 0; 				\
  BOARD[a_from] = BOARD_NO_ENTRY;		\
}

#define SEE_RESTORE_ATT {			\
  BOARD[a_from] = p_sav_att;			\
  *BOARD[a_from] = saved_attacker;		\
}

int
see(int attacking_color,move_t * m)
{
  square_t sq = GET_TO(m->from_to);
  square_t a_from = GET_FROM(m->from_to);
  plistentry_t saved_attacker = *BOARD[a_from]; /* attacker */ 
  plistentry_t *p_sav_att = BOARD[a_from]; /* reference to attacker */
 
  int defending_color = (attacking_color == WHITE) ? BLACK : WHITE;
  /* special promotion code needed */
  int score = see_piece_value[GET_CAP(m->cap_pro)];
  int good_score = score; /* notice lowest winning score at which
			    the defender could then pass (instead
			    of throwing another piece into it; and 
			    making the score possibly worse */
  int risk = see_piece_value[GET_PIECE(*BOARD[a_from])];
  /* best losing/trading score to which the attacker
     could fall back. */
  int bad_score = score - risk; 

  assert(score || GET_PRO(m->cap_pro));
  assert(risk);

#ifdef SEE_DEBUG
  printf("promises win of %d, risks %d [%d..%d]\n",score,-risk,
	 bad_score,good_score);
#endif

  /* note: no score - risk > 0 shortcut, since the *exact* value 
     of the capture is needed */

  /* The capture in question should not be executed on the board until
     after see. Therefore, the piecelist is manipulated "by hand" here,
     e.g. the attacker is temporarly removed from the piecelist!
     */

  SEE_REMOVE_ATT;

  /* reset see_data */
  reset_see_array();

  /* fill the defenders array first, he might not be able to
     capture back (directly) at all */

  if(!fill_see_array(defending_color,sq))
    {
#ifdef SEE_DEBUG
      printf("no direct defender found\n");
#endif
      
      SEE_RESTORE_ATT;
      return score;
    }

  /* if the attacker can't then support his piece, he loses */

  if(!fill_see_array(attacking_color,sq) 
     && see_array.indirect_counter[(attacking_color == WHITE) ? 0 : 1] == 0 )
    {
#ifdef SEE_DEBUG
      printf("no supporting attacker found\n");
#endif
      SEE_RESTORE_ATT;
      return score-risk;
    }

  SEE_RESTORE_ATT;

  /* evaluate capturing sequence here */
  {
    int a, d;
    /* init indices */
    if(attacking_color == WHITE)
      a = 0;
    else
      a = 1;
    
    d = 1 - a;

#ifdef SEE_DEBUG
    printf("def_direct: %d, def_indirect: %d, att_direct: %d, att_indir:%d\n",
	    see_array.direct_counter[d],see_array.indirect_counter[d], 
	    see_array.direct_counter[a],see_array.indirect_counter[a]);
#endif    

    while(1)
      {
	/* defender goes first */

	/* defender can pass ? */
	if(score < 0)
	  {
#ifdef SEE_DEBUG
	    printf("defender passes\n");
#endif
	    return MAX(bad_score,score);
	  }

	if(see_array.direct_counter[d])
	  {
	    score -= risk; /* attacker gone */
	    /* find lowest valued direct defender */
	    if(see_array.direct_counter[d] == 1)
	      risk = see_piece_value[see_array.direct[d][0]];
	    else /* more than one direct defender */
	      {
		int c,low_val= 20000,saved_index=~0;
		for(c = 0; c < see_array.direct_counter[d]; c++)
		  {
		    if(see_piece_value[see_array.direct[d][c]] < low_val)
		      {
			low_val = see_piece_value[see_array.direct[d][c]];
			saved_index = c;
		      }
		  }
		if(saved_index != (see_array.direct_counter[d]-1))
		  {
		    /* exchange in array before taking out */
		    see_array.direct[d][saved_index] = 
		      see_array.direct[d][(see_array.direct_counter[d]-1)];
		  }
		assert(saved_index >= 0 
		       && saved_index < see_array.direct_counter[d]);
		assert(low_val>0 && low_val < 1000);

		risk = low_val;
	      } /* else more defenders */
	    see_array.direct_counter[d]--;
	  }
	else if (see_array.indirect_counter[d])
	  {
	    /* take next indirect defender */
	    score -= risk;
	    risk = see_piece_value[see_array.indirect[d]
				  [--see_array.indirect_counter[d]]];
	  }
	else /* no defender left */
	  {
#ifdef SEE_DEBUG
	    printf("no defender left\n");
#endif
	    return MIN(good_score,score);
	  }

#ifdef SEE_DEBUG
	printf("D:score now: %d, bad_score %d, risk: %d\n",score,
	       bad_score,risk);
#endif

	/* was this a 'safe' capture? */ 
	if((score + risk) < 0)
	  {
#ifdef SEE_DEBUG
	    printf("defense is safe\n");
#endif
	    return MAX(bad_score,score);
	  }
	
	/* adjust bad_score */
	if(score > bad_score)
	  bad_score = score;
	
	/* A T T A C K E R */
	
	/* attacker can safely 'pass' ? */
	if(score > 0) 
	  {
#ifdef SEE_DEBUG
	    printf("attacker passes\n");
#endif
	    return MIN(good_score,score);
	  }

	/* attacker may strike back */
	if(see_array.direct_counter[a])
	  {
	    score += risk; /* defender gone */

	    /* find lowest valued direct attacker */
	    if(see_array.direct_counter[a] == 1)
	      risk = see_piece_value[see_array.direct[a][0]];
	    else /* more than one direct attacker */
	      {
		int c,low_val= 20000,saved_index=~0;
		for(c = 0; c < see_array.direct_counter[a]; c++)
		  {
		    if(see_piece_value[see_array.direct[a][c]] < low_val)
		      {
			low_val = see_piece_value[see_array.direct[a][c]];
			saved_index = c;
		      }
		  }
		if(saved_index != (see_array.direct_counter[a]-1))
		  {
		    /* exchange in array before taking out */
		    see_array.direct[a][saved_index] = 
		      see_array.direct[a][(see_array.direct_counter[a]-1)];
		  }
		assert(saved_index >= 0 
		       && saved_index < see_array.direct_counter[a]);
		assert(low_val>0 && low_val < 1000);

		risk = low_val;
	      } /* end else more attackers */
	    see_array.direct_counter[a]--;
	  }
	else if (see_array.indirect_counter[a])
	  {
	    /* take next indirect attacker */
	    score += risk;
	    risk = see_piece_value[see_array.indirect[a]
				  [--see_array.indirect_counter[a]]];
	  }
	else /* no attacker left */
	  return MAX(bad_score,score);

#ifdef SEE_DEBUG
	printf("A:score now: %d, good_score %d, risk: %d\n",score,
	       good_score,risk);
#endif

	/* adjust good_score */
	if(score < good_score)
	  good_score = score;

	/* is it a winner? */
	if(score - risk > 0)
	  return MIN(good_score,score - risk);
      }
  }

  assert(0);
  return 0;
}

int
fill_see_array(int color, const square_t sq)
{
  int i;
  plistentry_t *PListPtr, *StopPtr;

  /* init plist ptrs and check for pawns */ 
  if(color == WHITE)
    {
      if((((i=sq+DOWN_LEFT) & 0x88) == 0)  && BOARD[i] != BOARD_NO_ENTRY
	 && (GET_PIECE(*BOARD[i]) == PAWN)
	 && !(PLIST_OFFSET(BOARD[i]) & BLACK))
	LINK_SEE_DIRECT(WHITE,PAWN);

      if((((i=sq+DOWN_RIGHT) & 0x88) == 0)  && BOARD[i] != BOARD_NO_ENTRY
	 && (GET_PIECE(*BOARD[i]) == PAWN)
	 && !(PLIST_OFFSET(BOARD[i]) & BLACK))
	LINK_SEE_DIRECT(WHITE,PAWN);

      PListPtr=PList; StopPtr=PList+MaxWhitePiece;
    }
  else
    {
      assert(color == BLACK);
      if((((i=sq+UP_LEFT) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY
	 && (GET_PIECE(*BOARD[i]) == PAWN)
	 && (PLIST_OFFSET(BOARD[i]) & BLACK))
	LINK_SEE_DIRECT(BLACK,PAWN);

      if((((i=sq+UP_RIGHT) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY
	 && (GET_PIECE(*BOARD[i]) == PAWN)
	 && (PLIST_OFFSET(BOARD[i]) & BLACK))
	LINK_SEE_DIRECT(BLACK,PAWN);

      PListPtr=PList+BPIECE_START_INDEX; StopPtr=PList+MaxBlackPiece;
    }

  /* check for each piece SqRel Matrix */
  assert(PListPtr != StopPtr);
  do
    {
      if(*PListPtr != NO_PIECE)
	{
	  if(SqRel[GET_SQUARE(*PListPtr)+128-sq]
	     & PieceBits[GET_PIECE(*PListPtr)])
	    {
	      /* for king and knight its a direct attack always */
	      if((GET_PIECE(*PListPtr) == KNIGHT) 
		 || (GET_PIECE(*PListPtr) == KING))
		{
		  LINK_SEE_DIRECT(color,(GET_PIECE(*PListPtr)));
		}
	      else /* scan vector, but going from target to piece */
	      {
		int j,is_indirect=0,dest=GET_SQUARE(*PListPtr),
		  src=sq;
		
		j=vector[sq+128-dest]; 
		for(;;)
		  {
		    src+=j;
		    if(dest == src)
		      {
			if(!is_indirect)
			  {
			    LINK_SEE_DIRECT(color,GET_PIECE(*PListPtr));
			  }
			else
			  {
			    LINK_SEE_INDIRECT(color,GET_PIECE(*PListPtr));
			  }
			break;
		      }
		    
		    assert((src & 0x88)==0);
		    
		    /* 
		       if piece in between it must attack sq too 
		       does it have to be of the same color too?
		       I think its better if it has to, otherwise this
		       won't be the most promising capture here either.
		       Also, this configurations are unlikely, errors
		       will affect the performance, if any.
		       
		       Ignores color for pieces, 
		       just have to be of the same type */
		    if(BOARD[src])
		      {
			/* We must not do lookup in PieceBits if
			   we found a pawn the board.
			*/
			if(GET_PIECE(*BOARD[src] != PAWN))
			  {
			    if(PieceBits[GET_PIECE(*PListPtr)] &
			       PieceBits[GET_PIECE(*BOARD[src])])
			      is_indirect++;
			  }
			else 
			  /* 
			    For pawns we consider its 
			    position and its color. 
			    Pawn blocking the ray,
			    can be an _attacking_ pawn */
			  {
			    if(PLIST_OFFSET(BOARD[src]) & BLACK)
			      {
				if (sq - src == DOWN_LEFT ||
				    sq - src == DOWN_RIGHT)
				  {
				    is_indirect++;
#ifdef SEE_DEBUG
				    printf("** attacking black pawn.\n");
#endif
				    continue;
				  }
			      }
			    else /* white pawn */
			      if (sq - src == UP_RIGHT ||
				  sq - src == UP_LEFT)
				{
				  is_indirect++;
#ifdef SEE_DEBUG
				  printf("** attacking white pawn.\n");
#endif
				  continue;
				}
#ifdef SEE_DEBUG
			    printf("** serious block (blocker: %x, "
				   "src: %x target: %x)\n",src,dest,sq);
#endif
			    break; /* something seriously blocking the ray */
			  } /* pawn handling */
		      } /* blocker found */
		  } /* for(;;) - walk vector */ 
	      } /* scan vector */
	    } /* SqRel set for this piece */
	}
    }
  while(++PListPtr != StopPtr);

  return see_array.direct_counter[(color == WHITE) ? 0 : 1];
}

void
reset_see_array(void)
{
memset((void*)&see_array, 0, sizeof(see_array));
}
