/* $Id: tables.h,v 1.9 2005-02-16 21:20:32 martin Exp $ */

#ifndef __TABLES_H
#define __TABLES_H

#include <assert.h>
#include "pvalues.h"

typedef int table_t[128];

#define BAD 500

extern table_t knight_position,bishop_position,queen_position,
  king_eg_position,king_position,
  king_bnw_position,king_bnb_position,
  white_pawn_position,black_pawn_position;

/* rook bonus for side-to-side mobility (number of square available
   to both sides combined */
extern int rook_side_to_side_bonus[8];

/* Instead of keeping just the total sum of the piece values (which
   causes the trouble of extracting the values in the evaluate() 
   functions) we prepare a more distinguished way:

   There is a table of 128 entries which serves as an eval cache
   for common material configurations. The same table is used for 
   white and for black.
   
   It is indirectly indexed by the 16-bit material-keeping word:

   16-bit {w|b}pmat: (Example for starting position)
 
   QQQQRRRRBBBBNNNN
   ++++++++++++++++
   0001001000100010
   ++++++++++++++++
      !  !!  !!  !!

   Although we can keep up to 15 queens, rooks, bishops, knights 
   for each color (anyone thinking of bughouse ;) ), we use the
   table only if all unmarked (not "!") bits are zero.
   
   */ 

#define MAT_DIST_MAX 128 /* table size */

typedef struct material_distribution_tag {
  unsigned int material_value;
  unsigned int valid; 
#if 0
  unsigned char is_valid; 
  unsigned char attack_strength;
  unsigned char defense_strength;
  unsigned char harmony_bonus;
#endif
} material_distribution_t;

extern material_distribution_t mat_dist[];


/*
 * Calculate piece material score for one side from given 
 * 16-bit signature.
 * 
 * Returns: material value in centipawns. 
 *
 */

unsigned int piece_score_from_sig(unsigned int sig);

/*
 * convienience function to extract sum of piece values from
 * move_flags.[w|b]_mat.
 * This is just to ease the transition from the GET_PIECE_MATERIAL
 * macro. 
 */

unsigned int get_piece_material(unsigned int sig);


/*
 * Make 7bit index from 16bit index, if possible.
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |F|E|D|C|B|A|9|8|7|6|5|4|3|2|1|0|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Q Q Q Q|R R R R|B B B B|N N N N|  
 *        =     ===     ===     ===
 * 
 * Returns: valid 7bit-wide index corresponding to the  
 * given material signature "sig".
 *
 * On error, MAT_DIST_INVALID_INDEX is returned. 
 */
int make_mat_dist_index(int sig);

#define MAT_DIST_INVALID_INDEX ~0

/* inline keyword is an extension in both GCC and Visual C,
 * spelling differs. 
 */
#if defined (WIN32)
extern __inline int
#else 
extern __inline__ int 
#endif
make_mat_dist_index(int sig)
{
  int i;

  sig &= 0x0000ffff; /* low 16 bit have the piece info */

  /* error, this is an uncommon material distribution */ 
  if(sig & ~0x1333)  return MAT_DIST_INVALID_INDEX; 
  
  /* Now, simply take bits 0,1,4,5,8,9,C. Pack them into 7 bit
     and return 'em. */
  
  i = (sig & 0x0003) | ((sig & 0x0030) >> 2) | ((sig & 0x0300) >> 4)
    | ((sig & 0x1000) >> 6);
  
  assert((i & ~0x7f) == 0);
  
  return i;
}


/* 
 * Take a valid index and return the plain material score from 
 * mat_dist_table.
 *
 */

unsigned int get_mat_dist_mat(int sig_7bit);

#if defined (WIN32)
extern __inline unsigned int
#else 
extern __inline__ unsigned int 
#endif
get_mat_dist_mat(int sig_7bit)
{
  assert((sig_7bit & ~(MAT_DIST_MAX - 1)) == 0);
  
  return mat_dist[sig_7bit].material_value;
}


#endif /* tables.h */
