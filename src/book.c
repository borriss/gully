/* $Id: book.c,v 1.10 2011-03-05 20:45:07 martin Exp $ */

/*
 * Access and create opening book.
 * Most of its code and comments is directly (c) Bob Hyatts solution 
 * for Crafty.
 * Book code is just routine work, after all ;)
 *
 * There is a "favorite player" option, which lets us emulate 
 * the opening repertoire of a single player.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <errno.h> 

#include "chess.h"
#include "chessio.h" /* fprint_board */
#include "logger.h"
#include "init.h" /* setup_board */
#include "input.h" /* parse_abbreviated */
#include "execute.h" /* execute moves */
#include "hash.h"  /* 64 bit macros  */
#include "mstimer.h" /* get_time() */
#include "book.h"
#include "helpers.h"
#include "movegen.h" /* generate_moves() */

#define FAVORITE_PLAYER "Borriss"
#define DEFAULT_REJECT_MASK 3
#define DEFAULT_ACCEPT_MASK ~0343
#define FAVORITE_ACCEPT_MASK 0x801A
#define BOOK_GRACE_PERIOD 10 /* up to 5 moves after we are out of book
			       we still try the lookup */

#define BOOK_CLUSTER_SIZE   600
#define MERGE_BLOCKSIZE    1000
#define SMALL_SORT_BLOCKSIZE    50000
#define LARGE_SORT_BLOCKSIZE   200000


FILE *book_file = NULL;

int  book_accept_mask = DEFAULT_ACCEPT_MASK;
int  book_reject_mask = DEFAULT_REJECT_MASK;
int  book_random = 5;
int  book_lower_bound = 1;
int  book_absolute_lower_bound = 1;
float  book_min_percent_played = 0.005f;
int  show_book = 1;

unsigned long start_time;
struct bookstat_tag bookstat;

/*
 * book() is used to determine if the current position is in the book 
 * database.  it simply takes the set of moves produced by root_moves() 
 * and then tries each position's hash key to see if it can be found in 
 * the database. if so, such a move represents a "book move."  the set of
 * flags is used to decide on a sub-set of moves to be used as the 
 * "book move pool" from which a move is chosen randomly. 
 *
 * flag bits: 
 *      0000 0001  ?? flagged move                (0001)
 *      0000 0010   ? flagged move                (0002)
 *      0000 0100   = flagged move                (0004)
 *      0000 1000   ! flagged move                (0010)
 *      0001 0000  !! flagged move                (0020)
 *      0010 0000  white won at least one game    (0040)
 *      0100 0000  black won at least one game    (0100)
 *      1000 0000  at least one game was a draw   (0200)
 *
 *      Remainder of the bits are flag bits set by user (the next 16 bits
 *      only).
 */
