/*
 * io_tiff.c
 *
 * Jim Stewart
 * 14 Jun 2002
 *
 * Based on io_pnm.c from iomedley, xvtiff.c from xv v3.10a, and examples at
 * http://www.libtiff.org/libtiff.html and
 * http://www.cs.wisc.edu/graphics/Courses/cs-638-1999/libtiff_tutorial.htm
 *
 * This code looks Bad in <120 columns.
 *
 */

#ifdef HAVE_CONFIG_H
#include <iom_config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#ifndef _WIN32
#include <unistd.h>
#endif /* _WIN32 */
#include <sys/types.h>
#include <string.h>
#include <tiffio.h>

#include "iomedley.h"

/* Photometric names. */

static const char * const Photometrics[] = {
  "MINISWHITE",
  "MINISBLACK",
  "RGB",
  "PALETTE",
  "MASK",
  "SEPARATED",
  "YCBCR",
  "CIELAB" };

/* Magic numbers as defined in Linux magic number file. */

#define TIFF_MAGIC_BIGEND	"MM\x00\x2a"
#define TIFF_MAGIC_LITTLEEND	"II\x2a\x00"

int	iom_isTIFF(FILE *);
int	iom_GetTIFFHeader(FILE *, char *, struct iom_iheader *);
int	iom_ReadTIFF(FILE *, char *, int *, int *, int *, int *, unsigned char **, int *, int*);
int	iom_WriteTIFF(char *, unsigned char *, struct iom_iheader *, int);

static int uchar_overflow(unsigned char* data, size_t size);
static int int_overflow(unsigned char* data, size_t size);
static int ushort_overflow(unsigned char* data, size_t size);



/* Error handlers used by libtiff.
   They're also used directly by the code below. */

static void
tiff_warning_handler(const char *module, const char *fmt, va_list ap) {

  if (iom_is_ok2print_warnings()) {
    fprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
  }

}

static void
tiff_error_handler(const char *module, const char *fmt, va_list ap) {

  if (iom_is_ok2print_errors()) {
    fprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
  }

}

/* iom_isTIFF(FILE *fp)
 *
 * Magic number check.
 *
 * Returns: 1 if fp is a TIFF file, 0 otherwise.
 *
 */

int
iom_isTIFF(FILE *fp)
{

  unsigned char	magic[4];
  int		i, c;
  
  rewind(fp);

  for (i = 0; i < 4; i++) {
    if ((c = fgetc(fp)) == EOF)
      return 0;
    magic[i] = (unsigned char) c;
  }

  if (!strncmp(magic, TIFF_MAGIC_BIGEND, 4))
    return 1;
  else if (!strncmp(magic, TIFF_MAGIC_LITTLEEND, 4))
    return 1;
  else
    return 0;

}

/* iom_GetTIFFHeader() */

