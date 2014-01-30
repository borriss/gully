/* $Id: analyse.c,v 1.4 2003-04-11 16:44:28 martin Exp $ */

#include <assert.h>
#include <stdio.h>

#include "chess.h"
#include "movegen.h"
#include "execute.h"
#include "logger.h"
#include "analyse.h"
#include "test.h"
#include "iterate.h"
#include "mstimer.h"
#include "input.h"
#include "chessio.h"
#include "init.h"
#include "helpers.h"
#include "book.h"

/* local stats for analysis */
struct analysis_stats_tag {
  int move_no; /* number legal of moves already searched at this depth 
		* (including current move) */ 
  int current_move_index; /* index in move array which is currently analysed */
  int total_move_index; /* number of _legal_ moves here */
  int current_depth; /* search depth */
};

static struct analysis_stats_tag analysis_stats;

/* forwards */
static int do_analyse(void);
static int init_analysis_stats(void);


int 
analyse()
{
  global_search_state = ANALYZING;

  log_msg("Analysis mode.\n");
  
  while(do_analyse());

  return 0;
}

int 
analyse_exit()
{
  global_search_state = IDLING;
  abort_search = 1;
  log_msg("Leaving analysis mode.\n");
  return 0;
}

/* 
 * Handles analysis for the current position by calling the regular iterate()
 * function. 
 *
 * Three steps: 
 * 1) Prepares analysis by getting the static stats from movegen (to handle
 * the "." and "bk" commands while analyzing).
 * 2) call iterate() with infinite depth and time.
 * 3) handle commands (undo, exit, setboard, edit) by switching temporarily
 * to idle mode and return.
 */
static int 
do_analyse()
{
  int score;
  
  saved_command = 0;
  user_move.from_to = 0;

  reset_gamestats();
  timestamp();

  if (init_analysis_stats() == 0) 
    log_msg("init_analysis found no legal moves in current position.\n");

  score = iterate(gameopt.maxdepth, ANALYZING, NULL);

#if 0
  log_msg("Analyzed for %.2f (cmd:%d) (m_from_to %d)\n",
	  time_diff(get_time(),game_time.timestamp),
	  saved_command, user_move.from_to);
#endif
  
  /* returns on moves and on most legal commands in analysis mode:
   * undo, new, setboard, edit, exit.
   * These are handled as saved_command.     
   * In most cases, we need to update the position and start over again. 
   *
   * It is also possible that iterate() reached the full depth (on draws,
   * mate, stalemate) and returns. In that case, we check detailed iterate
   * stats and go to ANALYSIS_IDLE state where we sit and wait again
   * for commands.
  */


  if (iterate_stats.max_depth_reached) {
    log_msg("analysis idle.\n");
    fread_input(stdin, &user_move);
  }

  /* set during analyze or by fread_input */
  if (IS_IDLE) return 0;

  /* saved_command set during analyze or by fread_input */
  if (saved_command)  {
    /* now, change the mode temporarily back to idle */
    global_search_state = IDLING;
    if(do_command(saved_command, NULL) != EX_CMD_SUCCESS)
      log_msg("do_analyse: saved command %d failed.\n", saved_command);
    global_search_state = ANALYZING;
    /* reset the command, no matter if it successfully executed */
    saved_command = 0;
  }
  
  /* Alternatively to a possibly received command, we *may* have a
   * move.    
   */
  if (user_move.from_to) {
    if (! make_root_move(the_game, &user_move))
      log_msg("analyse: user move failed\n");
  }
    
  return 1;
}

/* find the number of legal moves by calling movegen + make_move + undo_move
 * for the current position.
 * 
 * Returns the number of legal moves in this position.
 */

static int 
init_analysis_stats()
{
  int k, legal = 0, new_index;

  /* trigger saving of available book moves */
  book();

  new_index = generate_moves(turn, 0);

  for(k = 0; k < new_index; k++) {
    if(make_move(&move_array[k], current_ply)) legal++;
    undo_move(&move_array[k], current_ply);
  }

  clear_move_list(0, new_index);

  analysis_stats.move_no = 0; 
  analysis_stats.current_depth = 0; 
  analysis_stats.current_move_index = 0;
  analysis_stats.total_move_index = legal;

  return legal;
}

void
update_analysis_stats(int move_no, int cur_index, int depth)
{
  analysis_stats.move_no = move_no;
  analysis_stats.current_move_index = cur_index;
  analysis_stats.current_depth = depth;
}

/* XXX note that there is no special handling for fail-low re-searches.
   That is, Gully shows in its analysis window really bad moves and scores.
   (I need move ordering here!).
*/

void 
post_analysis_update(void)
{
  char buf[3], buf1[3];
  move_t * m = &move_array[analysis_stats.current_move_index];
  
  fprintf(stdout, "stat01: %lu %lu %d %d %d %s%s%c\n",
	  get_time() - game_time.timestamp, 
	  gamestat.moves_looked_at_in_search,
	  analysis_stats.current_depth, 
	  analysis_stats.total_move_index - analysis_stats.move_no, 
	  analysis_stats.total_move_index, 
	  square_name(GET_FROM(m->from_to), buf),
	  square_name(GET_TO(m->from_to), buf1),
	  (GET_PRO(m->cap_pro)) ? piece_name(GET_PRO(m->cap_pro)) : 0x20);
}
