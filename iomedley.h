#ifndef _IOMEDLEY_H_
#define _IOMEDLEY_H_

#include <stdio.h>
#include "io_lablib3.h"

/*
** Should be moved to io_magic.h and io_magic.h should be included here.
** iomedley.h should be removed from io_magic.c
*/

#ifdef HAVE_LIBMAGIC
#include "magick.h"
#endif

typedef enum {
	iom_EDF_INVALID = 0, /* Invalid external-format. */

	iom_LSB_INT_1   = 1,
	iom_LSB_INT_2   = 2,
	iom_LSB_INT_4   = 4,

	iom_MSB_INT_1   = 11,
	iom_MSB_INT_2   = 12,
	iom_MSB_INT_4   = 14,

	iom_IEEE_REAL_4 = 24,
	iom_IEEE_REAL_8 = 28,

	iom_VAX_INT     = 32,
	iom_VAX_REAL_4  = 34,
	iom_VAX_REAL_8  = 38
} iom_edf;             /* daVinci I/O external data formats */

#define iom_NBYTES(ef) ((ef) > 30 ? ((ef) - 30) : ((ef) > 20 ? ((ef) - 20) : ((ef) > 10 ? ((ef) - 10) : (ef))))


typedef enum {
	iom_BYTE   = 1,
	iom_SHORT  = 2,
	iom_INT    = 3,
	iom_FLOAT  = 4,
	iom_DOUBLE = 5
} iom_idf;             /* daVinci I/O internal data formats */

#define iom_NBYTESI(ifmt) ((ifmt) == 5 ? 8 : ((ifmt) == 3 ? 4 : (ifmt)))

/**
 ** Data axis order
 ** Var->value.Sym->order
 **
 ** !!! CAUTION: these values must be 0 based.  They are used as array
 **              indices below.
 **/
 
typedef enum {
	iom_BSQ = 0,
	iom_BIL = 1,
	iom_BIP = 2
} iom_order;

#define iom_EFormat2Str(i)  iom_EFORMAT2STR[(i)]
#define iom_Format2Str(i)   iom_FORMAT2STR[(i)]
#define iom_Org2Str(i)      iom_ORG2STR[(i)]

#define iom_GetSamples(s,org)   (s)[((org) == iom_BIP ? 1 : 0)]
#define iom_GetLines(s,org)     (s)[((org) == iom_BSQ ? 1 : 2)]
#define iom_GetBands(s,org)     (s)[((org) == iom_BIP ? 0 : ((org) == iom_BIL ? 1 : 2))]

extern int iom_orders[3][3];
extern char *iom_EFORMAT2STR[];
extern char *iom_FORMAT2STR[];
extern char *iom_ORG2STR[];
/* extern int iom_VERBOSE; */



struct iom_iheader {
    int dptr;			/* offset in bytes to first data value    */
    int prefix[3];		/* size of prefix data (bytes)            */
    int suffix[3];		/* size of suffix data (bytes)            */

    int size[3];		/* size of data (pixels)                  */

	/* Sub-select relavant.                                       */
    int s_lo[3];        /* subset lower range (pixels)            */
    int s_hi[3];		/* subset upper range (pixels)            */
    int s_skip[3];		/* subset skip interval (pixels)          */

	/* Set by read_qube_data() once the data read is successful.  */
	/* It is derived from sub-selects.                            */
    int dim[3];			/* final dimension size */

    int corner;          /* size of 1 whole plane */

    int byte_order;		/* byteorder of data                      */

	iom_edf eformat;   /* extrnal format of data                 */
						/*   -- comes from iom_edf enum above    */
                        /* this is what the file says it has      */

    int format;			/* data format (INT, FLOAT, etc)          */
						/*   -- comes from iom_idf enum above    */
                        /* this is what read_qube_data() will return */
    
	int transposed;     /* IMath data is transposed               */

    int org;			/* data organization                      */

    float gain, offset;	/* data multiplier and additive offset    */

	char *data;         /* non-NULL if all of the image is loaded */

    char *ddfname;      /* detached data-file name (if any)       */
                        /* see io_isis.c                          */

    char *title;        /* non-NULL if there is title             */
};




/* Stolen from vanilla/header.h */

