/* $Id: transref.h,v 1.13 2005-02-16 21:22:36 martin Exp $ */

#ifndef __TRANSREF_H
#define __TRANSREF_H

#include "chess.h"

#define TT_EMPTY    0x00000000
#define EXACT_VALUE 0x00010000 
#define LOWER_BOUND 0x00020000
#define UPPER_BOUND 0x00040000
#define TT_MOVE_USEFUL (EXACT_VALUE | LOWER_BOUND)

#define GET_TT_FLAG(tt) ((tt).hf & 0xffff0000)
#define GET_TT_HEIGHT(tt) ((tt).hf & 0x0000ffff)

#define TT_MIN_BITS 10 
#define DEFAULT_TT_BITS 21 /* 2^21 entries: ~ 40 MB */
#define TT_MAX_BITS 24 /* ~ 320 MB  */

/* ret values of tt_store */
#define TT_ST_MATCH 0
#define TT_ST_STORED 1
#define TT_ST_REPLACED 2
#define TT_NO_TABLE -1

/* ret values of tt_retrieve */
#define TT_RT_FOUND 1
#define TT_RT_NOT_FOUND 0

typedef struct tt_entry_tag {
  position_hash_t signature; /* 64 bit */
  int ft; /* from_to - stored move 32 bit */
  int score;
  unsigned int hf;  /* contains flags and height */
} tt_entry_t;

extern tt_entry_t * ttable;

int init_transref_table(int);
/* returns -1 on failure */

int tt_store(const position_hash_t * sig,int from_to,int score,int h,
	     int flag);
/* returns TT_ST_MATCH,TT_ST_STORED, TT_ST_REPLACED */

int tt_retrieve(const position_hash_t * sig,int *from_to,int *score,
		int *h,int *flag);
/* returns TT_RT_FOUND or TT_RT_NOT_FOUND */

/* to make test suites deterministic */
int tt_clear(void);
/* should return 0 on error */

/* PAWN HASH TABLE RELATED DECLARATIONS */
#define PH_EMPTY    0x00000000

#define DEFAULT_PH_BITS 13 /* 2^13 = 8192 entries */
#define PH_MIN_BITS 9 
#define PH_MAX_BITS 15

/* ret values of ph_retrieve */
#define PH_RT_FOUND 1
#define PH_RT_NOT_FOUND 0

/* 
 * entries in the pawn hash table are 6 ints (24 byte):
 * 64 bits (8 bytes) signature.
 * 32 bits (4 byte, 1 int) score + some info on open files.
 *   There are 8 bits for white pawns and
 *   8 bits for black pawns (e.g., both bits 0 means
 *   this file is open).
 *   There are 4 unused bits. Use them as a measure
 *   on the degree of "closeness" of a position. Useful
 *   for 1) bishop vs. knight and 2) king safety. (Not done
 *   yet).
 * 32 bits (4 bytes, 1 int) weak/passed pawn info.
 *    (see macros in evaluate.c on how to access it)
 * 32 bits map of white pawn attacks (within a3 .. h6 )
 * 32 bits map of black pawn attacks (within a3 .. h6 )
 * 32 bit various info (closeness, info for bishops...)
 */
typedef struct ph_entry_tag {
  position_hash_t signature; /* 64 bit */
  unsigned int score;
  unsigned int weak_passed; 
  unsigned int wp_map; /* XXX not used yet */
  unsigned int bp_map; /* XXX not used yet */
} ph_entry_t;

extern ph_entry_t * ptable;

/* 
 * Sets everything to 0 except for the first element. 
 * Returns -1 on failure.
*/
int init_pawn_table(int);

/* this is in "always replace" mode */
int ph_store(const position_hash_t * sig,int score,int wp);

/* returns PH_RT_FOUND or PH_RT_NOT_FOUND */
int ph_retrieve(const position_hash_t * sig,int *score,int *wp);

/* to make test suites deterministic */
int ph_clear(void);

/* should return 0 on error */


#endif /* transref.h */
