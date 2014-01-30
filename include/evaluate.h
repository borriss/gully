/* $Id: evaluate.h,v 1.7 2006-08-13 09:55:05 martin Exp $ */

#ifndef __EVALUATE_H
#define __EVALUATE_H

/* a few #defines: eval bonusses and penalties */
/* bonusses for eval */
#define ROOK_HALFOPEN_FILE 8
#define ROOK_OPEN_FILE 12

/* macros for pawn eval */
#define DOUBLED_PAWN_PENALTY 5
#define ISOLATED_PENALTY 10
#define ISOLATED_HO_PENALTY 10  /* additional */
#define BACKWARD_PENALTY 8
#define FIXED_BACKWARD_PENALTY 10
#define LIGHTLY_BACKWARD_PENALTY 2
#define FIXED_LIGHTLY_BACKWARD_PENALTY 5
#define BACKWARD_HO_PENALTY 10 /* additional */

/* macros for convenient calculation of (endgame) distances,
   square of the pawn etc */

#define FILE_DIST(sq1,sq2) (MAX((int) (GET_FILE(sq1) - GET_FILE(sq2)), \
				((int) (GET_FILE(sq2) - GET_FILE(sq1)))))
#define RANK_DIST(sq1,sq2) (MAX((int) (GET_RANK(sq1) - GET_RANK(sq2)), \
				((int) (GET_RANK(sq2) - GET_RANK(sq1)))))

#define TAXI(sq1,sq2) (FILE_DIST(sq1, sq2) +  RANK_DIST(sq1,sq2))
#define RETI(sq1, sq2) MAX(FILE_DIST(sq1, sq2) , RANK_DIST(sq1,sq2))

#define W_DIST_TO_QUEEN(pawn_sq)  MIN(7 - GET_RANK(pawn_sq), 5)
#define B_DIST_TO_QUEEN(pawn_sq)  MIN(GET_RANK(pawn_sq), 5)

#define WP_Q_SQ(pawn_sq) (pawn_sq | 0x70)
#define BP_Q_SQ(pawn_sq) (pawn_sq & 0x0f)

/* basic bonus for connected passed pawns 
   ( for rank = 2, doubles each rank) */
#define CONNECTED_PASSED_PAWNS 6 


#define ROOK_7TH_RANK 10
#define ROOKPAIR_7TH_RANK 50

extern int max_pos_score;

/* returns the current material_score (based on the plist) 
   Normally, material is incrementally updated, that's for init
   only.
   If white is ahead, the score is a positive number (in centipawns)
   */
int get_material_score(int *wmat, int *bmat);
int evaluate(int alpha,int beta);

#endif /* evaluate.h */
