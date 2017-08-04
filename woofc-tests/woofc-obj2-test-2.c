#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "woofc.h"
#include "woofc-obj2.h"

#define ARGS "c:f:s:N:n:H:"
char *Usage = "woofc-obj2-test-2 -f filename\n\
\t-H namelog-path\n\
\t-s size (in events)\n\
\t-N first namespace-path\n\
\t-n second namespace-path\n";

char Fname[4096];
char Wname[4096];
char Wname2[4096];
char NameSpace[40967];
char NameSpace2[40967];
char Namelog_dir[4096];
int UseNameSpace;
char putbuf1[4096];
char putbuf2[4096];

int main(int argc, char **argv)
{
	int c;
	int size;
	int err;
	OBJ2_EL el;
	unsigned long seq_no;

	size = 5;
	UseNameSpace=0;

	while((c = getopt(argc,argv,ARGS)) != EOF) {
		switch(c) {
			case 'f':
				strncpy(Fname,optarg,sizeof(Fname));
				break;
			case 's':
				size = atoi(optarg);
				break;
			case 'N':
				UseNameSpace = 1;
				strncpy(NameSpace,optarg,sizeof(NameSpace));
				break;
			case 'n':
				UseNameSpace = 1;
				strncpy(NameSpace2,optarg,sizeof(NameSpace));
				break;
			case 'H':
				strncpy(Namelog_dir,optarg,sizeof(Namelog_dir));
				break;
			default:
				fprintf(stderr,
				"unrecognized command %c\n",(char)c);
				fprintf(stderr,"%s",Usage);
				exit(1);
		}
	}

	if(Fname[0] == 0) {
		fprintf(stderr,"must specify filename for object\n");
		fprintf(stderr,"%s",Usage);
		fflush(stderr);
		exit(1);
	}

	if((NameSpace[0] == 0) && 
	   (NameSpace2[0] != 0)) {
		fprintf(stderr,"must specify two name spaces or none\n");
		fprintf(stderr,"%s",Usage);
		exit(1);
	}

	if((NameSpace[0] != 0) && 
	   (NameSpace2[0] == 0)) {
		fprintf(stderr,"must specify two name spaces or none\n");
		fprintf(stderr,"%s",Usage);
		exit(1);
	}

	if(UseNameSpace == 1) {
		sprintf(putbuf1,"WOOFC_DIR=%s",NameSpace);
		putenv(putbuf1);
		sprintf(Wname,"woof://%s/%s",NameSpace,Fname);
		sprintf(Wname2,"woof://%s/%s",NameSpace2,Fname);
	} else {
		strncpy(Wname,Fname,sizeof(Wname));
	}

	if(Namelog_dir[0] != 0) {
		sprintf(putbuf2,"WOOF_NAMELOG_DIR=%s",Namelog_dir);
		putenv(putbuf2);
	}
		

	WooFInit();

	err = WooFCreate(Wname,sizeof(OBJ2_EL),size);

	if(err < 0) {
		fprintf(stderr,"couldn't create wf_1 from %s\n",Wname);
		fflush(stderr);
		exit(1);
	}

	memset(&el,0,sizeof(el));
	if(UseNameSpace == 1) {
		strncpy(el.next_woof,Wname,sizeof(el.next_woof));
		strncpy(el.next_woof2,Wname2,sizeof(el.next_woof2));
	} 

	el.counter = 0;
	seq_no = WooFPut(Wname,"woofc_obj2_handler_2",(void *)&el);

	if(WooFInvalid(seq_no)) {
		fprintf(stderr,"first WooFPut failed\n");
		fflush(stderr);
		exit(1);
	}

	pthread_exit(NULL);
	return(0);
}
