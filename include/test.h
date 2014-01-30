/* $Id: test.h,v 1.10 2000-06-12 16:27:18 martin Exp $ */

#ifndef __TEST_H
#define __TEST_H


#define MAX_SOL_DEPTH 40 /* provide space for up to 40 plies when solving
			    a position */

/* statistics when solving an individual position */
struct pos_solve_stat_tag {
  int sol_move[MAX_SOL_DEPTH]; /* only from_to characterizes move */
  int sol_time[MAX_SOL_DEPTH]; /* after this iteration was completed */
  int sol_nodes[MAX_SOL_DEPTH]; /* total evals ( == quies_nps) up
				 to this ply */
};

int do_test(void);
int update_pss(struct pos_solve_stat_tag * pss, int ply, move_t *m,
	       float time);
unsigned long bogomips(void);

#endif /* test.h */