int 
book(void)
{
  static book_position_t buffer[BOOK_CLUSTER_SIZE];
  static move_t book_moves[200];
  static move_t selected[200];
  static int selected_order[200], selected_status[200], book_development[200];
  static int book_order[200], book_status[200], evaluations[200];
  int m1_status,status;
  int done, i, j, last_move, temp, which;
  int cluster, test;
  position_hash_t temp_hash_key, common;
  int key, nmoves, num_selected, st;
  int percent_played, total_played, total_moves, distribution;
  int new_index,k;


  static char ch[16] = {'0','1','2','3','4','5','6','7',
                        '8','9','A','B','C','D','E','F'};

  if(!BOOK_ON) return 0;
  assert(IS_IDLE || IS_ANALYZING);

  /* clear the big buffer */
  bookstat.book_buf[0] = '\0';

  /*
    ----------------------------------------------------------
    |                                                          |
    |   if we have been out of book for several moves, return  |
    |   and start the normal tree search.                      |
    |                                                          |
    ----------------------------------------------------------
    */
  
  if(the_game->current_move > (the_game->last_in_book + BOOK_GRACE_PERIOD)) 
    return 0;
  
  /* generate the root move list */
  new_index = generate_moves(turn,0);

  /*
    ----------------------------------------------------------
    |                                                          |
    |   position is known, read in the appropriate cluster.    |
    |   note that this cluster will have all possible book     |
    |   moves from current position in it (as well as others   |
    |   of course.)                                            |
    |                                                          |
    ----------------------------------------------------------
    */

  test = move_flags[0].hash.part_one >> 17;

  if (book_file) 
    {
      /* look up the correct cluster based on high 15 bit of the
	 hash key */
      fseek(book_file,test*sizeof(int),SEEK_SET);
      fread(&key,sizeof(int),1,book_file);
      if (key > 0) 
	{
	  fseek(book_file,key,SEEK_SET);
	  fread(&cluster,sizeof(int),1,book_file);
	  fread(buffer,sizeof(book_position_t),cluster,book_file);

	  /*
	    ----------------------------------------------------------
	    |                                                          |
	    |   first cycle through the root move list, make each      |
	    |   move, and see if the resulting hash key is in the book |
	    |   database.                                              |
	    |                                                          |
	    ----------------------------------------------------------
	    */

	  total_moves=0;
	  nmoves=0;

	  for (k = 0; k < new_index; k++)
	    {
	      common = move_flags[0].hash;
	      AND64(common,0xffff0000,0); 
	      
	      /* cycle thru move list */
	      if(make_move(&move_array[k],0))
		{
		  temp_hash_key = move_flags[1].hash;
		  AND64(temp_hash_key,0x0000ffff,0xffffffff);
		  OR6464(temp_hash_key,common);
		  
		  for (i=0;i<cluster;i++) 
		    {
		      /* is this move in the book? */
		      if (CMP64(temp_hash_key,buffer[i].position)) 
			{
			  /* yes it is, but does the status allow it?
			     It must be not rejected by mask, 
			     and (provided that it is a game with result,
			     e.g. not a variation) must be in the current
			     accept mask and yielded not only losses to
			     the side who played this before ....
			   */
			  status = buffer[i].status.part_one;

#if 0
			  if(!(status & book_reject_mask) &&
			     (!(status & ~0340) || 
			      (status & book_accept_mask)) &&
			     (((turn == WHITE) && ((status & 040) || 
						   !(status & 0300))) ||
			      ((turn == BLACK) && ((status & 0100) || 
						   !(status & 0240)))))   
#else
			    /* gully is not as intelligent, rather
			       follow moves from a lost game than
			       think on your own.
			       So, just reject ? and ?? moves.
			       */

			    if(!(status & book_reject_mask)) 

#endif
			    {
			      book_status[nmoves] = status;
			      book_order[nmoves] = buffer[i].status.part_two;
			      book_development[nmoves]=0;
			      total_moves +=  book_order[nmoves];
			      /* 
			       * random 2 (static eval) not supported 
			       * yet 
			       */
			      evaluations[nmoves] = 0;
			      
			      book_moves[nmoves++]= move_array[k];
			    }
			  break;
			}
		      /* move not found in book */
		    }
		} /* legal move */ 
	      
	      undo_move(&move_array[k],0);
	    }

	  clear_move_list(0,new_index);

	  if (nmoves) 
	    {
	      do 
		{
		  done=1;
		  for (i=0;i<nmoves-1;i++)
		    if (((book_random != 2) && (book_order[i] < 
						book_order[i+1])) ||
			((book_random == 2) && evaluations[i] < 
			 evaluations[i+1])) 
		      {
			move_t temp_mv;

			temp=evaluations[i];
			evaluations[i]=evaluations[i+1];
			evaluations[i+1]=temp;
			temp=book_order[i];
			book_order[i]=book_order[i+1];
			book_order[i+1]=temp;
			temp=book_development[i];
			book_development[i]=book_development[i+1];
			book_development[i+1]=temp;
			temp_mv = book_moves[i];
			book_moves[i]=book_moves[i+1];
			book_moves[i+1] = temp_mv;
			temp=book_status[i];
			book_status[i]=book_status[i+1];
			book_status[i+1]=temp;
			done=0;
		      }
		} while (!done);

	      /* for analysis, store list of book moves and
		 return.
	      */
	      if(IS_ANALYZING) {
		char move_buf[64];

		the_game->last_in_book = the_game->current_move;
		
		sprintf(bookstat.book_buf, "\t%d games, %d different moves.\n",
			total_moves, nmoves);

		for (i = 0; i < nmoves; i++) {
		  sprintf(move_buf, "\t"); /* XB required space or tab */
		  strcat(bookstat.book_buf, move_buf);
		  sprint_move(move_buf, &book_moves[i]);
		  strcat(bookstat.book_buf, move_buf);
		  sprintf(move_buf, " %3d%%\n",
			  100 * book_order[i]/total_moves);
		  strcat(bookstat.book_buf, move_buf);
		}
		return 1;
	      }


	      if (show_book)
		for (i=0;i<nmoves;i++) 
		  {
		    printf("%3d%% ",100*book_order[i]/total_moves);
		    printf("%d ",evaluations[i]);
		    fprint_move(stdout,&book_moves[i]);
		    st=book_status[i] & book_accept_mask & (~224);
		    if (st) 
		      {
			if (st & 1) printf("??");
			else if (st & 2) printf("? ");
			else if (st & 4) printf("= ");
			else if (st & 8) printf("! ");
			else if (st & 16) printf("!!");
			else 
			  {
			    printf("  ");
			    st=st>>8;
			    printf("/");
			    for (j=0;j<16;j++) 
			      {
				if (st & 1) printf("%c",ch[j]);
				st=st>>1;
			      }
			  }
		      }
		    else printf("  ");
		    printf(" {played %d times}", book_order[i]);
		    if (book_order[i] < book_lower_bound) 
		      printf(" (<%d)",book_lower_bound);
		    if (book_development[i] < 0) printf(" (anti-thematic)");
		    printf("\n");
		  }

	      /*
	       * now select a move from the set of moves just found. 
	       * do this in four distinct passes:  
	       * (1) look for !! moves; 
	       * (2) look for ! moves; 
	       * (3) look for any other moves. 
	       * note: book_accept_mask *should* have a bit set for any 
	       * move that is selected, including !! and ! type moves so 
	       * that they *can* be excluded if desired.  note also that 
	       * book_reject_mask should have ?? and ? set (at a minimum)
	       * to exclude these types of moves.    
	       */

	      /* first, check for !! moves */
	      num_selected=0;
	      if (!num_selected)
		if (book_accept_mask & 16)
		  for (i=0;i<nmoves;i++)
		    if (book_status[i] & 16) 
		      {
			selected_status[num_selected]=book_status[i];
			selected_order[num_selected]=book_order[i]*100;
			selected[num_selected++]=book_moves[i];
		      }
	      /* if none, then check for ! moves */
	      if (!num_selected)
		if (book_accept_mask & 8)
		  for (i=0;i<nmoves;i++)
		    if (book_status[i] & 8) 
		      {
			selected_status[num_selected]=book_status[i];
			selected_order[num_selected]=book_order[i]*100;
			selected[num_selected++]=book_moves[i];
		      }
	      /* if none, then check for = moves */
	      if (!num_selected)
		if (book_accept_mask & 4)
		  for (i=0;i<nmoves;i++)
		    if (book_status[i] & 4)
		      if (book_order[i] >= book_lower_bound) 
			{
			  selected_status[num_selected]=book_status[i];
			  selected_order[num_selected]=book_order[i];
			  selected[num_selected++]=book_moves[i];
			}
	      /* if none, then check for any flagged moves */
	      if (!num_selected)
		for (i=0;i<nmoves;i++)
		  if (book_status[i] & book_accept_mask)
		    if (book_order[i] >= book_lower_bound) 
		      {
			selected_status[num_selected]=book_status[i];
			selected_order[num_selected]=book_order[i];
			selected[num_selected++]=book_moves[i];
		      }
	      /* if none, then any book move is acceptable */
	      if (!num_selected)
		for (i=0;i<nmoves;i++) 
		  {
		    if (book_development[i] >= 0) 
		      {
			selected_status[num_selected]=book_status[i];
			selected_order[num_selected]=book_order[i];
			selected[num_selected++]=book_moves[i];
		      }
		  }
	      if (!num_selected) return(0);
	      /*
		now copy moves to right place and sort 'em.
		*/
	      for (i=0;i<num_selected;i++) 
		{
		  book_status[i]=selected_status[i];
		  book_moves[i]=selected[i];
		  book_order[i]=selected_order[i];
		  book_order[i]=selected_order[i];
		}
	      nmoves=num_selected;
	      /*
	       * now determine what type of randomness is wanted, and 
	       * adjust the book_order counts to reflect this.  
	       * the options are:  
	       * (0) do a short search for each move in the list and play
	       * the best move found;  
	       * (1) play the most frequently played move; 
	       * (2) play the move that produces the best static 
	       * evaluation;
	       * (3) take the most frequently played moves, 
	       * which is usually only 2 or 3 out of the list;  
	       * (4) use the actual frequency the moves were played as a 
	       * model for choosing moves; 
	       * (5) use sqrt() of the frequency played, which adds a 
	       * larger random factor into selecting moves;  
	       * (6) choose from the known book moves completely randomly,
	       * without regard to frequency.
	       */
	      printf("              book moves {");
	      for (i = 0; i < nmoves; i++) 
		fprint_move(stdout, &book_moves[i]);
	      printf("}\n");

	      /* short search selection strategy
	       * XXX not supported in Gully now,
	       * will break, no move selected!! 
	       */
	      if (book_random == 0) 
		{
		  err_msg("book strategy not supported");
		  if (the_game->current_move < 20)
		    for (i=1;i<nmoves;i++)
		      if (book_order[0] > 4*book_order[i]) 
			{
			  nmoves=i;
			  break;
			}
		  printf("              moves considered {");
		  for (i = 0; i < nmoves; i++) 
		    {
		      fprint_move(stdout, &book_moves[i]);
		      if (i < nmoves-1) printf(", ");
		    }
		  printf("}\n");
#if 0
		  if (nmoves > 1) 
		    {
		      for (i = 0; i < nmoves; i++) 
			*(last[0] + i) = book_moves[i];
		      last[1] = last[0] + nmoves;
		      last_pv.path_iteration_depth = 0;
		      booking = 1;
		      (void) Iterate(wtm, booking);
		      booking = 0;
		    }
		  else 
		    {
		      pv[1].path[1] = book_moves[0];
		      pv[1].path_length = 1;
		    }
#endif
		  the_game->last_in_book = the_game->current_move;
		  return(1);
		}
  
	      for (last_move = 0; last_move < nmoves; last_move++)
		if ((book_order[last_move] 
		     < book_min_percent_played * total_moves) 
		    && !(book_status[last_move] & 060)) break;
	      if (last_move == 0) return (0);
	      if (book_order[0] >= 10 * book_lower_bound) 
		{
		  for (last_move = 1; last_move < nmoves; last_move++)
		    if ((book_order[last_move] < book_lower_bound) 
			&& !(book_status[last_move] & 060)) break;
		}
	      else if (book_order[0] >= book_absolute_lower_bound) 
		{
		  for (last_move = 1; last_move < nmoves; last_move++)
		    if ((book_order[last_move] 
			 < book_absolute_lower_bound) 
			&& !(book_status[last_move]&060)) break;
		}
	      else return (0);

	      which = rand();

	      switch (book_random) 
		{
		case 1:
		  last_move = 1;
		  break;
		case 2:
		  last_move = 1;
		  break;
		case 3:
		  for (i = 1; i < last_move; i++)
		    if (book_order[0] > 4 * book_order[i]) 
		      book_order[i] = 0;
		  break;
		case 4:
		  break;
		case 5:
		  for (i = 0; i < last_move; i++) 
		    book_order[i] = 1 + (int) sqrt(book_order[i]);
		  break;
		case 6:
		  for (i = 0; i < last_move; i++) 
		    book_order[i] = 100;
		  break;
		}
	      /*
	       * now randomly choose from the "doctored" random 
	       * distribution.
	       */
	      total_moves = 0;
	      for (i = 0; i < last_move; i++) 
		total_moves += book_order[i];
	      distribution = abs(which) % total_moves;
	      for (which = 0; which < last_move; which++) 
		{
		  distribution -= book_order[which];
		  if (distribution < 0) break;
		}
	      which = MIN(which, last_move);
	      clear_pv(1);
	      update_pv(&book_moves[which]);
	      percent_played = 100 * book_order[which] / total_moves;
	      total_played = book_order[which];
	      m1_status = book_status[which];
	      printf("               book   0.0s    %3d%%   ", 
		     percent_played);
	      fprint_move(stdout, &book_moves[which]);
	      st = m1_status & book_accept_mask & (~224);
	      if (st) 
		{
		  if (st & 1) printf("??");
		  else if (st & 2) printf("?");
		  else if (st & 4) printf("=");
		  else if (st & 8) printf("!");
		  else if (st & 16) printf("!!");
		  else 
		    {
		      st=st >> 8;
		      printf("/");
		      for (j = 0; j < 16; j++) 
			{
			  if (st & 1) printf("%c", ch[j]);
			  st = st >> 1;
			}
		    }
		}
	      printf("\n");
	      the_game->last_in_book = the_game->current_move;
	      return(1);
	    } /* if (nmoves)  - found any book moves */
	}
    }
  else
    printf("no book.bin file\n");

  return(0);
}



