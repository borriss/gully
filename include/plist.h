/* $Id: plist.h,v 1.7 2005-02-16 21:21:44 martin Exp $ */

#ifndef __PLIST_H
#define __PLIST_H

#define WPIECE_START_INDEX 0
#define WPAWN_START_INDEX 16
#define BPIECE_START_INDEX 32
#define BPAWN_START_INDEX 48
#define PLIST_MAXENTRIES 56

#define SQUARE_MASK 0x000000ffL /* low byte */
#define PIECE_MASK	0xffff0000L /* high word */

/* plistentry_t holds a piece and a square, of the low word (the square)
   only the low-order byte is used */
typedef unsigned long plistentry_t;
typedef unsigned long piece_t;

#define BOARD_NO_ENTRY ((plistentry_t*) 0)

extern int MaxWhitePiece, MaxWhitePawn, MaxBlackPiece, MaxBlackPawn;
extern plistentry_t PList[];

#define GET_SQUARE(pl_e) ((square_t)((pl_e) & SQUARE_MASK))
#define GET_PIECE(pl_e) ((int) ((((unsigned long)(pl_e)) & PIECE_MASK) >> 16))

#define GET_FILE(square) ((square) & 0x07)
#define GET_RANK(square) (((square) & 0x70) >> 4)

#define MAKE_SQUARE(file,rank) ((file) + ((rank) << 4))


#define MAKE_PL_ENTRY(p,sq) ((sq) | (((unsigned long)(p)) << 16))
#define PL_NEW_SQ(ple,sq) { (ple) &= PIECE_MASK; (ple) |= (sq); }

#define PLIST_OFFSET(b) ((b)-PList)

/* check whether plistentry is in the valid range */
#define PLE_IS_VALID(ple) (PLIST_OFFSET(ple) >= 0 && \
			   PLIST_OFFSET(ple) < PLIST_MAXENTRIES)

#endif /* plist.h */
