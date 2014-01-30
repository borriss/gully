/* $Id: transref.c,v 1.15 2011-03-05 20:45:08 martin Exp $ */

#include <assert.h>
#include <stdio.h> /* printf */
#include <stdlib.h> /* calloc */
#include <string.h> /* memset */

#include "logger.h"
#include "transref.h"
#include "hash.h" /* CMP64 */
#include "movegen.h" /* GET_FROM for debug only */
#include "chessio.h" /* debug */

tt_entry_t * ttable;
ph_entry_t * ptable;

static unsigned int tt_sizemask = 0;
static unsigned int ph_sizemask = 0;

#define TT_MAKE_INDEX(s) ((s)->part_one & tt_sizemask)
#define PH_MAKE_INDEX(s) ((s)->part_one & ph_sizemask)

#define TT_ENTER(tt,ti,sig,from_to,sc,h,f) {	\
  tt[ti].signature.part_one = sig->part_one;	\
  tt[ti].signature.part_two = sig->part_two;	\
  tt[ti].ft = from_to;				\
  tt[ti].score = sc;				\
  tt[ti].hf = h | f; }

int 
init_transref_table(int key_bits)
{
  unsigned int size; 
  
  if ((key_bits > TT_MAX_BITS) || (key_bits < TT_MIN_BITS)) {
    log_msg("transref.c: init_transref_table - reverting to default size.\n");
    key_bits = DEFAULT_TT_BITS;
  }

  size = 1 << key_bits;
  
  while ((ttable = (tt_entry_t *) malloc(size * sizeof(tt_entry_t))) == NULL) {
    log_msg("transref.c: Shrinking requested ttable size of %d\n", size);
    size = size >> 1;
    if (size < 1 << TT_MIN_BITS ) {
      err_msg("Error shrinking main hash table (%d).\n", size); 
      return -1;
    }
  }

  tt_sizemask = size-1;

  log_msg("transref.c: ttsize %d bytes (0x%08x entries of size %d), "
	  "sizemask %08x\n", size * sizeof(tt_entry_t), size, 
	  sizeof(tt_entry_t), tt_sizemask);

  return 0;

}

int 
tt_store(const position_hash_t * sig, int from_to, int score, int h, int flag)
{
  /* Store position to transposition table.
   * 
   * Parameters: 
   *     sig: 64 bit signature (lower x bits will be taken as index)
   *     from_to: move  (may be empty if fail-low == UPPER_BOUND)
   *     score: value of this position
   *     h: search depth where this score is based on (remaining search in
   *        search function)
   *     flag: 'quality' of this entry
   *     EXACT_VALUE: move produced in position a score > alpha and < beta.
   *     UPPER_BOUND: move produced a score <= alpha (it failed low)
   *     LOWER_BOUND: move produced a score >= beta (failed high).
   *
   * hash collision resolving:
   * a) for different positions discard the old position (otherwise
   * table gets polluted)
   * b) if both positions full keys are equal (same position),
   * we will replace if we searched deeper this time.
   * Modification:
   * replace if searched as deep as last time. This is done
   * to fix the situation if we encounter a position,
   * find a move alpha<score<beta. After storing the position
   * we find a even better move, but cannot overwrite the
   * entry since we have already inserted the position with
   * the 2nd best move.
   * Can this happen? Ideally the position should only be inserted
   * once with the best move (before returning).
   *
   * Note that in case of a fail low (UPPER_BOUND) there is no stored
   * move.
   */

  int ti = TT_MAKE_INDEX(sig); /* index in transref table */

  if (!TRANSREF_ON) return TT_NO_TABLE;

  /* this is an important detail :( */
  if (abort_search) return TT_NO_TABLE;

  assert(tt_sizemask);

  /*  Special treatment of "mate" scores:
   *  search gives us a score as seen from the root of the current
   *  search, e.g. (MATE - current_ply). This causes problems if
   *  we reach this position through a longer or shorter path later.
   *  Instead, we store the distance to mate from this very position
   *  and correct it back when retrieving a position. 
  */
  if(flag == EXACT_VALUE) {
    if(score > (-MATE - MATING_THRESHOLD) 
       || (score < (MATE + MATING_THRESHOLD))) {
      score = (score > 0) ? score + current_ply : score - current_ply;
    }
  }
  
  /* either other position or more valuable (deeper) score */
  TT_ENTER(ttable, ti, sig, from_to, score, h, flag);
  return TT_ST_REPLACED;
}