static char iom_ctmp;
typedef char *iom_cptr;
#define iom_swp(c1, c2)	(iom_ctmp = (c1) , (c1) = (c2) , (c2) = iom_ctmp)

#ifdef _LITTLE_ENDIAN

#define iom_MSB8(s) 	(iom_swp(((iom_cptr)(s))[0], ((iom_cptr)(s))[7]), \
                         iom_swp(((iom_cptr)(s))[1], ((iom_cptr)(s))[6]), \
                         iom_swp(((iom_cptr)(s))[2], ((iom_cptr)(s))[5]), \
                         iom_swp(((iom_cptr)(s))[3], ((iom_cptr)(s))[4]),(s))

#define iom_MSB4(s) 	(iom_swp(((iom_cptr)(s))[0], ((iom_cptr)(s))[3]), \
                         iom_swp(((iom_cptr)(s))[1], ((iom_cptr)(s))[2]),(s))

#define iom_MSB2(s) 	(iom_swp(((iom_cptr)(s))[0], ((iom_cptr)(s))[1]),(s))

#define iom_LSB8(s) 	(s)
#define iom_LSB4(s) 	(s)
#define iom_LSB2(s) 	(s)

#else /* _BIG_ENDIAN */

#define iom_MSB8(s) 	(s)
#define iom_MSB4(s) 	(s)
#define iom_MSB2(s) 	(s)

#define iom_LSB8(s) 	(iom_swp(((iom_cptr)(s))[0], ((iom_cptr)(s))[7]), \
                         iom_swp(((iom_cptr)(s))[1], ((iom_cptr)(s))[6]), \
                         iom_swp(((iom_cptr)(s))[2], ((iom_cptr)(s))[5]), \
                         iom_swp(((iom_cptr)(s))[3], ((iom_cptr)(s))[4]),(s))

#define iom_LSB4(s) 	(iom_swp(((iom_cptr)(s))[0], ((iom_cptr)(s))[3]), \
                         iom_swp(((iom_cptr)(s))[1], ((iom_cptr)(s))[2]),(s))

#define iom_LSB2(s) 	(iom_swp(((iom_cptr)(s))[0], ((iom_cptr)(s))[1]),(s))

#endif



/*
** Returns data size in number of items. Item size depending upon
** the internal data format.
**
** Byte-size can be calculated by multiplying the returned
** value with iom_NBYTESI(h->format).
**
*/
int iom_iheaderDataSize(struct iom_iheader *h);



int iom_is_compressed(FILE * fp);
FILE *iom_uncompress(FILE * fp, char *fname);



int iom_isVicar(FILE *fp);
int iom_isIMath(FILE *fp);
int iom_isISIS(FILE *fp);
int iom_isGRD(FILE *fp);
int iom_isGOES(FILE *fp);
int iom_isAVARIS(FILE *fp);
int iom_isPNM(FILE *fp);


/*
** GetXXXXHeader() require that the input file is already
** opened and its file pointer passed to it. The file-name
** parameter is for information purposes only.
**
** GetISISHeader() can be passed r_obj as NULL.
**
** read_qube_data() is passed a (potentially subsetted)
** _iheader structure obtained from GetXXXXHeader().
**
** A pre-loaded header can be subsetted by issuing a
** MergeHeaderAndSlice() on it and the subset-defining
** _iheader structure.
*/

int iom_GetVicarHeader(FILE *fp, char *fname, struct iom_iheader *h);
int iom_GetIMathHeader(FILE *fp, char *fname, struct iom_iheader *h);
int iom_GetISISHeader(FILE *fp, char *fname, struct iom_iheader *h, OBJDESC **r_obj);
int iom_GetGRDHeader(FILE *fp, char *fname, struct iom_iheader *h);
int iom_GetGOESHeader(FILE *fp, char *fname, struct iom_iheader *h);
int iom_GetAVIRISHeader(FILE *fp, char *fnmae, struct iom_iheader *h);

/*
** The following functions do not support data-retrievel
** using read_qube_data(). LoadGFXHeader() doesn't even support
** loading of the header by itself either.
**
** Data for these files is loaded from the file while loading
** the header. It is available through the _iheader.data field.
** This data block is allocated using malloc(). It is the
** caller's responsiblity to free it when it is no longer in
** use any more.
**
*/

