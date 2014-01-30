/* $Id: chess.h,v 1.51 2003-09-13 18:07:19 martin Exp $ */

#ifndef __CHESS_H
#define __CHESS_H

#include "board.h"

#ifndef NULL
#define NULL ((void*) 0)
#endif

#define MIN(A,B) ((A) < (B) ? (A) : (B))
#define MAX(A,B) ((A) > (B) ? (A) : (B))

#define WHITE                   0
#define BLACK                   32

#define INFINITY                50000
#define MATE                    -30000
#define DRAW                    0
#define REPETITION_DRAW         0
#define WINDOW                  40
/* window around MATE where we assume the score to be a mate score */
#define MATING_THRESHOLD        200 

/* move->cap_pro and others */
#define NO_PIECE                0
#define KING                    1
#define QUEEN                   2
#define ROOK                    3
#define BISHOP                  4
#define KNIGHT                  5
#define PAWN                    6

/* move->special */
#define NORMAL_MOVE		0
#define DOUBLE_ADVANCE          1
#define EN_PASSANT		2
#define CASTLING		4
#define PROMOTION		8
#define HASHMOVE 0xff

/* move_flags */
#define WHITE_SHORT 	        1
#define WHITE_LONG              2
#define WHITE_CASTLING          3
#define BLACK_SHORT             4
#define BLACK_LONG              8
#define BLACK_CASTLING          12

/* move structure
   from_to:

 lsb            msb
 0      8       16
 -------- --------
|        |        |
 -----------------
  from     to

 0      8       16
 -------- --------
|        |        |
 -----------------
 captures promotion
*/

typedef struct move_tag {
  /* low byte: from square; 2nd byte: to square; byte 3,4 unused */ 
  int from_to; 
  int special; 
  /* low byte: captured piece; 2nd byte: promoted to; byte 3,4 unused */ 
  int cap_pro;
  int key;
} move_t;


#define MAX_MOVE_ARRAY 	1800
#define MOVE_ARRAY_SAFETY_THRESHOLD 50
extern move_t move_array[];

#define MAX_SEARCH_DEPTH 70
extern move_t * current_line[];

typedef move_t line_t[MAX_SEARCH_DEPTH];
extern line_t principal_variation[];

typedef struct position_hash_tag {
  unsigned int part_one;
  unsigned int part_two;
} position_hash_t;

/* {white|black}_material are divided into pawn material and 
 * piece material. Here are macros to access it 
 * Piece material is now managed more sophisticated. Instead of
 * an incremental score we keep a 16-bit-signature of the material
 * distribution in the lower half of [wb]_material. (This is 
 * transformed into a 7 bit key in the eval function.)
 */

#define GET_PAWN_MATERIAL(pmat) (((pmat) & 0xffff0000L) >> 16)
#define CHANGE_PAWN_MATERIAL(pmat,delta) ((pmat) += ((delta) << 16))

/* experimental way of keeping track of the material in 16bit entity
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |F|E|D|C|B|A|9|8|7|6|5|4|3|2|1|0|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |Q Q Q Q|R R R R|B B B B|N N N N|
   
   That is, up to 15 queens, rooks, bishops and knights each can be
   kept.
*/
#define REMOVE_KNIGHT(m) ((m) -= (1 << 0))
#define REMOVE_BISHOP(m) ((m) -= (1 << 4))
#define REMOVE_ROOK(m) ((m) -= (1 << 8))
#define REMOVE_QUEEN(m) ((m) -= (1 << 12))
#define ADD_KNIGHT(m) ((m) += (1 << 0))
#define ADD_BISHOP(m) ((m) += (1 << 4))
#define ADD_ROOK(m) ((m) += (1 << 8))
#define ADD_QUEEN(m) ((m) += (1 << 12))


typedef struct move_flag_tag {
  int		castling_flags;
  unsigned int 	w_material;             /* see top for macros */
  unsigned int 	b_material;
  int           reverse_cnt;            /* no. of reversible moves */
  position_hash_t hash;                 /* regular hash value */
  position_hash_t phash;                /* pawn hash value */
  plistentry_t *just_deleted_entry;	/* undo info for captures */
  plistentry_t *last_promoted;
  int           extension_count;        /* restrict extensions - not used! */
  square_t	white_king_square;
  square_t	black_king_square;
  square_t	e_p_square;
}	move_flag_t;

#define FLAGS_INIT (-1)

#define MAX_MOVE_FLAGS	300
extern move_flag_t move_flags[];

typedef struct game_history_tag {
  move_t m;
  move_flag_t flags;
  unsigned char reversible;
} game_history_t;

/* init_the_game */

#define PLAYER_MAXNAME 64

struct game_time_tag; 

