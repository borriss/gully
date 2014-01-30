/* $Id: helpers.h,v 1.14 2011-03-05 20:45:07 martin Exp $ */

#ifndef __HELPERS_H
#define __HELPERS_H

/* zeroes move_array. excluding the 2nd index */
void clear_move_list(int,int);
void clear_move_flags(void);
void update_pv_hash(int from_to);
void update_pv(move_t *);
void clear_pv(int);
void cut_pv(void);
void update_material(piece_t);
void update_material_on_promotion(piece_t pro_p , piece_t cap_p);
void phase(void);
int solution_correct(char *buf);
void print_usage(char * progname);
void print_version(int verbosity);
void prompt(void);
/* Close logfile and do whatever housekeeping is neccessary */
void quit(void);
int command_help(int cmd);
#endif /* helpers.h */
