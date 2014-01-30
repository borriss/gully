/* $Id: test.c,v 1.30 2003-09-14 20:11:13 martin Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "chess.h"
#include "test.h"
#include "mstimer.h"
#include "init.h"
#include "attacks.h"
#include "movegen.h"
#include "execute.h"
#include "chessio.h"
#include "logger.h"
#include "helpers.h"
#include "evaluate.h"
#include "iterate.h"
#include "hash.h"
#include "transref.h" /* temporarily - tt_entry_t */

#define SOL_ARRAY_SIZE 3000 /* only testsuites < 3000 positions will work
			       correctly */

/* forward decl */
static int update_test_stats(struct pos_solve_stat_tag *pss, int *all_times,
			     int * nps, int *completed,int correct);

static int bench(void);
static double solve_from_file(void);
static unsigned long test_movegen(unsigned long iterate);
static unsigned long test_make_undo(unsigned long iterate);
static int test_see(void);
static int search_fixed(int depth,int index);
static void delay(void);


static int counter = 0;

/*
 * facade for several test activies in gully.
 * if the main loop finds that an option sets gameopt.test,
 * we go here. do_test() decides what specific test to run. 
 *
 * In contrast to normal game playing, do_test() does not
 * honor computer_color. It will always let the computer
 * move.
 */
int
do_test()
{
  if(gameopt.test == CMD_TEST_BENCH) return bench();
  else {
    if (gameopt.testfile[0] == '\0')
      err_quit("\nYou need to specify a test file or use \"bench\"."
	       " Exiting.\n");
    printf("Total test duration: %.2fs\n",solve_from_file());
  }

  return 0;
}

static unsigned long
test_movegen(unsigned long iterate)
{
  int i=0;
  unsigned long j,time;
  double timediff;

  time=get_time();
  for(j = iterate; j > 0; j--) i = generate_moves(turn, 0);

  timediff = time_diff(get_time(), time);

  printf("%lu gen_moves (%lu moves) in %10.3fs \n[%.2f Million moves/s]\n",
	 iterate, iterate * (long) i,
	 timediff, (((double) (iterate) / timediff) * (i / 1000000.0)));

  return iterate*i;

}

static unsigned long
test_make_undo(unsigned long iterate)
{
  int i=0,k;
  unsigned long j,time;
  double timediff;

  time=get_time();
  for(j = iterate; j > 0; j--) {
    i = generate_moves(turn, 0);
    for(k = 0; k < i; k++) {
      make_move(&move_array[k],0);
      undo_move(&move_array[k],0);
    }
  }
  
  timediff = time_diff(get_time(), time);

  printf("iterate: %lu: total %lu moves generated, done and undone\n"
	 "in %10.3fs [%.1f million moves/s]\n",
	 iterate,iterate*(long)i,
	 timediff,(((double) iterate  / timediff) * (i / 1000000.0)));

  return iterate*i;

}

static int
test_see(void)
{
  int i=0,k,j=0;
  
  fprint_board (stdout);
  i = generate_captures(turn,0);
  
  for(k=0;k<i;k++) {
    if(move_array[k].cap_pro) {
      int see_score;
      fprint_move(stdout, &move_array[k]);
      see_score = see(turn, &move_array[k]);
      fprintf(stdout, " see score: %d\n", see_score);
      if(see_score >= 0)
	++j;
    }
  }

  printf("Total %d moves generated\n", i);

  return j;

}

static int 
search_fixed(int depth,int index)
{
  int k,new_index,one_legal=0;

  if(depth == 0) {
    ++gamestat.full_evals;
    return 0;
  }
  
  new_index=generate_moves(turn,index);
  counter+=(new_index-index);

  for(k = index; k < new_index; k++) {
    if(make_move(&move_array[k], current_ply)) {
      one_legal++;
      
      turn= (turn == WHITE) ? BLACK : WHITE;
      current_ply++;
      search_fixed(depth-1, new_index);
      current_ply--;
      turn= (turn == WHITE) ? BLACK : WHITE;
    }
    
    undo_move(&move_array[k],current_ply);
  }

  clear_move_list(index, new_index);
  return counter;

}

