#include <stdio.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#endif /* _WIN32 */
#include <string.h>
#include "iomedley.h"
#include "io_lablib3.h"


iom_edf iom_ConvertISISType(char *type, char * bits, char *bytes);


int
iom_isISIS(FILE *fp)
{
    char        buf[256];

    rewind(fp);

    if (fgets(buf, 256, fp) == NULL) {
        return(0);
    }
    if (strncmp(buf, "CCSD", 4) &&
        strncmp(buf, "NJPL", 4) &&
        strncmp(buf,"PDS",3)) {
        return(0);
    }

    /**
     ** Best guess is yes, it is.
     **/
    return(1);
}



int
iom_GetISISHeader(
    FILE *fp, 
    char *filename, 
    struct iom_iheader *h,
    char *msg_file,     /* file to which the parse-messages should go */
    OBJDESC **r_obj     /* NULL allowed */
    )
{
    OBJDESC * ob, *qube, *image;
    KEYWORD * key, *key1, *key2;
    int rlen;
    int scope = ODL_THIS_OBJECT;
    char        *ptr, p1[16], p2[16],p3[16];
    int org, format, size[3];
    int suffix[3], suffix_size[3];
    int i;
    float       offset, gain;
    unsigned short start_type;
    unsigned long start;
    int suffix_bytes = 0;
    char *err_file = NULL;
    char *ddfname = NULL; /* Detached Data-File Name */

    suffix[0] = suffix[1] = suffix[2] = 0;
    suffix_size[0] = suffix_size[1] = suffix_size[2] = 0;

    if (!iom_isISIS(fp)){
        return 0;
    }
    
    /**
     ** Parse the label
     **/

    rewind(fp);
    /* memset(h, '\0', sizeof(struct _iheader)); */
    iom_init_iheader(h);

    /* err_file = msg_file; */
	if (iom_is_ok2print_details()){
		err_file = msg_file;
	}
	else {
#ifdef _WIN32
		err_file = "nul:";
#else
		err_file = "/dev/null";
#endif /* _WIN32 */
    }

    ob = (OBJDESC *)OdlParseLabelFptr(fp, err_file,
                                      ODL_EXPAND_STRUCTURE, 0);

    if (!ob || (key = OdlFindKwd(ob, "RECORD_BYTES", NULL, 0, 0)) == NULL) {
        OdlFreeTree(ob);
		if (iom_is_ok2print_errors()){
			fprintf(stderr, "%s is not a PDS file.", filename);
		}
        return(0);
    } else {
        rlen = atoi(key->value);
    }

    /**
     ** Check if this is an ISIS QUBE first.  If not, check for an IMAGE
     **/
    
    if ((qube = OdlFindObjDesc(ob, "QUBE", NULL, 0, 0, 0)) != NULL) {
        
        /**
         ** Get data organization
         **/
        
        org = -1;
        if ((key = OdlFindKwd(qube, "AXES_NAME", NULL, 0, scope)) ||
            (key = OdlFindKwd(qube, "AXIS_NAME", NULL, 0, scope))) {
            ptr = key->value;
            sscanf(ptr, " ( %[^,] , %[^,] , %[^)] ) ", p1, p2, p3);
            if (!strcmp(p1,"SAMPLE") && !strcmp(p2,"LINE") && !strcmp(p3,"BAND")) 
                org = iom_BSQ;
            else if (!strcmp(p1,"SAMPLE") && !strcmp(p2,"BAND") && !strcmp(p3,"LINE")) 
                org = iom_BIL;
            else if (!strcmp(p1,"BAND") && !strcmp(p2,"SAMPLE") && !strcmp(p3,"LINE")) 
                org = iom_BIP;
            else {
				if (iom_is_ok2print_unsupp_errors()){
					fprintf(stderr, "Unrecognized data organization: %s = %s",
							"AXIS_NAME", key->value);
				}
            }
        }
        
        /**
         ** Size of data
         **/
        
        if ((key = OdlFindKwd(qube, "CORE_ITEMS", NULL, 0, scope))) {
            sscanf(key->value, "(%d,%d,%d)", &size[0], &size[1], &size[2]);
        }
        
        /**
         ** Format
         **/
        
        key2 = OdlFindKwd(qube, "CORE_ITEM_BYTES", NULL, 0, scope);
        
        /**
         ** This tells us if we happen to be using float vs int
         **/
        
        key1 = OdlFindKwd(qube, "CORE_ITEM_TYPE", NULL, 0, scope);
        
        format = iom_ConvertISISType(key1 ? key1->value : NULL,
                                     NULL,
                                     key2 ? key2->value : NULL);

        if (format == iom_EDF_INVALID){
			if (iom_is_ok2print_unsupp_errors()){
				fprintf(stderr,
						"%s has unsupported/illegal SIZE+TYPE combination.\n",
						filename);
			}
            return 0;
        }
        
        if ((key = OdlFindKwd(qube, "CORE_BASE", NULL, 0, scope))) {
            offset = atof(key->value);
        }
        
        if ((key = OdlFindKwd(qube, "CORE_MULTIPLIER", NULL, 0, scope))) {
            gain = atof(key->value);
        }
        
        
        if ((key = OdlFindKwd(qube, "SUFFIX_ITEMS", NULL, 0, scope))) {
            sscanf(key->value, "(%d,%d,%d)", &suffix[0], &suffix[1], &suffix[2]);
        }
        if ((key = OdlFindKwd(qube, "SUFFIX_BYTES", NULL, 0, scope))) {
            suffix_bytes = atoi(key->value);
        }
        
        
        for (i = 0 ; i < 3 ; i++) {
            if (suffix[i]) {
                suffix_size[i] = suffix[i] * suffix_bytes;
            }
        }

		if (iom_is_ok2print_details()){
            fprintf(stderr, "ISIS Qube: ");
            fprintf(stderr, "%dx%dx%d %s %s\t\n",
                    iom_GetSamples(size, org),
                    iom_GetLines(size, org),
                    iom_GetBands(size, org),
                    iom_Org2Str(org), iom_EFormat2Str(format));
            
            /*************************************************************/
            /* THE FOLLOWING INFO MUST BE PLACED INTO THE HEADER SOMEHOW */
            /*************************************************************/
            
            for (i = 0 ; i < 3 ; i++) {
                if (suffix[i]) {
                    fprintf(stderr, "Suffix %d present, %d bytes\n", 
                            i, suffix_size[i]);
                }
            }
        }
        
        /**
         ** Cram everything into the appropriate header, and
         ** make sure we have the right data file.
         **/
        
        h->org = org;           /* data organization */
        h->size[0] = size[0];
        h->size[1] = size[1];
        h->size[2] = size[2];
        h->eformat = (iom_edf)format;
        h->offset = offset;
        h->gain = gain;
        h->prefix[0] = h->prefix[1] = h->prefix[2] = 0;
        h->suffix[0] = suffix_size[0];
        h->suffix[1] = suffix_size[1];
        h->suffix[2] = suffix_size[2];
        h->corner = suffix[0]*suffix[1]*suffix_bytes;
        
        if ((key = OdlFindKwd(ob, "^QUBE", NULL, 0, 0)) != NULL) {
            OdlGetFileName(key, &start, &start_type);
            if (start_type == ODL_RECORD_LOCATION) {
                start = (start-1)*rlen;
            }
            h->dptr = start;
        }
        if (r_obj != NULL) 
            *r_obj = ob;
        else 
            OdlFreeTree(ob);
        return(1);
    } else {
      if ((image = OdlFindObjDesc(ob, "IMAGE", NULL, 0, 0, 0)) != NULL) {
        
        if ((key = OdlFindKwd(image, "LINES", NULL, 0, scope))) {
          size[1] = atoi(key->value);
        }
        if ((key = OdlFindKwd(image, "LINE_SAMPLES", NULL, 0, scope))) {
          size[0] = atoi(key->value);
        }
        
        key2 = OdlFindKwd(image, "SAMPLE_BITS", NULL, 0, scope);
        key1 = OdlFindKwd(image, "SAMPLE_TYPE", NULL, 0, scope);
        
        format = iom_ConvertISISType( key1 ? key1->value : NULL,
                                      key2 ? key2->value : NULL, 
                                      NULL);
        
        if (format == iom_EDF_INVALID){
			if (iom_is_ok2print_unsupp_errors()){
				fprintf(stderr,
					  "%s has unsupported/illegal SIZE+TYPE combination.\n",
					  filename);
			}
          return 0;
	    }
        
        /**
         ** Load important IMAGE values
         **/
        if ((key = OdlFindKwd(ob, "^IMAGE", NULL, 0, 0)) != NULL) {
          
          /*
          ** If the actual image data is located in a detached
          ** data file, get that file name. Save this name in 
          ** the header for future retrieval of data.
          */
          
          ddfname = OdlGetFileName(key, &start, &start_type);
          ddfname = OdlGetFileSpec(ddfname, filename);
          
          if (ddfname) ddfname = strdup(ddfname);
          
          if (start_type == ODL_RECORD_LOCATION) {
            start = (start-1)*rlen;
            h->dptr = start;
          } 
        }
        if (r_obj != NULL) {
          *r_obj = ob;
        } else {
          OdlFreeTree(ob);
        }
        
        h->org = iom_BSQ;           /* data organization */
        h->size[0] = size[0];
        h->size[1] = size[1];
        h->size[2] = 0;
        h->eformat = (iom_edf)format;
        h->format = iom_Eformat2Iformat(h->eformat);
        h->offset = 0;
        h->gain = 0;
        h->prefix[0] = h->prefix[1] = h->prefix[2] = 0;
        h->suffix[0] = h->suffix[1] = h->suffix[2] = 0;
        h->ddfname = ddfname;
        return(1);
      }
    }
    return(0);
}