/*
 * BookUp() is used to create/add to the opening book file.  
 * typing "bookcreate" will erase the old book file and start from 
 * scratch, typing "book add" will simply add more moves to the 
 * existing file. 
 * the format of the input data is a left bracket ("[") followed by any 
 * title information desired, followed by a right bracket ("]") followed 
 * by a sequence of moves.  the sequence of moves is assumed to start at 
 * ply=1, with white-to-move (normal opening position) and can contain as
 * many moves as desired (no limit on the depth of each variation.)  
 * the file *must* be terminated with a line that begins with "end", 
 * since handling the EOF condition makes portable code difficult.
 *
 * book moves can either be typed in by hand, directly into book_add(), 
 * by using the "book create/add" command.  using the command 
 * "book add/create filename" will cause book_add() to read its opening 
 * text moves from filename rather than from the key. 
 *
 * in addition to the normal text for a move (reduced or full algebraic 
 * is accepted, ie, e4, ed, exd4, e3d4, etc. are all acceptable) some 
 * special characters can be appended to a move.  the move must be 
 * immediately followed by either a "/" or a "\" followed by one or more 
 * of the following mask characters with no intervening spaces.  if "/" 
 * preceeds the flags, the flags are added (or'ed) to any already existing
 * flags that might be from other book variations that pass through this 
 * position.  if "\" is used, these flags replace any existing flags, 
 * which is the easy way to clear incorrect flags and/or replace them 
 * with new ones. 
 *      ?? ->  never play this move.  since the same book is used for 
 * both black and white, you can enter moves in that white might play, 
 * but prevent the program from choosing them on its own.
 *      ?  ->  avoid this move except for non-important games.  these 
 * openings are historically those that the program doesn't play very 
 * well, but which aren't outright losing.
 *      =  ->  drawish move, only play this move if drawish moves are 
 * allowed by the operator.  this is used to encourage the program to 
 * play drawish openings (Petrov's comes to mind) when the program needs 
 * to draw or is facing a formidable opponent (deep thought comes to 
 * mind.)  
 *      !  ->  always play this move, if there isn't a move with the 
 * !! flag set also.  this is a strong move, but not as strong as a !! 
 * move.
 *      !! ->  always play this move.  this can be used to make the 
 * program favor particular lines, or to mark a strong move for certain 
 * opening traps.
 *     0-f ->  16 user-defined flags.  the program will ignore these 
 * flags unless the operator sets the "book mask" to contain them which 
 * will "require"" the program to play from the set of moves with at least
 * one of the flags in "book mask" set.
 */

