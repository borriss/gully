/* $Id: movegen.c,v 1.15 2000-04-20 15:19:22 martin Exp $ */

#include <assert.h>

#include "chess.h"
#include "movegen.h"
#include "logger.h"
#include "chessio.h"

/* -------- Macros for inserting moves into the move list ------------ */

/* check whether we move to empty square, own piece or capture.
   write move to move_array accordingly
*/

#define write_sliding_piece_move(ctm,index,i,sq) {			\
if(BOARD[i]==BOARD_NO_ENTRY)						\
  {									\
    move_array[index].from_to=FROM_TO(sq,i);				\
    ++index;								\
    continue;								\
  }									\
    if((PLIST_OFFSET(BOARD[i]) & 32) ^ ctm )				\
	{								\
	 move_array[index].from_to=FROM_TO(sq,i);			\
	 move_array[index].cap_pro=GET_PIECE(*BOARD[i]);		\
	++index;							\
	}								\
	break;								\
  }

#define write_nonsliding_piece_move(ctm,index,i,sq)		\
if(BOARD[i] == BOARD_NO_ENTRY)					\
{								\
move_array[index].from_to=FROM_TO(sq,i);			\
++index;							\
}								\
else								\
if((PLIST_OFFSET(BOARD[i]) & 32) ^ ctm )			\
{								\
assert(GET_PIECE(*BOARD[i]) != NO_PIECE);			\
move_array[index].from_to=FROM_TO(sq,i);			\
move_array[index].cap_pro=GET_PIECE(*BOARD[i]);			\
++index;							\
}

#define piece_capture(ctm,index,dest_sq,sq)			\
if((PLIST_OFFSET(BOARD[dest_sq]) & 32) ^ ctm )			\
{								\
assert(GET_PIECE(*BOARD[i]) != NO_PIECE);			\
move_array[(index)].from_to=FROM_TO(sq,dest_sq);		\
move_array[(index)].cap_pro=GET_PIECE(*BOARD[dest_sq]);		\
++(index);							\
}



#define pawn_move(index,dest_sq,sq)	  {		\
move_array[index].from_to=FROM_TO(sq,dest_sq);		\
++index;	}

#define pawn_capture(index,dest_sq,sq) {			\
assert(GET_PIECE(*BOARD[i]) != NO_PIECE);			\
move_array[index].from_to=FROM_TO(sq,dest_sq);			\
move_array[index].cap_pro=GET_PIECE(*BOARD[dest_sq]);		\
++index;	}

#define pawn_e_p_capture(index,dest_sq,sq) {		\
move_array[(index)].from_to=FROM_TO(sq,dest_sq);	\
move_array[(index)].cap_pro=PAWN;			\
move_array[(index)].special=EN_PASSANT;			\
++(index);	}

#define pawn_promotion_simple(index,dest_sq,sq)	\
{						\
int j=QUEEN;					\
while(j<=KNIGHT)				\
{						\
move_array[index].from_to=FROM_TO(sq,dest_sq);	\
move_array[index].cap_pro=CAP_PRO(0,j);		\
move_array[index].special=PROMOTION;		\
++j;++index; }					\
}

#define pawn_promotion_capture(index,dest_sq,sq) 			\
{ 									\
int j=QUEEN;							       	\
while(j<=KNIGHT)	{   						\
move_array[index].from_to=FROM_TO(sq,dest_sq);				\
move_array[index].cap_pro= CAP_PRO(GET_PIECE(*BOARD[dest_sq]),j);	\
move_array[index].special= PROMOTION;++j;++index; } 			\
}

#define castling_move(index,dest_sq,sq)	{		\
move_array[(index)].from_to=FROM_TO(sq,dest_sq);	\
move_array[(index)].special=CASTLING;++(index);		\
}


/* forward decls */
int generate_piece_move(const int ctm,int index,const piece_t piece,
			const square_t sq);
int generate_pawn_move(const int ctm,int index,const square_t sq);
int generate_piece_captures(const int ctm,int index,const piece_t piece,
			   const square_t sq);
