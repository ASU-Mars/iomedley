#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pwd.h>
#include "iomedley.h"


int iom_orders[3][3] = {
	{ 0,1,2 },
	{ 0,2,1 },
	{ 1,2,0 }
};

char *iom_EFORMAT2STR[] = {
	"(invalid)",
	"LSB BYTE",
	"LSB SHORT",
	"(invalid)",
	"LSB INT",
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"MSB BYTE",
	"MSB SHORT",
	"(invalid)",
	"MSB INT",	
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"IEEE FLOAT",
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"IEEE DOUBLE",	
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"VAX INTEGER",
	"(invalid)",
	"VAX FLOAT",
	"(invalid)",
	"(invalid)",
	"(invalid)",
	"VAX DOUBLE"
};

char *iom_FORMAT2STR[] = {
    0,
    "byte",
    "short",
    "int",
    "float",
    "double"
};
 
char *iom_ORG2STR[] = {
    "bsq",
    "bil",
    "bip"
};


FILE *iom_uncompress(FILE * fp, char *fname);
char *iom_expand_filename(char *s);
int iom_byte_swap_data(char *data, int dsize, iom_edf eformat);

/*
** SYNOPSIS:
**
** Some data formats have support for reading their headers.
** Files of these formats are loaded by calling LoadHeader()
** and then read_qube_data().
**
** Other data formats are supported through the separate
** ImageMagick library.
**
** Yet others are read through their own libraries,
** like HDF files.
**
** Yet others are read through their individual individual
** routines.
**
*/


void
iom_init_iheader(struct iom_iheader *h)
{
	int i;

	memset(h, 0, sizeof(struct iom_iheader));
    
    h->gain = 1.0;
    h->offset = 0.0;

	for (i = 0 ; i < 3 ; i++) {
		h->s_lo[i] = 0;
		h->s_hi[i] = 0;
		h->s_skip[i] = 1;
		h->prefix[i] = 0;
		h->suffix[i] = 0;
	}
}



/*
** detach_iheader_data()
**
** Remove the data associated with the _iheader structure (if any)
** and return it.
*/

void *
iom_detach_iheader_data(struct iom_iheader *h)
{
    void *data;

    data = h->data;
    h->data = NULL;
    
    return(data);
}


/*
** >> CAUTION << This will go away.
**
** supports_read_qube_data()
**
** Returns true if the header loaded suggests usage of
** read_qube_data() to read the data from the actual
** file.
*/
int
iom_supports_read_qube_data(struct iom_iheader *h)
{
    return (h->data == NULL);
}

/*
** cleanup_iheader()
**
** Cleans the memory allocated within the _iheader structure.
*/

void
iom_cleanup_iheader(struct iom_iheader *h)
{
    if (h->data){ free(h->data); }
    if (h->title){ free(h->title); }
}

/*
** iheaderDataSize()
**
** Calculates the size (in items) of data as the product
** of dimension-lengths in the _iheader structure
** and the size of each element.
**
** Byte-size can be calculated by multiplying the returned
** value with iom_NBYTESI(h->format).
*/
int
iom_iheaderDataSize(struct iom_iheader *h)
{
    int i;
    int dsize = 1;
    
    for(i = 0; i < 3; i++){
	    dsize *= h->size[i];
    }

    return (dsize * iom_NBYTESI(h->format));
}


int
iom_LoadHeader(
	FILE *fp,     /* File Pointer to "fname" or uncompressed "fname" */
	char *fname,  /* Original file name */
	struct iom_iheader *header  /* Header info is returned here */
)
{
	int success;  /* Flag indicating successful loading of header */

	iom_init_iheader(header); /* zero-out stuff */

	/*
	** NOTE: None of the routines below assume that the file
	**  pointer will be at the start of the file.
	*/

	success = 0;
	if (!success) success = iom_GetVicarHeader(fp, fname, header);
	if (!success) success = iom_GetISISHeader(fp, fname, header, NULL);
	if (!success) success = iom_GetAVIRISHeader(fp, fname, header);
	if (!success) success = iom_GetGRDHeader(fp, fname, header);
	if (!success) success = iom_GetGOESHeader(fp, fname, header);
	if (!success) success = iom_GetIMathHeader(fp, fname, header);
	if (!success) success = iom_GetENVIHeader(fp, fname, header);
	
    
	/*
	** We don't have header loaders any more, try loading the
	** image and gather header info from the loaded image.
	** The loaded image remains attached to the header now
	** until it is explicitly freed.
	*/
	if (!success) success = iom_GetPNMHeader(fp, fname, header);
#ifdef HAVE_LIBMAGICK
	if (!success) success = iom_GetGFXHeader(fname, header);
#endif /* HAVE_LIBMAGICK */

	return success;
}


