/* $Id: evaluate.c,v 1.31 2007-01-12 20:30:58 martin Exp $ */

#include <assert.h>
#include <stdlib.h> /* abs - non-ANSI */

#include "chess.h"
#include "plist.h"
#include "pvalues.h"
#include "logger.h"
#include "evaluate.h"
#include "tables.h"
#include "chessio.h"
#include "transref.h" /* pawn hashing */

#ifndef NDEBUG
#include "hash.h"
#endif

/* get score from 32bit int - its in the lowest 12 bit */
#define PH_GET_P_SCORE(i) (((i) & 0x0fff) - 0x0800)
#define PH_B(i) (((i) >> 24) & 0x00ff)
#define PH_W(i) (((i) >> 16) & 0x00ff)

/* folds score (12bit) and bytes for open files into 32bit */
#define PH_MAKE_P_SCORE(sc,w,b) ((((sc) + 0x0800) & 0x0fff) 	\
			      | (((w) & 0xff) << 16) 	\
			      | (((b) & 0xff) << 24))

/* fold info on weak / passed pawns into 32bit int */
#define PH_MAKE_WP(wweak,bweak,wpassed,bpassed ) (   \
     wweak | (bweak << 8) | (wpassed << 16) | (bpassed << 24))

/* Get those values back */
#define PH_GET_WWEAK(x32) (x32 & 0x00ff)
#define PH_GET_BWEAK(x32) ((x32 & 0x0000ff00) >> 8)
#define PH_GET_WPASSED(x32) ((x32 & 0x00ff0000) >> 16)
#define PH_GET_BPASSED(x32) ((x32 & 0xff000000) >> 24)

/* tests pawn mask for pawn. */
#define PH_IS_WP(sq) (WP_mask[(sq) >> 5] & (1 << ((sq) & 0x1f)))	

#define PH_IS_BP(sq) (BP_mask[(sq) >> 5] & (1 << ((sq) & 0x1f))) 


int eval_pawns(unsigned int *,unsigned char *, unsigned char *);
int evaluate_endgame(int alpha,int beta);
int evaluate_mate(int wpi,int bpi);
int evaluate_bn_mate(int wpi,int bpi);

/* for square output */
static char sq_buf[3];

/* characteristics of the material distribution */
static unsigned int wpiece_7sig, bpiece_7sig;

/* initially, that is */
int max_pos_score = PAWNVALUE;

/*
 * The long way to get a material score. Used by initialization (and
 * as debugging aid).
 *
 * Returns the material balance and sets *wmat and *bmat.
 *
 * XXX: Should be moved to init.c
 */
int
get_material_score(int * wmat, int * bmat)
{
  plistentry_t *PListPtr = PList;
  int is_white=1, wm, bm; 

  wm = bm = 0;
  
  while(PListPtr < PList + PLIST_MAXENTRIES) {
    if(*PListPtr != NO_PIECE) {
      if(is_white && PLIST_OFFSET(PListPtr) >= BPIECE_START_INDEX)
	is_white=0;

      assert((GET_SQUARE(*PListPtr) & 0x88) == 0);
      switch(GET_PIECE(*PListPtr)) {
      case PAWN:
	if(is_white) CHANGE_PAWN_MATERIAL(wm,PAWNVALUE);
	else CHANGE_PAWN_MATERIAL(bm,PAWNVALUE);
	break;
      case KNIGHT:
	if(is_white) ADD_KNIGHT(wm);
	else ADD_KNIGHT(bm);
	break;
      case BISHOP:
	if(is_white) ADD_BISHOP(wm);
	else ADD_BISHOP(bm);
	break;
      case ROOK:
	if(is_white) ADD_ROOK(wm);
	else ADD_ROOK(bm);
	break;
      case QUEEN:
	if(is_white) ADD_QUEEN(wm);
	else ADD_QUEEN(bm);
	break;
      case KING: /* not counted */
	break;
      default:
	err_quit("Bad piece: %d\n", GET_PIECE(*PListPtr));
      }
    }
    ++PListPtr;
  }
  
  if(wmat != NULL && bmat != NULL) { 
    *wmat = wm; 
    *bmat = bm;
  }

  return ((get_piece_material(wm) - get_piece_material(bm)) + 
	  (GET_PAWN_MATERIAL(wm) - GET_PAWN_MATERIAL(bm)));
}

/*
 * This function is responsible for full positional evaluation.
 * To enable lazy evaluation, it needs to know the current 
 * alpha/beta bounds. 
 */ 

