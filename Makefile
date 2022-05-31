#
#
#	Makefile for bprobe and cprobe
#
#	$Header: /home/roycroft/carter/Ping+/rpr/src/Release/RCS/Makefile,v 1.2 1996/06/28 16:48:12 carter Exp $

DEFINES =
CC = gcc
CFLAGS = -DDEBUG -g 
LDFLAGS = #-lsocket -lm 
DESTDIR = /usr/local/bin
MANDIR = /usr/local/man/man8
MANEXT = 8

all: bprobe cprobe

.c.o:
	${CC} ${CFLAGS} ${DEFINES} -c $*.c

bprobe:	bprobe.o filter.o rprobeGuts.o setPackage.o bprobe.h util.o
	${CC} ${CFLAGS} -o $@ rprobeGuts.o bprobe.o filter.o  setPackage.o util.o ${LDFLAGS}
	sudo chmod u+s $@
	sudo chown root $@

cprobe:	congtool.o rprobeGuts.o setPackage.o util.o
	${CC} ${CFLAGS} -o $@ congtool.o rprobeGuts.o setPackage.o util.o ${LDFLAGS}
	sudo chmod u+s $@
	sudo chown root $@

clean:
	rm -f bprobe cprobe core *.o linterrs