void
iom_PrintImageHeader(
	FILE *stream,
	char *fname,
	struct iom_iheader *header
)
{
	fprintf(stream, "Image %s is %s, %s %dx%dx%d\n", 
			(fname == NULL ? "": fname),
			iom_Org2Str(header->org),
			iom_Format2Str(header->format),
			iom_GetSamples(header->size,header->org),
			iom_GetLines(header->size,header->org),
			iom_GetBands(header->size,header->org));
}



/*
** MergeHeaderAndSlice()
**
** Merges sub-selection (subsetting/slicing) info
** into the header. So that a succeeding read_qube_data()
** returns the subset data only.
*/

void
iom_MergeHeaderAndSlice(
	struct iom_iheader *h, /* header from the image file (h <- s) */
	struct iom_iheader *s  /* user specified slice */
	)
{
	int i, j;
	int org;
	
    if (s != NULL) {
        /** 
         ** Set subsets
         **/
        org = h->org;
		
        for (i = 0 ; i < 3 ; i++) {

            j = iom_orders[org][i];

            if (s->s_lo[j] > 0) h->s_lo[i] = s->s_lo[j];
            if (s->s_hi[j] > 0) h->s_hi[i] = s->s_hi[j];
            if (s->s_skip[j] > 0) h->s_skip[i] = s->s_skip[j];
            if (s->prefix[j] > 0) h->prefix[i] = s->prefix[j];
            if (s->suffix[j] > 0) h->suffix[i] = s->suffix[j];
        }
    }
}



/**
 ** read_qube_data() - generalized cube input routines
 **/