int 
evaluate(int alpha,int beta)
{
  plistentry_t *PListPtr=PList;
  int is_white, material, score;

  unsigned char rook_flag;    /* indicates if other rook already on
				 7th rank */
  unsigned char pw_ho, pb_ho; /* half-open file bitmask retrieved 
				 from pawn table */
  unsigned int wp; /* info on weak and passed pawns from ptable */

  is_white = 1;

  /* just count how often eval was called */
  ++gamestat.evals;

  /* set the 7 bit signature. Note that this may contain an error-
   * code for uncommon material distributions. 
   */
  wpiece_7sig = make_mat_dist_index(move_flags[current_ply].w_material);
  bpiece_7sig = make_mat_dist_index(move_flags[current_ply].b_material);

  /* in case we are in an ending it is a completely different story 
   * currently, PAWNLESS is set only in endgames (see helpers.c: phase())
   */
  if((game_phase == ENDGAME) || (game_phase == PAWNLESS))
    return evaluate_endgame(alpha,beta);

  /* 
   * MIDDLEGAME EVALUATION 
   */

  /* 
   * Get the material balance. Common piece material distributions
   * are characterized by a 7 bit signature which is used for a 
   * table lookup in mat_dist_table.
   * If speed and the additional info available in this table are 
   * not important, the convienience function get_piece_material is
   * used. It returns simply the material balance according to the
   * piece value, without any bonuses or penalties.
   */
  
  material = (wpiece_7sig == MAT_DIST_INVALID_INDEX) 
    ? piece_score_from_sig(move_flags[current_ply].w_material) 
    : get_mat_dist_mat(wpiece_7sig);
  material -= (bpiece_7sig == MAT_DIST_INVALID_INDEX) 
    ? piece_score_from_sig(move_flags[current_ply].b_material) 
    : get_mat_dist_mat(bpiece_7sig);
  material +=  (GET_PAWN_MATERIAL(move_flags[current_ply].w_material) 
		- GET_PAWN_MATERIAL(move_flags[current_ply].b_material));
  
  score = material; /* first approximation of postions score :-) */
  
  if(PRINT_EVAL_ON) {
    printf("Material: %d (wpieces: %u bpieces: %u "
	   "wpawn %ld bpawn %ld)\n",
	   material,
	   get_piece_material(move_flags[current_ply].w_material), 
	   get_piece_material(move_flags[current_ply].b_material),
	   GET_PAWN_MATERIAL(move_flags[current_ply].w_material), 
	   GET_PAWN_MATERIAL(move_flags[current_ply].b_material));
  }
  
  /* Lazy evaluation 
   * If even a big positional score cannot bring the score into
   * the window, evaluation will be skipped.
   */ 
  
  if(! FULLEVAL_ON ) {
    if(turn == WHITE) {
      if((score + max_pos_score < alpha) 
	 || (score - max_pos_score > beta))
	return score;
    }
    else {
      assert(turn == BLACK);
      if((-score + max_pos_score < alpha) 
	 || (-score - max_pos_score > beta))
	return -score;
    }
  }

  /* PAWN evalution.
   *
   * Sets up info for piece information (open files, closeness of
   * position, good/back bishops, king safety.)
   */

  score += eval_pawns(&wp, &pw_ho, &pb_ho);

  if(PRINT_EVAL_ON)
    printf("total score after pawn eval: %d\n", score);

  /* Piece evaluation: 
   * Static scores from piece-square tables.
   */

  rook_flag = 0; /* used to detect rook pairs on 7th rank */ 

  /* important that we loop as little as possible, here only over the
     part of the list where we can expect pieces */

  while(PListPtr < PList + MaxBlackPiece) {
    if(*PListPtr != NO_PIECE) {
      assert((GET_SQUARE(*PListPtr) & 0x88) == 0);
      switch(GET_PIECE(*PListPtr)) {
      case KNIGHT:
	assert(knight_position[GET_SQUARE(*PListPtr)] != BAD);
	if(is_white) 
	  score += knight_position[GET_SQUARE(*PListPtr)];
	else score -= knight_position[GET_SQUARE(*PListPtr)];
	if(PRINT_EVAL_ON) {
	  printf("Knight bonus (%s) %d\n",(is_white) ? "white" : 
		 "black",knight_position[GET_SQUARE(*PListPtr)]);
	}
	break;
      case BISHOP:
	assert(bishop_position[GET_SQUARE(*PListPtr)] != BAD);
	if(is_white) 
	  score += bishop_position[GET_SQUARE(*PListPtr)];
	else score -= bishop_position[GET_SQUARE(*PListPtr)];
	if(PRINT_EVAL_ON) {
	  printf("Bishop bonus (%s) %d\n",(is_white) ? "white" : 
		 "black",bishop_position[GET_SQUARE(*PListPtr)]);
	}
	break;
      case ROOK: /* award rook on (half-)open files */
	
	/* XXX use weak and passed pawn info */
	{
	  square_t rook_square = GET_SQUARE(*PListPtr);
	  int rook_file = GET_FILE(rook_square);
	  
	  if(is_white) {
	    if(!(pw_ho & (1 << rook_file))) {
	      if(!(pb_ho & (1 << rook_file)))
		{
		  if(PRINT_EVAL_ON)
		    printf("%s rook %s open file bonus %d\n",
			   (is_white) ? "white" : "black",
			   square_name(rook_square,sq_buf),
			   ROOK_OPEN_FILE);
		  score += ROOK_OPEN_FILE;
		}
	      else {
		if(PRINT_EVAL_ON)
		  printf("%s rook %s half open file bonus %d\n",
			 (is_white) ? "white" : "black",
			 square_name(rook_square,sq_buf),
			 ROOK_HALFOPEN_FILE);
		score += ROOK_HALFOPEN_FILE;
	      }
	    }
	    /* not on half-open or open file, look for side
	       mobility */
	    else {
	      int sq = rook_square, rook_side_mob = 0;
	      if(PRINT_EVAL_ON) printf("Looking for mobility of white rook "
				       "%s\n", square_name(rook_square,
							   sq_buf));
	      while((++sq & 0x88) == 0 && ((BOARD[sq] == BOARD_NO_ENTRY) ||
					   (GET_PIECE(*BOARD[sq]) == ROOK)))
		rook_side_mob++;
	      sq = rook_square;
	      while((--sq & 0x88) == 0 && ((BOARD[sq] == BOARD_NO_ENTRY) ||
					   (GET_PIECE(*BOARD[sq]) == ROOK)))
		rook_side_mob++;

	      score += rook_side_to_side_bonus[rook_side_mob];
	      
	      if(PRINT_EVAL_ON) printf("Mobility bonus of white rook %s: "
				       "%d (%d squares available)\n",
				       square_name(rook_square, sq_buf),
				       rook_side_to_side_bonus[rook_side_mob],
				       rook_side_mob);
	    }
	    
	    if(GET_RANK(rook_square) == 6) {
	      if(rook_flag++) {
		if(PRINT_EVAL_ON)
		  printf("%s rook %s rook_pair 7th rank bonus %d\n",
			 (is_white) ? "white" : "black",
			 square_name(rook_square,sq_buf),
			 ROOKPAIR_7TH_RANK);
		score += ROOKPAIR_7TH_RANK;
	      }
	      else {
		if(PRINT_EVAL_ON)
		  printf("%s rook %s rook 7th rank bonus %d\n",
			 (is_white) ? "white" : "black",
			 square_name(rook_square,sq_buf),
			 ROOK_7TH_RANK);
		score += ROOK_7TH_RANK;
	      }
	    }
	    
	  }
	  else /* black rook */ {
	    if(!(pb_ho & (1 << rook_file))) {
	      if(!(pw_ho & (1 << rook_file))) {
		if(PRINT_EVAL_ON)
		  printf("%s rook %s open file bonus %d\n",
			 (is_white) ? "white" : "black",
			 square_name(rook_square,sq_buf),
			 ROOK_OPEN_FILE);
		score -= ROOK_OPEN_FILE;
	      }
	      else {
		if(PRINT_EVAL_ON)
		  printf("%s rook %s half open file bonus %d\n",
			 (is_white) ? "white" : "black",
			 square_name(rook_square,sq_buf),
			 ROOK_HALFOPEN_FILE);
		score -= ROOK_HALFOPEN_FILE;
	      }
	    }
	    /* not on half-open or open file, look for side
	       mobility */
	    else {
	      int sq = rook_square, rook_side_mob = 0;
	      if(PRINT_EVAL_ON) printf("Looking for mobility of black rook "
				       "%s\n", square_name(rook_square,
							   sq_buf));
	      while((++sq & 0x88) == 0 && ((BOARD[sq] == BOARD_NO_ENTRY) ||
					   (GET_PIECE(*BOARD[sq]) == ROOK)))
		rook_side_mob++;
	      sq = rook_square;
	      while((--sq & 0x88) == 0 && ((BOARD[sq] == BOARD_NO_ENTRY) ||
					   (GET_PIECE(*BOARD[sq]) == ROOK)))
		rook_side_mob++;

	      score -= rook_side_to_side_bonus[rook_side_mob];

	      if(PRINT_EVAL_ON) printf("Mobility bonus of black rook %s: "
				       "%d (%d squares available)\n",
				       square_name(rook_square, sq_buf),
				       rook_side_to_side_bonus[rook_side_mob],
				       rook_side_mob);
	    }
	    

	    if(GET_RANK(rook_square) == 1) {
	      if(rook_flag++) {
		if(PRINT_EVAL_ON)
		  printf("%s rook %s rook_pair 7th rank bonus %d\n",
			 (is_white) ? "white" : "black",
			 square_name(rook_square,sq_buf),
			 ROOKPAIR_7TH_RANK);
		score -= ROOKPAIR_7TH_RANK;
	      }
	      else {
		if(PRINT_EVAL_ON)
		  printf("%s rook %s rook 7th rank bonus %d\n",
			 (is_white) ? "white" : "black",
			 square_name(rook_square,sq_buf),
			 ROOK_7TH_RANK);
		score -= ROOK_7TH_RANK;
	      }
	    } /* rook 2nd rank */
		  
	  } /* black rook */
	  
	} /* rook eval block */ 
	break;
      case QUEEN:
	assert(queen_position[GET_SQUARE(*PListPtr)] != BAD);
	if(is_white) score += queen_position[GET_SQUARE(*PListPtr)];
	else score -= queen_position[GET_SQUARE(*PListPtr)];
	if(PRINT_EVAL_ON) {
	  printf("Queen bonus (%s) %d\n",(is_white) ? "white" : 
		 "black",queen_position[GET_SQUARE(*PListPtr)]);
	}
	break;
      default:
	break;
      }
    }
    ++PListPtr;
    if(is_white && PLIST_OFFSET(PListPtr) == MaxWhitePiece) {
      PListPtr = PList + BPIECE_START_INDEX;
      is_white = 0;
      rook_flag = 0;
    }
  }
  
  if(PRINT_EVAL_ON)
    printf("total score after static piece eval: %d\n", score);

#if 1
  assert(GET_PIECE(*BOARD[move_flags[current_ply].white_king_square]) 
	 == KING);
  assert(GET_PIECE(*BOARD[move_flags[current_ply].black_king_square]) 
	 == KING);

  /* a very primitive static king scoring */
  assert(king_position[move_flags[current_ply].white_king_square]
	 != BAD);
  assert(king_position[move_flags[current_ply].black_king_square]
	 != BAD);
  
  score += king_position[move_flags[current_ply].white_king_square];
  score -= king_position[move_flags[current_ply].black_king_square];
#endif

  ++gamestat.full_evals;

  if(PRINT_EVAL_ON)
    printf("================\n");


  /* adjust maximum positional score */
  if(abs(score - material) > max_pos_score)
    max_pos_score = abs(score - material);

  return (turn == WHITE) ? score : -score;
}