int generate_pawn_captures(const int ctm,int index,const square_t sq);
void generate_e_p(const int ctm,int * index,const square_t e_p_sq);
void generate_castling(const int ctm,int * index,
		       const int c_flags);


/* the current maximum index to which we can expect e.g. white
   pieces
   This will be packed if necessary at the root or extended for
   promotions
*/
int MaxWhitePiece=7,MaxWhitePawn=23,MaxBlackPiece=39,MaxBlackPawn=55;

/*
Generate all moves for side color.
Write these into the Array MoveList, starting with index index.
*/

int
generate_moves(const int ColorToMove,int index)
{
  register plistentry_t *PListPtr, *StopPtr;

  assert(index < MAX_MOVE_ARRAY - MOVE_ARRAY_SAFETY_THRESHOLD);

  if(ColorToMove==WHITE)
    {
      PListPtr=PList; StopPtr=PList+MaxWhitePiece;
    }
  else
    {
      PListPtr=PList+BPIECE_START_INDEX; StopPtr=PList+MaxBlackPiece;
    }

  /* generate piece moves */
  while(PListPtr != StopPtr)
    {
      if(*PListPtr != NO_PIECE)
	index=generate_piece_move(ColorToMove,index,
				  GET_PIECE(*PListPtr),
				  GET_SQUARE(*PListPtr));
      ++PListPtr;
    }

  if(ColorToMove==WHITE)
    {
      PListPtr=PList+WPAWN_START_INDEX; StopPtr=PList+MaxWhitePawn;
    }
  else
    {
      PListPtr=PList+BPAWN_START_INDEX; StopPtr=PList+MaxBlackPawn;
    }

  /* generate pawn moves */
  while(PListPtr != StopPtr)
    {
      if(*PListPtr != NO_PIECE)
	index=generate_pawn_move(ColorToMove,index,
				 GET_SQUARE(*PListPtr));
      ++PListPtr;
    }

  /*
    test whether ep square is set - generate ep moves here
    */
  if(move_flags[current_ply].e_p_square & 0x70)
    generate_e_p(ColorToMove,&index,
		 move_flags[current_ply].e_p_square);

  return index;

}

/* generates moves for piece on square sq.
   moves will be written to movelist starting at index.
   returns lowest unused free spot in movelist
   */
int
generate_piece_move(const int ctm,register int index,const piece_t piece,
		    register const square_t sq)
{
  register int i;

  /* depending on type of piece, walk over the board. Edges are
     detected with the 0x88 method. Each square on the 128 square-
     board contains an pointer into the piecelist, saying whether
     there is a piece or not.
     If there is no piece, the pointer will be NULL (e.g.,
     BOARD_NO_ENTRY)
     White pieces are from 0-15; White pawns from 16-23. Black
     pieces 32-47, Black pawns 48-55.
     */

  switch(piece)
    {
    case BISHOP:
    case QUEEN:
      i=sq;
      while(((i+=UP_RIGHT) & 0x88) == 0)
	write_sliding_piece_move(ctm,index,i,sq);
      i=sq;
      while(((i+=UP_LEFT) & 0x88) == 0)
	write_sliding_piece_move(ctm,index,i,sq);
      i=sq;
      while(((i+=DOWN_RIGHT) & 0x88) == 0)
	write_sliding_piece_move(ctm,index,i,sq);
      i=sq;
      while(((i+=DOWN_LEFT) & 0x88) == 0)
	write_sliding_piece_move(ctm,index,i,sq);

      if(piece==BISHOP) break;    /* queens fall through */
    case ROOK:
      i=sq;
      while(((i+=UP) & 0x88) == 0)
	write_sliding_piece_move(ctm,index,i,sq);
      i=sq;
      while(((i+=LEFT) & 0x88) == 0)
	write_sliding_piece_move(ctm,index,i,sq);
      i=sq;
      while(((i+=RIGHT) & 0x88) == 0)
	write_sliding_piece_move(ctm,index,i,sq);
      i=sq;
      while(((i+=DOWN) & 0x88) == 0)
	write_sliding_piece_move(ctm,index,i,sq);
      break;
    case KNIGHT:
      if(((i=sq+KNIGHT_UUR) & 0x88) == 0) 
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+KNIGHT_UUL) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+KNIGHT_URR) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+KNIGHT_ULL) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+KNIGHT_DDR) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+KNIGHT_DDL) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+KNIGHT_DRR) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+KNIGHT_DLL) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      break;
    case KING:
      if(((i=sq+UP_RIGHT) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+UP_LEFT) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+DOWN_RIGHT) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+DOWN_LEFT) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+UP) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+LEFT) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+RIGHT) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }
      if(((i=sq+DOWN) & 0x88) == 0)
	{ write_nonsliding_piece_move(ctm,index,i,sq); }

      if(move_flags[current_ply].castling_flags && 
	 ((ctm == WHITE && 
	   move_flags[current_ply].castling_flags & WHITE_CASTLING) ||
	  (ctm == BLACK && 
	   move_flags[current_ply].castling_flags & BLACK_CASTLING)))
	{
	  /* copy index, otherwise we couldn't hold this in 
	     register */
	  int copied_index = index; 

	  generate_castling(ctm,&copied_index,
			    move_flags[current_ply].castling_flags);
	  index=copied_index;
	}
      break;
    default:
      assert(0);
      break;
    }
  return index; 	/* lowest unused index */
}

