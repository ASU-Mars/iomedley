#ifdef HAVE_LIBMAGICK
#include <stdio.h>
#include <string.h>
#include <magick.h>
#include <magick/api.h>
#include <magick/xwindows.h>
#include "iomedley.h"




/*
** GetGFXHeader()
**
** Loads image header from an ImageMagick file. Since, ImageMagick
** does not support extracting the header by itself, therefore, the
** actual image data is loaded and header filled using it.
**
** CAUTION: The image data remains loaded in _iheader.data field.
** It is the caller's responsibility to release this data using
** free() when no longer required.
*/

int
iom_GetGFXHeader(
    char *fname,
    struct iom_iheader *h
)
{
    int x, y, z;
	Image *image;
	ImageInfo image_info;

    
	GetImageInfo(&image_info);
	strcpy(image_info.filename,fname);
    
	image=ReadImage(&image_info);
    if (image == (Image *) NULL) return 0;
    
    iom_init_iheader(h);
    if (!iom_ExtractMiffData(image,
                         &h->size[0], &h->size[1], &h->size[2],
                         (void *)&h->data)){

        DestroyImage(image); 
        return 0;
    }

    h->format = iom_BYTE;
    h->org = iom_BSQ;

    /****************************************************************
    ** At this point h->data contains the data stored in the file. **
    *****************************************************************/
    
    return 1;
}


/*
** WriteGFXImage()
**
** Writes the byte-"data" of the specified dimensions to
** a file of type specified in "GFX_type.".
**
** The only data organization supported is BSQ.
**
** Parameter "filename" specifies the prefix of the output
** file's name.
*/
int
iom_WriteGFXImage(
    void *data,           /* input: image data              */
    int x, int y, int z,  /* input: image dimensions        */
    char *filename,       /* input: output file name prefix */
    char *GFX_type        /* input/output: GFX type         */
    )
{
	char *newfn;
    
	Image *image;
  	ImageInfo image_info;

    newfn=(char *)calloc(strlen(filename)+strlen(GFX_type)+1,sizeof(char));

	if(z>3 && (strcmp(GFX_type,"mpgc") && strcmp(GFX_type,"mpgg") && 
               strcmp(GFX_type,"gifc") && strcmp(GFX_type,"gifg"))){
		fprintf(stderr, "A movie type must be specified if > 3 bands\n");
        free(newfn);
		return 0;
	}

	if (z==2) {
		fprintf(stderr, "Incorrect number of bands %d ... aborting.\n", z);
        free(newfn);
		return 0;
	}

	if (!strcmp(GFX_type,"gifc"))
		strcpy(GFX_type,"gif");
	if (!strcmp(GFX_type,"gifg"))
		strcpy(GFX_type,"gif");
	if (!strcmp(GFX_type,"mpgc"))
		strcpy(GFX_type,"mpg");
	if (!strcmp(GFX_type,"mpgg"))
		strcpy(GFX_type,"mpg");
	
    /* <GFX_type:filename> will designate desired file type */
	strcpy(newfn,GFX_type);
	strcat(newfn,":");
	strcat(newfn,filename);

    if ((image=ToMiff(data, x, y, z)) == NULL){ free(newfn); return 0; }
	GetImageInfo(&image_info);
	strcpy(image->filename,newfn);

	WriteImage(&image_info,image);
    DestroyImage(image);
	free(newfn);

    return 1;
}