/*
 * Evaluates pawn structure. 
 * Normally, it will succeed in looking up a previously calculated score,
 * return score (as seen from white) and
 * some info on weak/passed pawns etc.
 * If not, the pawn formation will be (expensively) evaluated and
 * stored into the pawn hash table.
 * weak_passed: 4 bytes having the files with passed and and weak pawns
 *              for both colors
 * pw_ho,pb_ho - byte pointers to half-open file bitmask
 */

int
eval_pawns(unsigned int *weak_passed,
	   unsigned char *pw_ho, unsigned char *pb_ho)
{
  int score;

#ifndef NDEBUG
  position_hash_t debug_phash64;

  generate_pawn_hash_value(&debug_phash64);
  if(!CMP64(move_flags[current_ply].phash,debug_phash64)) {
    fprint_current_line(stdout);
    err_quit("flags_phash %08x:%08x != debugphash %08x:08x\n",
	     move_flags[current_ply].phash.part_one,
	     move_flags[current_ply].phash.part_two,
	     debug_phash64.part_one,
	     debug_phash64.part_two);
  }
#endif

  /* look up pawn formation */
  if(ph_retrieve(&move_flags[current_ply].phash, &score, weak_passed) 
     ==  PH_RT_FOUND) {
    /* found entry */
    ++gamestat.p_hash_hits;
    
    *pw_ho =  PH_W(score);
    *pb_ho =  PH_B(score);

    return PH_GET_P_SCORE(score);
  }

  /* not found, we need to assess this position */
  {
    plistentry_t *PListPtr;
    square_t sq, aux_sq1, aux_sq2, aux_sq3;

    int i,j;

    /* two bytes telling the locations of half-open files each */
    unsigned char w_ho,b_ho;
  
    /*
     * {W|B}P_count represents the 8 files of the chessboard from a to h.
     * This structure just says how many pawns are on those files. Useful
     * for detecting open files etc.
     */

    unsigned char WP_cnt[8] = {0,0,0,0,0,0,0,0};
    unsigned char BP_cnt[8] = {0,0,0,0,0,0,0,0};

    /* {W|B}P_mask is a  128-bit mask. The individual bits are set
       if there is a pawn on this square.
    */
    unsigned int WP_mask[4] = {0,0,0,0};
    unsigned int BP_mask[4] = {0,0,0,0};

    /* 
     * info on weak pawns and passed pawns 
     * this will be stored into pawn hash table.
     */
    unsigned char WP_weak = 0, BP_weak = 0, WP_passed = 0, BP_passed = 0;
    
    ++gamestat.p_hash_misses;
    score = 0;


    PListPtr = PList + WPAWN_START_INDEX;

    while(PListPtr < PList + MaxWhitePawn) {
      if(*PListPtr != NO_PIECE) {
	assert((GET_SQUARE(*PListPtr) & 0x88) == 0);
	assert(GET_PIECE(*PListPtr) == PAWN);

	sq = GET_SQUARE(*PListPtr);
	WP_cnt[GET_FILE(sq)]++;

	WP_mask[sq >> 5] |= ( 1 << (sq & 0x1f)); 
	    
	assert(white_pawn_position[sq] != BAD);
	score += white_pawn_position[sq];
      }
      ++PListPtr;
    }

    /* same for black */
    PListPtr = PList + BPAWN_START_INDEX;
	
    while(PListPtr < PList + MaxBlackPawn) {
      if(*PListPtr != NO_PIECE) {
	assert((GET_SQUARE(*PListPtr) & 0x88) == 0);
	assert(GET_PIECE(*PListPtr) == PAWN);

	sq = GET_SQUARE(*PListPtr);
	BP_cnt[GET_FILE(sq)]++;
	    
	BP_mask[sq >> 5] |= ( 1 << (sq & 0x1f)); 

	assert(black_pawn_position[sq] != BAD);
	score -= black_pawn_position[sq];
      }
      ++PListPtr;
    }

    if(PRINT_EVAL_ON)
      printf("static pawn score: %d\n", score);


    /* count doubled pawns and fill out open files bytes */
    w_ho = b_ho = 0;

    for(i = 0; i < 8; i++) {
      if(WP_cnt[i] >= 1)
	w_ho |= 1 << i;
	  
      if(WP_cnt[i] > 1) {
	score -= DOUBLED_PAWN_PENALTY * (WP_cnt[i] - 1);
	if(PRINT_EVAL_ON) {
	  printf("Doubled white pawn on %c-file, penalty: %d\n",
		 i + 'a',
		 DOUBLED_PAWN_PENALTY * (WP_cnt[i] - 1));
	}
      }

      if(BP_cnt[i] >= 1)
	b_ho |= 1 << i;

      if(BP_cnt[i] > 1) {
	score += DOUBLED_PAWN_PENALTY * (BP_cnt[i] - 1);
	if(PRINT_EVAL_ON) {
	  printf("Doubled black pawn on %c-file, penalty: %d\n",
		 i + 'a',
		 DOUBLED_PAWN_PENALTY * (BP_cnt[i] - 1));
	}
      }
    }

    *weak_passed = 0;

    /* find out about weak pawns 
       for simplicity, walk again through pawn list.
    */
    PListPtr = PList + WPAWN_START_INDEX;
  
    while(PListPtr < PList + MaxWhitePawn) {
      int file,rank;

      if(*PListPtr != NO_PIECE) {
	sq = GET_SQUARE(*PListPtr);
	file = GET_FILE(sq);
	rank = GET_RANK(sq);

	assert(file >= 0 && file < 8);

	/* for all files */
	if((file == 0 || WP_cnt[file-1] == 0) && 
	   (file == 7 || WP_cnt[file+1] == 0)) {
	  if(PRINT_EVAL_ON)
	    printf("w pawn on %s isolated [-%d]\n",
		   square_name(sq,sq_buf),
		   ISOLATED_PENALTY);
	  score -= ISOLATED_PENALTY;

	  WP_weak |= 1 << file;

	  /* if on half-open file, its even worse */
	  if(!BP_cnt[file]) {
	    /* note possible failure, eg. wpg5 bpg4 */
	    if(PRINT_EVAL_ON)
	      printf("w isolated pawn half-open %s [-%d]\n",
		     square_name(sq,sq_buf),
		     ISOLATED_HO_PENALTY);
	    score -= ISOLATED_HO_PENALTY;
	  }		    
	}
	else { /* not isolated, but perhaps (lightly) backward */ 
	  int supported =  0;

	  /* for white, scan the adjacent files backwards for
	     supporting pawns */
	  for (j = rank; j > 0; j--) {
	    aux_sq1 = (file == 0) ? 0 : MAKE_SQUARE(file - 1,j); 
	    aux_sq2 = (file == 7) ? 0 : MAKE_SQUARE(file + 1,j);

	    if ((file != 0 && PH_IS_WP(aux_sq1)) || 
		(file != 7 && PH_IS_WP(aux_sq2)))
	      supported++;
	  }

		  
	  if (!supported) {
	    WP_weak |= 1 << file;

	    /* first, it gets a bonus! for advance (weak advanced
	       pawns aren't that bad */
	    score += rank;
	    if(PRINT_EVAL_ON)
	      printf("w advanced weak pawn on %s [+%d]\n",
		     square_name(sq,sq_buf),
		     rank);


	    if((file != 0 && 
		PH_IS_WP(MAKE_SQUARE(file - 1, rank + 1))) ||
	       (file != 7 && 
		PH_IS_WP(MAKE_SQUARE(file + 1, rank + 1)))) {
	      /* a lightly backward pawn */
	      /* maybe its fixed there */
	      if((rank < 7) &&
		 ((file != 0 && 
		   PH_IS_BP(MAKE_SQUARE(file - 1, rank + 2))) ||
		  (file != 7 && 
		   PH_IS_BP(MAKE_SQUARE(file + 1, rank + 2))))) {
		if(PRINT_EVAL_ON)
		  printf("fixed lightly w backward pawn %s "
			 "[-%d]\n",
			 square_name(sq,sq_buf),
			 FIXED_LIGHTLY_BACKWARD_PENALTY);
		score -= FIXED_LIGHTLY_BACKWARD_PENALTY;
	      }
	      else {
		if(PRINT_EVAL_ON)
		  printf("lightly backward w pawn on %s "
			 "[-%d]\n",
			 square_name(sq,sq_buf),
			 LIGHTLY_BACKWARD_PENALTY);
		score -= LIGHTLY_BACKWARD_PENALTY;
	      }
	    }
	    else { /* a backward pawn */ 
	      /* maybe its fixed there */
	      if((rank < 7) &&
		 ((file != 0 && 
		   PH_IS_BP(MAKE_SQUARE(file - 1, rank + 2))) ||
		  (file != 7 && 
		   PH_IS_BP(MAKE_SQUARE(file + 1, rank + 2))))) {
		if(PRINT_EVAL_ON)
		  printf("fixed w backward pawn on %s "
			 "[-%d]\n",
			 square_name(sq,sq_buf),
			 FIXED_BACKWARD_PENALTY);
		score -= FIXED_BACKWARD_PENALTY;
	      }
	      else {
		if(PRINT_EVAL_ON)
		  printf("w backward pawn on %s "
			 "[-%d]\n",
			 square_name(sq,sq_buf),
			 BACKWARD_PENALTY);
		score -= BACKWARD_PENALTY;
	      }
	    }
	    /* if on half-open file, its even worse */
	    if(!BP_cnt[file]) {
	      /* note possible failure, eg. wpg5 bpg4 */
	      if(PRINT_EVAL_ON)
		printf("w backward pawn half-open %s "
		       "[-%d]\n",
		       square_name(sq,sq_buf),
		       BACKWARD_HO_PENALTY);
	      score -= BACKWARD_HO_PENALTY;
	    }
	  }
		  
	} /* backward pawn eval */
	      
	/* passed pawn eval */
	{
	  /* detect passed pawns, award perhaps static bonusses
	     , fill relevant data structures.
	     Connected passed pawn eval etc. must be done later */

	  int j, is_passed = 1;
		
	  /* for white, scan the three files forwards for
	     enemy pawns */
	  for (j = rank + 1; j < 7; j++) {
	    aux_sq1 = (file == 0) ? 0 : MAKE_SQUARE(file - 1,j); 
	    aux_sq2 = (file == 7) ? 0 : MAKE_SQUARE(file + 1,j);
	    aux_sq3 = MAKE_SQUARE(file,j);

	    if((file != 0 && PH_IS_BP(aux_sq1)) || 
	       (file != 7 && PH_IS_BP(aux_sq2)) ||
	       (PH_IS_BP(aux_sq3))) {
	      is_passed = 0;
	      break;
	    }
	  }
		
	  if(is_passed) {
	    if(PRINT_EVAL_ON)
	      printf("white passed pawn on %s, bonus %d\n",
		     square_name(sq,sq_buf),
		     1 << rank );
	    score += 1 << rank;
	    WP_passed |= 1 << file;

	    /* if protected, it is even better */
	    aux_sq1 = (file == 0) 
	      ? 0 : MAKE_SQUARE(file - 1,rank - 1); 
	    aux_sq2 = (file == 7) 
	      ? 0 : MAKE_SQUARE(file + 1,rank - 1);

	    if((file != 0 && PH_IS_WP(aux_sq1)) || 
	       (file != 7 && PH_IS_WP(aux_sq2))) {
	      if(PRINT_EVAL_ON)
		printf("Is protected, bonus %d\n",
		       1 << rank );

	      score += 1 << rank;
	    }
	  }

	} /* passed pawn detection */

      }

      ++PListPtr;

    } /* walk thru pawns */

    /* now the same for black pawns... */

    PListPtr = PList + BPAWN_START_INDEX;
  
    while(PListPtr < PList + MaxBlackPawn) {
      int file,rank;

      if(*PListPtr != NO_PIECE) {
	sq = GET_SQUARE(*PListPtr);
	file = GET_FILE(sq);
	rank = GET_RANK(sq);

	assert(file >= 0 && file < 8);

	/* for all files */
	if((file == 0 || BP_cnt[file-1] == 0) && 
	   (file == 7 || BP_cnt[file+1] == 0)) {
	  if(PRINT_EVAL_ON)
	    printf("black pawn on %s is isolated [-%d]\n",
		   square_name(sq,sq_buf),
		   ISOLATED_PENALTY);
	  score += ISOLATED_PENALTY;

	  BP_weak |= 1 << file;

	  /* if on half-open file, its even worse */
	  if(!WP_cnt[file]) {
	    /* note possible failure, eg. wpg5 bpg4 */
	    if(PRINT_EVAL_ON)
	      printf("b isolated pawn half-open %s [-%d]\n",
		     square_name(sq,sq_buf),
		     ISOLATED_HO_PENALTY);
	    score += ISOLATED_HO_PENALTY;
	  }		    
	}
	else { /* not isolated, but perhaps (lightly) backward */
	  int supported =  0;

	  /* for black, scan the adjacent files upwards for
	     supporting pawns */
	  for (j = rank; j < 7; j++) {
	    aux_sq1 = (file == 0) ? 0 : MAKE_SQUARE(file - 1,j); 
	    aux_sq2 = (file == 7) ? 0 : MAKE_SQUARE(file + 1,j);

	    if((file != 0 && PH_IS_BP(aux_sq1)) || 
	       (file != 7 && PH_IS_BP(aux_sq2)))
	      supported++;
	  }

		  
	  if(!supported) {
	    BP_weak |= 1 << file;

	    /* first, it gets a bonus! for advance (weak advanced
	       pawns aren't that bad */
	    score -= (7 - rank);
	    if(PRINT_EVAL_ON)
	      printf("advanced weak b pawn on %s [+%d]\n",
		     square_name(sq,sq_buf),
		     7 - rank);

	    if((file != 0 && 
		PH_IS_BP(MAKE_SQUARE(file - 1, rank - 1))) ||
	       (file != 7 && 
		PH_IS_BP(MAKE_SQUARE(file + 1, rank - 1)))) {
	      /* a lightly backward pawn */
	      /* maybe its fixed there */
	      if((rank > 2) &&
		 ((file != 0 && 
		   PH_IS_WP(MAKE_SQUARE(file - 1, rank - 2))) ||
		  (file != 7 && 
		   PH_IS_WP(MAKE_SQUARE(file + 1, rank - 2))))) {
		if(PRINT_EVAL_ON)
		  printf("fixed lightly backward b pawn "
			 "on %s [-%d]\n",
			 square_name(sq,sq_buf),
			 FIXED_LIGHTLY_BACKWARD_PENALTY);
		score += FIXED_LIGHTLY_BACKWARD_PENALTY;
	      }
	      else {
		if(PRINT_EVAL_ON)
		  printf("lightly backward b pawn "
			 "on %s [-%d]\n",
			 square_name(sq,sq_buf),
			 LIGHTLY_BACKWARD_PENALTY);
		score += LIGHTLY_BACKWARD_PENALTY;
	      }
	    }
	    else { /* a backward pawn */ 
	      /* maybe its fixed there */
	      if((rank > 2) &&
		 ((file != 0 && 
		   PH_IS_WP(MAKE_SQUARE(file - 1, rank - 2))) ||
		  (file != 7 && 
		   PH_IS_WP(MAKE_SQUARE(file + 1, rank - 2))))) {
		if(PRINT_EVAL_ON)
		  printf("fixed backward b pawn "
			 "on %s [-%d]\n",
			 square_name(sq,sq_buf),
			 FIXED_BACKWARD_PENALTY);
		score += FIXED_BACKWARD_PENALTY;
	      }
	      else {
		if(PRINT_EVAL_ON)
		  printf("backward b pawn on %s [-%d]\n",
			 square_name(sq,sq_buf),
			 BACKWARD_PENALTY);
		score += BACKWARD_PENALTY;
	      }
	    }
	    /* if on half-open file, its even worse */
	    if(!WP_cnt[file]) {
	      /* note possible failure, eg. wpg5 bpg4 */
	      if(PRINT_EVAL_ON)
		printf("backward b pawn half-open %s [-%d]\n",
		       square_name(sq,sq_buf),
		       BACKWARD_HO_PENALTY);
	      score += BACKWARD_HO_PENALTY;
	    }
	  }
		  
	} /* backward pawn eval */
	      
	/* passed pawn eval */
	{
	  /* detect passed pawns, award perhaps static bonusses
	     , fill relevant data structures.
	     Connected passed pawn eval etc. must be done later */

	  int j, is_passed = 1;
		
	  /* for black, scan the three files downwards for
	     enemy pawns */
	  for (j = rank - 1; j > 0; j--) {
	    aux_sq1 = (file == 0) ? 0 : MAKE_SQUARE(file - 1,j); 
	    aux_sq2 = (file == 7) ? 0 : MAKE_SQUARE(file + 1,j);
	    aux_sq3 = MAKE_SQUARE(file,j);

	    if((file != 0 && PH_IS_WP(aux_sq1)) || 
	       (file != 7 && PH_IS_WP(aux_sq2)) ||
	       (PH_IS_WP(aux_sq3))) {
	      is_passed = 0;
	      break;
	    }
	  }
		
	  if(is_passed) {
	    if(PRINT_EVAL_ON)
	      printf("black passed pawn on %s [+%d]\n",
		     square_name(sq,sq_buf),
		     1 << (7 - rank) );
		    
	    score -= 1 << (7 - rank);
	    BP_passed |= 1 << file;

	    /* if protected, it is even better */
	    aux_sq1 = (file == 0) ? 0 : 
	      MAKE_SQUARE(file - 1,rank + 1); 
	    aux_sq2 = (file == 7) ? 0 : 
	      MAKE_SQUARE(file + 1,rank + 1);

	    if((file != 0 && PH_IS_BP(aux_sq1)) || 
	       (file != 7 && PH_IS_BP(aux_sq2))) {
	      if(PRINT_EVAL_ON)
		printf("Is protected, bonus %d\n",
		       1 << (7 - rank) );

	      score -= 1 << (7 - rank);
	    }
	  }

	} /* passed pawn detection */

      }

      ++PListPtr;

    } /* walk thru black pawns */


    /* see if there are connected passed pawn. Bonus depends on the
       more behind one and is doubled each rank */

    if (WP_passed) {
      for (i = 0; i < 7; i++) {
	if ((WP_passed & (1 << i)) && (WP_passed & (1 << (i+1)))) {
	  int found1 = 0, found2 = 0;

	  /* find the least advanced one */
	  for (j = 6; j > 0; j--) {
	    if (PH_IS_WP(MAKE_SQUARE(i, j))) found1++;
	    if (PH_IS_WP(MAKE_SQUARE(i + 1, j))) found2++;

	    if (found1 && found2) {
	      if(PRINT_EVAL_ON)
		printf("connected white passed pawns (%d-%d) "
		       "bonus %d\n",i,i+1,
		       CONNECTED_PASSED_PAWNS  << (j-1));
	      score += (CONNECTED_PASSED_PAWNS  << (j-1));
	      break;
	    }
	  }
	}
      }
    } /* white connected passed pawns */
  
    if (BP_passed) {
      for (i = 0; i < 7; i++) {
	if((BP_passed & (1 << i)) && (BP_passed & (1 << (i+1)))) {
	  int found1 = 0, found2 = 0;

	  /* find the least advanced one */
	  for(j = 1; j < 7; j++) {
	    if(PH_IS_BP(MAKE_SQUARE(i,j))) found1++;
	    if(PH_IS_BP(MAKE_SQUARE(i+1,j))) found2++;

	    if(found1 && found2) {
	      if(PRINT_EVAL_ON)
		printf("connected black passed pawns (%d-%d) "
		       "bonus %d\n",i,i+1,
		       CONNECTED_PASSED_PAWNS  << (6 - j));
	      score -= (CONNECTED_PASSED_PAWNS  << (6 - j));
	      break;
	    }
	  }
	}
      }
    } /* black connected passed pawns */
  

    if(PRINT_EVAL_ON) {
      printf("white pawn mask %08x %08x %08x %08x\n",
	     WP_mask[0],WP_mask[1],WP_mask[2],WP_mask[3]);
      printf("black pawn mask %08x %08x %08x %08x\n",
	     BP_mask[0],BP_mask[1],BP_mask[2],BP_mask[3]);
    }


    /* fresh calculation of pawn score *almost* done */

    /* set weak and passed pawn info */
    *weak_passed = PH_MAKE_WP(WP_weak,BP_weak,WP_passed,BP_passed);

    /* ... and enter it into the table */
    if(!ph_store(&move_flags[current_ply].phash,
		 PH_MAKE_P_SCORE(score,w_ho,b_ho),*weak_passed))
      err_msg("ph_store failed\n");
    
    *pw_ho = w_ho; *pb_ho = b_ho;

    if(PRINT_EVAL_ON) {
      printf("half open files w: %02x b: %02x\n",w_ho,b_ho);
      printf("*******************\nPawn score: %.2f\n",score / 100.0);
    }
    return score;

  } /* fresh calculation of pawn score done */
}

