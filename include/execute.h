/* $Id: execute.h,v 1.6 1997-04-09 21:10:54 martin Exp $ */

#ifndef __EXECUTE_H
#define __EXECUTE_H

int make_move(register const move_t *,int ply);
int undo_move(register const move_t *,int ply);
int make_null_move(int ply);
int make_root_move(struct the_game_tag *,move_t *);
int unmake_root_move(struct the_game_tag *g,move_t * m);
int verify_move(move_t *m, int ply, int turn, int index);
#endif /* execute.h */
