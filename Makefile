CC=gcc
CFLAGS=-g -DHAVE_LIBMAGICK
AR=ar

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

libiomedley.a:	$(OBJS)
	$(AR) -ruv $@ $(OBJS)

clean:
	rm -f *.o libiomedley.a