/* 
 * built-in test positions for bench command 
 * (same subset of bratko-kopec suite as used by crafty)
 */
static char * epdset[] =
{
  "3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - - bm d5; id bk.02;",
  "rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq - bm e6; id BK.04",
  "2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - - bm f5; id BK.03",
  "r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - - bm Bf5; id BK.12",
  "2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - - bm Bxe4; id bk.22;",
  "r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq - bm f6; id BK.23",
  NULL
};

static int
bench()
{
  int i = 0;
  struct pos_solve_stat_tag pss;
  int score = 0;
  int depth = MAX_SEARCH_DEPTH;
  unsigned this_time = 0l, total_time = 0;
  const int bench_time = 300;

  fprintf(stdout, "Gully2: Running speed benchmark. This will take"
	  " about %d minutes.\n", (6 * bench_time)/600);

  game_time.time_per_move = bench_time;
  
  /* run tests */
  reset_test_stats();

  while(epdset[i] != NULL)  {
    char buf[128];
    
    /* must not use constant string buffer fors
       parsing by strtok and friends */
    strcpy(buf, epdset[i]);

    setup_board(buf);

    timestamp();
       
    score = iterate(depth, SEARCHING, &pss);
 
    global_search_state = IDLING;
    this_time = (unsigned) time_diff(get_time(), game_time.timestamp);

    total_time += this_time;
    test_stat.nodes_total += (gamestat.quies_nps 
			      + gamestat.search_nps) / 1000;
    ++i;
  }
  
  fprintf(stdout, "nodes: %luK time: %u s [%luK nps]\n",
	  test_stat.nodes_total, total_time, 
	  test_stat.nodes_total / total_time);
  
  return 0;
}