/* ENDGAME EVAL

   XXX: determine the type of ending and call specialized 
   routine whenever possible.

   should check for trivial draws.
   call rip-off eval if necessary.

   TODO:
   (1) King position:
       use "center of pawn gravity" for dynamic rewards
       encourage kings to support squares in front of passed pawns
       (both enemy and own pawns)

   (2) Pawns:
       outside passed pawns

   (3) Use sort of material distribution to allow correct decisions
       about trades.
          Examples: 
          (a) Pawn endings with extra pawns are good, provided that
          there are no dramatic passed pawn configurations. Similar
	  for knight endings.
          (b) Endings with opposite bishops only are drawish.
	  (c) Rook endings are drawish.


   pawn and rook endings need special treatment.
   */
   


int
evaluate_endgame(int alpha, int beta)
{
  int escore, material;
  unsigned int wpimat,wpamat,bpimat,bpamat;
  unsigned int wp; /* weak and passed pawns stuffed in here */
  unsigned char pw_ho,pb_ho;
  plistentry_t *PListPtr;
  unsigned int sq;

  wpamat = GET_PAWN_MATERIAL(move_flags[current_ply].w_material);
  bpamat = GET_PAWN_MATERIAL(move_flags[current_ply].b_material);

  wpimat = (wpiece_7sig == MAT_DIST_INVALID_INDEX) 
    ? piece_score_from_sig(move_flags[current_ply].w_material) 
    : get_mat_dist_mat(wpiece_7sig);

  bpimat = (bpiece_7sig == MAT_DIST_INVALID_INDEX) 
    ? piece_score_from_sig(move_flags[current_ply].b_material) 
    : get_mat_dist_mat(bpiece_7sig);

  escore = material = wpimat + wpamat - ( bpimat + bpamat ); 
    
  if(PRINT_EVAL_ON)
    printf("Endgame_mat: %d (wpieces: %d bpieces: %d "
	   "wpawn %d bpawn %d)\n",
	   material, wpimat, bpimat, wpamat, bpamat);

  /* 1.27 (21.04.2002) -- removed call to kpk function */

  /* 
     Check for some special circumstances:
     If both sides still have a pawn, the war is still raging.
     Otherwise, if one side is completely bankrupt call the rip-off
     eval (the mating function).
  */
  if(!wpamat || !bpamat) {
    /* at least one side is totally missing any pawns */
    /* no pawns left */
    if(!wpamat && !bpamat) return evaluate_mate(wpimat,bpimat);
    
    /* one side has no pawns.

       check for draws such as wrong bishop + pawn vs. king;
       teach the program that the side without pawns often cannot
       win.

       Many interesting endings (such as RP vs R, minor piece endings,
       queen endings can be caught here.
    */
  }

  /* try shortcutting evaluation */ 
  if(! FULLEVAL_ON ) {
    if(turn == WHITE) {
      if((escore + max_pos_score < alpha) || (escore - max_pos_score > beta))
	return escore;
    }
    else {
      assert(turn == BLACK);
      if((-escore + max_pos_score < alpha) || (-escore - max_pos_score > beta))
	return -escore;
    }
  }

  ++gamestat.full_evals;

  /* pawn score */
  escore += eval_pawns(&wp, &pw_ho, &pb_ho);

  assert(king_eg_position[move_flags[current_ply].white_king_square]
	 != BAD);
  assert(king_eg_position[move_flags[current_ply].black_king_square]
	 != BAD);

  escore += king_eg_position[move_flags[current_ply].white_king_square];
  escore -= king_eg_position[move_flags[current_ply].black_king_square];

  if(PRINT_EVAL_ON)
    printf("Ending: white king position: %d \n"
	   "black king position: %d\n",
	   king_eg_position[move_flags[current_ply].white_king_square],
	   king_eg_position[move_flags[current_ply].black_king_square]
	   );


#if 0
  /* XXYY test distance functions */
  PListPtr = PList + WPAWN_START_INDEX;
  
  while(PListPtr < PList + MaxWhitePawn) {
    if(*PListPtr != NO_PIECE) {
      assert((GET_SQUARE(*PListPtr) & 0x88) == 0);
      assert(GET_PIECE(*PListPtr) == PAWN);
      
      sq = GET_SQUARE(*PListPtr);  
      
      if(PRINT_EVAL_ON) printf("wp %s: dist2q: %d, bk is %d reti away "
			       "(%d files, %d ranks, taxi: %d)\n"
			       "Reti dist to promo sq: %d (%s)\n",
			       square_name(sq, sq_buf), 
			       W_DIST_TO_QUEEN(sq),
			       RETI(move_flags[current_ply].black_king_square,
				    sq),
			       FILE_DIST(move_flags[current_ply].
					 black_king_square, sq),
			       RANK_DIST(move_flags[current_ply].
					 black_king_square, sq),
			       TAXI(move_flags[current_ply].
				    black_king_square, sq),
			       RETI(move_flags[current_ply].black_king_square,
				    WP_Q_SQ(sq)),
			       (W_DIST_TO_QUEEN(sq) < 
				RETI(move_flags[current_ply].
				      black_king_square,WP_Q_SQ(sq))) ?
			       "through!" : "hold");
    } 
    ++PListPtr;
  } 
#endif

  /* adjust maximum positional score */
  if(abs(escore - material) > max_pos_score)
    max_pos_score = abs(escore - material);

  if(PRINT_EVAL_ON)
    printf("Total ending score: %d (material: %d, position: %d)\n",
	   escore,material,escore-material);

  return (turn == WHITE) ? escore : -escore;
}


