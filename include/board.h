/* $Id: board.h,v 1.3 2000-06-12 16:26:10 martin Exp $ */

#ifndef __BOARD_H
#define __BOARD_H

#include "plist.h"

/* moves relating to the 8*16 chess board */
#define UP_LEFT		15
#define UP_RIGHT 	17
#define DOWN_LEFT	(-UP_RIGHT)
#define DOWN_RIGHT	(-UP_LEFT)
#define RIGHT		1
#define	LEFT		(-RIGHT)
#define	UP			16
#define DOWN		(-UP)
/* U expands to UP, D - DOWN, R -RIGHT, L -LEFT */
#define KNIGHT_UUR	33
#define	KNIGHT_UUL	31
#define	KNIGHT_URR	18
#define	KNIGHT_ULL	14
#define KNIGHT_DDR	(-KNIGHT_UUL)
#define	KNIGHT_DDL	(-KNIGHT_UUR)
#define	KNIGHT_DRR	(-KNIGHT_ULL)
#define	KNIGHT_DLL	(-KNIGHT_URR)

typedef unsigned char square_t;

/* the chess board is 16x8 squares. it contains pointers to
the piecelist */

extern  plistentry_t * BOARD[];

extern unsigned turn;
extern unsigned current_ply;
extern unsigned computer_color;

#endif /* __BOARD_H */
