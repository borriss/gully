/* $Id: repeat.h,v 1.4 1997-03-16 10:40:41 martin Exp $ */

#ifndef __REPEAT_H
#define __REPEAT_H

extern position_hash_t * repetition_head_w;
extern position_hash_t * repetition_head_b; 

void reset_rep_heads(void);
void rep_show_offset(void);
int repetition_check(const int ply, const position_hash_t *hash_value);
int draw_by_repetition(const position_hash_t *hash_value);

#endif /* repeat.h */