int
iom_GetTIFFHeader(FILE *fp, char *filename, struct iom_iheader *h)
{

  int		x, y, z, bits, org, type;
  unsigned char	*data;
  size_t i, dsize;

  if (!iom_ReadTIFF(fp, filename, &x, &y, &z, &bits, &data, &org, &type))
    return 0;

  iom_init_iheader(h);

  h->org = org;

  if (z == 1) {
    h->size[0] = x;
    h->size[1] = y;
    h->size[2] = z;
  } else {		/* z == 3 */
    h->size[0] = z;
    h->size[1] = x;
    h->size[2] = y;
  }

  h->data = data;
  dsize = ((size_t)x)*((size_t)y)*((size_t)z);

 int byte_size = bits/8;

  /* type == -1 means sampleformat tag was missing and we assume unsigned int type */
  if (bits == 8) {
    if( type == SAMPLEFORMAT_INT && uchar_overflow(h->data, dsize) < dsize ) {    /* upgrade to shorts for davinci if necessary */
    	  h->data =(unsigned char*)realloc(h->data, dsize*byte_size*2);
          short* temp_s = (short*)h->data;
          for(i=dsize-1; i>=0; i--)
            temp_s[i] = *((char*)(&h->data[i]));

          h->format = iom_SHORT;
          h->eformat = iom_NATIVE_INT_2;

    } else {        /* type is SAMPLEFORMAT_UINT or it will fit in an unsigned byte (we assume this if type == -1)*/
        h->format = iom_BYTE;
        h->eformat = iom_NATIVE_INT_1;
    }

  } else if (bits == 16) {
    if( (type == SAMPLEFORMAT_UINT || type == -1) && ushort_overflow(h->data, dsize) < dsize ) { /* upgrade to ints if necessary */
          h->data =(unsigned char*)realloc(h->data, dsize*byte_size*2);
          int* temp_i = (int*)h->data;
          for(i=dsize-1; i>=0; i--)
            temp_i[i] = *((unsigned short*)(&h->data[i*byte_size]));

          h->format = iom_INT;
    	  h->eformat = iom_NATIVE_INT_4;

	} else {                    /*type == SAMPLEFORMAT_INT or it fits */
      h->format = iom_SHORT;
      h->eformat = iom_NATIVE_INT_2;
    }

  } else if (bits == 32) {
    if( type == SAMPLEFORMAT_UINT ) { 
      if( int_overflow(h->data, dsize) < dsize) {      /* upgrade to doubles if necessary*/
        h->data =(unsigned char*)realloc(h->data, dsize*byte_size*2);
        double* temp_d = (double*)h->data;
        for(i=dsize-1; i>=0; i--)
            temp_d[i] = *((unsigned int*)(&h->data[i*4]));
        
        h->format = iom_DOUBLE;
        h->eformat = iom_MSB_IEEE_REAL_8;

      } else {      /* we can store the data in signed int -no upgrade necessary */
        h->format = iom_INT;
        h->eformat = iom_NATIVE_INT_4;
      }

    } else if( type == SAMPLEFORMAT_INT ) {
      h->format = iom_INT;
      h->eformat = iom_NATIVE_INT_4;
    } else {                    /* assume float: type == SAMPLEFORMAT_IEEEFP or type == -1 */
      h->format = iom_FLOAT;
      h->eformat = iom_MSB_IEEE_REAL_4;
    }
  } else if( bits == 64 ) {
    double* temp_d = (double*)h->data;
    if( type == SAMPLEFORMAT_INT ) {
      for(i=0; i<dsize; i++) {
        temp_d[i] = *((int *)&h->data[i*byte_size]); /*converting from int or unsigned int to double */
      }
    }
    else if( type == SAMPLEFORMAT_UINT ) {
      for(i=0; i<dsize; i++) {
        temp_d[i] = *((unsigned int *)&h->data[i*byte_size]); /*converting from int or unsigned int to double */
      }
    }
    /* 64 bits will always be double or converted to double (type == -1 or SAMPLEFORMAT_IEEEFP skip the conversion) */
    h->format = iom_DOUBLE;
	h->eformat = iom_MSB_IEEE_REAL_8;
  }

  return 1;

}

