# Generated automatically from Makefile.in by configure.
prefix=/usr/local

CC=gcc
DEFS=-DHAVE_CONFIG_H
CPPFLAGS=-I.   -I/usr/openwin/include -I/usr/local/include -I/usr/openwin/include -I/usr/openwin/include/X11
# X_CFLAGS= -I/usr/openwin/include
# X_PRE_LIBS= -lSM -lICE
# X_LIBS= -L/usr/openwin/lib -R/usr/openwin/lib
# X_EXTRA_LIBS=-lsocket  -lnsl
CFLAGS=-g -O2 -O $(X_CFLAGS)
# LDLIBS= -L/usr/local/lib -L/usr/openwin/lib -R/usr/openwin/lib -lMagick -lXext -lXt -lX11   -lSM -lICE  -L/usr/openwin/lib -R/usr/openwin/lib -lsocket  -lnsl -L/opt/local/src/ImageMagick-4.2.9/magick -lMagick -ltiff -ljpeg -lpng -ldpstk -ldps -lXext -lXt -lX11 -lsocket -lnsl -lz -lm $(X_PRE_LIBS) $(X_LIBS) $(X_EXTRA_LIBS)
LDLIBS= -L/usr/local/lib -L/usr/openwin/lib -R/usr/openwin/lib -lMagick -lXext -lXt -lX11   -lSM -lICE  -L/usr/openwin/lib -R/usr/openwin/lib -lsocket  -lnsl -L/opt/local/src/ImageMagick-4.2.9/magick -lMagick -ltiff -ljpeg -lpng -ldpstk -ldps -lXext -lXt -lX11 -lsocket -lnsl -lz -lm
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
