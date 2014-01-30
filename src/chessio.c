/* $Id: chessio.c,v 1.32 2006-10-01 12:55:24 martin Exp $ */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "chess.h"
#include "chessio.h"
#include "movegen.h"
#include "tables.h" /* get_piece_material */
#include "transref.h" /* UPPER_BOUND etc. */

void
fprint_board(FILE *where)
{
  int i=7;
  char buf[3]={"  "};

  if(XBOARD_ON) return;

  if(where == NULL)
    where = stdout;

  fprintf(where," +----+----+----+----+----+----+----+----+\n");

  while(i >= 0)
    {
      int j = 0;
      while(j < 8)
	{
	  int cur_offset = i * 16 + j;

	  fprintf(where," | ");
	  if( BOARD[cur_offset] == BOARD_NO_ENTRY )
	    fprintf(where,"  ");
	  else
	    {
	      if(PLIST_OFFSET(BOARD[cur_offset]) < BPIECE_START_INDEX) 
		buf[0]=' ';
	      else 
		buf[0] = '*';

	      buf[1] = piece_name(GET_PIECE(*BOARD[cur_offset]));
	      fprintf(where,"%s",buf);
	    }
	  ++j;
	}
      fprintf(where," |\n +----+----+----+----+----+----+----+----+\n");
      --i;
    }

  fflush(where);
}

char *
sprint_hash_flag(int flag)
{
  switch(flag)
    {
    case UPPER_BOUND: return ("UPPER BOUND");
    case LOWER_BOUND: return ("LOWER BOUND");
    case EXACT_VALUE: return ("EXACT VALUE");
    case TT_EMPTY: return("EMPTY");
    default: return("????");
    }
}

void
fprint_plist(FILE *where)
{
  int i=0;
  while(i<PLIST_MAXENTRIES)
    fprint_plist_entry(where,i++);

  fflush(where);
}

void
fprint_plist_entry(FILE *where,int offset)
{
  char sqbuf[3];

  fprintf(where,"%d piece: %c square: %s\n",offset,
	  piece_name(GET_PIECE(PList[offset])),
	  square_name(GET_SQUARE(PList[offset]),sqbuf));
}

char
piece_name(piece_t p)
{
  switch(p)
    {
    case NO_PIECE:	return ' ';
    case KING	:	return 'k';
    case QUEEN	:	return 'q';
    case ROOK	:	return 'r';
    case BISHOP	:	return 'b';
    case KNIGHT	:	return 'n';
    case PAWN	:	return 'p';
    default:	break;
    }
  return '?';
}

char *
square_name(square_t sq,char * buf)
{
  assert(!(sq & 0x88) && (sq < 120));

  buf[0]= sq%16 + 'a';
  buf[1]= (sq >> 4) + '1';
  buf[2]= '\0';

  return buf;
}

void
fprint_move_array(FILE * where,int start,int end)
{
  if(!start && !end)
    while(fprint_move(where,&move_array[start++]));
  else
    while((start <= end) && (fprint_move(where,&move_array[start++])));

  fprintf(where,"\n");
}

int
fprint_move(FILE * where, move_t *m)
{
  assert(m != NULL);

  if(!m->from_to) 
    return 0;
  else
    {
      char buf[3],buf1[3];
      fprintf(where,"%s%s%c ",
	      square_name(GET_FROM(m->from_to),buf),
	      square_name(GET_TO(m->from_to),buf1),
	      (GET_PRO(m->cap_pro)) ? piece_name(GET_PRO(m->cap_pro)) : 0x20);
    }

  if(m->special == HASHMOVE) fprintf(where,"<HT>");

  return 1;
}

int
sprint_move(char * strbuf, move_t *m)
{
  assert(strbuf);

  if(!m->from_to) 
    return 0;
  else {
    char buf[3],buf1[3];
    sprintf(strbuf, "%s%s%c ",
	    square_name(GET_FROM(m->from_to),buf),
	    square_name(GET_TO(m->from_to),buf1),
	    (GET_PRO(m->cap_pro)) ? piece_name(GET_PRO(m->cap_pro)) : 0x20);
    }

  if(m->special == HASHMOVE) strcat(strbuf, "<HT>");

  return 1;
}


/* xboard wrapper */
int 
fprint_computer_move(FILE *where, move_t *m)
{
  fprintf(where,"move ");
  fprint_move(where, m);
  fprintf(where,"\n");
  fflush(where);
  return 1;
}