int
generate_pawn_move(const int ctm,int index,const square_t sq)
{
  register int i;

  assert((sq & 0x88)==0);
  assert(BOARD[sq] != BOARD_NO_ENTRY);

  if(ctm==WHITE)
    {
      assert((GET_PIECE(*BOARD[sq])==PAWN)
	     && (PLIST_OFFSET(BOARD[sq]) < BPIECE_START_INDEX));

      /* pawn on 7th rank? */
      if((sq & 0x40) && (sq & 0x20))
	{
	  if(BOARD[i=sq+UP] == BOARD_NO_ENTRY)
	    {
	      pawn_promotion_simple(index,i,sq);
	    }

	  if( !((i=sq+UP_LEFT) & 0x88) &&
	      BOARD[i] &&
	      (PLIST_OFFSET(BOARD[i]) >= BPIECE_START_INDEX))
	    pawn_promotion_capture(index,i,sq);

	  if( !((i=sq+UP_RIGHT) & 0x88) &&
	      BOARD[i] &&
	      (PLIST_OFFSET(BOARD[i]) >= BPIECE_START_INDEX))
	    pawn_promotion_capture(index,i,sq);

	  return index;
	}

      if(BOARD[i=sq+UP]==BOARD_NO_ENTRY)
	{
	  pawn_move(index,i,sq);

	  if (!(sq & 0x60) && BOARD[i=sq+UP+UP] == BOARD_NO_ENTRY)
	    {
	      move_array[index].special=DOUBLE_ADVANCE;
	      pawn_move(index,i,sq);
	    }
	}
      /* captures */
      if( !((i=sq+UP_RIGHT) & 0x88) && BOARD[i] &&
	  (PLIST_OFFSET(BOARD[i]) >= BPIECE_START_INDEX))
	pawn_capture(index,i,sq);

      if( !((i=sq+UP_LEFT) & 0x88) && BOARD[i] &&
	  (PLIST_OFFSET(BOARD[i]) >= BPIECE_START_INDEX))
	pawn_capture(index,i,sq);

    }
  else
    {
      assert(GET_PIECE(*BOARD[sq]) == PAWN && ctm==BLACK);

      /* pawn on 2nd rank? */
      if(!(sq & 0x60))
	{
	  if(BOARD[i=sq+DOWN] == BOARD_NO_ENTRY)
	    {
	      pawn_promotion_simple(index,i,sq);
	    }

	  if( !((i=sq+DOWN_LEFT) & 0x88) && BOARD[i] &&
	      (PLIST_OFFSET(BOARD[i]) < BPIECE_START_INDEX))
	    pawn_promotion_capture(index,i,sq);

	  if( !((i=sq+DOWN_RIGHT) & 0x88) && BOARD[i] &&
	      (PLIST_OFFSET(BOARD[i]) < BPIECE_START_INDEX))
	    pawn_promotion_capture(index,i,sq);

	  return index;
	}

      if(BOARD[i=sq+DOWN]==BOARD_NO_ENTRY)
	{
	  pawn_move(index,i,sq);

	  if (sq >= 0x60 && BOARD[i=sq+DOWN+DOWN] == BOARD_NO_ENTRY)
	    {
	      move_array[index].special=DOUBLE_ADVANCE;
	      pawn_move(index,i,sq);
	    }
	}
      /* captures */
      if( !((i=sq+DOWN_RIGHT) & 0x88) && BOARD[i] &&
	  (PLIST_OFFSET(BOARD[i]) < BPIECE_START_INDEX))
	pawn_capture(index,i,sq);

      /* XXX get_piece */
      if( !((i=sq+DOWN_LEFT) & 0x88) && BOARD[i] &&
	  (PLIST_OFFSET(BOARD[i]) < BPIECE_START_INDEX))
	pawn_capture(index,i,sq);

    }

  return index;
}