void *
iom_read_qube_data(int fd, struct iom_iheader *h)
{
    void *data;
    void *p_data;
    int dsize;
    int i, x, y, z;
    int nbytes;
    int dim[3];                 /* dimension of output data */
    int d[3];                   /* total dimension of file */
    int plane;
    int count;
    int err;
    off_t offset; /* offset into the data to start reading from */

    /**
     ** data name definitions:
     **
     ** size: size of cube in file
     ** dim:  size of output cube
     ** d:    size of physical file (includes prefix and suffix values)
     **/

    /**
     ** WARNING!!!
     **  if (prefix+suffix)%nbytes != 0, bad things could happen.
     **/

	/**
	 ** CAUTION
	 **  external and internal size of the data is assumed to be
	 **  the same.
	 **/
    nbytes = iom_NBYTES(h->eformat);

    /**
     ** Touch up some default values
     **/
    dsize = 1;
    for (i = 0; i < 3; i++) {
        if (h->size[i] == 0) h->size[i] = 1;
        if (h->s_lo[i] == 0) h->s_lo[i] = 1;
        if (h->s_hi[i] == 0) h->s_hi[i] = h->size[i];
	if (h->s_hi[i] > h->size[i]) h->s_hi[i]=h->size[i];
        if (h->s_skip[i] == 0) h->s_skip[i] = 1;  

        h->s_lo[i]--;           /* value is 1-N.  Switch to 0-(N-1) */
        h->s_hi[i]--;

        dim[i] = h->s_hi[i] - h->s_lo[i] + 1;
        h->dim[i] = dim[i];

        d[i] = h->size[i] * nbytes + h->suffix[i] + h->prefix[i];
        if (i && (h->suffix[i] + h->prefix[i]) % nbytes != 0) {
            fprintf(stderr, "Warning!  Prefix+suffix not divisible by pixel size\n");
        }
        dsize *= (dim[i] - 1) / h->s_skip[i] + 1; 
    }
    if (h->gain == 0.0)
        h->gain = 1.0;

    /**
     ** compute output size, allocate memory.
     **/

    if ((data = malloc(dsize * nbytes)) == NULL) {
        fprintf(stderr, "Unable to allocate %d bytes of memory.\n", dsize * nbytes);
        return (NULL);
    }
    /**
     ** Allocate some temporary storage space
     **/

    plane = d[0] * dim[1];  /*     line * N-samples */

    if ((p_data = malloc(plane)) == NULL) {
        free(data);
        return (NULL);
    }
    /**
     ** loop, doesn't do skips yet.
     **/

    count = 0;
    for (z = 0; z < dim[2]; z += h->s_skip[2]) {

        /**
         ** read part of an entire plane 
         **/

        /*........label.....plane.....offset............. */
        offset = h->dptr + (z + h->s_lo[2]) * 
			(d[0] * h->size[1] + h->size[0] * (h->prefix[1]+h->suffix[1]) + h->corner) +
            d[0] * h->s_lo[1];

        if (!h->data){
            /*
            ** If this file supports read_qube_data() then the image
            ** directly from the file.
            */
            
            lseek(fd, offset, 0);
            
            if ((err = read(fd, p_data, plane)) != plane) {
                fprintf(stderr, "Early EOF");
                break;
            }
        }
        else {
            /*
            ** Otherwise, get the data from iom_iheader->data.
            */
            if (h->data == NULL){ return NULL; }
            memcpy(p_data, (char *)((long)h->data + offset), plane);
        }


        for (y = 0; y < dim[1]; y += h->s_skip[1]) {

            /**
             ** find the line we are interested in 
             **/

            if (h->s_skip[0] == 1) {
                memcpy((char *) data + count,
                  (char *) p_data + (h->prefix[0] + h->s_lo[0] * nbytes + y * d[0]), 
                    dim[0] * nbytes);
                count += dim[0] * nbytes;
            } else {
                for (x = 0; x < dim[0]; x += h->s_skip[0]) {
                    memcpy((char *) data + count,
                     (char *) p_data + (h->prefix[0] + (h->s_lo[0] + x) * nbytes + y * d[0]), 
                           nbytes);
                    count += nbytes;
                }
            }
        }
        /* if (iom_VERBOSE > 1) */
            fprintf(stderr, ".");
    }

    /**
     ** do byte swap,
     ** find data limits, 
     ** apply multiplier.
     **/
    if (!h->data){
        /*
        ** If we were reading the data from the file then it may
        ** not be byte-swapped already. However, if we were
        ** reading it from the "data" portion of iheader then
        ** it is already byte-swapped.
        */
        h->format = iom_byte_swap_data(data, dsize, h->eformat);
    }

    free(p_data);
    return (data);
}


/*
** iom_byte_swap_data()
**
** If necessary, swap the data bytes to convert external data
** type to the native data type of the current machine.
**
** NOTE: No size adjustment is done here.
*/
int
iom_byte_swap_data(
	char     *data,    /* data to be modified/adjusted/swapped */
	int       dsize,   /* number of data elements */
	iom_edf  eformat  /* external format of the data */
)
{
	int i;
	int format = -1;  /* native format of data as derived from "eformat" */


	switch (eformat) {
	case iom_VAX_REAL_4:       /* VAX_FLOATs to IEEE REALs */
		for (i = 0 ; i < dsize ; i++) {
			iom_vax_ieee_r(&((float *)data)[i],&((float *)data)[i]);
		}
		format = iom_FLOAT;
		break;

	case iom_MSB_INT_1:        /* MSB-Bytes to Bytes */
		format = iom_BYTE;
		break;
	
	case iom_MSB_INT_2:        /* MSB-Shorts to Shorts */
		#ifdef _LITTLE_ENDIAN
		for (i = 0 ; i < dsize ; i++) { iom_MSB2(&((short *)data)[i]); }
		#endif /* _LITTLE_ENDIAN */
		format = iom_SHORT;
		break;
	
	case iom_MSB_INT_4:        /* MSB-Ints to Ints */
		#ifdef _LITTLE_ENDIAN
		for (i = 0 ; i < dsize ; i++) { iom_MSB4(&((int *)data)[i]); }
		#endif /* _LITTLE_ENDIAN */
		format = iom_INT;
		break;

	case iom_LSB_INT_1:        /* LSB-Bytes to Bytes */
		format = iom_BYTE;
		break;
	
	case iom_VAX_INT:          /* Vax Integers to Integers */
	case iom_LSB_INT_2:        /* LSB-Shorts to Shorts */
		#ifndef _LITTLE_ENDIAN
		for (i = 0 ; i < dsize ; i++) { iom_LSB2(&((short *)data)[i]); }
		#endif /* _LITTLE_ENDIAN */
		format = iom_SHORT;
		break;
	
	case iom_LSB_INT_4:        /* LSB-Ints to Ints */
		#ifndef _LITTLE_ENDIAN
		for (i = 0 ; i < dsize ; i++) { iom_LSB4(&((int *)data)[i]); }
		#endif /* _LITTLE_ENDIAN */
		format = iom_INT;
		break;

	case iom_IEEE_REAL_4:      /* IEEE-Floats to Floats */
      #ifdef _LITTLE_ENDIAN
         for (i = 0 ; i < dsize ; i++) { iom_MSB4(&((float *)data)[i]); }
      #endif
		format = iom_FLOAT;
		break;

	case iom_IEEE_REAL_8:      /* IEEE-Doubles to Doubles */
      #ifdef _LITTLE_ENDIAN
         for (i = 0 ; i < dsize ; i++) { iom_MSB8(&((double *)data)[i]); }
      #endif
		format = iom_DOUBLE;
		break;
	
	default:
		fprintf(stderr, "Unsupported format. See file %s line %d.\n",
			__FILE__, __LINE__);
		format = -1;
		break;
	}
	
	return format;
}


