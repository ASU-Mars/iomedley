#include "io_envi.h"
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

extern int iom_orders[3][3];

void
Load_Data_Name(struct iom_iheader *h, char *fname)
{
	char *p;
	DIR *dirp;
	struct dirent *dp;

	
	p=strstr(fname,".");
	if (p==NULL) /*problem...*/
		return;

	p++;

	dirp = opendir(".");
	while ((dp = readdir(dirp)) != NULL){
		if (strncmp(dp->d_name,fname,(strlen(fname)-4))==0){
			if (strcmp(&dp->d_name[strlen(dp->d_name)-3],p)) {
				h->ddfname=strdup(dp->d_name);
				closedir(dirp);
				return;
			}
		}
	}

	fprintf(stderr,"%s the header file, couldn't find a data file with same name, different extension\n",
				fname);

	closedir(dirp);
}	



int
iom_isENVI(FILE *fp)
{
	char 	mcookie[256];

	rewind(fp);

	fgets(mcookie,256,fp);

	if (!(strncmp(mcookie,"ENVI",4)))
		return(1);
	else
		return(0);
}

int
Envi2iheader(ENVI_Header *e, struct iom_iheader *h)
{
	h->size[iom_orders[e->org][0]]=e->samples;
	h->size[iom_orders[e->org][1]]=e->lines;
	h->size[iom_orders[e->org][2]]=e->bands;
	h->dptr=e->header_offset;
	h->byte_order=1234;
	h->org=e->org;
	
	switch(e->data_type) {
		
		case 1:
			if (e->byte_order)
				h->eformat=iom_MSB_INT_1;
			else
				h->eformat=iom_LSB_INT_1;
			break;

		case 2:
		case 12:
			if (e->byte_order)
				h->eformat=iom_MSB_INT_2;
			else
				h->eformat=iom_LSB_INT_2;
			break;

		case 3:
		case 13:
			if (e->byte_order)
				h->eformat=iom_MSB_INT_4;
			else
				h->eformat=iom_LSB_INT_4;
			break;

		case 4:
			if (e->byte_order)
				h->eformat=iom_MSB_IEEE_REAL_4;
			else
				h->eformat=iom_LSB_IEEE_REAL_4;
			break;

		case 5:
			if (e->byte_order)
				h->eformat=iom_MSB_IEEE_REAL_8;
			else
				h->eformat=iom_LSB_IEEE_REAL_8;
			break;

		default:
			h->eformat=iom_EDF_INVALID;
			break;
	}
}


int
iom_GetENVIHeader(FILE *fp, char *fname, 
						struct iom_iheader *h )

{
	
	char label[256];
	char dummy[10];
	int	size;
	int	which_label;
	int	which_type;
	ENVI_Header	Eh;
	unsigned char *value_object=NULL;

	/* memset(h,0x0,sizeof(struct iom_iheader)); */
	iom_init_iheader(h);
	memset(&Eh,0x0,sizeof(ENVI_Header));

	if (!(iom_isENVI(fp)))
		return(0);

/* As we move through the header, keywords which aren't recognized should be
** skipped over.  Find_Label_and_Type will fail, and the flow will return to
** Read_Label which also fail.  Read_Label will contine to read the header, line by
** line until it encounters another label (as defined by <string> = ).  The process will
** repeat until the header is finished
*/

	while(!(feof(fp))){
		if (Read_Label(fp,label)){ /*Read in the label; if the return value is zero, EOF*/

			if(Find_Label_and_Type(label,&which_label,&which_type)) {

				value_object=(unsigned char *)Read_Value[which_type](fp,&size);

/*Added check for standard type*/

				if (!(strcmp(label,"file type"))){
					if (strcmp((char *)value_object,"ENVI Standard"))
						return(0);
				}
			
				else
					Assign_Value(which_label,(void *) value_object,size,&Eh);

				free(value_object);
			}
		}
	}

	/*Okay, so now we have the FullHeader loaded into Eh...now we
	** need to figure out the data file's filename...say that 3 times fast!*/
	
	Envi2iheader(&Eh,h);

	Load_Data_Name(h,fname);

	return(1);
}


int  	Read_Label(FILE *fp,char *label)
{
	int i=0;
	while ((label[i]=(char)fgetc(fp))!='=' && i< 256 && !(feof(fp)) ){
		if (label[i]=='\n' || label[i]=='\r') /*End of the line without an ='s means we're not at the start of a label*/
			break;
		i++;
	}

	if (feof(fp))
		return(0);

	if (label[i]=='\n' || label[i]=='\r')
		return(0);

	if (i>=256) /*This would mean a label that is greater than 255 character...no such thing*/
		return(0);

	/*Okay, now i is sitting on the index holding the =
		we want to back up to the nearest non-whitespace character*/

	i--; /*get off the = */

	while (label[i]==' ' && i > 0)
		i--;

	if (i==0) { /*This would be VERY bad*/
		fprintf(stderr,"I'm lost!!! Bailing!\n");
		exit(0);
	}

	/*Now i is the index of the first non-white space
		so we step ahead by one, and set it to NULL to
		cap the string*/

	i++;

	label[i]='\0';

	return(1);

}