int
iom_WriteISIS(
    char *fname,
    void *data,
    struct iom_iheader *h,
    int force_write,
	char *title
    )
{
    int dsize;
    int fsize;
    char buf[1025];
    FILE *fp = NULL;

    if (!force_write && access(fname, F_OK) == 0){
		if (iom_is_ok2print_errors()){
			fprintf(stderr, "File %s exists already.\n");
		}
        return 0;
    }

    if ((fp = fopen(fname, "wb")) == NULL){
		if (iom_is_ok2print_sys_errors()){
			fprintf(stderr, "Unable to write file %s. Reason: %s.\n",
					fname, strerror(errno));
		}
        return 0;
    }
    
    dsize = iom_iheaderDataSize(h); /* >>> I GUESS <<< */
    fsize = dsize/512+1;

    /*
    ** FOLLOWING CODE NEEDS TO BE AUGMENTED TO INCLUDE FLOATING POINT
    ** DATA FILE I/O.
    */
    if (h->format > iom_INT) {
		if (iom_is_ok2print_unsupp_errors()){
			fprintf(stderr, "Unable to write ISIS files with %s format.\n",
					iom_Format2Str(h->format));
		}
        fclose(fp);
        unlink(fname);
        return(0);
    }

    sprintf(buf, "CCSD3ZF0000100000001NJPL3IF0PDS200000001 = SFDU_LABEL\r\n");
    sprintf(buf+strlen(buf), "RECORD_TYPE = FIXED_LENGTH\r\n");
    sprintf(buf+strlen(buf), "RECORD_BYTES = 512\r\n");
    sprintf(buf+strlen(buf), "FILE_RECORDS = %d\r\n",fsize+2);
    sprintf(buf+strlen(buf), "LABEL_RECORDS = 2\r\n");
    sprintf(buf+strlen(buf), "FILE_STATE = CLEAN\r\n");
    sprintf(buf+strlen(buf), "^QUBE = 3\r\n");
    sprintf(buf+strlen(buf), "OBJECT = QUBE\r\n");
    sprintf(buf+strlen(buf), "    AXES = 3\r\n");
    if (h->org == iom_BIL) {
        sprintf(buf+strlen(buf), "    AXES_NAME = (SAMPLE,BAND,LINE)\r\n");
    } else if (h->org == iom_BSQ) {
        sprintf(buf+strlen(buf), "    AXES_NAME = (SAMPLE,LINE,BAND)\r\n");
    } else {
        sprintf(buf+strlen(buf), "    AXES_NAME = (BAND,SAMPLE,LINE)\r\n");
    }
    sprintf(buf+strlen(buf), "    CORE_ITEMS = (%d,%d,%d)\r\n",
	    h->size[0], h->size[1], h->size[2]);
    sprintf(buf+strlen(buf), "    CORE_ITEM_BYTES = %d\r\n", 
            iom_NBYTESI(h->format));

    /* Always output native-endian data. */
#ifdef WORDS_BIGENDIAN
    sprintf(buf+strlen(buf), "    CORE_ITEM_TYPE = SUN_INTEGER\r\n");
#else
    sprintf(buf+strlen(buf), "    CORE_ITEM_TYPE = PC_INTEGER\r\n");
#endif /* WORDS_BIGENDIAN */
    
    sprintf(buf+strlen(buf), "    CORE_BASE = 0.0\r\n");
    sprintf(buf+strlen(buf), "    CORE_MULTIPLIER = 1.0\r\n");
    sprintf(buf+strlen(buf), "    CORE_NAME = RAW_DATA_NUMBERS\r\n");
    sprintf(buf+strlen(buf), "    CORE_UNIT = DN\r\n");
    sprintf(buf+strlen(buf), "    OBSERVATION_NOTE = \"%s\"\r\n",
            (title ? title : "BLANK"));
    sprintf(buf+strlen(buf), "END_OBJECT = QUBE\r\n");
    sprintf(buf+strlen(buf), "END\r\n");
    memset(buf+strlen(buf), ' ', 1024-strlen(buf));

    /* write header to file */
    fwrite(buf, 1, 1024, fp);

    /*
    ** Write data to file.
    ** We don't need to byte-swap data because it is written in
    ** the machine's native format.
    */
    fwrite(data, iom_NBYTESI(h->format), dsize, fp);

    if (ferror(fp)){
		if (iom_is_ok2print_sys_errors()){
			fprintf(stderr, "Error writing to file %s. Reason: %s.\n",
					fname, strerror(errno));
		}
        fclose(fp);
        unlink(fname);
        return 0;
    }
    
    fclose(fp);

    return(1); /* success */
}




