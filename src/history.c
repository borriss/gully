/* $Id: history.c,v 1.5 1997-06-09 20:07:05 martin Exp $

   Routines for killer and history heuristics

   Gully uses two killers for each ply. A use count is kept to decide
   which is to replace.
   Also, the killer table is initialized with the PV from the last ply
   which seems smart to me (?) XXX:Crafty keeps the PV in the transposition
   table which looks even better.
   */

#include <string.h>
#include <assert.h>

#include "chess.h"
#include "history.h"
#include "chessio.h"

struct killer_struct_tag Killer[MAX_KILLER_PLY][2];

void 
reset_killers(void)
{
  if(KILLERS_ON)
    memset( (void*) &Killer, 0 , sizeof(Killer));
}


void 
update_killers(int from_to)
{  
  assert(current_ply < MAX_KILLER_PLY);

  /* update killers - look if move is already in there*/
  if(from_to == Killer[current_ply][0].from_to)
    {
      Killer[current_ply][0].use_count++;
      return;
    }

  if(from_to == Killer[current_ply][1].from_to)
    {
      Killer[current_ply][1].use_count++;
      return;
    }

  /* replace less frequently used killer */
  if(Killer[current_ply][0].use_count < Killer[current_ply][0].use_count)
    {
      Killer[current_ply][0].from_to = from_to;
      Killer[current_ply][0].use_count = 1;
      return;
    }

  Killer[current_ply][1].from_to = from_to;
  Killer[current_ply][1].use_count = 1;

}

/* 
   when searching the next candidate move, the killer usecount for
   killers of the NEXT HIGHER ply should be reset.
   Otherwise one of the killers would amass abig usecount and probably 
   stay at the ply for the whole iteration.
   */

void 
reset_killer_use_count(int ply)
{

  assert(ply < MAX_KILLER_PLY);

  Killer[ply][0].use_count = MIN (Killer[ply][0].use_count,RESETTED_USE_COUNT);
  Killer[ply][1].use_count = MIN (Killer[ply][1].use_count,RESETTED_USE_COUNT);
}

/*
  Init killer array with old PV. This is done before every new iteration.
  Ensures that the PV will be looked at first at each level.
  PV moves are always copied to killer one. They are subject to 
  expiration like any other killer, but will definitely be used once.
  */

void 
pv_2_killer()
{
  int i=0;

  while(principal_variation[0][i].from_to)
    {
      Killer[i][0].from_to = principal_variation[0][i].from_to;
      Killer[i][0].use_count = RESETTED_USE_COUNT;
      i++;

      assert( i < MAX_SEARCH_DEPTH && i < MAX_KILLER_PLY);
    }
}



