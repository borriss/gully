/* $Id: init.h,v 1.6 1997-03-23 20:28:03 martin Exp $ */

#ifndef __INIT_H
#define __INIT_H

int setup_board(char *epdbuf);
void reset_gamestats(void);
void reset_test_stats(void);
void reset_gameoptions(void);
int forsythe_2_board(char* szForsythe);
int get_next_position_from_file(FILE *);
int init_the_game(struct the_game_tag **);
int extend_the_game(struct the_game_tag **);
int edit_board(char * fen_buf);
int board128_2_fen(const char * board128,char *fen_buf, const int turn,
		   const int castling_flags,const int ep_sq,
		   const int no_irreversible,const int move_no);
#endif /* INIT_H */