static double 
solve_from_file(void)
{
  FILE *fd;
  unsigned long time,total = 0l;
  double total_time;
  char * a; /* array keeping info about which pos was solved */
  int * all_times; /* array keeping info on when pos was solved */
  int * all_nps_ply; /* how many avg nodes needed for each ply */
  int * ply_completed; /* counter how many testpos completed this ply */

  struct pos_solve_stat_tag pss;

  unsigned int i, j, correct_counter = 0;
  
  if(gameopt.testfile[0] == '\0') return 0;

  a = (char *) calloc(SOL_ARRAY_SIZE, sizeof(char));
  all_times = (int *) calloc(SOL_ARRAY_SIZE, sizeof(int));
  all_nps_ply = (int *) calloc (MAX_SOL_DEPTH, sizeof(int));
  ply_completed = (int*) calloc (MAX_SOL_DEPTH, sizeof(int));

  if(a == NULL || all_times == NULL || all_nps_ply == NULL 
     || ply_completed == NULL)
    err_sys("calloc");

  /* output current configuration */
  printf("Thinking time per move: %d seconds.\n", 
	 game_time.time_per_move/10); 
  if(TRANSREF_ON) {
    int tts = 1 << gameopt.transref_size;
    printf("Main hash table size: %d MBytes (%d entries).\n",
	   ((tts / 1024) * sizeof(tt_entry_t)) / 1024, tts);
  } else printf("Main hash table OFF.\n");
  printf("Null moves %s.\n", NULL_ON ? "ON" : "OFF"); 

  time = get_time();

  if((fd = fopen(gameopt.testfile,"r")) == NULL)
    err_sys("fopen %s failed",gameopt.testfile);
  
  reset_test_stats();

  while(get_next_position_from_file(fd)) {
    int score,depth = gameopt.maxdepth;
    
    switch(gameopt.test) {
    case CMD_TEST_MOVEGEN:
      total += test_movegen(200000);
      break;
    case CMD_TEST_MAKE:
      total += test_make_undo(50000);
      break;
    case CMD_TEST_SEE:
      total += test_see();
      break;
    case CMD_TEST_EVAL:
      fprint_board(stdout);
      phase();
      printf("total score: %.2f\n\n",
	     evaluate(-INFINITY,INFINITY) / 100.0);
      break;
    case CMD_TEST_SEARCH:
      fprintf(stdout,"fixed full tree search to depth %d\n", depth);
      search_fixed(depth,0);
      fprintf(stdout,"%d leaf nodes visited.\n",gamestat.full_evals);
      total += gamestat.full_evals;
      break;
    default:
      if(total >= SOL_ARRAY_SIZE) { 
	log_msg("WARNING: Maximum test suite size (%d) exceeded.\n",
		 SOL_ARRAY_SIZE);
      }
      gameopt.test = CMD_TEST_SOLVE;
      memset(&pss,0,sizeof(pss));
      timestamp();
      fprint_board(stdout);
	      
      printf("pos: %s \tSolution: %s Avoid: %s\n", gamestat.testpos_id,
	     gamestat.testpos_sol, gamestat.testpos_avoid);

      score = iterate(depth, SEARCHING, &pss);

      global_search_state = IDLING;
      total_time = time_diff(get_time(), game_time.timestamp);

      printf("evals:%d (full:%d lazy:%.2f%%), time: %.2f [%.2f Knps]\n"
	     "Nodes in search: %.1f million, quies: %.1f million (%.2f%%)\n"
	     "moves in search - generated: %lu million, "
	     "looked at: %lu million (%.2f%%)\n",
	     gamestat.evals, gamestat.full_evals,
	     100 - (100.0 * (gamestat.full_evals / (float) gamestat.evals)),
	     total_time,
	     (gamestat.quies_nps + gamestat.search_nps) / 
	     (total_time * 1000),
	     gamestat.search_nps / 1000000.0, gamestat.quies_nps / 1000000.0,
	     100 * (gamestat.quies_nps 
		    / ((float) (gamestat.quies_nps + gamestat.search_nps))), 
	     gamestat.moves_generated_in_search / 1000000,
	     gamestat.moves_looked_at_in_search / 1000000,
	     100.0 * (gamestat.moves_looked_at_in_search
		      / (float) gamestat.moves_generated_in_search));
      printf("pawn hash: hits: %u misses: %u (%.3f%%)\n",
	     gamestat.p_hash_hits,gamestat.p_hash_misses,
	     ((gamestat.p_hash_hits + gamestat.p_hash_misses) ?
	      (100.0 * gamestat.p_hash_hits 
	       / (gamestat.p_hash_hits 
		  + gamestat.p_hash_misses)) : 0.0));

      test_stat.nodes_total += (gamestat.quies_nps 
				+ gamestat.search_nps) / 1000;

      /* 
	 First, match against the list of solution moves. If Gully
	 came up with one those moves, OK.
	 Second, try the list of "avoid" moves. If this list exists
	 and we're not in there; we win again.
      */

      if(solution_correct(gamestat.testpos_sol) 
	 || (!strncmp(gamestat.testpos_sol, "none", 4) 
	     && strncmp(gamestat.testpos_avoid, "none", 4) 
	     && !solution_correct(gamestat.testpos_avoid))) {
	printf("Correct (%ld)\n", total + 1);
	correct_counter++;
	if(total < SOL_ARRAY_SIZE)
	  update_test_stats(&pss, &all_times[total], all_nps_ply,
			     ply_completed, 1);
      }
      else {
	/* mark those positions */
	if(total < SOL_ARRAY_SIZE) {
	  a[total] = 1;
	  update_test_stats(&pss, &all_times[total], all_nps_ply,
			    ply_completed, 0);
	}
	printf("Wrong (%ld)\n", total + 1);
      }
      total++;
    }
  }
  fclose(fd);
  
  test_stat.pos = total;
  
  if(gameopt.test == CMD_TEST_MOVEGEN ||
     gameopt.test == CMD_TEST_MAKE ||
     gameopt.test == CMD_TEST_SEARCH)
    printf("total positions: %lu\n",total);
    
  if(gameopt.test == CMD_TEST_SEE)
    printf("%lu total moves were non-losing captures\n",total); 
    
  if(gameopt.test == CMD_TEST_SOLVE)
    printf("\nTotal stats:Correct %d of %ld \n",
	   correct_counter,total); 
    
  total_time=time_diff(get_time(),time);

  /* some statistics over test output */
  if(gameopt.test == CMD_TEST_SOLVE) {
    if(total_time > 0.01 && test_stat.pos)
      printf("%luK total nodes, plies: %lu (avg: %.2f ), "
	     "time: %.2fs, [ %.2f Knps]\n",
	     test_stat.nodes_total, test_stat.ply_total,
	     (float)test_stat.ply_total / (float)test_stat.pos,total_time,
	     test_stat.nodes_total / total_time);

    /* list of positions not solved */
    total = MIN(SOL_ARRAY_SIZE,total);
    j = 0;
    printf("Failure list:\n");
    for(i = 0; i < total; i++)
      if(a[i]) {
	++j;
	printf("%d%c",i+1,((j % 8) == 0) ? '\n' : '\t');
      }
	
    fprint_int_array(stdout,all_times,total,10,4,-1);
	
    for(i = 0; i < MAX_SOL_DEPTH; i++) {
      if(ply_completed[i] == 0 && all_nps_ply[i] == 0)
	break;
      printf("ply %2d: avg. nodes:%8d (%4d)\n",i+1,
	     all_nps_ply[i],ply_completed[i]);
    }
  }
    
  free(a);
  free(all_times);
  free(all_nps_ply);
  free(ply_completed);

  return total_time;
}

