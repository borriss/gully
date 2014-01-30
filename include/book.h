/* $Id: book.h,v 1.6 2003-04-11 16:44:26 martin Exp $ */

#ifndef __BOOK_H
#define __BOOK_H

extern FILE           *book_file;
extern int            book_accept_mask;
extern int            book_reject_mask;
extern int            book_random;
extern int            book_lower_bound;
extern int            book_absolute_lower_bound;
extern float          book_min_percent_played;
extern int            show_book;

#define DEFAULT_BOOK "book.bin"
#define BOOK_BUF_SIZE 512

struct bookstat_tag {
  char name[128];
  /* list of current book moves, used by analysis */
  char book_buf[BOOK_BUF_SIZE]; 
  long size;
};

extern struct bookstat_tag bookstat;


/* swiss army knife - execute all possible moves from current position
 * and look them up. Select a move from the sert of book moves. 
*/
int book(void);

void book_up(char *output_filename);
int book_mask(char *flags);
book_position_t book_up_next_position(int files, int init);
int book_up_compare(const void *pos1, const void *pos2);
int book_open(const char *bookname,FILE ** f);

/* print book status */
int book_status(void);

#endif /* __BOOK_H */
