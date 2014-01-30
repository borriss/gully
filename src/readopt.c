/* $Id: readopt.c,v 1.16 2003-03-02 13:53:06 martin Exp $ */

#include <stdio.h>
#include <stdlib.h> /* atoi */
#include <assert.h>
#include <string.h>
#ifdef UNIX 
#include <getopt.h>
#else
#include "getopt.h" /* win32 only */
#endif

#include "chess.h"
#include "logger.h"
#include "readopt.h"
#include "helpers.h"
#include "book.h"
#include "mstimer.h"

int
read_options(int argc, char ** argv)
{
  int c;

  while (1)
    {
      int option_index = 0;
      static struct option long_options[] =
      {
	{"test", 1, 0, 't'}, 
	{"help", 0, 0, 'h'},
	{"maxdepth", 1, 0, 0},
	{"time", 1, 0, 0},
	{"verbose", 1, 0, 0},
	{"fulleval", 0, 0, 0},
	{"file", 1, 0, 'f'},
	{"transref", 1, 0, 0},
	{"nokiller", 0, 0, 0},
	{"ponder", 0, 0, 'p'}, 
	{"null", 0, 0, 0},
	{"book", 1, 0, 0},
	{"version", 0, 0, 'v'},
	{"xboard", 0, 0, 'x'},
	{"bench", 0, 0, 0},
	{0, 0, 0, 0}
      };

      /* double :: is an optional argument, : required arg */
      c = getopt_long(argc, argv, "t:hf:pvx",
		      long_options, &option_index);
      if (c == -1)
	break;

      switch (c)
	{
	case 0:
	  log_msg("%s: read_options() %d long option %s\n", 
		  __FILE__, option_index, 
		  long_options[option_index].name);
	  if (optarg)
	    log_msg(" with arg %s\n", optarg);
	  /* special treatment for long-only options */
	  switch(option_index)
	    {
	    case 2: /* maxdepth */
	      log_msg("setting maxdepth to %d\n",atoi(optarg));
	      gameopt.maxdepth = atoi(optarg);
	      break;
	    case 3: /* time */
	      game_time.time_per_move = MAX ((int) (atof(optarg) * 10), 1);
	      log_msg("Time per move: %d s.\n", game_time.time_per_move/10);
	      break;	  
	    case 4: /* verbose -- ignored yet */
	      log_msg("verbosity level %s yet ignored.\n", optarg);
	      break;
	    case 5: /* fulleval */
	      log_msg("No lazy evals.\n");
	      SET_OPTION(O_FULLEVAL_BIT);
	      break;
	    case 7: /* transref */
	      gameopt.transref_size = MAX(atoi(optarg),0);
	      if (gameopt.transref_size == 0)
		{
		  log_msg("No transposition table.\n");
		  RESET_OPTION(O_TRANSREF_BIT);
		}
	      else
		log_msg("Readopt.c: Hash table size: 2 exp(%d)\n", 
			gameopt.transref_size);
	      break;
	    case 8: /* nokiller */
	      RESET_OPTION(O_KILLER_BIT);
	      log_msg("No killers.\n");
	      break;
	    case 10: /* no null */
	    RESET_OPTION(O_NULL_BIT);
	    log_msg("Null move disabled.\n");
	    break;
	    case 11: /* book */
	      if (!strcmp(optarg,"on"))
		{
		  SET_OPTION(O_BOOK_BIT);
		  log_msg("book on\n");
		}
	      else if (!strcmp(optarg,"off"))
		{
		  RESET_OPTION(O_BOOK_BIT);
		  log_msg("book off\n");
		}
	      else
		{
		  if (!book_open(optarg,&book_file))
		    {
		      RESET_OPTION(O_BOOK_BIT);
		      break;
		    }
		  SET_OPTION(O_BOOK_BIT);
		  log_msg("Book: using %s.\n",optarg);
		}
	      break;
	    case 14: /* bench */
	      log_msg("bench command\n");
	      gameopt.test = CMD_TEST_BENCH;
	      break;
	    default:
	      err_msg("c == %c ?\n", c);
	      break;
	    }
	  break;

	case 'v':
	  print_version(4);
	  exit(0);
	case 'h':
	  print_usage(argv[0]);
	  exit(0);
	case 't':
	  log_msg("option t with arg `%s'\n", optarg);

	  if (!optarg || (!strcmp(optarg,"solve")))
	    {
	      gameopt.test = CMD_TEST_SOLVE;
	      break;
	    }
	  else if (!strcmp(optarg,"eval"))
	    {
	      gameopt.test = CMD_TEST_EVAL;
	      break;
	    }
	  else if (!strcmp(optarg,"see"))
	    {
	      gameopt.test = CMD_TEST_SEE;
	      break;
	    }
	  else if (!strcmp(optarg,"search"))
	    {
	      gameopt.test = CMD_TEST_SEARCH;
	      break;
	    }
	  else if (!strcmp(optarg,"movegen"))
	    {
	       gameopt.test = CMD_TEST_MOVEGEN;
	       break;
	    }
	  else if (!strcmp(optarg,"make"))
	    {
	       gameopt.test = CMD_TEST_MAKE;
	       break;
	    }
	  else
	    err_msg("Ignoring optional argument for test: %s"
		    " -- running default test.", optarg);
	  
	  gameopt.test =  CMD_TEST_DEFAULT;
	  break;
	case 'p':
	  RESET_OPTION(O_PONDER_BIT);
	  log_msg("No pondering allowed\n");
	  break;
	case 'f':
	  log_msg("option f with value `%s'\n", optarg);
	  strcpy(gameopt.testfile, optarg);
	  if (gameopt.test == CMD_TEST_NONE)
	    gameopt.test = CMD_TEST_DEFAULT;
	  break;
	case 'x': 
	  /* force xboard compatibility mode */
	  /* I found this neccessary in Gully 2.14pl2, since the
	     changed NEW behaviour does not "autodetect" XBoard
	     for older Xboard version (3.5.x) from the NEW cmd. */
	  SET_OPTION(O_XBOARD_BIT);
	  log_msg("XBoard / WinBoard mode forced.\n");
	  break;
	case '?':
	  return -1;
	default:
	  err_msg("?? getopt returned character code 0%o ??", c);
	  return -1;
	}
    }

  if (optind < argc)
    {
      err_msg("non-option ARGV-elements: ");
      while (optind < argc) err_msg("%s ", argv[optind++]);
      return -1;
    }

  return 1;
}

