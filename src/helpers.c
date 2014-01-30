/* $Id: helpers.c,v 1.30 2011-03-05 20:42:15 martin Exp $ */

#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "chess.h"
#include "helpers.h"
#include "pvalues.h"
#include "logger.h"
#include "chessio.h"
#include "movegen.h"
#include "input.h" /* parse abbreviated() - should perhaps come here */
#include "tables.h" /* get_piece_material */
#include "version.h"
#include "compile.h" /* compile time options */

/* forward decl */
static void strip_buf(const char *src, char *dest);

/* 
 * Compares first move in PV with the set of moves in buf 
 */
int
solution_correct(char *buf)
{
  char *token;
  char stripped_buf[12]; 
  move_t parsed_move, pv_move;

  if((token = strtok(buf, " ")) == NULL) return 0;

  if(strncmp(buf, "none", 4) == 0) return 0;

  pv_move = principal_variation[0][0];
  
  do { /* look for one matching token */ 
    /* strip off redundant characters */
    strip_buf(token, stripped_buf);

    if (!parse_abbreviated(stripped_buf, &parsed_move))
      err_msg("Could not parse solution:\n%s\n%s\n", buf, stripped_buf);
	
    /* compare parsed move with PV. */
    if (parsed_move.from_to == pv_move.from_to 
	&& parsed_move.cap_pro == pv_move.cap_pro)
      return 1;
  }
  while ((token = strtok(NULL, " ")) != NULL );
      
  return 0;
}

/* 
 * strips 'x','#','=', and '+' from moves.
 */
static void 
strip_buf(const char *src, char *dest)
{
  while (src[0] != '\0') {
    if (src[0] != 'x' && src[0] != '+' && src[0] != '#' && src[0] != '=')  {
      dest[0] = src[0];
      dest++;
    }
    src++;
  }
  dest[0]='\0';
}

void
clear_move_list(int index1,int index2)
{
  assert(index2 >= index1);

  if (index2 == index1) return;
  memset((char*) & (move_array[index1]), 0, (index2-index1) * sizeof(move_t));
}


void
clear_move_flags(void)
{
  memset((char*)&move_flags, FLAGS_INIT,
	 (size_t)MAX_MOVE_FLAGS * sizeof(move_flag_t));
}

/* 
 * clear PV 
 * 
 * ply == 1 is a shallow clear (everything except for the ply 0)
 * ply == 0 is a deep clear (used during position setup)
*/
void
clear_pv(int ply) 
{
  assert(ply < MAX_SEARCH_DEPTH);

  memset((void*) &principal_variation[ply][0], 0,
	 (MAX_SEARCH_DEPTH - ply) * sizeof(line_t));
}


/* 
 * When inserting the terminal move into the PV, make sure that higher
 * levels don't get into PV
 */

void 
cut_pv(void)
{
  assert(current_ply+1 < MAX_SEARCH_DEPTH);
  memset((void*) &principal_variation[current_ply][0], 0, sizeof(move_t));
}


void
update_pv_hash(int from_to)
{

  assert(current_ply+1 < MAX_SEARCH_DEPTH);
  assert(from_to);

  memset((void*)&principal_variation[current_ply][0], 0, 2 * sizeof(move_t));

  principal_variation[current_ply][0].from_to = from_to;
  principal_variation[current_ply][0].special = HASHMOVE;

}

void 
update_pv(move_t *m)
{
  assert(current_ply < (MAX_SEARCH_DEPTH - 1));

  principal_variation[current_ply][0] = *m;

  memcpy((void*) &principal_variation[current_ply][1],
	 (void*) &principal_variation[current_ply+1][0],
	 sizeof(move_t) * ((MAX_SEARCH_DEPTH - current_ply) - 1));
}

/*
 * incremental update of material score.
 * move_flags[current_ply+1].w_material contains white piece and pawn
 * material separately, same for black.
 */

void
update_material(piece_t p)
{
  assert(p);

  switch(p) {
  case PAWN:
    if (turn == WHITE ) 
      CHANGE_PAWN_MATERIAL(move_flags[current_ply+1].b_material, -PAWNVALUE);
    else 
      CHANGE_PAWN_MATERIAL(move_flags[current_ply+1].w_material, -PAWNVALUE);
    break;
  case KNIGHT:
    if (turn == WHITE) REMOVE_KNIGHT(move_flags[current_ply+1].b_material);
    else  REMOVE_KNIGHT(move_flags[current_ply+1].w_material);
    break;
  case BISHOP:
    if (turn == WHITE) REMOVE_BISHOP(move_flags[current_ply+1].b_material);
    else REMOVE_BISHOP(move_flags[current_ply+1].w_material);
    break;
  case ROOK:
    if (turn == WHITE) REMOVE_ROOK(move_flags[current_ply+1].b_material);
    else REMOVE_ROOK(move_flags[current_ply+1].w_material);
    break;
  case QUEEN:
    if (turn == WHITE) REMOVE_QUEEN(move_flags[current_ply+1].b_material);
    else REMOVE_QUEEN(move_flags[current_ply+1].w_material);
    break;
  case KING: /* error if capturing king */
  default:
    err_quit("Bad piece (mat_update): %d\n",p);
  }

}