int iom_GetPNMHeader(FILE *fp, char *fname, struct iom_iheader *h);
int iom_GetGFXHeader(char *fname, struct iom_iheader *h);

/*
** Some functions that higher level routines may use.
*/
#ifdef HAVE_LIBMAGIC
Image *ToMiff(char *data, int x, int y, int z);
int iom_ExtractMiffData(Image *image, int *ox, int *oy, int *oz, void **image_data);
#endif

/*
** WriteXXXX() take an already opened output file's pointer.
** The file name is for informational purposes only.
**
** These routines take their image dimensions from iom_iheader.size[3].
** User iom_var2iheader() to construct a proper iom_iheader for the
** specified variable.
**
** These routines byte-swap the data as per the specified external
** format and the current machine's endian-inclination.
*/

int iom_WriteIMath(FILE *fp, char *fname, void *data, struct iom_iheader *h);
int iom_WriteISIS(FILE *fp, char *fname, void *data, struct iom_iheader *h, char *title);
int iom_WritePNM(FILE *fp, char *fname, void *data, struct iom_iheader *h);
int iom_WriteGFXImage(void *data, int x, int y, int z, char *filename, char *GFX_type);
int iom_WriteGRD(FILE *fp, char *fname, void *data, struct iom_iheader *h, char *title, char *pgm);
int iom_WriteVicar(FILE *fp, char *filename, void *data, struct iom_iheader *h);



/*
** Support Functions
*/

void iom_init_iheader(struct iom_iheader *h);

/*
** MergeHeaderAndSlice()
**
** Merges the user specified data cube selections in "s" into
** the header "h". The header "h" can be passed to functions
** like read_qube_data() to read a data slice.
*/
void iom_MergeHeaderAndSlice(struct iom_iheader *h, struct iom_iheader *s);


/*
** detach_iheader_data()
**
** Remove the data associated with the iom_iheader structure (if any)
** and return it.
*/
void *iom_detach_iheader_data(struct iom_iheader *h);

/*
** supports_read_qube_data()
**
** Returns true if the header loaded suggests usage of
** read_qube_data() to read the data from the actual
** file.
*/
int iom_supports_read_qube_data(struct iom_iheader *h);


/*
** cleanup_iheader()
**
** Cleans the memory allocated within the _iheader structure.
*/
void iom_cleanup_iheader(struct iom_iheader *h);

/*
** iheaderDataSize()
**
** Calculates the size (in bytes) of data as the product
** of non-zero dimension-lengths in the _iheader structure
** and the size of each element.
*/
int iom_iheaderDataSize(struct iom_iheader *h);

/*
** Prints the image header in the specified "stream" file.
** Note that "fname" is the name associated with the "header"
** and not the "stream."
*/
void iom_PrintImageHeader(FILE *stream, char *fname, struct iom_iheader *header);

/*
** read_qube_data()
**
** Actually reads the data from the specified file for which
** the header has already been loaded in "h". Use with only
** those formats which support read_qube_data().
**
*/
void *iom_read_qube_data(int fd, struct iom_iheader *h);

void *iom_ReadImageSlice(FILE *fp, char *fname, struct iom_iheader *slice);

/*
** Byte-swap the data stored in "data" of size "dsize" with
** external format "eformat" and return the resulting internal
** format of the data (as one of the values from iom_idf).
*/
int iom_byte_swap_data(
	char     *data,    /* data to be modified/adjusted/swapped */
	int       dsize,   /* number of data elements */
	iom_edf  eformat   /* external format of the data */
    );


char *iom_get_env_var(char *name);


/**
 ** Try to expand environment variables and ~
 ** puts answer back into argument.  Make sure its big enough...
 **/
char *iom_expand_filename(char *s);


/*----------------------------------------------------------------------
  VAX real to IEEE real format conversion from VICAR RTL.
  ----------------------------------------------------------------------*/
void iom_vax_ieee_r(float *from, float *to);

/*----------------------------------------------------------------------
  IEEE real to VAX real format conversion from VICAR RTL.
  ----------------------------------------------------------------------*/
void iom_ieee_vax_r(int *from, float *to);


void iom_long_byte_swap(unsigned char from[4], unsigned char to[4]);


int iom_Eformat2Iformat(iom_edf efmt);



#endif /* _IOMEDLEY_H_ */