/*
I don't particulary care about efficiency _here_, those are
rarely called. Should be inline, but looks better this way...
e_p square is the square the enemy pawn hopped over.
The resulting move is _from_ the square of the capturing pawn
_to_ the e_p_square.
*/
void
generate_e_p(const int ctm,int * index,const square_t e_p_sq)
{
  int i;

  assert((e_p_sq & 0x88) == 0);
  assert(BOARD[e_p_sq] == BOARD_NO_ENTRY);

  if(ctm == WHITE)
    {
      assert(BOARD[e_p_sq + DOWN]
	     && GET_PIECE(*BOARD[e_p_sq+DOWN]) == PAWN);

      if(!((i = e_p_sq + DOWN_LEFT) & 0x88) && BOARD[i] != BOARD_NO_ENTRY
	 && GET_PIECE(*BOARD[i]) == PAWN
	 && PLIST_OFFSET(BOARD[i]) < BPIECE_START_INDEX)
	pawn_e_p_capture(*index,e_p_sq,i);
      if(!((i = e_p_sq + DOWN_RIGHT) & 0x88) && BOARD[i] != BOARD_NO_ENTRY
	 && GET_PIECE(*BOARD[i]) == PAWN
	 && PLIST_OFFSET(BOARD[i]) < BPIECE_START_INDEX)
	pawn_e_p_capture(*index,e_p_sq,i);
    }
  else	/* black to move */
    {
      assert(BOARD[e_p_sq+UP]
	     && GET_PIECE(*BOARD[e_p_sq+UP]) == PAWN);

      if(!((i = e_p_sq + UP_LEFT) & 0x88) && BOARD[i] != BOARD_NO_ENTRY
	 && GET_PIECE(*BOARD[i]) == PAWN
	 && PLIST_OFFSET(BOARD[i]) >= BPIECE_START_INDEX)
	pawn_e_p_capture(*index,e_p_sq,i);
      if(!((i = e_p_sq + UP_RIGHT) & 0x88) && BOARD[i] != BOARD_NO_ENTRY
	 && GET_PIECE(*BOARD[i]) == PAWN
	 && PLIST_OFFSET(BOARD[i]) >= BPIECE_START_INDEX)
	pawn_e_p_capture(*index,e_p_sq,i);
    }
}


