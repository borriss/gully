# $Id: Makefile,v 1.30 2013/10/01 08:04:39 martin Exp $ 
#
# Gullydeckel 2 
#
#

INCLUDEPATH = ../include

# fastest 
CFLAGS = -Wall -Wmissing-prototypes -ansi -fomit-frame-pointer -DCOMPILE_FAST -DUNIX  -DNDEBUG -O3	-march=native -I$(INCLUDEPATH)
LDFLAGS = -lm

# debug ready 
#CFLAGS = -Wall -Wmissing-prototypes -ansi -DCOMPILE_DEBUG -DUNIX -O3 -g  \
	-I$(INCLUDEPATH)
#LDFLAGS = -g -lm

# profile and debug ready
#CFLAGS = -Wall -Wmissing-prototypes -ansi -DCOMPILE_DEBUG_PROFILE -DUNIX  \
	-g -pg -m486 -I$(INCLUDEPATH)
#LDFLAGS = -g -pg

# debug rep list ready 
#CFLAGS = -Wall -Wmissing-prototypes -ansi -DCOMPILE_DEBUG -DUNIX  -DSHOW_REP \
	-g -m486 -I$(INCLUDEPATH)
#LDFLAGS = -g -lm

CC =	gcc
#CC =	clang

SRCS	=	attacks.c data.c helpers.c  main.c mstimer.c  chessio.c  \
	execute.c  init.c     movegen.c  test.c	logger.c evaluate.c \
	tables.c search.c quies.c readopt.c history.c input.c hash.c \
	transref.c repeat.c iterate.c	order.c	book.c analyse.c

OBJECTS	=	attacks.o data.o helpers.o  main.o mstimer.o  chessio.o  \
	execute.o  init.o     movegen.o  test.o logger.o evaluate.o \
	tables.o search.o quies.o readopt.o history.o input.o hash.o \
	transref.o repeat.o iterate.o	order.o	book.o analyse.o

EXECUTABLE = gully2

%.d:    %.c
	@echo building dependencies for $<
	@$(SHELL) -ec \
                '$(CC) -I$(INCLUDEPATH) -M $< | sed '\''s,$*.o,& $@,g'\'' > $@'
%.o:    %.c Makefile
	$(CC) -c $(CFLAGS) $<

all:	gully2

gully2	: $(OBJECTS)
	$(CC) -o $(EXECUTABLE) $(OBJECTS) $(LDFLAGS)
	@echo "Created $(EXECUTABLE)."

clean	:	
	rm -f $(OBJECTS) *.d *~

tags	:
	find .. -name "*.[ch]" | etags --output ../TAGS -

st	:
	cvs st | grep Loc


metrics	:
	@echo "Compute some metrics (man 1 metrics)."
	mccabe *.c
	csize *.c
	halstead *.c | sort -rgk 3
	lc *.c

html:	
	@echo "Add HTML tags source code (useful for web display)." 
	@echo "(not yet implemented - use chilight or better c2html." 

-include $(SRCS:.c=.d)


