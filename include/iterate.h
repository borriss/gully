/* $Id: iterate.h,v 1.7 2003-04-06 17:56:47 martin Exp $ */

#ifndef __ITERATE_H
#define __ITERATE_H

/* local stats for iterate */
struct iterate_stats_tag {
  int max_depth_reached;
  int reason; /* score: either mate, draw, -mate */
};

extern struct iterate_stats_tag iterate_stats;

int iterate(int depth,enum global_search_state_tag,
	    struct pos_solve_stat_tag *);

#define P_WAIT 1
#define P_USER_MOVED 2
#define P_USE_PMOVE 3
#define P_GO 4

int ponder(void);

#endif /* iterate.h */