/*
** ConvertISISType()
**
** Returns one of the formats given in dvio_edf as per
** the specified "type", "bits", and "bytes." If both
** "bits" and "bytes" are specified, "bits" value is
** given preference over the "bytes" value.
**
** Returns EDF_INVALID on invalid format.
*/
iom_edf
iom_ConvertISISType(char *type, char * bits, char *bytes)
{
    int item_bytes = 0;
    int format = iom_EDF_INVALID; /* Assume invalid format to start with */
	char *q;
    
    if (bits){
        switch(atoi(bits)){
        case 8: item_bytes = 1; break;
        case 16: item_bytes = 2; break;
        case 32: item_bytes = 4; break;
        }
    }
    else if (bytes){
        item_bytes = atoi(bytes);
    }
    
    q = type;
    
    if ((!strcmp(q, "INT")) || 
		(!strcmp(q,"UNSIGNED_INTEGER")) || 
		(!strcmp(q,"INTEGER"))){
        switch(item_bytes){
#ifdef WORDS_BIGENDIAN
        case 1: format = iom_MSB_INT_1; break;
        case 2: format = iom_MSB_INT_2; break;
        case 4: format = iom_MSB_INT_4; break;
#else /* little endian */
        case 1: format = iom_LSB_INT_1; break;
        case 2: format = iom_LSB_INT_2; break;
        case 4: format = iom_LSB_INT_4; break;
#endif /* WORDS_BIGENDIAN */
        }
    }
    else if (!strcmp(q, "SUN_INTEGER") ||
		(!strcmp(q,"MSB_UNSIGNED_INTEGER")) || 
		(!strcmp(q,"MSB_INTEGER"))) {
        switch(item_bytes){
        case 1: format = iom_MSB_INT_1; break;
        case 2: format = iom_MSB_INT_2; break;
        case 4: format = iom_MSB_INT_4; break;
        }
    }
    else if (!strcmp(q, "VAX_INT")){
        switch(item_bytes){
        case 1: format = iom_MSB_INT_1; break;
        case 2: format = iom_MSB_INT_2; break;
        case 4: format = iom_MSB_INT_4; break;
        }
    }
    else if (!strcmp(q, "PC_INTEGER") ||
		(!strcmp(q,"LSB_UNSIGNED_INTEGER")) || 
		(!strcmp(q,"LSB_INTEGER"))) {
        switch(item_bytes){
        case 1: format = iom_LSB_INT_1; break;
        case 2: format = iom_LSB_INT_2; break;
        case 4: format = iom_LSB_INT_4; break;
        }
    }
    else if (!strcmp(q, "REAL") || !strcmp(q, "IEEE_REAL")){
        switch(item_bytes){
#ifdef WORDS_BIGENDIAN
        case 4: format = iom_MSB_IEEE_REAL_4; break;
#else /* little endian */
        case 4: format = iom_LSB_IEEE_REAL_4; break;
#endif /* WORDS_BIGENDIAN */
        }
    }
    else if (!strcmp(q, "SUN_REAL")) {
        switch(item_bytes){
        case 4: format = iom_MSB_IEEE_REAL_4; break;
        }
    }
    else if (!strcmp(q, "PC_REAL")){
        switch(item_bytes){
        case 4: format = iom_LSB_IEEE_REAL_4; break;
        }
    }
    else if (!strcmp(q, "VAX_REAL")) {
        switch(item_bytes){
        case 4: format = iom_VAX_REAL_4; break;
        }
  }
    return(format);
}
