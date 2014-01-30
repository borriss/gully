/* $Id: chessio.h,v 1.13 2006-10-01 12:55:22 martin Exp $ */

#ifndef __CHESSIO_H
#define __CHESSIO_H

#include <stdio.h>
#include "board.h"

#define PLY_COMPLETE 1
#define UPDATE 0
#define FAIL_HIGH 2
#define FAIL_LOW 3
#define INTERRUPTED 4

void fprint_board(FILE*);

void fprint_plist(FILE*);

void fprint_plist_entry(FILE *,int);

char* sprint_hash_flag(int);

char piece_name(piece_t);

char* square_name(square_t,char*);

void fprint_move_array(FILE *, int, int);

void fprint_current_line(FILE *);

int fprint_move(FILE *, move_t *m);

/* print move_ t * m into buffer */
int sprint_move(char * strbuf, move_t *m);

int fprint_computer_move(FILE *, move_t *);

void fprint_move_flags(FILE *, move_flag_t *);

void fprint_pv(FILE * where);

void fpost(FILE * where, int full_depth, int what, int score, float time);

int fprint_game(FILE * where, struct the_game_tag * g);

int fprint_int_array(FILE *where,int *a, int maxelems, int columns,
		 int fieldwidth, int skip_what);
/* print integer array in matrix format:
   a: array to print
   maxelems: number of elements (highest unused idex)
   columns: how many columns in matrix
   fieldwidth: width of individual field.
   skip_what: if this is != 0, a '-' is printed instead of
   the specified value.
   */
#endif /* chessio.h */
