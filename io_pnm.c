#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#endif /* _WIN32 */
#include <sys/types.h>
#include <string.h>
#include "iomedley.h"



static int ReadPNMHeader(FILE *fp, int *xout, int *yout, int *zout, int *bits, off_t *data_offset);
static int get_int(FILE *fp);
static int getbit(FILE *fp);

int
iom_isPNM(FILE *fp)
{
    char id, format;

    rewind(fp);
    
    id = fgetc(fp);
    format = fgetc(fp);

    if (id == 'P' && strchr("123456", format)) {
        return 1;
    }
    
    return 0;
}

int
iom_GetPNMHeader(
    FILE *fp,
    char *fname,
    struct iom_iheader *h
    )
{
    int x,y,z;
    int i;
    unsigned char *data = NULL;
    int bits;
	int offset;

    if (!iom_isPNM(fp)){ return 0; }

    /* offset = ftell(fp); */
    
    /**
    *** Get format
    **/
    if (iom_ReadPNM(fp, fname, &x, &y, &z, &bits, &data, &offset) == 0){
        return 0;
    }

	iom_init_iheader(h);

	/* h->dptr = offset; */

	if (z == 1) {
		h->size[0] = x;
		h->size[1] = y;
		h->size[2] = z;
		h->org = iom_BSQ;
	}
	else {
		h->size[0] = z;
		h->size[1] = x;
		h->size[2] = y;
		h->org = iom_BIP;
	}
	/* h->org = (z == 1 ? iom_BSQ : iom_BIP); */       /* data organization */
	/*
	iom_GetSamples(h->size, h->org) = x;
	iom_GetBands(h->size, h->org) = z;
	iom_GetLines(h->size, h->org) = y;
	*/

	if (bits == 16)  {
        h->eformat = iom_MSB_INT_2;
		h->format = iom_SHORT;
	} else {
        h->eformat = iom_MSB_INT_1;
		h->format = iom_BYTE;
	}

	/*
	h->offset = 0;
	h->gain = 0;
	h->prefix[0] = h->prefix[1] = h->prefix[2] = 0;
	h->suffix[0] = h->suffix[1] = h->suffix[2] = 0;
	*/

    h->data = data;

	return(1);
}

static int
ReadPNMHeader(FILE *fp, int *xout, int *yout, 
              int *zout, int *bits, off_t *data_offset)
{
    char id,format;
    int x,y,z,count;
    int bitshift;
    int maxval;
    unsigned char *data;

    /**
    *** Get format
    **/
    rewind(fp);

    id = fgetc(fp);
    format = fgetc(fp);

    if (id != 'P') {
        return(0);
    }
    if (format < '1' || format > '6') {
        return(0);
    }
    x = get_int(fp);
    y = get_int(fp);
    z = 1;

    *bits = 0;
    
    count = 0;
    switch (format) {
    case '1':                 /* plain pbm format */
        *bits = 1;
        break;
    case '2':                 /* plain pgm format */
        maxval = get_int(fp);
        break;
    case '3':                 /* plain ppm format */
        maxval = get_int(fp);
        z=3;
        *bits = 8;
        break;
    case '4':                 /* raw pbm format */
        *bits = 1;
        break;
    case '5':                 /* raw pgm format */
        maxval = get_int(fp);
        *bits = 8;
        break;
    case '6':                 /* raw ppm format */
        maxval = get_int(fp);
        z=3;
        *bits = 8;
        break;
    default:
        return(0);
    }

	if (ferror(fp)){
		fprintf(stderr, "Error while reading PNM header. Reason: %s.\n",
			strerror(errno));
		return 0;
	}

	*data_offset = ftell(fp);

    *xout = x;
    *yout = y;
    *zout = z;

    if (*bits == 0) {
		if (maxval < 256) {
			*bits = 8;
		} else {
			*bits = 16;
		}
	}

    return(1);
}

int
iom_WritePNM(
    char *fname,
    void *data,
    struct iom_iheader *h,
    int force_write
    )
{
    int   x, y, z;
    FILE *fp = NULL;

	if (h->format != iom_BYTE){
		fprintf(stderr, "Cannot write %s data in a PNM file.\n",
			iom_FORMAT2STR[h->format]);
		return 0;
	}

    x = iom_GetSamples(h->size, h->org);
    y = iom_GetLines(h->size, h->org);
    z = iom_GetBands(h->size, h->org);

    if (z != 1 && z != 3){
        fprintf(stderr, "Cannot write PNM files with depths other than 1 or 3.\n");
        fprintf(stderr, "See file %s  line %d.\n", __FILE__, __LINE__);
        return 0;
    }

	if (z == 3 && h->org != iom_BIP){
		fprintf(stderr, "For depth 3 data must be in BIP organization.");
		return 0;
	}
    
    if (!force_write && access(fname, F_OK) == 0){
        fprintf(stderr, "File %s already exists.\n", fname);
        return 0;
    }

    if ((fp = fopen(fname, "wb")) == NULL){
        fprintf(stderr, "Unable to write file %s. Reason: %s.\n",
                fname, strerror(errno));
        return 0;
    }
    
    fprintf(fp, "P%c\n%d %d\n255\n", (z == 1 ? '5' : '6'), x, y);

    /*
    ** Ordinarily we would be byte-swapping data here but there is no
    ** need to do so, since we will only be writing byte data.
    */
    fwrite(data, z, x*y, fp);
    
    if (ferror(fp)){
        fprintf(stderr, "Error writing to file %s. Reason: %s.\n",
                fname, strerror(errno));
        fclose(fp);
        unlink(fname);
        return 0;
    }

    fclose(fp);
    
    return 1;
}

