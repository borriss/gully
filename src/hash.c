/* $Id: hash.c,v 1.10 2003-03-02 13:52:56 martin Exp $ */

#include <assert.h>

#include "chess.h"
#include "movegen.h"
#include "hash.h"
#include "logger.h"

#define ALTER_TURN hash_array64[0][0][8]
#define EP_HASHVAL(sq) (hash_array64[0][0][sq+8])
#define CASTLING_HASH_WS hash_array64[0][0][9]
#define CASTLING_HASH_WL hash_array64[0][0][10]
#define CASTLING_HASH_BS hash_array64[0][0][11]
#define CASTLING_HASH_BL hash_array64[0][0][12]

/* classic random number algorithm.
   (see Knuth "The Art of Programming", vol.2, pp. 26-27.)
   y(n) = y(n - 24) + y(n - 55) mod 2^32
   this implementation of random32 heavily borrowed from Robert Hyatt. Thx!
    
   huge array (12288 bytes) holding 64bit random numbers for any piece
   on any square. This will practically uniquely identify any board 
   position, including enpassant rights and castling flags.
   Half of it is wasted (128 squares) but access is easier.
   */
position_hash_t hash_array64[2][6][128];

unsigned int 
random32(void)
{
  /*
    random numbers from Mathematica 2.0.
    SeedRandom = 1;
    Table[Random[Integer, {0, 2^32 - 1}]
    */
  static unsigned long x[55] = {
    1410651636UL, 3012776752UL, 3497475623UL, 2892145026UL, 1571949714UL,
    3253082284UL, 3489895018UL, 387949491UL, 2597396737UL, 1981903553UL,
    3160251843UL, 129444464UL, 1851443344UL, 4156445905UL, 224604922UL,
    1455067070UL, 3953493484UL, 1460937157UL, 2528362617UL, 317430674UL, 
    3229354360UL, 117491133UL, 832845075UL, 1961600170UL, 1321557429UL,
    747750121UL, 545747446UL, 810476036UL, 503334515UL, 4088144633UL,
    2824216555UL, 3738252341UL, 3493754131UL, 3672533954UL, 29494241UL,
    1180928407UL, 4213624418UL, 33062851UL, 3221315737UL, 1145213552UL,
    2957984897UL, 4078668503UL, 2262661702UL, 65478801UL, 2527208841UL,
    1960622036UL, 315685891UL, 1196037864UL, 804614524UL, 1421733266UL,
    2017105031UL, 3882325900UL, 810735053UL, 384606609UL, 2393861397UL };

  static int j, k, init = 1;
  static unsigned long y[55];
  unsigned long ul;
  
  if (init)
    {
      int i;
    
      init = 0;
      for (i = 0; i < 55; i++) y[i] = x[i];
      j = 24 - 1;
      k = 55 - 1;
    }
  
  ul = (y[k] += y[j]);
  if (--j < 0) j = 55 - 1;
  if (--k < 0) k = 55 - 1;
  return((unsigned int)ul);
}

/* deterministic fill of the huge rnd64 array */
void 
init_hash(void)
{
  int i,j,k;
  
  for(k=0;k<2;k++)
    for(j=0;j<6;j++)
      for(i=0;i<128;i++)
	{
	  hash_array64[k][j][i].part_one = random32();
	  hash_array64[k][j][i].part_two = random32();
	}
}

/*
  sets hashed_position according to current board position 
  and turn (used for initialization).
  Uses castling and ep flags from move_flags[current_ply].
  */
