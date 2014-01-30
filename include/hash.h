/* $Id: hash.h,v 1.9 2003-06-01 16:44:34 martin Exp $ */

#ifndef __HASH_H
#define __HASH_H

#define POS_EXISTS 1
#define ENTRY_OCCUPIED 2
#define POSITION_STORED 0

/* some 64bit macros */

#define SET64(s,v1,v2)  { (s).part_one =(v1) ; (s).part_two = (v2); }

/* and 64bit s with 32bit high and low */
#define AND64(s,h,l) { (s).part_one &= (h); (s).part_two &= (l); }

/* or 64bit s with 32bit high and low */
#define OR64(s,h,l) { (s).part_one |= (h); (s).part_two |= (l); }

/* or 64bit numbers*/
#define OR6464(s,v) { (s).part_one |= (v).part_one; \
  (s).part_two |= (v).part_two; }


/* xor v1 to first - 32 bit - part, v2 (16bit) to second */
#define XOR64(s,v)  { (s).part_one ^= (v).part_one;\
  (s).part_two ^= (v).part_two; }

/* compare 64 bit number return TRUE if equal else FALSE */
#define CMP64(p1,p2)    (((p1).part_one == (p2).part_one ) 	\
			 && ((p1).part_two == (p2).part_two))

/* compare 64 bit number return TRUE if equal else FALSE */
#define CMP6432(p1,h,l)    (((p1).part_one == h ) 	\
			 && ((p1).part_two == l ))


/* XXX 64 bit addition (ignores overflow to high word)*/
#define ADD64(p1,a) { (p1).part_two += (a); }     

/* 64 bit Greater Than */
#define GTH64(p1,p2)    (((p1).part_one > (p2).part_one ) 	\
			 || (((p1).part_one == (p2).part_one) \
			 && ((p1).part_two > (p2).part_two))) 

     
unsigned int random32(void);
void init_hash(void);
int generate_hash_value(position_hash_t *);

/* 
   'normal' incremental update (called from make_move)
   */
void update_hash(position_hash_t * h, const move_t * m);

/* ep_square has changed */
void update_hash_epsq(position_hash_t * h, const square_t epsq);

/* ep_move was made */
void update_hash_ep_move(position_hash_t * h, const move_t * m);

/* castling was made */
void update_hash_castling(position_hash_t * h, const move_t * m);

/* castling flags have changed */
void update_hash_cflags(position_hash_t * h, const int old_flags, 
			const int new_flags);

/* something promoted */
void update_hash_prom(position_hash_t * h, const move_t * m);

/* null move */
void hash_change_turn(position_hash_t *);

/* init pawn hashing */
int generate_pawn_hash_value(position_hash_t * h);

/* incrementally update pawn hash key */
void update_pawn_hash(position_hash_t * h, const move_t * m);

void
remove_pawn_hash(position_hash_t * h, const move_t * m);
#endif /* __HASH_H */