/* 
 * this is currently not as robust as crafties way - I intend to exit
 * after building the book. 
 */

void 
book_up(char *output_filename)
{
  book_position_t *buffer;
  move_t move;
  position_hash_t temp_hash_key, common;
  FILE *book_input, *output_file;
  char flags[40], fname[64], text[30], nextc, which_mask[20], *start;
  int white_won=0, black_won=0, drawn=0, fplayer = 0, i, 
    mask_word, total_moves;
  int move_num, book_positions, rarely_played;
  int cluster, max_cluster, discarded, errors, data_read;
  int following, ply, max_ply = 40;
  int files, buffered=0;
  book_position_t current, next;
  unsigned int last, next_cluster;
  int cluster_seek;
  int counter, *index;
  int sort_blocksize,max_book_depth;

  /*
    ----------------------------------------------------------
    |                                                          |
    |   determine if we should read the book moves from a file |
    |   or from the operator (which is normally used to add/   |
    |   delete moves just before a game.)                      |
    |                                                          |
    ----------------------------------------------------------
    */
  fscanf(stdin,"%s",text);
  book_input = stdin;
  if (!strcmp(text, "add") || !strcmp(text, "create")) 
    {
      nextc = getc(stdin);
      if (nextc == ' ') 
	{
	  data_read = fscanf(stdin, "%s", fname);
	  if (!(book_input = fopen(fname, "r"))) 
	    {
	      printf("file %s does not exist.\n", fname);
	      return;
	    }
	  nextc = getc(stdin);
	  if (nextc == ' ') 
	    {
	      data_read = fscanf(stdin, "%d", &max_ply);
	    }
	}
    }
  /*
    ----------------------------------------------------------
    |                                                          |
    |   open the correct book file for writing/reading         |
    |                                                          |
    ----------------------------------------------------------
    */
  if (!strcmp(text, "create")) 
    {
      if (book_file)
	if (fclose(book_file) == EOF) 
	  err_quit("book_up: could not close book file.\n");
      if((book_file = fopen(output_filename, "wb+")) == NULL)
	err_quit("book_up: could not open (wb+) %s.\n", output_filename);
      
      buffer = (book_position_t *) malloc(sizeof(book_position_t) * 100);
      if (!buffer) 
	{
	  printf("not enough memory for buffer.\n");
	  return;
	}
      fseek(book_file, 0, SEEK_SET);
    }
  else if (!strcmp(text, "off")) 
    {
      if (book_file) fclose(book_file);
      book_file = 0;
      fprintf(stdout, "book file disabled.\n");
      return;
    }
  else if (!strcmp(text, "on")) 
    {
      if (!book_file) 
	{
	  sprintf(fname, "%s/book.bin" ,".");
	  book_file = fopen(fname, "rb+");
	  printf("book file enabled.\n");
	}
      return;
    }
  else if (!strcmp(text, "played")) 
    {
      fscanf(stdin, "%f", &book_min_percent_played);
      book_min_percent_played /= 100.0;
      printf("moves must be played at least %6.2f to be used.\n",
	     book_min_percent_played * 100);
      return;
    }
  else if (!strcmp(text, "random")) 
    {
      data_read = fscanf(stdin, "%d", &book_random);
      switch (book_random) 
	{
	case 0:
	  printf("play best book line after search.\n");
	  break;
	case 1: 
	  printf("play the most popular book line.\n");
	  break;
	case 2:
	  printf("play the book move that produces the best static "
		 "value.\n");
	  break;
	case 3: 
	  printf("play best book lines.\n");
	  break;
	case 4:
	  printf("play from the most popular book lines.\n");
	  break;
	case 5:
	  printf("play from the most popular book lines, but vary "
		 "more.\n");
	  break;
	case 6:
	  printf("play random book lines.\n");
	  break;
	case 7:
	  printf("play only lines which have the famous player flag.\n");
	  break;
	default:
	  printf("valid options are 0-7.\n");
	  break;
	}
      return;
    }
  else if (!strcmp(text, "mask")) 
    {
      data_read=fscanf(stdin, "%s", which_mask);
      data_read=fscanf(stdin, "%s", flags);
      if (!strcmp(which_mask, "accept")) 
	{
	  book_accept_mask = book_mask(flags);
	  book_reject_mask = book_reject_mask & ~book_accept_mask;
	}
      else if (!strcmp(which_mask, "reject")) 
	{
	  book_reject_mask = book_mask(flags);
	  book_accept_mask = book_accept_mask & ~book_reject_mask;
	}
      else 
	{
	  printf("usage:  book mask accept|reject <chars>\n");
	}
      return;
    }
  else if (!strcmp(text, "lower")) 
    {
      data_read = fscanf(stdin, "%d", &book_lower_bound);
      return;
    }
  else 
    {
      printf("usage:  book edit/create/off [filename]\n");
      return;
    }

  setup_board(NULL);

  /*
    ----------------------------------------------------------
    |                                                          |
    |   now, read in a series of moves (terminated by the "["  |
    |   of the next title or by "end" for end of the file)     |
    |   and make them.  after each MakeMove(), we can grab     |
    |   the hash key, and use it to access the book data file  |
    |   to add this position.  note that we have to check the  |
    |   last character of a move for the special flags and     |
    |   set the correct bit in the status for this position.   |
    |   when we reach the end of a book line, we back up to    |
    |   the starting position and start over.                  |
    |                                                          |
    ----------------------------------------------------------
    */
  start = strstr(output_filename, "books.bin");
  printf("parsing pgn move file (1000 moves/dot)\n");

  start_time = get_time();

  if (book_file) 
    {
      total_moves = 0;
      max_book_depth = 0;
      discarded = 0;
      errors = 0;
      if (book_input == stdin) 
	{
	  printf("enter book text, first line must be "
		 "[title information],\n");
	  printf("type \"end\" to exit.\n");
	  printf("title(1): ");
	}
      do 
	data_read = fscanf(book_input, "%s", text);
      while ((text[0] != '[') && strcmp(text, "end") && (data_read > 0));
      do 
	{
#if 0
	  if (book_input != stdin) 
	    printf("%s ", text);
#endif
	  if (start) 
	    {
	      white_won = 1;
	      black_won = 1;
	      drawn = 0;
	      fplayer = 0;
	    }
	  while ((text[strlen(text) - 1] != ']') && 
		 strcmp(text, "end") && (data_read > 0)) 
	    {
	      if (strstr(text, "esult")) 
		{
		  data_read = fscanf(book_input, "%s", text);
		  white_won = 0;
		  black_won = 0;
		  drawn = 0;
		  if (strstr(text, "1-0")) white_won = 1;
		  else if (strstr(text, "0-1")) black_won = 1;
		  else if (strstr(text, "1/2-1/2")) drawn = 1;
		  if (strchr(text,']')) break;
		}
	      /* favorite player option */
	      else if(!strcmp(text, "[White"))
		{
		  data_read = fscanf(book_input, "%s", text);
		  if(strstr(text, FAVORITE_PLAYER))
		    fplayer = 1;
		  if (strchr(text, ']')) break;
		}
	      else if(!strcmp(text, "[Black"))
		{
		  data_read = fscanf(book_input, "%s", text);
		  if(strstr(text, FAVORITE_PLAYER))
		    fplayer = 2;
		  if (strchr(text,']')) break;
		} 

	      data_read = fscanf(book_input, "%s", text);
#if 0
	      if (book_input != stdin) printf("%s ", text);
#endif
	    }
#if 0
	  if (book_input != stdin) printf("\n");
#endif
	  if (!strcmp(text, "end") || (data_read <= 0)) break;

	  assert(turn == WHITE);
	  move_num = 1;
	  ply = 0;
	  following = 1;
	  while (data_read > 0) 
	    {
	      if (book_input == stdin) 
		{
		  if (turn == WHITE)
		    printf("WhitePieces(%d): ", move_num);
		  else
		    printf("BlackPieces(%d): ", move_num);
		}
	      do
		data_read = fscanf(book_input, "%s", text);
	      while ((text[0] >= '0') && (text[00] <= '9') 
		     && (data_read > 0));
	      mask_word = 0;
	      if (!strcmp(text, "end") || (data_read <= 0)) 
		{
		  if (book_input != stdin) fclose(book_input);
		  break;
		}
	      else if (strchr(text,'/')) 
		{
		  strcpy(flags, strchr(text,'/') + 1);
		  *strchr(text, '/') = '\0';
		  mask_word = book_mask(flags);
		}
	      else 
		{
		  for (i = 0; i < (int) strlen(text); i++)
		    if (strchr("?!", text[i])) 
		      {
			strcpy(flags, &text[i]);
			text[i] = '\0';
			mask_word = book_mask(flags);
			break;
		      }
		}
	      mask_word |= (white_won << 5) + (black_won << 6) 
		+ (drawn << 7);

	      /* fplayer option */
	      if((fplayer == 1 && turn == WHITE) ||
		 (fplayer == 2 && turn == BLACK))
		mask_word |= book_mask("f");

	      if (text[0] == '[') break;
	      if (!strchr(text, '$') && !strchr(text, '*')) 
		{
		  /* 
		     read_move() parses any legal representation
		     and checks its legality against the current position.
		   */
		  if(read_move(text, &move) != 0)
		    {
		      ply++;
		      max_book_depth = MAX(max_book_depth, ply);
		      total_moves++;
		      if (!(total_moves % 1000)) 
			{
			  printf(".");
			  if (!(total_moves % 60000)) 
			    printf(" (%d)\n", total_moves);
			  fflush(stdout);
			}

		      /* get the high 16 bit of the parents key */
		      assert(current_ply == 0);
		      common = move_flags[current_ply].hash;
		      AND64(common, 0xffff0000, 0);
		      
		      /* make the move */
		      if(!make_move(&move, 0))
			err_quit("book_up - make_move");
		      move_flags[0] = move_flags[1];
		      turn = (turn == WHITE) ? BLACK : WHITE;

		      if ((ply <= max_ply) || 
			  (following && move.cap_pro)) 
			{
			  temp_hash_key = move_flags[0].hash;
			  AND64(temp_hash_key, 0x0000ffff, 0xffffffff);
			  OR6464(temp_hash_key, common);

			  buffer[buffered].position = temp_hash_key;
			  SET64(buffer[buffered].status, mask_word, 0);
			  buffered++;

			  if (buffered > 99) 
			    {
			      fwrite(buffer, sizeof(book_position_t), 100,
				     book_file);
			      buffered = 0;
			    }
			}
		      else 
			{
			  following = 0;
			  discarded++;
			}
		    }
		  else 
		    {
		      errors++;
		      printf("ERROR!  move %d: %s is illegal\n",
			     move_num, text);
		      fprint_board(stdout);
		      break;
		    }
		  if (turn == WHITE) move_num++;
		} 
	    }
	  setup_board(NULL);
	} while (strcmp(text, "end") && (data_read > 0));
      if (buffered) 
	{
	  fwrite(buffer, sizeof(book_position_t), buffered, book_file);
	}
      /*
	----------------------------------------------------------
	|                                                          |
	|   done, now we have to sort this mess into ascending     |
	|   order, move by move, to get moves that belong in the   |
	|   same record adjacent to each other so we can "collapse |
	|   and count" easily.                                     |
	|                                                          |
	----------------------------------------------------------
	*/
#if defined(AUTO_BLOCKSIZE)
      if (total_moves - discarded > 2950000) 
	{
	  sort_blocksize = LARGE_SORT_BLOCKSIZE;
	} else 
	  {
	    sort_blocksize = SMALL_SORT_BLOCKSIZE;
	  }
#elif defined(SMALL_BLOCKSIZE)
      sort_blocksize = SMALL_SORT_BLOCKSIZE;
#else
      sort_blocksize = LARGE_SORT_BLOCKSIZE;
#endif
      printf("\nsorting %d moves (%dK/dot) ", total_moves, 
	     sort_blocksize / 1000);
      fflush(stdout);
      free(buffer);
      buffer=(book_position_t *) malloc(sizeof(book_position_t) 
					* sort_blocksize);
      if (!buffer) 
	{
	  printf("out of memory.  aborting. \n");
	  exit(1);
	}
      fclose(book_file);
      book_file = fopen(output_filename, "rb+");
      data_read = sort_blocksize;
      for (files = 1; files < 10000; files++) 
	{
	  if (data_read < sort_blocksize) break;
	  data_read = fread(buffer, sizeof(book_position_t), 
			    sort_blocksize, book_file);
	  if (!data_read) break;
	  qsort((char *) buffer, data_read, sizeof(book_position_t),
		book_up_compare);
	  sprintf(fname, "sort.%d", files);
	  if(!(output_file = fopen(fname, "wb+"))) 
	    printf("ERROR.  unable to open sort output file\n");
	  fwrite(buffer, sizeof(book_position_t), data_read, output_file);
	  fclose(output_file);
	  if (files % 10) printf(".");
	  else printf("(%d)", files);
	  fflush(stdout);
	}
      fclose(book_file);
      free(buffer);
      printf("<done>\n");
      /*
	----------------------------------------------------------
	|                                                          |
	|   now merge these "chunks" into book.bin, keeping all of |
	|   the "flags" as well as counting the number of times    |
	|   that each move was played.                             |
	|                                                          |
	----------------------------------------------------------
	*/
      printf("merging sorted files (%d) (10K/dot)\n", files - 1);
      counter = 0;
      index = (int*) malloc(32768 * sizeof(int));
      if (index == NULL)
	err_sys("malloc");
      for (i = 0; i < 32768; i++) index[i] = -1;
      current = book_up_next_position(files, 1);
      if (start) 
	{
	  AND64(current.status, 0, 0xffffffff);
	  ADD64(current.status, 100);
	}
      else 
	ADD64(current.status, 1);
      book_file = fopen(output_filename, "wb+");
      fseek(book_file, sizeof(int) * 32768, SEEK_SET);
      last = current.position.part_one >> 17;
      index[last] = ftell(book_file);
      book_positions = 0;
      rarely_played = 0;
      cluster = 0;
      cluster_seek = sizeof(int) * 32768;
      fseek(book_file, cluster_seek + sizeof(int), SEEK_SET);
      max_cluster = 0;
      while (1) 
	{
	  next = book_up_next_position(files, 0);
	  counter++;
	  if (counter % 10000 == 0) 
	    {
	      printf(".");
	      if (counter % 600000 == 0) printf("(%d)\n",counter);
	      fflush(stdout);
	    }
	  if (CMP64(current.position, next.position)) 
	    {
	      if (!start) 
		ADD64(current.status, 1);
	      OR6464(current.status, next.status);
	    }
	  else 
	    {
	      book_positions++;
	      cluster++;
	      max_cluster = MAX(max_cluster, cluster);
	      fwrite(&current, sizeof(book_position_t), 1, book_file);
	      if (last != next.position.part_one >> 17)
		{
		  next_cluster = ftell(book_file);
		  fseek(book_file, cluster_seek, SEEK_SET);
		  fwrite(&cluster, sizeof(int), 1, book_file);
		  if ((next.position.part_one  == 0) 
		      && ((next.position.part_two  == 0))) break;
		  fseek(book_file, next_cluster + sizeof(int), SEEK_SET);
		  cluster_seek = next_cluster;
		  last = next.position.part_one >> 17;
		  index[last] = next_cluster;
		  cluster = 0;
		  }
		  if (current.status.part_two < book_lower_bound) 
		    rarely_played++;
		  current = next;
		  if (start)
		    {
		      AND64(current.status, 0, 0xffffffff);
		      ADD64(current.status, 100);
		    }
		  else 
		    ADD64(current.status, 1);
	    }
	  if ((next.position.part_one  == 0) 
	      && ((next.position.part_two  == 0))) break;
	}
      fseek(book_file, 0, SEEK_SET);
      fwrite(index, sizeof(int), 32768, book_file);
      /*
	----------------------------------------------------------
	|                                                          |
	|   now clean up, remove the sort.n files, and print the   |
	|   statistics for building the book.                      |
	|                                                          |
	----------------------------------------------------------
	*/
      for (i = 1; i < files; i++) 
	{
	  sprintf(fname, "sort.%d", i);
	  remove(fname);
	}
      printf("\n\nparsed %d moves.\n", total_moves);
      printf("found %d errors during parsing.\n", errors);
      printf("discarded %d moves (maxply=%d).\n", discarded,max_ply);
      printf("book contains %d unique positions.\n", book_positions);
      printf("deepest book line was %d plies.\n", max_book_depth);
      printf("longest cluster of moves was %d.\n", max_cluster);
      printf("%d positions were played less than %d times.\n",
	     rarely_played, book_lower_bound);
      printf("time used:  %10.3f s\n", time_diff(get_time(),start_time));
    }
}