/*
  keep some more detailed info when solving testsuites
  */

int
update_pss(struct pos_solve_stat_tag * pss, int ply, move_t *m,
	   float time)
{
  if (!(pss != NULL))
    err_quit("pss == NULL");
  if (!(ply >= 0 && ply < MAX_SOL_DEPTH))
    err_quit("ply: %d",ply);

  pss->sol_move[ply] = m->from_to;
  pss->sol_time[ply] = (int) (time+0.5);
  pss->sol_nodes[ply] = gamestat.evals;

  return 1;
}

static int
update_test_stats(struct pos_solve_stat_tag *pss,int *all_times,
		  int *all_nps,int *completed,int solved_correct)
{

  int ft = principal_variation[0][0].from_to;
  int i = MAX_SOL_DEPTH - 1;
  int j;

  /* get to last completed ply */
  while(i && (pss->sol_move[i] == 0)) --i;

  for(j = i; j >= 0; j--) {
    if(pss->sol_nodes[j] == 0)
      printf("strange, ply completed w/o nodes\n");

    all_nps[j] = (all_nps[j] * completed[j] + pss->sol_nodes[j]) /
      ++completed[j];
      
  }
  
  if(solved_correct) {
    /* solved, but ply was not completed */
    if(pss->sol_move[i] != ft) {
      *all_times = (int) game_time.time_allocated/100;
      return 2;
    }
  
    while(i && (pss->sol_move[i] == ft)) --i;
    *all_times = pss->sol_time[i+1];
  }
  else
    *all_times = -1;

  return 1;
}

/* bogomips measurements, as in l4atm */

#if 0
/* Linux bogomips measures */
static __inline__ void 
delay(int loops)
{
  __asm__(".align 2,0x90\n1:\tdecl %0\n\tjns 1b": :"a" (loops):"ax");
}
#endif

/* portable loop */
static volatile unsigned bogo_cnt = 0;

static void 
delay(void)
{
  for (;bogo_cnt;bogo_cnt--);
}


#define BOGO_DURATION 1000

unsigned long
bogomips(void)
{
  unsigned long loops_per_sec = 1;
  unsigned long t,s,td;
  
  fprintf(stdout, "Calculating bogomips number..");
  fflush(stdout);

  while ((loops_per_sec <<= 1)) {
    t = get_time();
    bogo_cnt = loops_per_sec;
    delay(); 
    s = get_time();
    td = 1000 * (unsigned long) time_diff(t,s);
    
    if (td >= BOGO_DURATION) { 
      loops_per_sec /= td;
      fprintf(stdout, "ok - %lu.%02lu BogoMips\n",
	      loops_per_sec/100,
	      (loops_per_sec % 100));
      return loops_per_sec;
    }
  }
  fprintf(stdout, "failed\n");
  return 0;
}