void
fprint_move_flags(FILE * where,move_flag_t *mf)
{
  assert(mf);

  fprintf(where,"[mat: wpi %u wpa %lu bpi %u bpa %lu] "
	  "c:%02x k(%x,%x) ep:%x ext: %x rev:%d ",
	  get_piece_material(mf->w_material),
	  GET_PAWN_MATERIAL(mf->w_material),
	  get_piece_material(mf->b_material),
	  GET_PAWN_MATERIAL(mf->b_material),
	  mf->castling_flags,
	  mf->white_king_square,mf->black_king_square,
	  mf->e_p_square, mf->extension_count,
	  mf->reverse_cnt);

  if((int)mf->just_deleted_entry != -1)
    fprintf(where,"jde: %p ", mf->just_deleted_entry);

  if((int)mf->last_promoted != -1)
    fprintf(where,"pro: %p ", mf->last_promoted);

  fprintf(where,"]");
}

void
fprint_current_line(FILE * where)
{
#ifndef NDEBUG

  int i;

  for(i=0; current_line[i] != NULL; i++)
    {
      assert(i < MAX_SEARCH_DEPTH);
      if(!fprint_move(where,current_line[i]))
	break;
    }
  fprintf(where,"\n");

#endif
}

void
fprint_pv(FILE * where)
{
  /* assumes standard terminal width (80 chars) */
  int i;

  for(i=0;i < MAX_SEARCH_DEPTH;i++)
    {
      if(!fprint_move(where, &principal_variation[0][i]))
	break;
      if((i+1) % 8 == 0)
	fprintf(where,"\n                               ");
    }
  fprintf(where,"\n");
}

/* output depth, time, score, pv like gnu does:
 * 3.   -304    0      595   e4d4  h2g3  a4a5  g3b8 
 * after depth: . means ply complete; & means still in this search;
 * + means fail high, - fail low   
 */

void
fpost(FILE * where,int full_depth,int what, int score,float time)
{
  char w;

  if(!POST_ON /* || (IS_PONDERING && time < 5) */ ) return;

  switch(what)
    {
    case PLY_COMPLETE: w = '.'; break;
    case UPDATE: w = '&'; break;
    case INTERRUPTED: w = 'i'; break;
    case FAIL_HIGH: w = '+'; break;
    case FAIL_LOW: w = '-'; break;
    default:
      w ='?'; break;
    }

  if(XBOARD_ON)
    { 
      fprintf(where," %2d%c %6d %4d %8d T%d ",full_depth,w,score,
	      (int)time,gamestat.evals,full_depth);
      fprint_pv(where);
      fflush(where);
      return;
    }


  if(score > (-MATE - MATING_THRESHOLD) || score < MATE + MATING_THRESHOLD)
      fprintf(where,"%2d%c %cMAT%03d  %-7.2f %-9d ",full_depth,w,
	      (score > 0) ? ' ' : '-', 
	      (score > 0) ? (-(score + MATE) +1)/2 : (score - MATE)/2,
	      time,gamestat.evals);
  else
    fprintf(where, "%2d%c %7.2f  %-7.2f %-9d ", full_depth, w,
	    score / 100.0, time, gamestat.evals);

  fprint_pv(where);
}

int
fprint_game(FILE * where, struct the_game_tag * g)
{
  unsigned int i;

  if(g == NULL) return 0;

  for(i = 0; i < g->current_move; i++)
    {
      fprintf(where, "%d.", (i+3)/2);
      fprint_move(where, &g->history[i].m);
      fprint_move_flags(where, &g->history[i].flags);
      fprintf(where," rev: %d\n", g->history[i].reversible);
    }

  return 1;
}

int 
fprint_int_array(FILE *where, int *a, int maxelems, int columns,
		 int fieldwidth, int skip_what)
{
  int i;

  assert(a != NULL);
  assert(maxelems > 0 && columns > 0 && fieldwidth > 0);

  fprintf(where,"\n\n       ");
  for(i = 0; i < columns; i++)
    fprintf(where,"%*d", fieldwidth, i);
  fprintf(where,"\n       ");
  for(i = 0; i < columns*fieldwidth; i++)
    fprintf(where,"-");

  for(i = 0; i < maxelems; i++)
    {
      if(i % columns == 0)
	fprintf(where,"\n%*d | ", fieldwidth,i/columns);

      if(skip_what && a[i] != skip_what)
	fprintf(where,"%*d", fieldwidth,a[i]);
      else
	fprintf(where,"%*c", fieldwidth,'-');

    }
  fprintf(where,"\n");
  return 1;
}
