/* $Id: movegen.h,v 1.7 2003-03-02 13:52:43 martin Exp $ */

/*
Movegen based on 128 square board and piece lists.
PList is an array of 56 entries which are reserved as follows:
0-15 	white officers
16-23 	white pawns
32-47	black officers
48-55	black pawns

24-31 are unused.

An entry in the piece list contains the kind of piece and
the square it is currently standing on.
*/

#define GET_FROM(from_to)	((square_t) ((from_to) & 0xff))
#define GET_TO(from_to)		((square_t) (((from_to) & 0xff00) >> 8))

#define GET_CAP(cap_pro)	((cap_pro) & 0xff)
#define GET_PRO(cap_pro)	(((cap_pro) & 0xff00) >> 8)

#define FROM_TO(from,to)	((from) | ((to) << 8))
#define CAP_PRO(cap_what,promote_to) ((cap_what) | ((promote_to) << 8))

int generate_moves(const int ColorToMove, int index);
int generate_captures(const int ColorToMove, int index);