void
update_material_on_promotion(piece_t pro_p, piece_t cap_p)
{
  assert(pro_p);

  /* subtract promoting pawn */
  if (turn == WHITE) 
    CHANGE_PAWN_MATERIAL(move_flags[current_ply+1].w_material,-PAWNVALUE);
  else CHANGE_PAWN_MATERIAL(move_flags[current_ply+1].b_material, -PAWNVALUE);

  switch(pro_p) {
  case QUEEN:
    if (turn == WHITE) ADD_QUEEN(move_flags[current_ply+1].w_material);
    else ADD_QUEEN(move_flags[current_ply+1].b_material);
      break;
  case KNIGHT:
    if (turn == WHITE) ADD_KNIGHT(move_flags[current_ply+1].w_material);
    else ADD_KNIGHT(move_flags[current_ply+1].b_material);
    break;
  case BISHOP:
    if (turn == WHITE) ADD_BISHOP(move_flags[current_ply+1].w_material);
    else ADD_BISHOP(move_flags[current_ply+1].b_material);
    break;
  case ROOK: 
    if (turn == WHITE) ADD_ROOK(move_flags[current_ply+1].w_material);
    else ADD_ROOK(move_flags[current_ply+1].b_material);
    break;
  case KING:
  case PAWN:
  default:
    err_quit("Bad piece (material_promo_update): %d\n",pro_p);
  }
  
  /* in case something got captured by the promoting pawn */
  switch(cap_p) {
  case 0: /* normal promotion */
    break;
  case QUEEN: 
    if (turn == WHITE) REMOVE_QUEEN(move_flags[current_ply+1].b_material);
    else REMOVE_QUEEN(move_flags[current_ply+1].w_material);
    break;
  case KNIGHT:
    if (turn == WHITE) REMOVE_KNIGHT(move_flags[current_ply+1].b_material);
    else REMOVE_KNIGHT(move_flags[current_ply+1].w_material);
    break;
  case BISHOP:
    if (turn == WHITE) REMOVE_BISHOP(move_flags[current_ply+1].b_material);
    else REMOVE_BISHOP(move_flags[current_ply+1].w_material);
    break;
  case ROOK:
    if (turn == WHITE) REMOVE_ROOK(move_flags[current_ply+1].b_material);
    else REMOVE_ROOK(move_flags[current_ply+1].w_material);
    break;
  case KING:
  case PAWN:
  default:
    err_quit("Bad piece (material_promo_update2): %d\n",cap_p);
  }
} 


/*
 * As in crafty, looking which game phase we are in is done
 * just once per call to iterate().
 * Its responsibility is also to allow null moves or not.
 * The decisions of phase() are based on the root position (before
 * search is done).
 *
 * XXX : This function is currently used to let the eval decide which
 * evalution should be applied. This is too simplistic.
 *
 * phase() is useful for search configuration (null moves), but not
 * much more.
 *
 * XXX Decision which eval routine will be used should be based on
 * info from mat_dist_table!
 */

void
phase(void)
{
  int wpi_mat,wpa_mat,bpi_mat,bpa_mat;
  static int user_nulloption = -1;

  /* save default or command line null setting */
  if (user_nulloption == -1) { 
    user_nulloption = NULL_ON;
    log_msg("phase: init user_nulloption == %d.\n", user_nulloption);
  }

  wpa_mat = GET_PAWN_MATERIAL(move_flags[0].w_material);
  bpa_mat = GET_PAWN_MATERIAL(move_flags[0].b_material);

  wpi_mat = get_piece_material(move_flags[0].w_material);
  bpi_mat = get_piece_material(move_flags[0].b_material);

  if (user_nulloption) SET_OPTION(O_NULL_BIT);
  
  /* 
   *  what game phase is it 
   * FIXME: hardcoded material limit.
   *
   */
  if ((wpi_mat < 1400) && (bpi_mat < 1400)) {
    game_phase = ENDGAME;
    if ((wpa_mat == 0) && (bpa_mat == 0))
      game_phase = PAWNLESS;
    
    /* sometimes null move is better disabled. 
       However, there is no easy way to re-set
       the option. See top.
    */
    if ((wpi_mat < 1000) && (bpi_mat < 1000))
      RESET_OPTION(O_NULL_BIT);
    
    return;
  }
  
  /* well, here something can be done to help in the opening */
  
  game_phase = MIDDLEGAME;
  
  return;
}

