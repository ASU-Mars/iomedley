#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "iomedley.h"

/*
** Writes data into the specified file in the current
** machine's native format.
** Move this to iom_raw.c and fix code appropriately to
** write data for either-ended machine.
*/
int
iom_WriteRaw(
    char *fname,
    void *data,
    struct iom_iheader *h,
    int force_write
    )
{
    long item_ct_out, item_ct_in;
    FILE *fp = NULL;

    if (!force_write && access(fname, F_OK) == 0){
        fprintf(stderr, "File %s already exists.\n", fname);
        return 0;
    }

    if ((fp = fopen(fname, "wb")) == NULL){
        fprintf(stderr, "Unable to write %s. Reason: %s.\n",
                fname, strerror(errno));
        return 0;
    }

    item_ct_in = iom_iheaderDataSize(h);
    item_ct_out = fwrite(data, iom_NBYTESI(h->format), item_ct_in, fp);
    
    if (item_ct_in != item_ct_out){
        fprintf(stderr, "Failed to write to %s. Reason: %s.\n",
                fname, strerror(errno));
        fclose(fp);
        unlink(fname);
        return 0;
    }

    fclose(fp);
    
    return(1);
}