/*
 * book_mask() is used to convert the flags for a book move into a 32 
 * bit mask that is either kept in the file, or is set by the operator 
 * to select which opening(s) the program is allowed to play.
 */

int 
book_mask(char *flags)
{
  int f, i, mask;
  mask = 0;
  for (i = 0; i < (int) strlen(flags); i++) 
    {
      if (flags[i] == '*')
	mask = -1;
      else if (flags[i] == '?') 
	{
	  if (flags[i+1] == '?') 
	    {
	      mask = mask | 1;
	      i++;
	    }
	  else
	    mask = mask | 2;
	}
      else if (flags[i] == '=') 
	{
	  mask = mask | 4;
	}
      else if (flags[i] == '!') 
	{
	  if (flags[i+1] == '!') 
	    {
	      mask = mask | 16;
	      i++;
	    }
	  else
	    mask = mask | 8;
	}
      else 
	{
	  f = flags[i] - '0';
	  if ((f >= 0) && (f <= 9))
	    mask = mask | (1 << (f+8));
	  else 
	    {
	      f = flags[i] - 'a';
	      if ((f >= 0) && (f <= 5))
		mask = mask | (1 << (f+18));
	      else 
		{
		  f = flags[i] - 'A';
		  if ((f >= 0) && (f <= 5))
		    mask = mask | (1 << (f+18));
		  else
		    printf("unknown book mask character -%c- ignored.\n",
			   flags[i]);
		}
	    }
	}
    }
  return(mask);
}

