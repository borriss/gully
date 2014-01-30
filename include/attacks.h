/* $Id: attacks.h,v 1.4 1996-08-26 18:43:45 martin Exp $ */

#ifndef __ATTACKS_H
#define __ATTACKS_H

#define ATTACKED 		1
#define NOT_ATTACKED 	0

int attacks(int ctm,square_t sq);
int see(int attacking_color,move_t * m);
#endif /* __ATTACKS_H */
