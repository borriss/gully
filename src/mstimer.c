/* $Id: mstimer.c,v 1.11 2007-01-15 20:35:45 martin Exp $ */

/* 
 *  Routines for getting the time. 
 *  Resolution of portable calls to read the time is not enough:
 *  Under Unix, times() is used which gives 10 milliseconds
 *  resolution, under WIN32 I do _fread() which probably has also 
 *  a 10 millisecond resolution. 
 */

#if defined (UNIX)
#include <sys/times.h>
#elif defined (WIN32)
/* for win32 based systems _ftime is used */
#include <sys/timeb.h>
#endif

#include <time.h>
#include <assert.h>
#include <stdio.h> /* XXX only for printf */

#include "mstimer.h"
#include "logger.h"

#ifndef MAX
#define MAX(A,B) ((A) > (B) ? (A) : (B))
#endif

/* all time related information */
struct game_time_tag game_time;

void
init_game_time()
{
  if(! game_time.time_per_move)
    game_time.time_per_move = DEFAULT_TIME_PER_MOVE;

  game_time.next_check = 0;
  game_time.timestamp = 0;
  game_time.stop_time = 0;
  game_time.time_allocated = 0;
  game_time.increment = 0; /* no increment (in seconds) */
  game_time.moves = 0; /* default: sudden death */
  game_time.use_game_time = 0; /* level command has to use this info */ 
  game_time.total = 300; /* seconds! */
  game_time.left = 30000; /* 1/100 seconds */
  game_time.o_left = 30000; /* 1/100 seconds */
}

int
time_check()
{
  if (get_time() >= game_time.stop_time) return 1;
  else return 0;
}

/* 
   This will be a *hint* to the chess engine how much time to use.

   The obey_ponder parameter subtracts time spent pondering the
   correct move from the allocated time.

   game_time.time_allocated is in 1/100 seconds
   game_time.time_per_move is in 1/10 seconds
*/
void 
allocate_time(int obey_ponder)
{
  /* simple time - no level command, constant time per move */
  if(!game_time.use_game_time)
    game_time.time_allocated = game_time.time_per_move * 10;
  else {
    if(game_time.moves > 0) 
      game_time.time_allocated = game_time.left / game_time.moves +
	80 * game_time.increment;
    else /* sudden death game */ {
      game_time.time_allocated = (game_time.left >> 6) +
	80 * game_time.increment;
    }
  }

  if(obey_ponder) {
    log_msg("allocated were %ld 1/100 s\n", game_time.time_allocated);
    game_time.time_allocated -= (get_time() - game_time.timestamp);
    log_msg("due to ponder this is %ld 1/100 s\n", game_time.time_allocated);
    game_time.time_allocated = MAX(game_time.time_allocated, 0);
  }

  game_time.stop_time = get_time() + game_time.time_allocated;
}

unsigned long
get_time(void)
{
#ifdef UNIX
  struct tms t;
  
  (void) times(&t);
  return (t.tms_utime + t.tms_stime);

#elif defined (WIN32)
  struct _timeb time_buf;
  _ftime(&time_buf);
  return (time_buf.time * 100) + (time_buf.millitm / 10);

#else
  err_quit("No timing code implemented for this platform.\n");
  return 0l; /* never reached */
#endif
}

double
time_diff(unsigned long t1,unsigned long t2)
{
  return ((double)((t1>t2) ? (t1-t2) : (t2-t1)) / 100.0);
}

void 
timestamp()
{
  game_time.timestamp = get_time();
}
 