void
generate_castling(const int ctm,int * index,const int c_flags)
{
  if(ctm==WHITE)
    {
      assert(move_flags[current_ply].white_king_square == 4);

      if(c_flags & WHITE_SHORT
	 &&	BOARD[5]==BOARD_NO_ENTRY && BOARD[6]==BOARD_NO_ENTRY)
	castling_move(*index,6,4);

      if(c_flags & WHITE_LONG
	 && BOARD[3]==BOARD_NO_ENTRY && BOARD[2]==BOARD_NO_ENTRY
	 && BOARD[1]==BOARD_NO_ENTRY)
	castling_move(*index,2,4);
    }
  else	/* black to move */
    {
      assert(move_flags[current_ply].black_king_square == 0x74);

      if(c_flags & BLACK_SHORT
	 &&	BOARD[0x75]==BOARD_NO_ENTRY && BOARD[0x76]==BOARD_NO_ENTRY)
	castling_move(*index,0x76,0x74);

      if(c_flags & BLACK_LONG
	 && BOARD[0x73]==BOARD_NO_ENTRY && BOARD[0x72]==BOARD_NO_ENTRY
	 && BOARD[0x71]==BOARD_NO_ENTRY)
	castling_move(*index,0x72,0x74);
    }
}

/* similar to generate_moves, but only captures */

int
generate_captures(const int ColorToMove,int index)
{
  register plistentry_t *PListPtr, *StopPtr;

  if(ColorToMove==WHITE)
    {
      PListPtr=PList; StopPtr=PList+MaxWhitePiece;
    }
  else
    {
      PListPtr=PList+BPIECE_START_INDEX; StopPtr=PList+MaxBlackPiece;
    }


  /* generate piece captures */
  while(PListPtr != StopPtr)
    {
      if(*PListPtr != NO_PIECE)
	index=generate_piece_captures(ColorToMove,index,
				     GET_PIECE(*PListPtr),
				     GET_SQUARE(*PListPtr));
      ++PListPtr;
    }

  if(ColorToMove==WHITE)
    {
      PListPtr=PList+WPAWN_START_INDEX; StopPtr=PList+MaxWhitePawn;
    }
  else
    {
      PListPtr=PList+BPAWN_START_INDEX; StopPtr=PList+MaxBlackPawn;
    }

  /* generate pawn captures and promotions */
  while(PListPtr != StopPtr)
    {
      if(*PListPtr != NO_PIECE)
	index=generate_pawn_captures(ColorToMove,index,
				 GET_SQUARE(*PListPtr));
      ++PListPtr;
    }

  /* generate ep captures */
  if(move_flags[current_ply].e_p_square & 0x70)
    generate_e_p(ColorToMove,&index,
		 move_flags[current_ply].e_p_square);

  return index;

}

/* generates captures for piece on square sq.
   moves will be written to movelist starting at index.
   returns lowest unused free spot in movelist
   */
int
generate_piece_captures(const int ctm, register int index,
			const piece_t piece, register const square_t sq)
{
  int i;

  switch(piece)
    {
    case BISHOP:
    case QUEEN:
      i=sq;
      while((((i+=UP_RIGHT) & 0x88) == 0) && BOARD[i] == BOARD_NO_ENTRY);
      if((i & 0x88) == 0) piece_capture(ctm,index,i,sq);
      i=sq;
      while((((i+=UP_LEFT) & 0x88) == 0) && BOARD[i] == BOARD_NO_ENTRY);
      if((i & 0x88) == 0) piece_capture(ctm,index,i,sq);
      i=sq;
      while((((i+=DOWN_RIGHT) & 0x88) == 0) && BOARD[i] == BOARD_NO_ENTRY);
      if((i & 0x88) == 0) piece_capture(ctm,index,i,sq);
      i=sq;
      while((((i+=DOWN_LEFT) & 0x88) == 0) && BOARD[i] == BOARD_NO_ENTRY);
      if((i & 0x88) == 0) piece_capture(ctm,index,i,sq);

      if(piece == BISHOP) break;    /* queens fall through */
    case ROOK:
      i=sq;
      while((((i+=UP) & 0x88) == 0) && BOARD[i] == BOARD_NO_ENTRY);
      if((i & 0x88) == 0) piece_capture(ctm,index,i,sq);
      i=sq;
      while((((i+=LEFT) & 0x88) == 0) && BOARD[i] == BOARD_NO_ENTRY);
      if((i & 0x88) == 0) piece_capture(ctm,index,i,sq);
      i=sq;
      while((((i+=RIGHT) & 0x88) == 0) && BOARD[i] == BOARD_NO_ENTRY);
      if((i & 0x88) == 0) piece_capture(ctm,index,i,sq);
      i=sq;
      while((((i+=DOWN) & 0x88) == 0) && BOARD[i] == BOARD_NO_ENTRY);
      if((i & 0x88) == 0) piece_capture(ctm,index,i,sq);

      break;
    case KNIGHT:
      if((((i=sq+KNIGHT_UUR) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+KNIGHT_UUL) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+KNIGHT_URR) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+KNIGHT_ULL) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+KNIGHT_DDR) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+KNIGHT_DDL) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+KNIGHT_DRR) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+KNIGHT_DLL) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      break;
    case KING:
      if((((i=sq+UP_RIGHT) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+UP_LEFT) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+DOWN_RIGHT) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+DOWN_LEFT) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+UP) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+DOWN) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+RIGHT) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      if((((i=sq+LEFT) & 0x88) == 0) && BOARD[i] != BOARD_NO_ENTRY)
	piece_capture(ctm,index,i,sq);
      break;
    default:
      assert(0);
      break;
    }
  return index; 	/* lowest unused index */
}