void
print_usage(char * progname)
{
 printf("Usage: %s [options]\nOptions:\n"
	"-h | --help                  \t\tThis screen.\n"
	"-v | --version               \t\tProgram version.\n"
	"-p | --ponder                \t\tPermanent brain off.\n"
	"-x | --xboard                \t\tForce XBoard/WinBoard mode.\n"
	"-f | --file <filename>       \t\tRead from epd testfile.\n"
	"--fulleval                   \t\tNo lazy evals.\n"
	"--null                       \t\tNever use null move.\n"
	"--book {on,off,<book_file_name>}\n"
	"-t | --test {solve,see,eval,search,movegen,make} \n"
	"\tIn conjunction with file.\n" 
	"--bench                      \t\tSome standard numbers\n"
	"--maxdepth <max_dep>         \t\tMax. full width "
	"search depth.\n"
	"--time <max_time>         \t\ttime per move in [1/10s]\n\n"
	"--nokiller                  \t\t Killers off.\n"
	"--transref <size>         \tmain size 2exp(size), 0 == off\n"	
	"(options may be abbreviated as long as uniquely "
	"identified)\n",
	progname);
}

/*
 * verbosity == 0 means to print just a single line.
 */

void
print_version(int verbosity)
{
  if (verbosity & 1)
    fprintf(stdout,"Gullydeckel "VERSION", chess playing program.\n\n");
  
  if (verbosity & 2) {
    
    fprintf(stdout, "Last compiled %s %s with" ,__DATE__,__TIME__);
#ifdef __GNUC__
    fprintf(stdout," gcc %s.\n",__VERSION__);
#elif defined (_MSC_VER)
    fprintf(stdout," Microsoft Visual C++ %d.\n",_MSC_VER);
#else
    fprintf(stdout,"unknown C compiler.\n");
#endif
      
#if defined (UNIX)
    system("cat /proc/cpuinfo | egrep 'model|bogo';date");
#elif defined (_WIN32)
    fprintf(stdout, "Win32 system.\n");
#endif

#if 0 /* supply a separate config command or switch instead */      
#ifdef COMPILE_FAST
    fprintf(stdout,"Fastest compilation\n");
#elif defined COMPILE_DEBUG
    fprintf(stdout,"Compilation with debugging support\n");
#elif defined COMPILE_DEBUG_PROFILE
    fprintf(stdout,"Compilation with debugging and profiling support\n");
#else
    fprintf(stdout,"Unknown configuration\n");
#endif
    
#ifdef QUIES_CHECK
    fprintf(stdout,"Testing for check in quiescence\n");
#else
    fprintf(stdout,"No tests for check in quiescence\n");
#endif
      
#endif /* #if 0 */
  } /* program configuration */
  
  /* say something about licensing */
  if (verbosity & 4) {
    fprintf(stdout, "Copyright (C) 1997-2003 Martin Borriss.\n"
	    "Gullydeckel comes with ABSOLUTELY NO WARRANTY.\n"
	    "You may redistribute copies of Gullydeckel\n"
	    "under the terms of the GNU General Public License.\n"
	    "For more information about these matters, "
	    "see the file named COPYING.\n");
  }
  
}

/* Close logfile and do whatever housekeeping is neccessary */
void 
quit()
{
  log_msg("Exiting.\n");
  fflush(NULL);
  log_close();
  exit(0);
}

/* Print command prompt. Do not print this in xboard mode. */
void
prompt()
{
  if (XBOARD_ON) return;

  fprintf(stdout, "%s:", (turn == WHITE) ? "white" : "black");
}

/* XXX This needs to be updated, of course. Should also support some
   paging mode. Command specific help is totally missing yet.
*/

int
command_help(int command)
{

  if (!command)
    printf("commands:\n"
	   "xboard [Set xboard compatibility]\n"
	   "bd [Show board]\n"
	   "list [Show game moves]\n"
	   "undo [Takeback one half move]\n"
	   "edit [Input a chess position]\n"
	   "switch [Switch sides]\n"
	   "white\n"
	   "depth\n"
	   "post [Show main line]\n"
	   "save\n"
	   "random\n"
	   "quit [Exit program]\n"
	   "material\n"
	   "easy [Permanent brain OFF]\n"
	   "hard [Permanent brain ON ]\n"
	   "hash [Toggle transposition table usage]\n"
	   "reverse\n"
	   "book [admin | on | off | status | <book_file>]\n"
	   "remove [Take back one full move]\n"
	   "force [Accept moves from both sides]\n"
	   "both [Computer vs. computer]\n"
	   "black\n"
	   "clock\n"
	   "hint\n"
	   "get\n"
	   "new\n"
	   "go [Leave force mode. Make computer play a move]\n"
	   "help [This screen.]\n"
	   "nopost [Do not show main line]\n"
	   "otim [Set opponents remaining time in 1/100 s]\n"
	   "time [Set computers remaining time in 1/100 s]\n"
	   "level [Set game time format, e.g. level 0 5 0]\n"
	   "ponder [Toggle permanent brain usage]\n"
	   "setup [Set position up from FEN or EPD String]\n");
  else
    printf("No help available for command\n");
  
  return 0;
}
