/*********************************** vicar.c **********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iomedley.h"

#include <pwd.h>
#include <sys/types.h>

#define VICAR_MAGIC		"LBLSIZE="

/**
 ** Vicar I/O routines
 **
 ** is_Vicar()  - detect VICAR magic cookie 
 ** GetVicarHeader() - read and parse a vicar header
 ** LoadVicar() - Load VICAR data file
 **/

/**
 ** This detects the magic cookie for vicar files.
 ** returns:
 **             0: on failure
 **             1: on success
 **/
int
iom_isVicar(FILE *fp)
{
    int len;
    char buf[16];

    rewind(fp);
    len = fread(buf, 1, strlen(VICAR_MAGIC), fp);
    return (len == strlen(VICAR_MAGIC) && !strncmp(buf, VICAR_MAGIC, len));
}


static char *
get_value(char *s1, char *s2)
{
    char *p;
    int len;

    len = strlen(s2);
    for (p = s1 ; p && *p ; p++) {
        if (!strncasecmp(p, s2, len)) {
            return(p+len);
        }
    }
    return(NULL);
}


typedef enum {
    VICAR_INTFMT_INVALID,    /* Invalid - Sentinal value */
    VICAR_INTFMT_LOW,        /* Little endian - default */
    VICAR_INTFMT_HIGH
} vic_ifmt;

typedef enum {
    VICAR_REALFMT_INVALID,   /* Invalid - Sentinal value */
    VICAR_REALFMT_VAX,       /* VAX format - default */
    VICAR_REALFMT_IEEE,      /* IEEE Real */
    VICAR_REALFMT_RIEEE      /* Reverse IEEE Real - UNIMPLEMENTED */
} vic_rfmt;


/**
 ** GetVicarHeader() - read and parse a vicar header
 **
 ** This routine returns 
 **         0 if the specified file is not a Vicar file.
 **         1 on success
 **/