/*
** Image Reader
*/
int
iom_ExtractMiffData(
    Image *image,              /* input:  MIFF image          */
    int *ox, int *oy, int *oz, /* output: dimensions of image */
    void **image_data          /* output: actual image data   */
    )
{
	Image *tmp_image;
	ImageInfo image_info;
	ImageType it;
	int Gray=0;


	int layer2,layer3;

	int Frames=1;
	int Frame_Size=0;

	int x, y, i;

	char *data;

    
	GetImageInfo(&image_info);
    if (image == (Image *) NULL) return 0;

	it=GetImageType(image);
	x=image->columns;
	y=image->rows;
	if (it==GrayscaleType){
		Gray=1;
	}

	tmp_image=image;
	while (tmp_image->next!=NULL){
		Frames++;
		tmp_image=tmp_image->next;
	}

	if (Gray){
		data=(char *)calloc(Frames*x*y,sizeof(char));
		tmp_image=image;
		while (tmp_image!=NULL) {
			UncondenseImage(tmp_image);
			for (i=0;i<(x*y);i++){
				data[i+Frame_Size]=tmp_image->pixels[i].red;
			}
			Frame_Size+=(x*y);
			tmp_image=tmp_image->next;
		}
	}

	else{
		int total=x*y;
		int count=0;
		layer2=total;
		layer3=2*total;
		data=(char *)calloc(Frames*total*3,sizeof(char));
		tmp_image=image;
		while(tmp_image!=NULL){
			UncondenseImage(tmp_image);
			for (i=0;i<total;i++){
				data[i+Frame_Size]=tmp_image->pixels[i].red;
				data[i+layer2+Frame_Size]=tmp_image->pixels[i].green;
				data[i+layer3+Frame_Size]=tmp_image->pixels[i].blue;
			}
			Frame_Size+=(x*y*3);
			tmp_image=tmp_image->next;
		}
	}

    *ox = x;
    *oy = y;
    *oz = (Gray ? (Frames*1) : (Frames*3));
    *image_data = data;

    return 1;
}

/*
** ToMiff() - Miff Image Preparer
**
** Given byte-data in BSQ format, this routines generates
** a Miff Image out of it.
*/
Image *
ToMiff(
    char *data,             /* input: image data to write */
    int x, int y, int z     /* input: image dimensions    */
    )
{
	int i,j,k;
	Image *image,*tmp_image,*start_image;
	int layer2,layer3;

	int Frames=1;
	int Frame_Size=0;
	int First_Time=1;
	int Gray=0;
	int scene=0;
	QuantizeInfo quantize_info;
	ImageInfo	tmp_info,image_info;
	Display *display;
	unsigned long state;
  	XResourceInfo resource;
  	XrmDatabase resource_database;


	unsigned char r,g,b;



	if ((z % 3)){
		Frames=z;
		Gray=1;
	}
	else
		Frames = z/3;

	GetImageInfo(&image_info);

	while (Frames > 0) {
	  	image=AllocateImage(&image_info);

		if (image == (Image *) NULL){
			parse_error("Can't allocate memory for image write");
			return(NULL);
		}

		if (First_Time)
			start_image=image;


		image->columns=x;
  		image->rows=y;
 		image->packets=image->columns*image->rows;
		image->pixels=(RunlengthPacket *) malloc(image->packets*sizeof(RunlengthPacket));
  		if (image->pixels == (RunlengthPacket *) NULL) {
			parse_error("Can't allocate memory for image write");
      			DestroyImage(image);
			return (NULL);
		}
	
		layer2=x*y;
		layer3=2*x*y;
  		for (i=0; i < y ; i++) {
			for (j = 0; j < x; j++){
				if (Gray){
					r=g=b=data[i*x+j+Frame_Size];
				}
				else {
					r=data[i*x+j+Frame_Size];
					g=data[i*x+j+layer2+Frame_Size];
					b=data[i*x+j+layer3+Frame_Size];
				}
		    		image->pixels[i*x+j].red=r;
    				image->pixels[i*x+j].green=g;
   			 	image->pixels[i*x+j].blue=b;
  			  	image->pixels[i*x+j].index=0;
 			   	image->pixels[i*x+j].length=0;
			}
  		}
		Frame_Size+=((Gray) ? (x*y) : (3*x*y));
		Frames--;
		image->scene=scene++;
/* 		CondenseImage(image); */
		if (First_Time){
			tmp_image=image;
			First_Time=0;
		}

		else {
			tmp_image->next=image;
			image->previous=tmp_image;
			tmp_image=image;
		}

	}
	image=start_image;
    return(image);
}
#endif /* HAVE_LIBMAGICK */ 
