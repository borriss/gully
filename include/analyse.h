/* $Id: analyse.h,v 1.2 2003-04-05 11:07:50 martin Exp $ */

#ifndef __ANALYSE_H
#define __ANALYSE_H

/* called from input.c: do_command() 
 * sets global state to analyzing and iterates.
 */
int analyse(void);

/* called from input.c: do_command() 
 * sets global state back to idling.
 */
int analyse_exit(void);

/* called from search to update info which move we are thinking about */
void update_analysis_stats(int move_no, int cur_index, int depth);

/* send response to "."  command */
void post_analysis_update(void);

#endif /* analyse.h */