/* 
   There aren't any pawns. Make the program understand what pieces are needed
   for mate and how to do this efficiently. The simple strategy is to drive 
   the king to the edges/corners.
   Parameters passed are white and black piece material counts.
 */
int
evaluate_mate(int wpi, int bpi)
{
  int score = wpi - bpi;

  if(PRINT_EVAL_ON)
    printf("Rip-off eval\n");

  if(!wpi || !bpi)
    {
      /* trivial case, *nothing* left anymore */
      if(!wpi && !bpi) return 0;

      /* otherwise, some mating heuristics:
	 driving back the weaker king is strongly encouraged.
	 Walking into the middle is good for the stronger king (since
	 the search will find the mate then).
	 Needed is the desire of the strong king to follow the weak
	 king.
      */

      if(!bpi) {
	/* XXX depends on KNIGHTVALUE < BISHOPVALUE */
	if((wpi < ROOKVALUE) || (wpi == 2 * KNIGHTVALUE)) {
	  if(PRINT_EVAL_ON) printf("white has not enough"
				   "material to mate (%d)\n",
				   wpi);
	  return 0;
	}

	/* white mates ?*/
	if(PRINT_EVAL_ON) printf("white will mate (bmat = 0)\n");
	
	/* XXX relies on BISHOPVALUE != KNIGHTVALUE */
	if(wpi == BISHOPVALUE + KNIGHTVALUE)
	  return evaluate_bn_mate(wpi,bpi);
	
	score -= king_eg_position
	  [move_flags[current_ply].black_king_square] * 5;
	
	  score += king_eg_position
	    [move_flags[current_ply].white_king_square];
	}
      else if(!wpi) {
	/* XXX depends on KNIGHTVALUE != BISHOPVALUE */
	if((bpi < ROOKVALUE) || (bpi == 2 * KNIGHTVALUE))
	  {
	    if(PRINT_EVAL_ON) printf("black has not enough"
				     "material to mate (%d)\n",
				     bpi);
	    return 0;
	  }
	/* black mates ?*/
	if(PRINT_EVAL_ON) printf("black will mate (wmat = 0)\n");
	
	/* XXX relies on BISHOPVALUE != KNIGHTVALUE */
	if(bpi == BISHOPVALUE + KNIGHTVALUE)
	  return evaluate_bn_mate(wpi, bpi);
	
	score += king_eg_position
	  [move_flags[current_ply].white_king_square] * 5;
	score -= king_eg_position
	  [move_flags[current_ply].black_king_square];
	}
    }

  else
    /* other cases, such as R+B vs. R come here */
    {
      if(wpi > bpi) {
	score -= king_eg_position
	  [move_flags[current_ply].black_king_square] * 5;
      }
      else if (bpi > wpi)
	score += king_eg_position
	  [move_flags[current_ply].white_king_square] * 5;
    }
  return (turn == WHITE) ? score : -score;
}