int	Find_Label_and_Type(char *label,int *which_label, int *which_type)
{
	int i;

	for (i=0;i<HEADER_ENTRIES;i++){
		if (!(strcmp(label,Labels[i]))){
			*which_label=i; /*We now exactly which header entry*/
			*which_type=Types[i];/*And we know it's value type*/
			return(1);
		}
	}

/*Hmmmm, cound't find the label...
that's okay we'll just skip whatever it is we did find*/

	return(0);
}

void	Assign_Value(int which_label,void *valueObj, int size,ENVI_Header *Eh)
{
	int i;
	char *p;
	int q;


	switch(which_label) {

	case 0:
		Eh->samples=*((int *)valueObj);
		break;

	case 1:
		Eh->lines=*((int *)valueObj);
		break;

	case 2:
		Eh->bands=*((int *)valueObj);
		break;

	case 3:
		Eh->header_offset=*((int *)valueObj);
		break;

	case 4:
		Eh->data_type=*((int *)valueObj);
		break;

	case 5:
		if(!(strcmp((char *)valueObj,"bsq")))
			Eh->org=iom_BSQ;
		else if(!(strcmp((char *)valueObj,"bil")))
			Eh->org=iom_BIL;
		else
			Eh->org=iom_BIP;
		break;

	case 6:
		Eh->byte_order=*((int *)valueObj);
		break;

	}
}


void * Read_Int(FILE *fp,int *size)
{
	/*this should be easy..*/

	char in[256];
	int	value;
	void *valueobj;

	fgets(in,256,fp);
	value=atoi(in);

	*size=1;

	valueobj=malloc(sizeof(int));

	memcpy(valueobj,&value,sizeof(int));

	return (valueobj);
}

void * Read_Float(FILE *fp,int *size)
{

	/*this should be easy..*/

	char in[256];
	float	value;
	void *valueobj;

	fgets(in,256,fp);
	value=atof(in);

	*size=1;

	valueobj=malloc(sizeof(float));

	memcpy(valueobj,&value,sizeof(float));

	return (valueobj);
}

void * Read_String(FILE *fp,int *size)
{
	/*A little trickier....*/

	char input[257];
	int  nochar=0;
	void *valueobj;
	int suffix;
	char *prefix;
	int first=1;
	int ptr=0;
	

	*size=0;

	valueobj=malloc(256);

	while(!(nochar)){
		fgets(input,256,fp);
		suffix=0;
		prefix=input;

		/*Get rid of leading spaces in the first pass*/
		
		if (first) {
			while (prefix[ptr]==' ' && ptr < 256)
				ptr++;
			prefix=(prefix+ptr);
			first=0;
		}

/*Now we need to chop off the ending carridge return*/
		suffix=strlen(prefix)-1;

		while((prefix[suffix]=='\n' ||
				prefix[suffix]=='\r') && suffix > 0)
			suffix--;
		
		suffix++;
		prefix[suffix]='\0';
/*Done and Done*/

		nochar=strlen(prefix);
		memcpy((valueobj+(*size)),prefix,nochar);
		(*size)+=nochar;
		if (nochar >= 255) {/*Hmmm, could be more string*/
			nochar=0;
		}
	}
	(*size)++; /*one last space for the NULL char*/

	valueobj=realloc(valueobj,(*size));

	memset((valueobj+((*size)-1)),0x0,1); /*It doesn't get any nuller than this */

	*size=1; /*It's really just one string*/

	return(valueobj);
}
		
