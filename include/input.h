/* $Id: input.h,v 1.16 2003-04-11 16:49:29 martin Exp $ */

#ifndef __INPUT_H
#define __INPUT_H

#define UNKNOWN_COMMAND 0

/* Interactive commands. 
 * Somewhat loose prefix rules:
 * Commands supported by Gnuchess (== old, partially deprecated): GNU
 * xboard/winboard commands which extend this: XB (particulary XB
 * protocol version 2 commands).
 * Gullydeckel extension commands: GULLY
 * 
 * There are other GUIs and more extensions (like 'ponder [move]' by
 * chessbase adapters.
 * 
 */

#define GNU_CMD_O_O 1
#define GNU_CMD_O_O_O 2
#define GNU_CMD_BD 3
#define GNU_CMD_LIST 4
#define GNU_CMD_UNDO 5
#define GNU_CMD_EDIT 6
#define GNU_CMD_SWITCH 7
#define GNU_CMD_WHITE 8
#define GNU_CMD_DEPTH 9
#define GNU_CMD_POST 10
#define GNU_CMD_SAVE 11
#define GNU_CMD_RANDOM 12
#define GNU_CMD_QUIT 13
#define GNU_CMD_MATERIAL 14
#define GNU_CMD_EASY 15
#define GNU_CMD_HASH 16
#define GNU_CMD_REVERSE 17
#define GNU_CMD_BOOK 18
#define GNU_CMD_REMOVE 19
#define GNU_CMD_FORCE 20
#define GNU_CMD_BOTH 21
#define GNU_CMD_BLACK 22
#define GNU_CMD_CLOCK 23
#define GNU_CMD_HINT 24
#define GNU_CMD_GET 25
#define GNU_CMD_NEW 26
#define GNU_CMD_GO 27
#define GNU_CMD_HELP 28
#define GNU_CMD_NOPOST 29
#define XB_CMD_OTIM 30
#define XB_CMD_TIME 31
#define XB_CMD_LEVEL 32
#define GULLY_CMD_PONDER 33 
#define XB_CMD_SETBOARD 34 
#define GNU_CMD_HARD 35
#define XB_CMD_XBOARD 36
#define GULLY_CMD_BOGOMIPS 37
#define GULLY_CMD_BENCH 38

#define XB_CMD_ANALYZE 39
#define XB_CMD_ANALYSIS_EXIT 40
#define XB_CMD_MOVENOW 41
#define XB_CMD_ANALYSIS_STATUS 42
#define XB_CMD_PING 43
#define XB_CMD_BK 44
#define XB_CMD_NAME 45
#define XB_CMD_RATING 46
#define XB_CMD_COMPUTER 47
#define XB_CMD_ICS 48
#define XB_CMD_PAUSE 49
#define XB_CMD_RESUME 50
#define GULLY_CMD_FRITZ 51
#define XB_CMD_DRAW 52
#define XB_CMD_RESULT 53
#define XB_CMD_PROTOVER 54
#define XB_CMD_ACCEPTED 55
#define XB_CMD_REJECTED 56
#define GULLY_CMD_RESET 57


/* returns 1 if buf contains a legal move in this position, 0 otherwise.
   accepts abbreviated notations. (used for book building).
   */
int read_move(char *buf,move_t *m);

int fread_input(FILE *,move_t *);

/* Checks stdin. If there is input, the number of chars waiting is returned
   and set in static input_pending.
   (May be multiple commands such as "time 12345\n otim 54321\n e2e4\n" */
int check_input(void);

/* 
 * If input during pondering is found (check_input()).  
 * m has space for move to be read, p is move we are pondering at.
 */
int handle_ponder_input(FILE *,move_t *m, move_t *p);

/* 
 * If input during analysis is found (check_input()).  
 * m has space for move to be read.
 */
int handle_analysis_input(FILE *,move_t *m);


/* 
 * If input is found while searching. (via check_input()).  
 */
int handle_search_input(FILE *);

#define EX_CMD_FAILURE -1
#define EX_CMD_BUSY 0
#define EX_CMD_SUCCESS 1

/* tries to exec command c.
   returns EX_CMD_FAILURE if there is a problem
   returns EX_CMD_SUCCESS if cmd is handled immediately
   returns EX_CMD_BUSY if cmd cannot be handled now ( e.g.,
   switching hash off during pondering )

   cmd_buf can be supplied here, but is normally set to NULL which
   causes it to use the file-global static_inbuf.
   */
int do_command(int c,char * cmd_buf);

#define G2_NO_RANK 8 
#define G2_NO_FILE 8

/* 
 * Parse a move in reduced algebraic format. Returns 1 on success.
 * Users will probably prefer this; ICS sends always in the long 
 * format.
 * Moves are checked for legality by using move_generator and
 * make_move() function.
 * Function is also used when comparing solutions of test suites
 * and for the opening book building process. 
 *
 * Return: 0 on error, 1 on success, in this case move_t m holds 
 * the result of parsing. 
 */
int parse_abbreviated(char * inbuf,move_t * m);
#endif /* input.h */
