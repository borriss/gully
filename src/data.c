/* $Id: data.c,v 1.13 2000-06-12 16:29:19 martin Exp $ */

#include "chess.h"

plistentry_t * BOARD[128];
plistentry_t PList[PLIST_MAXENTRIES];
move_t move_array[MAX_MOVE_ARRAY];
move_t * current_line[MAX_SEARCH_DEPTH];

/* triangular array holding the principal variation.
   It isn't triangular by any means, wasting 
   MAX_SEARCH_DEPTH*MAX_SEARCH_DEPTH*sizeof(move_t) bytes.
   */
line_t principal_variation[MAX_SEARCH_DEPTH];
move_flag_t move_flags[MAX_MOVE_FLAGS];

int abort_search;

/* set of very prominent global variables follows */

unsigned turn; /* which side is on move in current position */
unsigned computer_color; /* which color does computer play */
unsigned current_ply; /* current depth while searching */


struct gamestat_tag gamestat;
struct gameoptions_tag gameopt;
struct test_stats_tag test_stat;

struct the_game_tag * the_game = NULL;

enum global_search_state_tag global_search_state  = IDLING;

enum game_phase_tag game_phase = MIDDLEGAME;

move_t user_move, ponder_move;
int saved_command;