int 
generate_hash_value(position_hash_t * h)
{
  plistentry_t *PListPtr, *StopPtr;
  
  SET64(*h,0l,0l);
  /* scan the plist */
  PListPtr = PList; StopPtr=PList+PLIST_MAXENTRIES;
  
  while(PListPtr != StopPtr)
    {
      if(*PListPtr != NO_PIECE)
	{
	  int color_index = (PListPtr < PList+BPIECE_START_INDEX) ? 0 : 1;
	  
	  assert((GET_PIECE(*PListPtr))-1 < 6);
	  assert(GET_SQUARE(*PListPtr) < 128);
	  
	  XOR64(*h, hash_array64[color_index]
		[(GET_PIECE(*PListPtr))-1][GET_SQUARE(*PListPtr)]);
	}
      ++PListPtr;
    }

  /* turn */
  if(turn == BLACK)
    XOR64(*h, ALTER_TURN);

  /* castling */
  if(move_flags[current_ply].castling_flags)
    {
      assert(move_flags[current_ply].white_king_square == 0x04
	     || move_flags[current_ply].black_king_square == 0x74);

      if(move_flags[current_ply].castling_flags & WHITE_SHORT)
	XOR64( *h , CASTLING_HASH_WS);
      if(move_flags[current_ply].castling_flags & WHITE_LONG)
	XOR64( *h , CASTLING_HASH_WL);
      if(move_flags[current_ply].castling_flags & BLACK_SHORT)
	XOR64( *h , CASTLING_HASH_BS);
      if(move_flags[current_ply].castling_flags & BLACK_LONG)
	XOR64( *h , CASTLING_HASH_BL);
    }

  /* en passant */
  if(move_flags[current_ply].e_p_square)
    XOR64( *h , EP_HASHVAL(move_flags[current_ply].e_p_square));

  return 1;
}

/*
   UPDATE HASH POSITION VALUE
   ( called from make_move )
   Function: incrementally updates position hash value

   Updating hashvalue:
   The main hash update function is below:
   It removes piece from origin and puts it to destination by
   xor'ing them in and out.

   In some special situations 
   separate functions for updating castling and ep status below.
   
   Check make_move() for where all those are called.
   Captured pieces are XOR'ed out.
*/

/* XXX test whether passing from, to etc. separately is better */
void 
update_hash(position_hash_t * h, const move_t * m)
{
  int color_index = (turn == WHITE) ? 0 : 1;
  int from = GET_FROM(m->from_to), to = GET_TO(m->from_to);
  piece_t piece = GET_PIECE(*BOARD[to]);

  /* sanity: check whether color_index corresponds to
     move - note that move is already made on imaginary board
     */
  assert(PLE_IS_VALID(BOARD[to]));
  assert((PLIST_OFFSET(BOARD[to]) & (32 * color_index))
	 == (32 * color_index));

  assert((m->special == NORMAL_MOVE) || (m->special == DOUBLE_ADVANCE));

  /* turn changes with every move */
  XOR64(*h, ALTER_TURN);  

  XOR64(*h, hash_array64[color_index][piece-1][from]);
  XOR64(*h, hash_array64[color_index][piece-1][to]);

  if(m->cap_pro)
    {
      assert(GET_PRO(m->cap_pro) == 0);
      assert(GET_CAP(m->cap_pro) <= PAWN);

      XOR64(*h , hash_array64[color_index^1][GET_CAP(m->cap_pro)-1][to]);
    }
}

/* ep_square has changed */
void 
update_hash_epsq(position_hash_t * h, const square_t epsq)
{
  assert((epsq >= 0x20 && epsq <= 0x27) || (epsq >= 0x50 && epsq <= 0x57));

  XOR64(*h , EP_HASHVAL(epsq));
}

/* ep_move  
   responsible for all changes an ep_move implies
   (move capturing pawn; remove captured pawn)
   As for the other functions: move has already been
   exec'ed on imaginary board.
   Clearing the e_p square in the hash value is already
   done.
*/
void 
update_hash_ep_move(position_hash_t * h, const move_t * m)
{
  int color_index = (turn == WHITE) ? 0 : 1;
  int from = GET_FROM(m->from_to), to = GET_TO(m->from_to);

  assert(GET_PIECE(*BOARD[to]) == PAWN);

  /* turn changes with every move */
  XOR64(*h, ALTER_TURN);

  XOR64(*h, hash_array64[color_index][PAWN-1][from]);
  XOR64(*h, hash_array64[color_index][PAWN-1][to]);

  /* to^10 (or epsq^10) should give the square the enemy 
     pawn should be removed from */
  assert(((turn == WHITE) && ((to^0x10) == to + DOWN)) ||
	 ((turn == BLACK) && ((to^0x10) == to + UP)));
  XOR64(*h, hash_array64[color_index^1][PAWN-1][to^0x10]);
  
}

