/* $Id: version.h,v 1.43 2011-03-05 20:45:07 martin Exp $ */
#ifndef __VERSION_H
#define __VERSION_H

#define VERSION "2.16.pl2"

/*
  Version log:
  2.0:
  Essential functions incorporated:
  generate_moves and friends
  generate_captures and friends
  make_move ; undo_move
  attacks
  basic fixed search.

  This version provides a testing frame for move generation and execution
  functions.

  2.1:
  Quiescence search.
  Killers (incl. PV)
  Commandline interface.
  
  2.2:
  Hash table support (functions for generating and updating 64bit
  hash keys)

  2.3:
  Search now cleanly uses transref table. Transref move is used.
  Both TT and Killers can be switched off.
  Some improvements in the user interface.

  2.4:
  Basic repetition check (within search)
  TTable now always replaces old entries.
  moved test from file code from main.c to test.c
  moved iterate function from main into separate file (iterate.c)
  introduces data structures for game_history (to actually play a game)

  2.5:
  Better repetition check (within the game history, but only tests for
  one recurrence.
  Check for threefold repetition after move is made.
  Interaction support, game playing is possible.
  limited XBoard support (post,time,otim, level, new commands)
  simple time allocation

  2.6:
  Cleaned up interface a bit.
  Handles fail low situations better (aborted fail-low re-searches have
  resulted in unpredictable PV)
  Handles end of game OK (except if computer stalemates the opponent,
  it then just sits and waits)
  Since this version has proven to be stable it might serve as comparison
  for future search engine changes.

  2.7:
  unmake_root_move function. 
  remove command.
  bd and unpost commands.
  pondering.
  setup and edit commands.
  (more rebust setup_from_epd etc.)
  50 move rule works.
  Improved time management.

  2.8:
  (rough) Null move implementation.
  Read abbreviated algebraic notation (using a state machine)
  Ponder bugfix (when pondering on ep moves)

  2.8.1:
  Improved test output (include list of not solved positions)
  null move : tests show that with R=2 allowing when n == 2 and 
  going into quies is slower than doing null only at n > 2.
  quies: no tests for check anymore ... makes it a bit faster in wac/bk 


  2.8.2: 
  bugfix: Updating bounds (best) through null move led to storing a 
  random move (precisely the move best_move_index was initialized to)
  extended move_array to hold 1800 elements (was 1000) - bwtc 726 is the
  critical test here 


  2.9.
  trying hung piece detection... (removed in 2.15)
  extension counters added to move flags
  determined irregularities in see... (distinguishing trades or winners is
  important for hp detection, but in normal quies

  2.9.1:
  Fix: clear hash and killer tables to make test runs deterministic.

  2.10:
  see is now called before the capture in question is executed.
  It also distingushes winners and trades much better and may be
  used to restrict quies further (e.g. winners only) or for move
  ordering in the main search.

  2.11:
  quies searches only winners further.
  search uses order() to give keys to moves. tt_award_bonus and
  award_killer_bonus removed. Also there is no need to assign keys
  to moves when generating moves.
  Also, pick() function only picks the first 6 legal moves.
  Currently, the dropping node rate and the resulting smaller tree
  evens out (for bk.epd). see is beaten a lot harder now...
  older quies() versions did test score against alpha/beta/beta
        if even move was not executed -- fixed.

  2.11.pl1
  don't call pick() in quies() since there are no keys.
  hung piece code removed from quiescence. If any, it belongs to
  search since it simply blows quies() up heavily.
  (There is still the problem of a killer getting a capture score
  if it is a capture)

  2.11.pl2
  better test output.

  2.11.pl3
  even stricter quiescence search ( winning captures must reach alpha )
  more test output (plies, nps/ply)

  2.12.pl0
  pawn hash tables + very little information in there (doubled pawns
  and static bonuses )

  2.12.pl1
  added isolated / backward detection for pawns (for white only)

  2.12.pl2
  basic pawn eval complete for both colors (king safety, holes,
  strong/weak bishops etc. missing)
  bugfix (?) for see (shortcut "winner anyway" disabled since for clean
  quies an *exact* value is needed. This version scored a 4:4 (gnu_vs_gully)
  against gnu, but mainly due to bugs in gnu... And it did much worse in a
  longer match. (about 20%)
  
  2.12.pl3
  evaluate_endgame function.
  Detects trivial draws. Can mate with BN vs. King.

  Stores and restores weak_passed info from pawn hash table, but doesnt 
  actually use it.
  
  Makefile: Better optimization (-O3 and -fomit-framepointer)

  2.13.pl0
  added opening book support

  2.13.pl1
  fixed opening book support a little up (better support via options and
  commands).
  bugfix: the new parse_abbreviated() function had a small error in the 
  state machine.

  2.13.pl2
  Removed gcc 2.95 compiler warnings.
  Minor streamlining of commandline parameter handling.
  Support for --version | -v switch.

  2.13.pl3
  Initial support for bench command.
  bugfix: attacks.c:fill_see_array contained an array index violation.
  (reported by D. Corbit)
  bugfix: null move bit was cleared in phase() but never reset for
  new test postions.

  2.13.pl4
  Make Gully2 more conforming to current xboard versions (see
  engine-intf text by Tim Mann).

  2.13.pl5
  Make Gully2 more conforming to current xboard versions. Honors
  "white" and "black" commands. Introduced "bench" and "bogomips"
  commands. Implements "new" command. Some cleanup in book code.

  2.13pl6
  Bugfix: input.c:fread_input now checks return value of fgets 
  and handles the interrupted system call condition.

  2.13pl7:
  Do not allow that input commands pile up during pondering, since
  extra commands will be lost. Fixed the "new" command when running
  interactively (there is still a problem if Gully is reset while it
  is searching, see BUGS.)

  2.13pl8:
  Fixed another obscure problem when running fast match games via xboard.
  (G2 did eat a user move while pondering)

  2.14pl0:
  Extended evaluation function (in progress). Currently, the new version
  is matched against the proven old version. Seems to work.
  
  2.14pl1:
  Old way of keeping material balance removed after tests.

  2.14pl2:
  Revisit log facility. (clean compile for windows?, crashing err_msg?)
  Finish/Rewrite Don Beals KPK algorithm.
  New flag -x (--xboard) for explicit xboard mode (necessary for old
  xboard version (e.g., 3.5.x)).

  2.14pl3:
  Removed Don Beal's KPK algorithm. 

  2.14pl4 - 2.15pl0 :
  Win32 compatibility (in progress).
  Status: Pondering for Win32 initially supported. 
  [2.14pl7: tightened return type of GET_SQUARE macro to square_t.]
  
  2.15pl0:
  Modified check_input() function to support pondering in Win32.

  2.15pl1: 
  Some error checking in book.c. Fixed bug in move parser (input.c).

  2.15pl2: Test suites evaluate success with parse_abbreviated() instead
  of rolling an own imperfect parsing.

  2.15pl3: Log to file instead to syslog (Unix, this is still optional here) 
  or to stderr. Implement 'Move now'. 

  2.15pl4:
  Source cleanup and  semantic cleanup of timer code. 
  'Analyse' commands and friends (not completed yet).

  2.15pl5:
  Bugfix: pawn hash table initialization.

  2.15pl6:
  Bugfix: pawn hash retrieve (broken by prev. patch)
  Analyse mode.

  2.15pl7: analysis + fritz mode. Implemented chessbase extensions
  to wb protocol 1.
  Book support in analysis mode.

  2.15pl8: Bugfix: For Win32: setvbuf (to modify buffering behavior of
  stdin and stdout) works only if set to _IONBF (is it true that _IOLBF
  haves like _IOFBF for win32 ?!)

  2.15pl9: Bugfix: win32 console - input_pending would trigger fgets even if 
  there would be focus or menu events delivered to the terminal (mouse click
  into the window would still halt e.g. "gully2 --bench").
  Bugfix: epd parser (tolerate empty lines and comments starting with #)
  Test management: use "am" (avoid moves) tag in fen string.

  2.15pl10: Correct MATE values in transref (stores distance to mate now).

  2.16pl0: Evaluation:
  Prepare evaluation functions for individual endings (pawns, opposite colored
  bishops)
  Rook "sideways" mobility
  Increase bonusses for static piece placement (encourage development)
  Mat_dist_table (pair of bishops, QN bonus etc.)
  Remove backward compatibilty feature of move output, uses new style now 

  2.16pl1;
  Includes bugfix (thanks to Andrew and Olivier!) regarding pondering 
    (called check_input() way too often)
  Removed extra printf (introduced by Jim Ablett) from main.c
  Uncommented experimental distance functions in eval.

  2.16pl2:
  * clear_pv() clears PV completely when setup_board 
  * fail-low() : use low-failing move from hash when re-searching with open window
  * move ordering : order moves using (??) -- planned: quies()



*/
#endif /* __VERSION_H */