void * Read_Many_Ints(FILE *fp,int *size)
{
	/*Okay, we need to parse a bunch of ints seperated by ','s
	and stuff them in valueobj (as ints!)*/


	int value;
	void *valueobj;
	char string[15];
	int  current_size=256*sizeof(int);
	int i=0;
	char c;

	*size=0;
	valueobj=malloc(current_size);



	/*Find the start: */
	while (fgetc(fp)!='{')
		;

	/*Now keep reading until the end, marked by a '}' */
	while ((c=fgetc(fp))!='}'){

		/*The ',' is our delimiter */

		if (c==','){

			string[i]='\0';
			i=0;
			value=atoi(string);

			/*check to make sure we still have enough room; other-wise...resize! */

			if ((*size)+sizeof(int)>=current_size) { /*We're getting to big!!*/
				current_size*=2;
				valueobj=realloc(valueobj,current_size);
			}

			memcpy((valueobj+(*size)),&value,sizeof(int));
			(*size)+=sizeof(int);

		}

		/*Keep packing the string since we're not a dilimiter*/
		else {
			string[i]=c;
			i++;
		}
	}

	/*Okay, we hit the end, now we just need to stuff in the last number,
		resize the valueobj and return*/

	string[i]='\0';
	value=atoi(string);

	if ((*size)+sizeof(int)>=current_size) { /*We're getting to big!!*/
		current_size*=2;
		valueobj=realloc(valueobj,current_size);
	}

	memcpy((valueobj+(*size)),&value,sizeof(int));
	(*size)+=sizeof(int);

	valueobj=realloc(valueobj,(*size));

	while (fgetc(fp)!='\n') /*Chews up the rest of the line*/
		;

	(*size)/=sizeof(int); /*Now it's the number of int items*/

	return (valueobj);
}

void * Read_Many_Floats(FILE *fp,int *size)
{

	/*Okay, we need to parse a bunch of floats seperated by ','s
	and stuff them in valueobj (as floats!)*/


	float value;
	void *valueobj;
	char string[35];
	int  current_size=256*sizeof(float);
	int i=0;
	char c;

	*size=0;
	valueobj=malloc(current_size);



	/*Find the start: */
	while (fgetc(fp)!='{')
		;

	/*Now keep reading until the end, marked by a '}' */
	while ((c=fgetc(fp))!='}'){
		if (c==','){

			string[i]='\0';
			i=0;
			value=atof(string);

			if ((*size)+sizeof(float)>=current_size) { /*We're getting to big!!*/
				current_size*=2;
				valueobj=realloc(valueobj,current_size);
			}

			memcpy((valueobj+(*size)),&value,sizeof(float));
			(*size)+=sizeof(float);

		}

		else {
			string[i]=c;
			i++;
		}
	}

	/*Okay, we hit the end, now we just need to stuff in the last number,
		resize the valueobj and return*/

	string[i]='\0';
	value=atof(string);

	if ((*size)+sizeof(float)>=current_size) { /*We're getting to big!!*/
		current_size*=2;
		valueobj=realloc(valueobj,current_size);
	}

	memcpy((valueobj+(*size)),&value,sizeof(float));
	(*size)+=sizeof(float);

	valueobj=realloc(valueobj,(*size));

	while (fgetc(fp)!='\n') /*Chews up the rest of the line*/
		;

	(*size)/=sizeof(float); 

	return (valueobj);
}

void * Read_Many_Strings(FILE *fp,int *size)
{

	/*Okay, we need to parse a bunch of strings seperated by ','s
	and stuff them in valueobj (as null-terminated strings!)*/


	char string[1500];
	void *valueobj;
	int  current_size=1024; /*Eh, why not?*/
	int i=0;
	char c;
	int cnt=0;

	*size=0;
	valueobj=malloc(current_size);



	/*Find the start: */
	while (fgetc(fp)!='{')
		;


	/*Now keep reading until the end, marked by a '}' */
	while ((c=fgetc(fp))!='}'){
		if (c==','){

			string[i]='\0';
			i=0;
			cnt++;

			if ((*size)+strlen(string)>=current_size) { /*We're getting to big!!*/
				current_size*=2;
				valueobj=realloc(valueobj,current_size);
			}

			memcpy((valueobj+(*size)),string,strlen(string)+1);
			(*size)+=(strlen(string)+1);

		}

		else {
			/* This will skip leading spaces and return 
			**	chars which we don't want embedded in our strings!
			*/
			if ( !((i==0 && c==' ') || (c=='\n') || (c=='\r'))){ 
				/* Okay, no junk...keep it.*/
				string[i]=c;
				i++;
			}
		}
	}

	/*Okay, we hit the end, now we just need to stuff in the last string,
		resize the valueobj and return*/

	string[i]='\0';
	cnt++;
	if ((*size)+strlen(string)>=current_size) { /*We're getting to big!!*/
		current_size*=2;
		valueobj=realloc(valueobj,current_size);
	}

	memcpy((valueobj+(*size)),string,strlen(string)+1);
	(*size)+=(strlen(string)+1);

	valueobj=realloc(valueobj,(*size));

	while (fgetc(fp)!='\n') /*Chews up the rest of the line*/
		;

	*size=cnt; /*Set size to the number of NULL terminated strings in valueobj*/

	return (valueobj);
}