int
iom_GetVicarHeader(FILE *fp, char *fname, struct iom_iheader *h)
{
    char *p, *q;
    int s = 0;
    int i;
    
    int org=-1;
    int format=-1;

    int size[3], suffix[3], prefix[3];
    int r;

    vic_ifmt intfmt;
    vic_rfmt realfmt;

    /**
     ** Read enough to get identifying label and total label size.
     ** Get total label size and mallocate enough space to hold it.
     **/

    rewind(fp);

    p = (char *)malloc(65);
    fread(p, 1, 64, fp);
    p[64] = '\0';
    sscanf(p, "LBLSIZE=%d", &s);
    if (s <= 0) {
        free(p);
        return(0);
    }

    p = (char *)realloc(p, s+1);

    /**
     ** Read entire label, and parse it.
     **/

    fread(p+64, 1, s-64, fp);
    p[s] = '\0';
    
    r = atoi(get_value(p, "RECSIZE="));

    size[0] =   atoi(get_value(p, "NS=")); /* width */
    size[1] =   atoi(get_value(p, "NL=")); /* height */
    size[2] =   atoi(get_value(p, "NB=")); /* depth */

    suffix[0] = suffix[1] = suffix[2] = 0;
    prefix[0] = prefix[1] = prefix[2] = 0;

    s += atoi(get_value(p, "NLB="))*r;
    prefix[0] = atoi(get_value(p, "NBB="));

    if ((q = get_value(p, "ORG=")) != NULL) {
        if (!strncmp(q, "'BIL'", 5)) org = iom_BIL;
        if (!strncmp(q, "'BSQ'", 5)) org = iom_BSQ;
        if (!strncmp(q, "'BIP'", 5)) org = iom_BIP;
    }
    if (org == -1) {
        fprintf(stderr, "%s has no org.", fname);
        free(p);
        return(0);
    }

    intfmt = VICAR_INTFMT_INVALID;
    if ((q = get_value(p, "INTFMT=")) != NULL){
        if (strncmp(q, "'LOW'", 5) == 0) intfmt = VICAR_INTFMT_LOW;
        else if (strncmp(q, "'HIGH'", 6) == 0) intfmt = VICAR_INTFMT_HIGH;
    }
    else {
        /* Default INTFMT=LOW */
        intfmt = VICAR_INTFMT_LOW;
    }
    if (intfmt == VICAR_INTFMT_INVALID){
        fprintf(stderr, "%s has unsupported/invalid INTFMT.\n", fname);
        free(p);
        return 0;
    }
    
    realfmt = VICAR_REALFMT_INVALID;
    if ((q = get_value(p, "REALFMT=")) != NULL){
        if (strncmp(q, "'VAX'", 5) == 0) realfmt = VICAR_REALFMT_VAX;
        else if (strncmp(q, "'IEEE'", 6) == 0) realfmt = VICAR_REALFMT_IEEE;
    }
    else {
        /* Default REALFMT=VAX */
        realfmt = VICAR_REALFMT_VAX;
    }
    if (realfmt == VICAR_REALFMT_INVALID){
        fprintf(stderr, "%s has unsupported/invalid REALFMT.\n", fname);
        free(p);
        return 0;
    }
    
    format = iom_EDF_INVALID;
    if ((q = get_value(p, "FORMAT=")) != NULL) {
        if (!strncmp(q, "'BYTE'", 6)){
            switch(intfmt){
            case VICAR_INTFMT_LOW: format = iom_LSB_INT_1; break;
            case VICAR_INTFMT_HIGH: format = iom_MSB_INT_1; break;
            }
        }
        else if (!strncmp(q, "'HALF'", 6) || !strncmp(q, "'WORD'", 6)){
            switch(intfmt){
            case VICAR_INTFMT_LOW: format = iom_LSB_INT_2; break;
            case VICAR_INTFMT_HIGH: format = iom_MSB_INT_2; break;
            }
        }
        else if (!strncmp(q, "'FULL'", 6) || !strncmp(q, "'LONG'", 6)){
            switch(intfmt){
            case VICAR_INTFMT_LOW: format = iom_LSB_INT_4; break;
            case VICAR_INTFMT_HIGH: format = iom_MSB_INT_4; break;
            }
        }
        else if (!strncmp(q, "'REAL'", 6)){
            switch(realfmt){
            case VICAR_REALFMT_VAX: format = iom_VAX_REAL_4; break;
            case VICAR_REALFMT_IEEE: format = iom_IEEE_REAL_4; break;
            }
        }
        else if (!strncmp(q, "'DOUB'", 6)){
            switch(realfmt){
            case VICAR_REALFMT_VAX: format = iom_VAX_REAL_8; break;
            case VICAR_REALFMT_IEEE: format = iom_IEEE_REAL_8; break;
            }
        }
        /* 'COMP'/'COMPLEX' - complex -- UNIMPLEMENTED */
        else format = iom_EDF_INVALID;
    }

    if (format == iom_EDF_INVALID) {
        fprintf(stderr, "%s has unsupported/invalid format.", fname);
        free(p);
        return(0);
    }

    /**
     ** Put together a proper structure from the read values.
     **/

    iom_init_iheader(h);

    h->dptr = s;
    h->byte_order = 4321;       /* can this be reliably determined? */
    h->eformat = (iom_edf)format;
    h->org = org;
    h->gain = 1.0;
    h->offset = 0.0;
    for (i = 0 ; i < 3 ; i++) {
        h->size[iom_orders[org][i]] = size[i];
        h->suffix[iom_orders[org][i]] = suffix[i];
        h->prefix[iom_orders[org][i]] = prefix[i];
    }

    free(p);
    return(1);
}


/*
** WriteVicar()
**
** Writes the given data into a vicar file. The data should
** be in the current machine's native-internal format.
**
** The output file pointer must point to an open file. The
** file name is used for reference purpose only.
**
** This operation is not desctructive on the input data.
*/