int
iom_ReadTIFF(FILE *fp, char *filename, int *xout, int *yout, int *zout, 
             int *bits, unsigned char **dout, int *orgout, int *type)
{

  TIFF		*tifffp;
  uint32	x, y, row;
  tdata_t	buffer;
  size_t	row_stride;		/* Bytes per scanline. */
  unsigned char	*data;
  unsigned short z, bits_per_sample, planar_config, plane, photometric;

  rewind(fp);

  /* TIFF warning/error handlers.
     Sneakily using them to report our own errors too.
  */

  TIFFSetWarningHandler(tiff_warning_handler);
  TIFFSetErrorHandler(tiff_error_handler);

#if 0
  if ((tifffp = TIFFFdOpen(fileno(fp), filename, "r")) == NULL) {
      TIFFError(NULL, "ERROR: unable to open file %s", filename);
    return 0;
  }
#endif

  if ((tifffp = TIFFOpen(filename, "r")) == NULL) {
    TIFFError(NULL, "ERROR: unable to open file %s", filename);
    return 0;
  }

  /* FIX: check for multiple images per file? */

  TIFFGetFieldDefaulted(tifffp, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);	/* Not always set in file. */

  if (bits_per_sample != 8 && bits_per_sample != 16 && bits_per_sample != 32 && bits_per_sample != 64) {
    TIFFError(NULL, "File %s contains an unsupported (%d) bits-per-sample.", filename, bits_per_sample);
    TIFFClose(tifffp);
    return 0;
  }

#if 0
  /* Flip orientation so that image comes in X order.
     Taken from xv's xvtiff.c.  Why not both ORIENTATION_TOPLEFT? */

  TIFFGetFieldDefaulted(tifffp, TIFFTAG_ORIENTATION, &orient);
  switch (orient) {
  case ORIENTATION_TOPLEFT:
  case ORIENTATION_TOPRIGHT:
  case ORIENTATION_LEFTTOP:
  case ORIENTATION_RIGHTTOP:   orient = ORIENTATION_BOTLEFT;   break;

  case ORIENTATION_BOTRIGHT:
  case ORIENTATION_BOTLEFT:
  case ORIENTATION_RIGHTBOT:
  case ORIENTATION_LEFTBOT:    orient = ORIENTATION_TOPLEFT;   break;
  }

  TIFFSetField(tifffp, TIFFTAG_ORIENTATION, orient);
#endif

  if (!TIFFGetField(tifffp, TIFFTAG_IMAGEWIDTH, &x)) {
    TIFFError(NULL, "TIFF image width tag missing.");
    TIFFClose(tifffp);
    return 0;
  }

  if (!TIFFGetField(tifffp, TIFFTAG_IMAGELENGTH, &y)) {
    TIFFError(NULL, "TIFF image length tag missing.");
    TIFFClose(tifffp);
    return 0;
  }

  TIFFDataType tiff_type;
  if( !TIFFGetField(tifffp, TIFFTAG_SAMPLEFORMAT, &tiff_type) ) {
    tiff_type = -1;     /* will cause calling function to make assumptions/guesses */
    fprintf(stdout, "SAMPLEFORMAT tag missing; assumptions will be made in data format determination\n"); 
  }

  TIFFGetFieldDefaulted(tifffp, TIFFTAG_SAMPLESPERPIXEL, &z);			/* Not always set in file. */

  if (!TIFFGetField(tifffp, TIFFTAG_PLANARCONFIG, &planar_config)) {
    TIFFError(NULL, "TIFF planar config tag missing.");
    TIFFClose(tifffp);
    return 0;
  }

  if (!TIFFGetField(tifffp, TIFFTAG_PHOTOMETRIC, &photometric)) {
    TIFFError(NULL, "TIFF photometric tag missing.");
    TIFFClose(tifffp);
    return 0;
  }

  row_stride = TIFFScanlineSize(tifffp); /* Bytes per scanline. */

  buffer = (tdata_t) _TIFFmalloc(row_stride);
  data = (unsigned char *) malloc(((size_t)y) * row_stride);

  /* FIX: deal with endian issues? */

  if (planar_config == PLANARCONFIG_CONTIG) {
    for (row = 0; row < y; row++) {
      if (TIFFReadScanline(tifffp, buffer, row, 0) == -1) {	/* 4th arg ignored in this planar config. */
        if (buffer) {
          _TIFFfree(buffer);
        }
        if (data) {
          free(data);
        }
	    _TIFFfree(buffer);
	    TIFFClose(tifffp);
        return 0;
      } else
	    memcpy(data + ((size_t)row) * row_stride, buffer, row_stride);
    }
  } else {
    for (plane = 0; plane < z; plane++) {
      for (row = 0; row < y; row++) {

	    if(TIFFReadScanline(tifffp, buffer, row, plane) == -1) {
          if (buffer) {
            _TIFFfree(buffer);
          }
          if (data) {
            free(data);
          }

	      _TIFFfree(buffer);
	      TIFFClose(tifffp);
          return 0;

	    } else
	      memcpy(data + ((size_t)row) * row_stride, buffer, row_stride);
      }
    }
  }

  _TIFFfree(buffer);
  TIFFClose(tifffp);

  if (iom_is_ok2print_progress()) {
    printf("TIFF photometric is %s\n", Photometrics[photometric]);
  }

  *xout = x;
  *yout = y;
  *zout = z;
  *bits = bits_per_sample;
  *dout = data;
  *type = tiff_type;

  if (z == 1)
    *orgout = iom_BSQ;
  else
    if (planar_config == PLANARCONFIG_CONTIG)
      *orgout = iom_BIP;
    else
      *orgout = iom_BIL;

  return 1;

}