/* castling move 
   everything that needs to be done after a castling was played.
   except for clearing the ep_square (if necessary) it has
   to do it all.
   It does however _not_ deal with updating the castling
   flags.
*/
void 
update_hash_castling(position_hash_t * h, const move_t * m)
{
  XOR64(*h, ALTER_TURN);

  /* the destination square says it all */
  switch(GET_TO(m->from_to))
    {
    case 0x06: /* e1g1 */
      assert(turn == WHITE && GET_FROM(m->from_to) == 0x04);
      XOR64(*h, hash_array64[0][KING-1][0x04]);
      XOR64(*h, hash_array64[0][KING-1][0x06]);
      XOR64(*h, hash_array64[0][ROOK-1][0x07]);
      XOR64(*h, hash_array64[0][ROOK-1][0x05]);
      break;
    case 0x02: /* e1c1 */
      assert(turn == WHITE && GET_FROM(m->from_to) == 0x04);
      XOR64(*h, hash_array64[0][KING-1][0x04]);
      XOR64(*h, hash_array64[0][KING-1][0x02]);
      XOR64(*h, hash_array64[0][ROOK-1][0x00]);
      XOR64(*h, hash_array64[0][ROOK-1][0x03]);
      break;
    case 0x76: /* e8g8 */
      assert(turn == BLACK && GET_FROM(m->from_to) == 0x74);
      XOR64(*h, hash_array64[1][KING-1][0x74]);
      XOR64(*h, hash_array64[1][KING-1][0x76]);
      XOR64(*h, hash_array64[1][ROOK-1][0x77]);
      XOR64(*h, hash_array64[1][ROOK-1][0x75]);
      break;
    case 0x72: /* e8c8 */
      assert(turn == BLACK && GET_FROM(m->from_to) == 0x74);
      XOR64(*h, hash_array64[1][KING-1][0x74]);
      XOR64(*h, hash_array64[1][KING-1][0x72]);
      XOR64(*h, hash_array64[1][ROOK-1][0x70]);
      XOR64(*h, hash_array64[1][ROOK-1][0x73]);
      break;
    default:
      err_msg("update_hash_castling - invalid destination\n");
    }
}

/* castling flags have changed */
void 
update_hash_cflags(position_hash_t * h, const int old_flags,
			const int new_flags)
{
  assert(old_flags > new_flags);

  if(old_flags & WHITE_SHORT && !(new_flags & WHITE_SHORT))
    XOR64( *h , CASTLING_HASH_WS);
  if(old_flags & BLACK_SHORT && !(new_flags & BLACK_SHORT))
    XOR64( *h , CASTLING_HASH_BS);
  if(old_flags & WHITE_LONG && !(new_flags & WHITE_LONG))
    XOR64( *h , CASTLING_HASH_WL);
  if(old_flags & BLACK_LONG && !(new_flags & BLACK_LONG))
    XOR64( *h , CASTLING_HASH_BL);
}

/* something promoted 
   1) remove promoting pawn
   2) insert promoted piece
   3) remove captured piece (if any)
*/
void 
update_hash_prom(position_hash_t * h, const move_t * m)
{
  int color_index = (turn == WHITE) ? 0 : 1;
  int from = GET_FROM(m->from_to), to = GET_TO(m->from_to);

  assert(GET_PIECE(*BOARD[to]) == GET_PRO(m->cap_pro));

  XOR64(*h, ALTER_TURN);

  /* remove pawn */
  XOR64(*h , hash_array64[color_index][PAWN-1][from]);
  /* insert new piece */
  XOR64(*h , hash_array64[color_index][GET_PRO(m->cap_pro)-1][to]);

  /* capture? */
  if(GET_CAP(m->cap_pro))
    {
      assert(GET_CAP(m->cap_pro) <= PAWN);

      XOR64(*h , hash_array64[color_index^1][GET_CAP(m->cap_pro)-1][to]);
    }
}

/* needed for nullmove - XXX should be made inline */
void
hash_change_turn(position_hash_t *h)
{
  XOR64(*h, ALTER_TURN);  
}

/***********************************************************************

  PAWN HASHING

  ********************************************************************/


/*
  Initializes pawn hash value.
  Turn and ep_flags are ignored.
  */