/*
 * BookUpNextPosition() is the heart of the "merge" operation that is 
 * done after the chunks of the parsed/hashed move file are sorted.  
 * this code opens the sort.n files, and returns the least (lexically) 
 * position key to counted/merged into the main book database.
 */
book_position_t 
book_up_next_position(int files, int init)
{
  char fname[20];
  static FILE *input_file[150];
  static book_position_t *buffer[150];
  static int data_read[150], next[150];
  int i, used;
  book_position_t least;
  if (init) 
    {
      for (i = 1; i < files; i++) 
	{
	  sprintf(fname, "sort.%d", i);
	  if (!(input_file[i] = fopen(fname, "rb")))
	    printf("unable to open sort.%d file,"
		   " may be too many files open.\n", i);
	  buffer[i] = (book_position_t*) malloc(sizeof(book_position_t)
						* MERGE_BLOCKSIZE);
	  if (!buffer[i]) 
	    {
	      printf("out of memory.  aborting. \n");
	      exit(1);
	    }
	  fseek(input_file[i], 0, SEEK_SET);
	  data_read[i] = fread(buffer[i], sizeof(book_position_t),
			     MERGE_BLOCKSIZE, input_file[i]);
	  next[i] = 0;
	}
    }
  SET64(least.position, 0, 0);
  used = -1;
  for (i = 1; i < files; i++)
    if (data_read[i]) 
      {
	least = buffer[i][next[i]];
	used = i;
	break;
      }
  if (CMP6432(least.position, 0, 0)) 
    {
      for (i = 1;i < files; i++) fclose(input_file[i]);
      return (least);
    }
  for (i++; i<files; i++) 
    {
      if (data_read[i]) 
	{
	  if (GTH64(least.position, buffer[i][next[i]].position)) 
	    {
	      least = buffer[i][next[i]];
	      used = i;
	    }
	}
    }
  if (--data_read[used] == 0) 
    {
      data_read[used] = fread(buffer[used], sizeof(book_position_t),
			    MERGE_BLOCKSIZE, input_file[used]);
      next[used] = 0;
    }
  else
    next[used]++;
  return(least);
}