int
iom_ReadPNM(FILE *fp, char *filename, int *xout, int *yout, 
            int *zout, int *bits, void **dout, off_t *data_offset)
{
    char id,format;
    int x,y,z,count;
    int i,j,k,d;
    int bitshift;
    int maxval;
    unsigned char *data;

    /**
    *** Get format
    **/
    rewind(fp);

    id = fgetc(fp);
    format = fgetc(fp);

    if (id != 'P') {
        return(0);
    }
    if (format < '1' || format > '6') {
        return(0);
    }
    x = get_int(fp);
    y = get_int(fp);
    z = 1;

	*data_offset = ftell(fp);

    count = 0;
    switch (format) {
    case '1':                 /* plain pbm format */
        data = (unsigned char *)calloc(1, x*y);
        for (i = 0 ; i < y ; i++) {
            for (j = 0 ; j < x ; j++) {
                data[count++] = getbit(fp);
            }
        }
        *bits = 1;
        break;
    case '2':                 /* plain pgm format */
        maxval = get_int(fp);
		*data_offset = ftell(fp);
        if (maxval == 255) {
            data = (unsigned char *)calloc(1, x*y);
            k = x*y;
            for (i = 0 ; i < k ; i++) {
                data[count++] = get_int(fp);
            }
            *bits = 8;
        } else if (maxval == 65535) {
            unsigned short *sdata = (unsigned short *)calloc(2, x*y);
            k = x*y;
            for (i = 0 ; i < k ; i++) {
                sdata[count++] = get_int(fp);
            }
            data = (unsigned char *)sdata;
            *bits = 16;
        } else {
            fprintf(stderr, "Unable to read pgm file %s.  Odd maxval.\n",
                    filename == NULL ? "(null)" : filename);
            return(0);
        }
        break;
    case '3':                 /* plain ppm format */
        maxval = get_int(fp);
		*data_offset = ftell(fp);
        data = (unsigned char *)calloc(3, x*y);
        for (i = 0 ; i < y ; i++) {
            for (j = 0 ; j < x ; j++) {
                data[count++] = get_int(fp);
                data[count++] = get_int(fp);
                data[count++] = get_int(fp);
            }
        }
        z=3;
        *bits = 8;
        break;
    case '4':                 /* raw pbm format */
        /**
        *** this code is roughly identical to the libpbm code.
        *** It does not appear to be correctly compatable with
        *** the code to convert raw to plain. Oh well.
        **/
        data = (unsigned char *)calloc(1, x*y);
        for (i = 0 ; i < y ; i++) {
            bitshift = -1;
            for (j = 0 ; j < x ; j++) {
                if (bitshift == -1) {
                    d = fgetc(fp);
                    bitshift = 7;
                }
                data[count++] = ((d >> bitshift) & 1);
                bitshift--;
            }
        }
        *bits = 1;
        break;
    case '5':                 /* raw pgm format */
        maxval = get_int(fp);
		*data_offset = ftell(fp);
        if (maxval <= 255) {
            data = (unsigned char *)calloc(1, x*y);
            fread(data, 1, x*y, fp);
            *bits = 8;
        } else if (maxval == 65535) {
            data = (unsigned char *)calloc(2, x*y);
            fread(data, 2, x*y, fp);
            *bits = 16;

            /*
            ** I am guessing byte-swapping may be required here.
            */
#ifndef WORDS_BIGENDIAN
            swab(data, data, 2*x*y);
#endif /* WORDS_BIGENDIAN */
            
        } else {
            fprintf(stderr, "Unable to read pgm file %s.  Odd maxval.\n",
                    filename == NULL ? "(null)" : filename);
            return(0);
        }
        break;
    case '6':                 /* raw ppm format */
        maxval = get_int(fp);
		*data_offset = ftell(fp);
        if (maxval > 255) {
            fprintf(stderr, "Unable to read ppm file %s with maxval > 255.\n",
                    filename == NULL ? "(null)" : filename);
            return(0);
        }
        data = (unsigned char *)calloc(3, x*y);
        fread(data, 3, x*y, fp);
        z=3;
        *bits = 8;
        break;
    default:
        return(0);
    }

    *xout = x;
    *yout = y;
    *zout = z;
    *dout = data;

    return(1);
}


/*
**                        >>>>>    C A U T I O N    <<<<<
**
**  T H I S    F U N C T I O N    M U S T    B E    R E M O V E D    F R O M    H E R E
*/
int
iom_LoadPNM(
    FILE *fp,
    char *fname,
    struct iom_iheader *h
    )
{
    int   x, y, z, bits;
    void *data;
	off_t offset;

    iom_init_iheader(h);
    
    if (!iom_isPNM(fp)){ return 0; }
    
    if (iom_ReadPNM(fp, fname, &x, &y, &z, &bits, &data, &offset) == 0){
        return 0;
    }

    h->data = data;

    if (bits == 16){ h->format = iom_SHORT; }
    else { h->format = iom_BYTE; }
    
    h->dim[0] = x;
    h->dim[1] = y;
    h->dim[2] = z;
	/* h->dptr = offset; */
    
    return 1;
}


static int
get_int(FILE *fp)
{
    int i;
    int c;

    while((c = getc(fp)) != EOF) {
        if (c == '#') {
            while(c != '\n')
                c = getc(fp);
        }
        if (c >= '0' && c <= '9') {
            i = 0;
            while (c >= '0' && c <= '9') {
                i = i*10 + (c - '0');
                c = getc(fp);
            }
            return(i);
        }
    }
    return(-1);
}

static int
getbit(FILE *fp)
{
    int c;

    while((c = getc(fp)) != EOF) {
        if (c == '#') {
            while(c != '\n') 
                c = getc(fp);
        }
        if (c == '0' || c == '1') {
            return(c-'0');
        }
    }
    return(-1);
}