int
iom_Eformat2Iformat(iom_edf efmt)
{
    int ifmt = -1;

    switch(efmt){
    case iom_MSB_INT_1:
    case iom_LSB_INT_1:
        ifmt = iom_BYTE;
        break;

    case iom_MSB_INT_2:
    case iom_LSB_INT_2:
    case iom_VAX_INT:
        ifmt = iom_SHORT;
        break;

    case iom_MSB_INT_4:
    case iom_LSB_INT_4:
        ifmt = iom_INT;
        break;

    case iom_IEEE_REAL_4:
    case iom_VAX_REAL_4:
        ifmt = iom_FLOAT;
        break;

    case iom_IEEE_REAL_8:
    case iom_VAX_REAL_8:
        ifmt = iom_DOUBLE;
        break;
        
    }

    return ifmt;
}


#define PACK_MAGIC     "\037\036"       /* Magic header for packed files */
#define GZIP_MAGIC     "\037\213"       /* Magic header for gzip files, 1F 8B */
#define OLD_GZIP_MAGIC "\037\236"       /* Magic header for gzip 0.5 = freeze 1.x */
#define LZH_MAGIC      "\037\240"       /* Magic header for SCO LZH Compress files */
#define LZW_MAGIC      "\037\235"       /* Magic header for lzw files, 1F 9D */

int
iom_is_compressed(FILE * fp)
{
    char buf[8];

    fread(buf, 1, 2, fp);
    buf[2] = '\0';
    rewind(fp);

    return (!strcmp(buf, LZW_MAGIC) ||
            !strcmp(buf, GZIP_MAGIC) ||
            !strcmp(buf, LZH_MAGIC) ||
            !strcmp(buf, PACK_MAGIC) ||
            !strcmp(buf, OLD_GZIP_MAGIC));
}

FILE *
iom_uncompress(FILE * fp, char *fname)
{
    /**
     ** Try gzip first, then compress
     **/
    char buf[256];
    FILE *pfp;
    char *tptr;

    tptr = tempnam(NULL, NULL);
    sprintf(buf, "gzip -d < %s > %s ; echo 1", fname, tptr);

    /* if (iom_VERBOSE > 1) */
        fprintf(stderr, "Uncompressing %s\n", fname);

    if ((pfp = popen(buf, "r")) == NULL) {
        sprintf(buf, "compress -d < %s > %s ; echo 1", fname, tptr);
        if ((pfp = popen(buf, "r")) == NULL) {
            free(tptr);
            return (NULL);
        }
    }
    /** this should wait for the pfp to finish. **/
    while (1) {
		if (fgets(buf, 256, pfp) && !strncmp(buf, "1", 1)) {
			pclose(pfp);
			break;
		}
	}

    fclose(fp);
    fp = fopen(tptr, "r");

    unlink(tptr);
    free(tptr);

    return (fp);
}

char *
iom_get_env_var(char *name)
{
    char *value = NULL;

    /**
     ** Shell command line variables still have the '$' in front of them.
     ** Environment variables shouldn't.
     **/
    if ((value = getenv(name)) == NULL) {
        fprintf(stderr, "Environment variable not found: %s", name);
        return (NULL);
    }
    return (value);
}