/* 
 *  helper function to mate with Bishop and Knight.
 *  1. Find out who is mating who.
 *  2. Find the color of the bishop.
 *  3. Do the appropriate table lookups and return the score
 *  depending on turn.
 */
int
evaluate_bn_mate(int wpi,int bpi)
{
  square_t bishop_sq = 0x88;
  int is_white_bishop, score = wpi - bpi;

  plistentry_t *PListPtr,*StopPtr;

  if (!wpi) {
    PListPtr = PList + BPIECE_START_INDEX;
    StopPtr = PList + MaxBlackPiece;
  }
  else {
    PListPtr = PList;
    StopPtr = PList + MaxWhitePiece;
  }

  while(PListPtr < StopPtr) {
    if((*PListPtr != NO_PIECE) && (GET_PIECE(*PListPtr) == BISHOP))
      bishop_sq = GET_SQUARE(*PListPtr);
    
    ++PListPtr;
  }
      
  if(bishop_sq & 0x88) {
    err_msg("**BUG** no bishop in evaluate_bn_mate.\n");
    return (turn == WHITE) ? score : -score;
  }

  is_white_bishop =  ((GET_FILE(bishop_sq) + GET_RANK(bishop_sq)) & 1) ?
    1 : 0;

  if(PRINT_EVAL_ON) printf("white_bishop: %d\n", is_white_bishop);

  if(score > 0) /* white mates */ {
    score -= (is_white_bishop) ? king_bnw_position
      [move_flags[current_ply].black_king_square] * 5 :
      king_bnb_position[move_flags[current_ply].
		       black_king_square] * 5 ;
    score += king_eg_position [move_flags[current_ply].
			      white_king_square];
  }
  else {
    score += (is_white_bishop) ? king_bnw_position
      [move_flags[current_ply].white_king_square] * 5 :
      king_bnb_position[move_flags[current_ply].
		       white_king_square] * 5 ;
    score -= king_eg_position[move_flags[current_ply].
			     black_king_square];
  }
  
  if(PRINT_EVAL_ON) printf("bn_mater: %d\n", score);
  
  return (turn == WHITE) ? score : -score;

}