int
iom_WriteTIFF(char *filename, unsigned char *indata, struct iom_iheader *h, int force)
{

  int		x, y, z;
  int		row;
  tsize_t	row_stride;
  TIFF		*tifffp;
  unsigned char	*scanline;
  unsigned char *data;
  unsigned short bits_per_sample;
  uint16         fillorder;
  TIFFDataType   sample_fmt = -1;

  /* Check for file existance if force overwrite not set. */

  if (!force && access(filename, F_OK) == 0) {
    if (iom_is_ok2print_errors()) {
      fprintf(stderr, "File %s already exists.\n", filename);
    }
    return 0;
  }

  if (h->format == iom_BYTE) {
    bits_per_sample = 8;
    fillorder = (h->eformat == iom_MSB_INT_1 ? FILLORDER_MSB2LSB : FILLORDER_LSB2MSB);
    sample_fmt = SAMPLEFORMAT_UINT;
  } else if (h->format == iom_SHORT) {
    bits_per_sample = 16;
    fillorder = (h->eformat == iom_MSB_INT_2 ? FILLORDER_MSB2LSB : FILLORDER_LSB2MSB);
    sample_fmt = SAMPLEFORMAT_INT;
  } else if (h->format == iom_INT) {
    bits_per_sample = 32;
    fillorder = (h->eformat == iom_MSB_INT_4 ? FILLORDER_MSB2LSB : FILLORDER_LSB2MSB);
    sample_fmt = SAMPLEFORMAT_INT;
  } else if (h->format == iom_FLOAT) {
    bits_per_sample = 32;
    fillorder = (h->eformat == iom_MSB_IEEE_REAL_4 ? FILLORDER_MSB2LSB : FILLORDER_LSB2MSB);
    sample_fmt = SAMPLEFORMAT_IEEEFP;
  } else if (h->format == iom_DOUBLE) {
    bits_per_sample = 64;
    fillorder = (h->eformat == iom_MSB_IEEE_REAL_8 ? FILLORDER_MSB2LSB : FILLORDER_LSB2MSB);
    sample_fmt = SAMPLEFORMAT_IEEEFP;
  } else {
    if (iom_is_ok2print_errors()) {
      fprintf(stderr, "Cannot write %s data in a TIFF file.\n", iom_FORMAT2STR[h->format]);
    }
    return 0;
  }

  z = iom_GetBands(h->size, h->org);

  /* Make sure data is 1-band, 3-band (RGB) or 4-band (RGBA). */

  if (z > 4) {
    if (iom_is_ok2print_errors()) {
      fprintf(stderr, "Cannot write TIFF files with depths greater than 4.\n");
    }
    return 0;
  }

  /* Convert data to BIP if not already BIP. */

  if (h->org == iom_BIP) {
    data = indata;
  } else {
    /* iom__ConvertToBIP allocates memory, don't forget to free it! */
    if (!iom__ConvertToBIP(indata, h, &data)) {
      return 0;
    }
  }

  TIFFSetWarningHandler(tiff_warning_handler);
  TIFFSetErrorHandler(tiff_error_handler);

  if ((tifffp = TIFFOpen(filename, "w")) == NULL) {
    if (iom_is_ok2print_sys_errors()) {
      fprintf(stderr, "Unable to write file %s. Reason: %s.\n", filename, strerror(errno));
    }
    if (h->org != iom_BIP) {
      free(data);
    }
    return 0;
  }

  /* Set TIFF image parameters. */

#define max(a,b) (a>b?a:b)

  x = iom_GetSamples(h->size, h->org);
  y = iom_GetLines(h->size, h->org);

  TIFFSetField(tifffp, TIFFTAG_IMAGEWIDTH, x);
  TIFFSetField(tifffp, TIFFTAG_IMAGELENGTH, y);
  TIFFSetField(tifffp, TIFFTAG_SAMPLESPERPIXEL, z);
  TIFFSetField(tifffp, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
  TIFFSetField(tifffp, TIFFTAG_FILLORDER, fillorder);
  TIFFSetField(tifffp, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
  TIFFSetField(tifffp, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(tifffp,  TIFFTAG_ROWSPERSTRIP,
              	max(1,(int)(8*1024 / TIFFScanlineSize(tifffp))));
  TIFFSetField(tifffp,  TIFFTAG_COMPRESSION, COMPRESSION_LZW);

  if (sample_fmt != -1){
    TIFFSetField(tifffp,  TIFFTAG_SAMPLEFORMAT, sample_fmt);
  }

#ifdef WORDS_BIGENDIAN
  TIFFSetField(tifffp,  TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
#else
  TIFFSetField(tifffp,  TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);
#endif

  if (z == 1 || z == 2) {
    TIFFSetField(tifffp, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
  } else if (z == 3 || z == 4) {
    /* 3 or 4 */
    TIFFSetField(tifffp, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
  }

  if (z == 2 || z == 4) {
    // Identify that we have an alpha channel.
    // This is a goofy way to pass an extra value, but everyone seems to do it.
    unsigned short  sample_info[1];
    sample_info[0] = EXTRASAMPLE_ASSOCALPHA;
    TIFFSetField(tifffp, TIFFTAG_EXTRASAMPLES, 1, &sample_info[0]);
  }

  row_stride = ((size_t)x) * ((size_t)z) * (bits_per_sample / 8);

  /* Allocate memory for one scanline of output. */

  scanline = (unsigned char *) _TIFFmalloc(row_stride);

#if 0
  if (TIFFScanlineSize(tifffp) > row_stride)
    scanline = (unsigned char *) malloc(TIFFScanlineSize(tifffp));
  else
    scanline = (unsigned char *) malloc(row_stride);
#endif

  /* FIX: need this?  lib version problem..
     TIFFSetField(tifffp, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tifffp, row_stride));
  */

  /* Write the file out one line at a time. */

  for (row = 0; row < y; row++) {
    memcpy(scanline, data + (row * row_stride), row_stride);
    if (TIFFWriteScanline(tifffp, scanline, row, 0) < 0) {
      if (scanline) {
        _TIFFfree(scanline);
      }
      (void) TIFFClose(tifffp);
      return 0;
    }
  }

  (void) TIFFClose(tifffp);

  if (scanline) {
    _TIFFfree(scanline);
  }

  if (h->org != iom_BIP) {
    free(data);
  }

  return 1;

}

static int
uchar_overflow(unsigned char* data, size_t size)
{
	int i;
	for(i=0; i<size; i++) {
		if(*((char*)&data[i]) < 0)
			break;
	}
	return i;
}

static int
int_overflow(unsigned char* data, size_t size)
{
	int i;
	for(i=0; i<size; i++) {
		if(*((unsigned int*)&data[i*4]) > INT_MAX)
			break;
	}
	return i;
}

static int
ushort_overflow(unsigned char* data, size_t size)
{
	size_t i;
	for(i=0; i<size; i++) {
		if(*((short*)&data[i*2]) < 0)
			break;
	}
	return i;
}