int 
generate_pawn_hash_value(position_hash_t * h)
{
  plistentry_t *PListPtr, *StopPtr;
  
  SET64(*h,0l,0l);
  /* scan the plist for white pawns */
  PListPtr = PList + WPAWN_START_INDEX;
  StopPtr = PList + MaxWhitePawn; 
  
  while(PListPtr != StopPtr)
    {
      if(*PListPtr != NO_PIECE)
	{
	  assert((GET_PIECE(*PListPtr)) == PAWN);
	  assert(GET_SQUARE(*PListPtr) < 128);
	  
	  XOR64(*h, hash_array64[0][PAWN-1][GET_SQUARE(*PListPtr)]);
	}
      ++PListPtr;
    }

  /* scan the plist for black pawns */
  PListPtr = PList + BPAWN_START_INDEX;
  StopPtr = PList + MaxBlackPawn; 
  
  while(PListPtr != StopPtr)
    {
      if(*PListPtr != NO_PIECE)
	{
	  assert((GET_PIECE(*PListPtr)) == PAWN);
	  assert(GET_SQUARE(*PListPtr) < 128);
	  
	  XOR64(*h, hash_array64[1][PAWN-1][GET_SQUARE(*PListPtr)]);
	}
      ++PListPtr;
    }

  return 1;
}


/*
  incrementally update pawn hash key.
  called from make_move() *AFTER* the
  move has been made on the imaginary board and checked
  for legality.
  Catches the two special cases (e_p and promotion) which
  are relevant (The other special moves are no different 
  from normal moves).

  turn is ignored for now, but might be important for some
  pawn races?
  */

void 
update_pawn_hash(position_hash_t * h, const move_t * m)
{
  int color_index = (turn == WHITE) ? 0 : 1;
  int from = GET_FROM(m->from_to), to = GET_TO(m->from_to);
  /*   piece_t piece = GET_PIECE(*BOARD[to]); */

  /* sanity: check whether color_index corresponds to
     move - note that move is already made on imaginary board
     */
  assert(PLE_IS_VALID(BOARD[to]));
  assert((PLIST_OFFSET(BOARD[to]) & (32 * color_index))
	 == (32 * color_index));

  switch(m->special) 
    {
    
    case NORMAL_MOVE:
      assert(GET_PRO(m->cap_pro) == 0);
      /* did we capture a pawn? */
      if((m->cap_pro) && (GET_CAP(m->cap_pro) == PAWN))
	XOR64(*h , hash_array64[color_index^1][PAWN-1][to]);
      /* fall thru */
    case DOUBLE_ADVANCE:
    XOR64(*h, hash_array64[color_index][PAWN-1][from]);
    XOR64(*h, hash_array64[color_index][PAWN-1][to]);
    break;
    /* now for some rare special cases */
    case EN_PASSANT: 
      /* quite similar to regular ep update (except for turn) */
      XOR64(*h, hash_array64[color_index][PAWN-1][from]);
      XOR64(*h, hash_array64[color_index][PAWN-1][to]);
      
      /* to^10 (or epsq^10) should give the square the enemy 
	 pawn should be removed from */
      assert(((turn == WHITE) && ((to^0x10) == to + DOWN)) ||
	     ((turn == BLACK) && ((to^0x10) == to + UP)));
      XOR64(*h, hash_array64[color_index^1][PAWN-1][to^0x10]);
      break;
    case PROMOTION: /* actually quite easy */
      assert(GET_PIECE(*BOARD[to]) == GET_PRO(m->cap_pro));

      /* remove pawn */
      XOR64(*h , hash_array64[color_index][PAWN-1][from]);
      /* 
	 do not insert new piece and don't care for capturing
	 promotions.
       */
      break;
    default:
      err_quit("update_pawn_hash - never reached");
    }
      
}

void
remove_pawn_hash(position_hash_t * h, const move_t * m)
{
  assert(GET_PRO(m->cap_pro) == 0);
  assert(GET_CAP(m->cap_pro) == PAWN);
  assert(PLE_IS_VALID(BOARD[GET_TO(m->from_to)]));

  XOR64(*h , hash_array64[(turn == WHITE) ? 1 : 0][PAWN-1]
	[GET_TO(m->from_to)]);  
}