int 
book_up_compare(const void *pos1, const void *pos2)
{
  
  if (GTH64(((book_position_t *)pos2)->position,
	   (((book_position_t *)pos1)->position)))
    return (-1);
  else if (GTH64(((book_position_t *)pos1)->position,
	   (((book_position_t *)pos2)->position)))
    return (1);

  return 0;
}

/* wrapper for opening the book for reading */
int 
book_open(const char * book_name, FILE ** f)
{
  const char * permission = "rb";

  *f = fopen(book_name, permission);
  if (!*f)
    {
      err_msg("Unable to open (%s) book file: %s", permission, book_name);
      return 0;
    }
  else
    {
      fseek(*f, 0L, SEEK_END);
      bookstat.size = ftell(*f);
      fseek(*f, 0L, SEEK_SET);

      if(bookstat.size == -1 && errno > 0)
	{
	  err_ret("ftell");
	  return 0;
	}

      strcpy(bookstat.name, book_name);
    }
  return 1;
}

/* Leading tab and finishing newline required by xboard */
int
book_status()
{
  if (BOOK_ON)
    fprintf(stdout, "\tUsing: %s, size %ld (about %ld positions)\n",
	    bookstat.name,bookstat.size,
	    (bookstat.size - 0x020000) / 16 );
  else { 
    fprintf(stdout, "\tbook is off.\n");
    return 0;
  }
  fprintf(stdout, "%s\n", bookstat.book_buf);
  
  return BOOK_ON;
}