int
iom_WriteVicar(
    FILE *fp,              /* An already opened file pointer */
    char *filename,        /* File name for reference purpose */
    void *data,            /* The data to the written out - "native" format */
    struct iom_iheader *h  /* A header describing the data */
    )
{
    char ptr[4096];     
    int rec;
    int bands;
    int org;
    int dim;
    int len;
    time_t t = time(0);


    memset(ptr, ' ', sizeof(ptr));

    org = h->org;
    bands = iom_GetBands(h->size, org);
    dim = 3;

    /**
     ** If no depth, force BSQ org.
     **/

    if (bands == 1) {
        org = iom_BSQ;
        dim = 2;
    }

    rec = h->size[0] * iom_NBYTESI(h->format);

#ifdef _LITTLE_ENDIAN
    /*
    ** Write low-endian output only.
    ** This ensures no output endian-translation.
    */
    
    sprintf(ptr+strlen(ptr), "HOST='PC'  INTFMT='LOW'  ");
#else
    /*
    ** Write high-endian output.
    ** This ensures no output endian-translation.
    */
    
    sprintf(ptr+strlen(ptr), "HOST='SUN-SOLR'  INTFMT='HIGH' ");
#endif /* _LITTLE_ENDIAN */

    /*
    ** Always write IEEE-floats.
    */
    sprintf(ptr+strlen(ptr), "REALFMT='IEEE'  ");
    
    switch(h->format) {
    case iom_BYTE:  sprintf(ptr+strlen(ptr), "FORMAT='BYTE'  "); break;
    case iom_SHORT: sprintf(ptr+strlen(ptr), "FORMAT='HALF'  "); break;
    case iom_INT:   sprintf(ptr+strlen(ptr), "FORMAT='FULL'  "); break;
    case iom_FLOAT: sprintf(ptr+strlen(ptr), "FORMAT='REAL'  "); break;
    default:
        fprintf(stderr, "VICAR files support bytes, shorts, ints, and floats only.");
        return 0;
        break;
    }
    
    sprintf(ptr+strlen(ptr),
            "TYPE='IMAGE'  BUFSIZ=%d  DIM=%d  EOL=0  RECSIZE=%d  ",
            24576, dim, rec);


    switch(h->org) {
    case iom_BIL: sprintf(ptr+strlen(ptr), "ORG='BIL'  "); break;
    case iom_BIP: sprintf(ptr+strlen(ptr), "ORG='BIP'  "); break;
    case iom_BSQ: sprintf(ptr+strlen(ptr), "ORG='BSQ'  "); break;
    default:
        fprintf(stderr,"VICAR files support BIL, BIP, & BSQ organzations only.");
        return 0;
        break;
    }

    sprintf(ptr+strlen(ptr), "NL=%d  NS=%d  NB=%d  ",
            iom_GetLines(h->size, h->org),
            iom_GetSamples(h->size, h->org),
            iom_GetBands(h->size, h->org));

    if (org == iom_BSQ) {           /* done cause we may have forced bsq */
        sprintf(ptr+strlen(ptr), "N1=%d  N2=%d  N3=%d  ",
                iom_GetLines(h->size, h->org),
                iom_GetSamples(h->size, h->org),
                iom_GetBands(h->size, h->org));
    } else {
        sprintf(ptr+strlen(ptr), "N1=%d  N2=%d  N3=%d  ",
                h->size[0],
                h->size[1],
                h->size[2]);
    }

    sprintf(ptr+strlen(ptr), "N4=0  NBB=0  NLB=0  ");

    /*
    ** BHOST, BINFMT, BREALFMT, BLTYPE and TASK are not used by
    ** us but they are required by the standard.
    */
    
#ifdef _LITTLE_ENDIAN
    sprintf(ptr+strlen(ptr), "BHOST='PC'  BINFMT='LOW'  ");
#else
    sprintf(ptr+strlen(ptr), "BHOST='SUN-SOLR'  BINTFMT='HIGH'  ");
#endif /* _LITTLE_ENDIAN */

    sprintf(ptr+strlen(ptr), "BREALFMT='IEEE'  BLTYPE=''  TASK='IOMEDLEY'  ");
    
#ifdef _WIN32
    sprintf(ptr+strlen(ptr), "USER='%s'  ", "MSDOS");
#else
    sprintf(ptr+strlen(ptr), "USER='%s'  ", getpwuid(getuid())->pw_name);
#endif

    sprintf(ptr+strlen(ptr), "DAT_TIM='%24.24s'  ", ctime(&t));
    
    /**
     ** compute final label size and stuff it in at the front.
     **/
    len = (((strlen(ptr) + 24) / rec)+1) * rec;

#if 0
    if (VERBOSE > 1) {
        fprintf(stderr, "Writing %s: VICAR %s %dx%dx%d %d bit IMAGE\n",
                filename,
                iom_Org2Str(org), 
                iom_GetSamples(h->size, h->org),
                iom_GetLines(h->size, h->org),
                iom_GetBands(h->size, h->org),
                iom_NBYTESI(h->format) * 8);
        fflush(stderr);
    }
#endif 

    fprintf(fp, "LBLSIZE=%-5d           ",len);
    fwrite(ptr, strlen(ptr), 1, fp);
    fprintf(fp, "%*s", len-strlen(ptr)-24-2, " ");
	{
        /* zero does not change form in any-endian machine */
		int i = 0;
		fwrite(&i, 1, sizeof(int), fp);
	}
    
    /*
    ** Write Data
    **
    ** This works here because we never write non-native data.
    ** i.e. we never write big-endian data on a little machine
    ** and vice-versa.
    **
    */
    fwrite(data, iom_NBYTESI(h->format), h->size[0]*h->size[1]*h->size[2], fp);

    return(1);
}
