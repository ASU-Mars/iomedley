#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "iomedley.h"

/**
 ** These are fortran sequential unformatted records.
 **
 ** Each line is prefixed and suffixed with an int containing the
 ** size of the record, not including the prefix and suffix values.
 **/

struct GRD {
    char id[56];                /* Title */
    char pgm[8];                /* program that generated this file */
    int ncol, nrow, nz;         /* width, height, depth */
    float xo, dx, yo, dy;       /* cartesian location and scaling factors */
};

int
iom_isGRD(FILE *fp)
{
    char buf[256];
    int size,size2;

    /**
     ** Get record size.
     **/

    rewind(fp);
    fread(&size, sizeof(int), 1, fp);
#ifdef _LITTLE_ENDIAN
    iom_MSB4((char *)&size);
#endif /* _LITTLE_ENDIAN */
    
    if (size == 104 || size == 92) {
        /**
         ** read record, and get trailing record size
         **/
        fread(buf, size, 1, fp);
        fread(&size2, sizeof(int), 1, fp);
#ifdef _LITTLE_ENDIAN
        iom_MSB4((char *)&size2);
#endif /* _LITTLE_ENDIAN */
        
        if (size == size2) {
            return 1;
        }
    }
    return 0;
}

int
iom_GetGRDHeader(
    FILE *fp,
    char *filename,
    struct iom_iheader *h
    )
{
    struct GRD *grd;
    char buf[256];
    int size,size2;


    iom_init_iheader(h);

    if (!iom_isGRD(fp)){
      return 0;
    }
    
    rewind(fp);
    
    /* Get record size */
    fread(&size, sizeof(int), 1, fp);
#ifdef _LITTLE_ENDIAN
    iom_MSB4((char *)&size);
#endif /* _LITTLE_ENDIAN */
    
    fread(buf, size, 1, fp);
    
    grd = (struct GRD *)buf;
#ifdef _LITTLE_ENDIAN
    /*
    ** byte-swap the integers in the header
    ** ieee floats have the same format though
    */
    iom_MSB4((char *)&grd->ncol);
    iom_MSB4((char *)&grd->nrow);
    iom_MSB4((char *)&grd->nz);
#endif /* _LITTLE_ENDIAN */
    
    fprintf(stderr, "GRD file - ");
    fprintf(stderr, "Title: %s, program: %s\n", grd->id, grd->pgm);
    fprintf(stderr, "Size: %dx%d [%d]\n", grd->ncol, grd->nrow, grd->nz);
    fprintf(stderr, "X,Y: %f,%f\tdx,dy: %f,%f\n", 
            grd->xo,grd->yo,grd->dx,grd->dy);

    h->dptr = size+8;
    h->prefix[0]= 8;
    h->suffix[0]= 4;
    h->size[0] = grd->ncol;
    h->size[1] = grd->nrow;
    h->size[2] = grd->nz;
    h->eformat = iom_MSB_IEEE_REAL_4;
    h->format = iom_FLOAT;
    h->org = iom_BSQ;
    
    /* iom_iheader does not have h->title */
    /* h->title = strdup(grd->id); */

	return(1);
}


/*
** WriteGRD()
**
** Given data in BSQ format; write it to a GRD file in float format.
*/

int
iom_WriteGRD(
    char *fname,
    void *data,
    struct iom_iheader *h,
    int force_write,
    char *title,
    char *pgm
    )
{
    struct GRD *grd = NULL;
    char buf[256];
    int size;
    int x,y,z;
    float *fptr = (float *)data;
    int   *iptr = (int *)data;
    short *sptr = (short *)data;
    char  *bptr = (char *)data;
    double *dptr = (double *)data;
    int i, j;
    float f;
    FILE *fp = NULL;

    if (!force_write && access(fname, F_OK) == 0){
        fprintf(stderr, "File %s already exists.\n", fname);
        return 0;
    }

    if ((fp = fopen(fname, "wb")) == NULL){
        fprintf(stderr, "Unable to write file %s. Reason: %s.\n",
                fname, strerror(errno));
        return 0;
    }
    
    grd = (struct GRD *)buf;

    strcpy(grd->id, (title ? title : "default"));
    sprintf(grd->pgm, "%-8s", pgm);
    
    x = grd->ncol = iom_GetSamples(h->size, h->org);
    y = grd->nrow = iom_GetLines(h->size, h->org);
    z = grd->nz = iom_GetBands(h->size, h->org);
    
    grd->xo = grd->yo = 0.0;
    grd->dx = grd->dy = 1.0;

    if (z != 1) {
        fprintf(stderr, "Cannot write GRD files with more than 1 band.\n");
        fclose(fp);
        unlink(fname);
        return 0;
    }
    
    size = sizeof(struct GRD);
    
#ifdef _LITTLE_ENDIAN
    iom_LSB4((char *)&size);
    iom_LSB4((char *)&grd->ncol);
    iom_LSB4((char *)&grd->nrow);
    iom_LSB4((char *)&grd->nz);
#endif /* _LITTLE_ENDIAN */
    
    fwrite(&size, 1, sizeof(int), fp);
    fwrite(grd, 1, size, fp);
    fwrite(&size, 1, sizeof(int), fp);

    size = (x+1)*4;
    
#ifdef _LITTLE_ENDIAN
    iom_LSB4((char *)&size);
#endif /* _LITTLE_ENDIAN */


    for (i = 0 ; i < y ; i++) {
        fwrite(&size, 1, sizeof(int), fp);
        fwrite("\0\0\0\0", 1, 4, fp);
        for (j = 0 ; j < x ; j++) {
            switch(h->format){
            case iom_BYTE:   f = (float)bptr[j+i*x]; break;
            case iom_SHORT:  f = (float)sptr[j+i*x]; break;
            case iom_INT:    f = (float)iptr[j+i*x]; break;
            case iom_FLOAT:  f =        fptr[j+i*x]; break;
            case iom_DOUBLE: f = (float)dptr[j+i*x]; break;
            default:
                fprintf(stderr, "Unsupported internal file format for GRD file.\n");
                fprintf(stderr, "See file: %s  line: %d.\n", __FILE__, __LINE__);
				fclose(fp);
                return 0;
            }
#if _LITTLE_ENDIAN
            /* LSB -> MSB */
            iom_MSB4((char *)&f);
#endif /* _LITTLE_ENDIAN */
            fwrite(&f, 1, sizeof(float), fp);
        }
        fwrite(&size, 1, sizeof(int), fp);
    }

	if (ferror(fp)){
		fprintf(stderr, "Unable to write to %s. Reason: %s.\n",
			fname, strerror(errno));
		fclose(fp);
		unlink(fname);
		return 0;
	}

	fclose(fp);
    return 1;
}
