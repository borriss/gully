/* $Id: repeat.c,v 1.9 1997-03-24 21:54:44 martin Exp $ */

#include <assert.h>
#include <stdio.h>

#include "chess.h"
#include "hash.h" /* CMP64 */
#include "logger.h"
#include "repeat.h"
#include "chessio.h"

#define REPETITION_PLIES 100
#define REP_LIST_MAX_SIZE (REPETITION_PLIES/2 + MAX_SEARCH_DEPTH/2)

static position_hash_t repetition_list_w[REP_LIST_MAX_SIZE];
static position_hash_t repetition_list_b[REP_LIST_MAX_SIZE];

position_hash_t * repetition_head_w =  repetition_list_w;
position_hash_t * repetition_head_b =  repetition_list_b;


void 
reset_rep_heads(void)
{
  repetition_head_w =  repetition_list_w;
  repetition_head_b =  repetition_list_b;
}

void 
rep_show_offset(void)
{
#ifdef SHOW_REP

  int i,j;
  position_hash_t * p;

  i = repetition_head_w -  repetition_list_w;
  j = repetition_head_b -  repetition_list_b;

  assert((i-j) == 0 || (i-j) == 1 || (i-j) == -1);

  printf("Rep heads: white [%02d]: ",i);
  p = repetition_list_w;
  while(i>0)
    {
      printf("[%08x:%08x] ",p->part_one,p->part_two);
      p++;
      i--;
    }

  printf("\nRep heads: black [%02d]: ",j);
  p = repetition_list_b;
  while(j>0)
    {
      printf("[%08x:%08x] ",p->part_one,p->part_two);
      p++;
      j--;
    }
  printf("\n");
#endif
}

int
repetition_check(const int ply, const position_hash_t *hash_value)
{
  /*
   * repetition_list_x has all positions since last irreversible move
   * repetition_head_x points to all reversible positions seen in
   *    search so far.
   *
   * general scheme inspired by Bob Hyatt's Crafty.
   * Also "entries" is one less than it really is, because there is no
   *  need to check the position one move ago!
   */
  int entries = move_flags[ply].reverse_cnt/2 - 1;
  position_hash_t * p;

  /* don't insert at ply 0 its already done by root_execute */
  if(!ply) return 0;

  /* 50 moves rule */
  if(move_flags[ply].reverse_cnt > 99)
      return 2;

  assert(entries < REP_LIST_MAX_SIZE);
  
  if(turn == WHITE)
    {
      p = repetition_head_w + (ply-1)/2;
      *p-- = *hash_value;

      assert(repetition_head_w >= repetition_list_w);
      assert(repetition_head_w < repetition_list_w + REP_LIST_MAX_SIZE);

      while(entries-- > 0)
	{
	  p--;
	  assert(p >= repetition_list_w);

	  /* XXX try the 2 fold game repetition */
	  if( CMP64(*hash_value,*p))
	    return 1;
	}
    }
  else /* black */
    {
      p = repetition_head_b + (ply-1)/2;
      *p-- = *hash_value;

      assert(repetition_head_b >= repetition_list_b);
      assert(repetition_head_b < repetition_list_b + REP_LIST_MAX_SIZE);

      while(entries-- > 0)
	{
	  p--;
	  assert(p >= repetition_list_b);
	  
	  if( CMP64(*hash_value,*p))
	    return 1;
	}
    }
  return 0;    
}

/* After we made a move move at the root, check for threefold repetition
   and the 50 move rule.
   This will end the game.
   */

int
draw_by_repetition(const position_hash_t *hash_value)
{
  int rep_cnt = 0;
  position_hash_t * p;
 
  if(move_flags[0].reverse_cnt > 99)
    {
      printf("50 move rule\n");
      return 50;
    }

  if (turn == WHITE) 
    {
      for (p = repetition_list_w; p < repetition_head_w; p++)
	if(CMP64(*hash_value,*p)) 
	  rep_cnt++;
    }
  else 
    {
      for (p = repetition_list_b; p < repetition_head_b; p++)
	if(CMP64(*hash_value,*p)) 
	  rep_cnt++;
    }

  return(rep_cnt == 3);
}