struct the_game_tag {
  unsigned size; /* entries currently allocated for history */
  unsigned current_move;
  unsigned last_in_book; /* used to skip book lookup */
  struct game_time_tag *timeinfo;
  game_history_t *history;
  char my_name[PLAYER_MAXNAME];
  char opponent_name[PLAYER_MAXNAME];
  int my_elo, opponent_elo;
};

extern struct the_game_tag * the_game; 

#define TESTPOS_ID_LENGTH 32
#define TESTPOS_SOL_LENGTH 64 /* sometimes there are like 10 solution moves... */

/* initialized by reset_gamestat() */

struct gamestat_tag {
  int full_evals;
  int evals;
  int search_nps;
  int quies_nps;
  unsigned long moves_generated_in_search;
  unsigned long moves_looked_at_in_search;
  char testpos_id[TESTPOS_ID_LENGTH];
  char testpos_sol[TESTPOS_SOL_LENGTH];
  char testpos_avoid[TESTPOS_SOL_LENGTH];
  unsigned p_hash_hits;
  unsigned p_hash_misses;
};

extern struct gamestat_tag gamestat;

/**************** TEST RELATED DEFINITIONS ***********************/

struct test_stats_tag {
  unsigned long ply_total;
  unsigned long nodes_total;
  int pos;
} test_stat;

extern struct test_stats_tag test_stat;

#define CMD_TEST_NONE 0
#define CMD_TEST_SOLVE 1
#define CMD_TEST_SEE 2
#define CMD_TEST_MOVEGEN 3
#define CMD_TEST_SEARCH 4 /* basic full width alpha-beta (?) search */ 
#define CMD_TEST_MAKE 5 /* generate, do, undo */
#define CMD_TEST_EVAL 6 /* perform static analysis only */
#define CMD_TEST_BENCH 7 /* coarse set of built-in test runs */

#define CMD_TEST_DEFAULT CMD_TEST_SOLVE

/******************* OPTIONS ****************************/

/* options field of gameoptions struct below */
#define O_TRANSREF_BIT 1
#define O_FULLEVAL_BIT 2 /* if set, no lazy evals are used */
#define O_KILLER_BIT 4
#define O_XBOARD_BIT 8 /* are we running under xboard? */
#define O_FORCE_BIT 16 /* if set wait for moves pushed in */
#define O_POST_BIT 32 /* shall we post the PV during search? */
#define O_PONDER_BIT 64 /* permanent brain if set */
#define O_NULL_BIT 128 /* if 0, never use null move */
#define O_BOOK_BIT 256 /* if 0, do not use book*/
#define O_FRITZ_BIT 512 /* chessbase interpretation of wb protocol */

/* useful macros for runtime option testing */
#define TRANSREF_ON (gameopt.options & O_TRANSREF_BIT)
#define FULLEVAL_ON (gameopt.options & O_FULLEVAL_BIT)
#define KILLERS_ON (gameopt.options & O_KILLER_BIT)
#define XBOARD_ON (gameopt.options & O_XBOARD_BIT)
#define FORCE_ON (gameopt.options & O_FORCE_BIT)
#define POST_ON (gameopt.options & O_POST_BIT)
#define PONDER_ON (gameopt.options & O_PONDER_BIT)
#define NULL_ON (gameopt.options & O_NULL_BIT)
#define BOOK_ON (gameopt.options & O_BOOK_BIT)
#define FRITZ_ON (gameopt.options & O_FRITZ_BIT)

#define PRINT_EVAL_ON (gameopt.test == CMD_TEST_EVAL)

#define TOGGLE_OPTION(o_bit) if(gameopt.options & (o_bit)) 	\
gameopt.options &= ~(o_bit);					\
else gameopt.options |= (o_bit) 
 
#define SET_OPTION(o_bit) gameopt.options |= (o_bit)
#define RESET_OPTION(o_bit) gameopt.options &= ~(o_bit)

/* see reset_gameoptions for init! */
struct gameoptions_tag {
  int maxdepth;
  int options; /* see O_*_BIT flags above */
  int transref_size;
  int test;
  char testfile[1024]; /* linux PATH_MAX hardcoded... */
  /* see top for possible values of test */
};

extern struct gameoptions_tag gameopt;


/* book related types */
typedef struct book_position_tag {
  position_hash_t position;
  position_hash_t status;
} book_position_t;

extern int abort_search;

/* special globals */
extern move_t user_move, ponder_move;
extern int saved_command;

enum global_search_state_tag { IDLING, SEARCHING, PONDERING, ANALYZING };

extern enum global_search_state_tag global_search_state;

#define IS_PONDERING (global_search_state == PONDERING)
#define IS_SEARCHING (global_search_state == SEARCHING)
#define IS_ANALYZING (global_search_state == ANALYZING)
#define IS_IDLE (global_search_state == IDLING)

enum game_phase_tag { BOOK, OPENING, MIDDLEGAME, ENDGAME, PAWNLESS};
extern enum game_phase_tag game_phase;


#endif /* chess.h */
