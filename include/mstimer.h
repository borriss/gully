/* $Id: mstimer.h,v 1.10 2007-01-12 20:29:04 martin Exp $ */

#ifndef __MSTIMER_H
#define __MSTIMER_H

/* change NPS if machine according to machine */
#define NPS 800000
/* interval after which the time is checked */
#define NODES_BETWEEN_TIME_CHECK (NPS/3)

#define DEFAULT_TIME_PER_MOVE 100 /* this is 10 seconds */


/* 
 * Keep all time related info here:
 * time allocated for the next iteration,
 * ICS-related information, such as own time, time of
 * opponent etc.
 */

struct game_time_tag {
  int time_per_move; /* in 1/10 of seconds, from command line or default */

  /* next nodecount triggering checking for time and input  */
  unsigned long next_check; 
  unsigned long timestamp; /* 1/100 s - marks start of search, ponder, .. */
  unsigned long stop_time;   /* 1/100 s */
  unsigned long time_allocated;   /* 1/100 s */
  int increment; /* 1 s */
  int moves; /* number of moves to make till next time control -
		first argument in level command - e.g. level 40 5 0 */
  int total; /* 1 s */
  int left; /* 1/100 s */
  int o_left; /* opponents remaining time is useful info :) */
  int use_game_time; /* flag triggered by level. Otherwise fixed time
			per move */
};

/* defined in mstimer.c */
extern struct game_time_tag game_time;

/* platform specific timer - returns time in 1/100 s */
unsigned long get_time(void);

double time_diff(unsigned long,unsigned long);

/* returns 1 if time has exceeded allocated time. */
int time_check(void);

/* resets timer data. Currently only used by setup_board.
*/
void init_game_time(void);

/* obey_ponder == 0 -> normal allocation, else
   the time we are pondering is subtracted from allocation 
   */
void allocate_time(int obey_ponder);

/* Marks the current time in time_stamp member (moved from gamestat.timer) */
void timestamp(void);

#endif /* mstimer.h */
