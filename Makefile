# Generated automatically from Makefile.in by configure.
prefix=/usr/local

CC=gcc
DEFS=-DHAVE_CONFIG_H
CPPFLAGS=-I.   -I/usr/X11R6/include/X11
# X_CFLAGS= -I/usr/X11R6/include/X11
# X_PRE_LIBS= -lSM -lICE
# X_LIBS= -L/usr/X11R6/lib
# X_EXTRA_LIBS=
CFLAGS=-g -O2 $(X_CFLAGS)
# LDLIBS= -lMagick -ltiff -ljpeg -lpng -lbz2 -lz -lm -lXext -lXt -lX11   -lSM -lICE  -L/usr/X11R6/lib  $(X_PRE_LIBS) $(X_LIBS) $(X_EXTRA_LIBS)
LDLIBS= -lMagick -ltiff -ljpeg -lpng -lbz2 -lz -lm -lXext -lXt -lX11   -lSM -lICE  -L/usr/X11R6/lib 
RANLIB=ranlib

OBJS= \
	io_aviris.o \
	io_goes.o \
	io_grd.o \
	io_imath.o \
	io_isis.o \
	io_lablib3.o \
	io_magic.o \
	io_pnm.o \
	io_vicar.o \
	io_ers.o \
	io_raw.o \
	io_envi.o \
	iomedley.o \
	tools.o

.c.o:
	$(CC) -c $(DEFS) $(CPPFLAGS) $(CFLAGS) $<

libiomedley.a:	$(OBJS)
	ar -ruv $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f *.o libiomedley.a 

io_aviris.o: io_aviris.c iomedley.h io_lablib3.h toolbox.h
io_envi.o: io_envi.c io_envi.h iomedley.h io_lablib3.h toolbox.h
io_ers.o: io_ers.c iomedley.h io_lablib3.h toolbox.h
io_goes.o: io_goes.c iomedley.h io_lablib3.h toolbox.h
io_grd.o: io_grd.c iomedley.h io_lablib3.h toolbox.h
io_imath.o: io_imath.c iomedley.h io_lablib3.h toolbox.h
io_isis.o: io_isis.c iomedley.h io_lablib3.h toolbox.h
io_lablib3.o: io_lablib3.c header.h tools.h io_lablib3.h toolbox.h
io_magic.o: io_magic.c
io_pnm.o: io_pnm.c iomedley.h io_lablib3.h toolbox.h
io_raw.o: io_raw.c iomedley.h io_lablib3.h toolbox.h
io_vicar.o: io_vicar.c iomedley.h io_lablib3.h toolbox.h
iomedley.o: iomedley.c iomedley.h io_lablib3.h toolbox.h
tools.o: tools.c tools.h
