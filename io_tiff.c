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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#endif /* _WIN32 */
#include <sys/types.h>
#include <string.h>
#include <tiffio.h>

#include "iomedley.h"

/* Photometric names. */

static const unsigned char * const Photometrics[] = {
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
int	iom_ReadTIFF(FILE *, char *, int *, int *, int *, int *, unsigned char **, int *);
int	iom_WriteTIFF(char *, unsigned char *, struct iom_iheader *, int);

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

  int		x, y, z, bits, org;
  unsigned char	*data;

  if (!iom_ReadTIFF(fp, filename, &x, &y, &z, &bits, &data, &org))
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

  if (bits == 8) {
    h->format = iom_BYTE;
    h->eformat = iom_NATIVE_INT_1; /* libtiff always reads in native format */
  } else { /* 16 */
    h->format = iom_SHORT;
    h->eformat = iom_NATIVE_INT_2;
	/*
	** TIFF 16-bit is unsigned.  We need to deal with that here.
	**
	** Our two options are to shift down to signed 15-bits, or
	** promote all the way up to int.
	*/
	{
		unsigned short *us;
		short *s;
		int i;

		us = (unsigned short *)data;
		s = (short *)data;
		for (i = 0 ; i < x*y*z ; i++) {
			s[i] = ((int)(us[i]))-32768;
		}
		h->data = data;
	}
  }

  return 1;

}

int
iom_ReadTIFF(FILE *fp, char *filename, int *xout, int *yout, int *zout, int *bits, unsigned char **dout, int *orgout)
{

  TIFF		*tifffp;
  uint32	x, y, row;
  tdata_t	buffer;
  int		row_stride;		/* Bytes per scanline. */
  unsigned char	*data;
  unsigned short orient, z, bits_per_sample, planar_config, plane, fillorder, photometric;
  int            eformat;

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

  if (bits_per_sample != 8 && bits_per_sample != 16) {
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
  data = (unsigned char *) malloc(y * row_stride);

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
	memcpy(data + row * row_stride, buffer, row_stride);
    }
  } else {
    for (plane = 0; plane < z; plane++)
      for (row = 0; row < y; row++) {
	if (TIFFReadScanline(tifffp, buffer, row, plane) == -1) {
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
	  memcpy(data + row * row_stride, buffer, row_stride);
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

  /* Check for file existance if force overwrite not set. */

  if (!force && access(filename, F_OK) == 0) {
    if (iom_is_ok2print_errors()) {
      fprintf(stderr, "File %s already exists.\n", filename);
    }
    if (h->org != iom_BIP) {
      free(data);
    }
    return 0;
  }

  /* Input data must be 8 or 16 bit. */

  if (h->format == iom_BYTE) {
    bits_per_sample = 8;
    fillorder = (h->eformat == iom_MSB_INT_1 ? FILLORDER_MSB2LSB : FILLORDER_LSB2MSB);
  } else if (h->format == iom_SHORT) {
    bits_per_sample = 16;
    fillorder = (h->eformat == iom_MSB_INT_2 ? FILLORDER_MSB2LSB : FILLORDER_LSB2MSB);
  } else {
    if (iom_is_ok2print_errors()) {
      fprintf(stderr, "Cannot write %s data in a TIFF file.\n", iom_FORMAT2STR[h->format]);
    }
    return 0;
  }

  z = iom_GetBands(h->size, h->org);

  /* Make sure data is 1-band, 3-band (RGB) or 4-band (RGBA). */

  if (z != 1 && z != 3) {
    if (iom_is_ok2print_errors()) {
      fprintf(stderr, "Cannot write TIFF files with depths other than 1 or 3.\n");
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

#ifdef WORDS_BIGENDIAN
  TIFFSetField(tifffp,  TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
#else
  TIFFSetField(tifffp,  TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);
#endif

  if (z == 1)
    TIFFSetField(tifffp, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
  else /* 3 or 4 */
    TIFFSetField(tifffp, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

  row_stride = x * z * (bits_per_sample / 8);

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
