#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iomedley.h"


/**
 ** These are fortran sequential unformatted records.
 **
 ** Each line is prefixed and suffixed with an int containing the
 ** size of the record, not including the prefix and suffix values.
 **/

#define BINARY_LABEL "vector_file"

int
iom_isIMath(FILE *fp)
{
    char buf[256];

    /**
    ** Get record size.
    **/

    rewind(fp);
    fread(buf, strlen(BINARY_LABEL), 1, fp);
	
    if (!strncasecmp(buf, BINARY_LABEL, strlen(BINARY_LABEL))) {
        return(1);
    }
    return 0;
}


int
iom_GetIMathHeader(
    FILE *fp,
    char *filename,
    struct iom_iheader *h
)
{
    char buf[256];
    int  wt, ht;
	int  transpose;

	if (!(iom_isIMath(fp)))
		return 0;

    rewind(fp);
    fread(buf, strlen(BINARY_LABEL)+1, 1, fp);
    fread(&wt, sizeof(int), 1, fp);
    fread(&ht, sizeof(int), 1, fp);

#ifdef _LITTLE_ENDIAN
    /* convert MSB -> LSB */

    iom_MSB4((char *)&wt);
    iom_MSB4((char *)&ht);
#endif

    transpose = (buf[0] == 'V');


    iom_init_iheader(h);

    h->dptr = strlen(BINARY_LABEL)+9;
    h->size[0] = ht;
    h->size[1] = wt;
    h->size[2] = 1;
    h->eformat = iom_IEEE_REAL_8;
    h->format = iom_DOUBLE;
    h->org = iom_BSQ;
    h->transposed = transpose;
    h->prefix[0] = h->prefix[1] = h->prefix[2] = 0;
    h->suffix[0] = h->suffix[1] = h->suffix[2] = 0;

    return 1;
}


int
iom_WriteIMath(
    FILE *fp,
    char *filename,
    void *data,
    struct iom_iheader *h
    )
{
    int i, j;
    double d;
	int height, width;

 
    /* Write header */
    fwrite(BINARY_LABEL, strlen(BINARY_LABEL)+1, 1, fp);

    /* Get image width and height */
    height = iom_GetLines(h->dim, h->org);
    width = iom_GetSamples(h->dim, h->org);

#ifdef _LITTLE_ENDIAN
    /* Convert LSB -> MSB */
    iom_LSB4((char *)&width);
    iom_LSB4((char *)&height);
#endif

    fwrite(&width, 4, 1, fp);
    fwrite(&height, 4, 1, fp);

    /* Write data. */
    for (i = 0 ; i < width ; i++) {
        for (j = 0 ; j < height ; j++) {
            /*
            ** IEEE reals are stored the same way in LSB & MSB
            ** architectures.
            */
            d = ((double *)data)[j*width+i];
            fwrite(&d, 8, 1, fp);
        }
    }

    return(1);
}