/**
 ** Try to expand environment variables and ~
 ** puts answer back into argument.  Make sure its big enough...
 **/

char *
iom_expand_filename(char *s)
{
    char buf[1024];
    char ebuf[256];
    char *p, *q, *e;
    struct passwd *pwent;

    buf[0] = '\0';

    p = s;
    while (p && *p) {
        if (*p == '$') {        /* environment variable expansion */
            q = p + 1;
            while (*q && (isalnum(*q) || *q == '_')) {
                q++;
            }
            strncpy(ebuf, p + 1, q - p - 1);
            if ((e = iom_get_env_var(ebuf)) == NULL) {
                fprintf(stderr, "error: unknown environment variable: %s\n",
                        ebuf);
                return (NULL);
            } else {
                strcat(buf, e);
            }
            p = q;
        } else if (*p == '~' && p == s) { /* home directory expansion */
            q = p + 1;
            while (*q && (isalnum(*q) || *q == '_')) {
                q++;
            }
            if (q == p + 1) {   /* no username specified, use $HOME */
                strcat(buf, getenv("HOME"));
            } else {
                strncpy(ebuf, p + 1, q - p - 1);
                if ((pwent = getpwnam(ebuf)) == NULL) {
                    fprintf(stderr, "error: unknown user: %s\n", ebuf);
                    return (NULL);
                } else {
                    strcat(buf, pwent->pw_dir);
                }
            }
            p = q;
        } else {
            strncat(buf, p, 1);
            p++;
        }
    }
    strcpy(s, buf);
    return (s);
}


/*----------------------------------------------------------------------
  VAX real to IEEE real format conversion from VICAR RTL.
  ----------------------------------------------------------------------*/

void
iom_vax_ieee_r(float *from, float *to)
{
    unsigned int *j;
    unsigned int k0, k1;
    unsigned int sign, exp, mant;
    float *f;
    unsigned int k;

    j = (unsigned int *) from;

    /* take care of byte swapping */
    k0 = ((*j & 0xFF00FF00) >> 8) & 0x00FF00FF;
    k1 = ((*j & 0x00FF00FF) << 8) | k0;

    if (k1) {
        /* get sign bit */
        sign = k1 & 0x80000000;

        /* turn off sign bit */
        k1 = k1 ^ sign;

        if (k1 & 0x7E000000) {
            /* fix exponent by subtracting one, since
               IEEE uses an implicit 2^0 not an implicit 2^-1 as VAX does */
            k = (k1 - 0x01000000) | sign;
        } else {
            exp = k1 & 0x7F800000;
            mant = k1 & 0x007FFFFF;
            if (exp == 0x01000000) {
                /* fix for subnormal numbers */
                k = (mant | 0x00800000) >> 1 | sign;
            } else if (exp == 0x00800000) {
                /* fix for subnormal numbers */
                k = (mant | 0x00800000) >> 2 | sign;
            } else if (sign) {
                /* not a number */
                k = 0x7FFFFFFF;
            } else {
                /* zero */
                k = 0x00000000;
            }
        }
    } else {
        /* zero */
        k = 0x00000000;
    }
    f = (float *) (&k);
    *to = *f;
}

void
iom_long_byte_swap(unsigned char from[4], unsigned char to[4])
/* Types do not need to match */
{
    to[0] = from[1];
    to[1] = from[0];
    to[2] = from[3];
    to[3] = from[2];
}

/*----------------------------------------------------------------------
  IEEE real to VAX real format conversion from VICAR RTL.
  ----------------------------------------------------------------------*/
void
iom_ieee_vax_r(int *from, float *to)
{
    unsigned int sign, exponent;
    int f;
    float g;

    f = *from;
    exponent = f & 0x7f800000;
    if (exponent >= 0x7f000000) {   /*if too large */
        sign = f & 0x80000000;  /*sign bit */
        f = 0x7fffffff | sign;
    } else if (exponent == 0) { /*if 0 */
        f = 0;      /*vax 0 */
    } else {
        f += 0x01000000;    /*add 2 to exponent */
    }
    iom_long_byte_swap((unsigned char *)&f, (unsigned char *)&g);
    *to = g;
}

