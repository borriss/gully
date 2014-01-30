/* $Id: order.h,v 1.2 2011-03-05 20:45:07 martin Exp $ */

#ifndef __ORDER_H
#define __ORDER_H

/* 
   used to order the move_array between index and end_index.
   keys are assigned for being the TT move, being a winning, even or
   losing capture and being a killer.
   Currently, the result is still unordered.
   */
int order_moves(int index, int end_index, int tt_from_to, int n);

int order_root_moves(int index, int end_index, int tt_from_to, int n);

#endif /* order.h */