int
generate_pawn_captures(const int ctm,int index,const square_t sq)
{
  int i;

  assert((sq & 0x88)==0);
  assert(BOARD[sq] != BOARD_NO_ENTRY);

  if(ctm==WHITE)
    {
      assert((GET_PIECE(*BOARD[sq])==PAWN)
	     && (PLIST_OFFSET(BOARD[sq]) < BPIECE_START_INDEX));

      /* pawn on 7th rank? */
      if((sq & 0x40) && (sq & 0x20))
	{
	  if(BOARD[i=sq+UP] == BOARD_NO_ENTRY)
	    {
	      pawn_promotion_simple(index,i,sq);
	    }

	  if( !((i=sq+UP_LEFT) & 0x88) &&
	      BOARD[i] &&
	      (PLIST_OFFSET(BOARD[i]) >= BPIECE_START_INDEX))
	    pawn_promotion_capture(index,i,sq);

	  if( !((i=sq+UP_RIGHT) & 0x88) &&
	      BOARD[i] &&
	      (PLIST_OFFSET(BOARD[i]) >= BPIECE_START_INDEX))
	    pawn_promotion_capture(index,i,sq);

	  return index;
	}

      if( !((i=sq+UP_RIGHT) & 0x88) && BOARD[i] &&
	  (PLIST_OFFSET(BOARD[i]) >= BPIECE_START_INDEX))
	pawn_capture(index,i,sq);

      if( !((i=sq+UP_LEFT) & 0x88) && BOARD[i] &&
	  (PLIST_OFFSET(BOARD[i]) >= BPIECE_START_INDEX))
	pawn_capture(index,i,sq);

    }
  else
    {
      assert(GET_PIECE(*BOARD[sq]) == PAWN && ctm==BLACK);

      /* pawn on 2nd rank? */
      if(!(sq & 0x60))
	{
	  if(BOARD[i=sq+DOWN] == BOARD_NO_ENTRY)
	    {
	      pawn_promotion_simple(index,i,sq);
	    }

	  if( !((i=sq+DOWN_LEFT) & 0x88) && BOARD[i] &&
	      (PLIST_OFFSET(BOARD[i]) < BPIECE_START_INDEX))
	    pawn_promotion_capture(index,i,sq);

	  if( !((i=sq+DOWN_RIGHT) & 0x88) && BOARD[i] &&
	      (PLIST_OFFSET(BOARD[i]) < BPIECE_START_INDEX))
	    pawn_promotion_capture(index,i,sq);

	  return index;
	}

      if( !((i=sq+DOWN_RIGHT) & 0x88) && BOARD[i] &&
	  (PLIST_OFFSET(BOARD[i]) < BPIECE_START_INDEX))
	pawn_capture(index,i,sq);

      if( !((i=sq+DOWN_LEFT) & 0x88) && BOARD[i] &&
	  (PLIST_OFFSET(BOARD[i]) < BPIECE_START_INDEX))
	pawn_capture(index,i,sq);

    }

  return index;
}