int 
tt_retrieve(const position_hash_t * sig, int *from_to, int *score, int *h,
	    int *flag)
{
  int ti = TT_MAKE_INDEX(sig);

  if (!TRANSREF_ON)  {
    *h = -1;
    *flag = TT_EMPTY;
    return TT_NO_TABLE;
  }

  assert(tt_sizemask);

  if (CMP64(ttable[ti].signature, *sig)) {
    /* found */
    
    *h = GET_TT_HEIGHT(ttable[ti]);
    *flag = GET_TT_FLAG(ttable[ti]);
    *score = ttable[ti].score;
    *from_to = ttable[ti].ft;
    
    /* correct mate scores, see comment on top */
    if(*flag == EXACT_VALUE) {
      if(*score > (-MATE - MATING_THRESHOLD)) {
	*score -= current_ply;
      }
      else if (*score < (MATE + MATING_THRESHOLD))
	*score += current_ply;
    }
    
    return TT_RT_FOUND;
  }

  /* not found */
  *h = -1;
  *flag = TT_EMPTY;
  *from_to = 0;

  return TT_RT_NOT_FOUND;
}

int
tt_clear(void)
{
  int size = (tt_sizemask+1) * sizeof(tt_entry_t);

  if (TRANSREF_ON) {
    assert(size >= TT_MIN_BITS * sizeof(tt_entry_t));
    memset(ttable, 0, size);
  }
  return 1;
}

/**************** PAWN HASHING STUFF *************************/

int 
init_pawn_table(int key_bits)
{
  unsigned int size; 
  assert(key_bits <= PH_MAX_BITS);

  if (key_bits == 0) 
    size = 1 << DEFAULT_PH_BITS;
  else
    size = 1 << key_bits;
  
  while ((ptable = (ph_entry_t *) malloc(size * sizeof(ph_entry_t))) == NULL) {
    log_msg("Shrinking requested ptable size of %d",size);
    size = size >> 1;
    if (size < 1 << PH_MIN_BITS ) {
      err_msg("Error shrinking ptable (%d).\n", size);
      return -1;
    }
  }

  ph_sizemask = size - 1;

  log_msg("Pawn table: %d bytes (0x%08x entries of size %d), sizemask %08x\n",
	  size * sizeof(ph_entry_t), size, sizeof(ph_entry_t), ph_sizemask);

  return 0;
}

int 
ph_store(const position_hash_t * sig, int score, int wp)
{
  /* always replaces old entries. */
  int p_i = PH_MAKE_INDEX(sig); 

  assert(ph_sizemask);

  ptable[p_i].signature.part_one = sig->part_one;
  ptable[p_i].signature.part_two = sig->part_two;
  ptable[p_i].score = score;
  ptable[p_i].weak_passed = wp;
  
  return 1;
}

int 
ph_retrieve(const position_hash_t * sig, int *score, int *wp)
{
  int p_i = PH_MAKE_INDEX(sig);

  assert(ph_sizemask);

  if (CMP64(ptable[p_i].signature, *sig)) {
    /* found */
    *score = ptable[p_i].score;
    *wp = ptable[p_i].weak_passed;
    return PH_RT_FOUND;
  }

  /* not found */
  *score = 0;
  *wp = 0;

  return PH_RT_NOT_FOUND;
}

int
ph_clear(void)
{
  int size = (ph_sizemask+1) * sizeof(ph_entry_t);

  if (!ph_sizemask)
    return 0;

  assert(size >= PH_MIN_BITS * sizeof(ph_entry_t));
  memset(ptable, 0, size);
  
  /* No pawns on the board has signature 0:0 and would hit entry 0
   * with the wrong score. It actually happened in a game where Gully
   * had 2Q vs 1Q evaluated the position with -11 (pawns -20.48) ;)
   * So mix it up.
   */
  ptable[0].signature.part_two = -1;
  return 1;
}
