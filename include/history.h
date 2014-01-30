/* $Id: history.h,v 1.5 1997-06-09 20:14:08 martin Exp $ */

#ifndef __HISTORY_H
#define __HISTORY_H

#define MAX_KILLER_PLY MAX_SEARCH_DEPTH
#define RESETTED_USE_COUNT 3  /* max value which a usecount is resetted to */

struct killer_struct_tag {
  int from_to;
  int use_count;
};

extern struct killer_struct_tag Killer[MAX_KILLER_PLY][2];

void reset_killers(void);
void update_killers(int from_to);
void reset_killer_use_count(int ply);
void pv_2_killer(void);

#endif /* history.h */
