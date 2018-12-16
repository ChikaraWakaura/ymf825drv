# make
LIB        = /usr/local/lib

CC         = gcc
CFLAGS     = -O2 -Wall
#CFLAGS     = -DDEBUG -g -O0 -Wall
LDFLAGS    = -L$(LIB) -lbcm2835 -lpthread -lasound

OBJ_LINK   =	ymf825drv.o \
		fmasgn.o \
		fmif.o \
		fmnote.o \
		fmpart.o \
		fmtone.o \
		fmvoice.o \
		fmsd1_raspi.o

CHECK_LIST =	Makefile \
		ymf825drv.o \
		ymf825drv.c \
		fmasgn.o \
		fmif.o \
		fmnote.o \
		fmpart.o \
		fmtone.o \
		fmvoice.o \
		fmsd1_raspi.o \
		fmasgn.c \
		fmif.c \
		fmnote.c \
		fmpart.c \
		fmtone.c \
		fmvoice.c \
		fmsd1_raspi.c

all : ymf825drv

# linke
ymf825drv : $(CHECK_LIST)
	$(CC) -o $@ $(OBJ_LINK) $(LDFLAGS)

# cc compile
ymf825drv.o : ymf825drv.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

fmasgn.o : fmasgn.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

fmif.o : fmif.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

fmnote.o : fmnote.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

fmpart.o : fmpart.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

fmtone.o : fmtone.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

fmvoice.o : fmvoice.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

fmsd1_raspi.o : fmsd1_raspi.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o
