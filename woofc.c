#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#include "log.h"
#include "woofc.h"

extern char WooF_dir[2048];
extern char Namelog_name[2048];
extern unsigned long Name_id;
extern LOG *Name_log;

int WooFCreate(char *name,
	       unsigned long element_size,
	       unsigned long history_size)
{
	WOOF_SHARED *wfs;
	MIO *mio;
	unsigned long space;
	char local_name[4096];
	char fname[20];

	/*
	 * each element gets a seq_no and log index so we can handle
	 * function cancel if we wrap
	 */
	space = ((history_size+1) * (element_size +sizeof(ELID))) + 
			sizeof(WOOF_SHARED);

	if(WooF_dir == NULL) {
		fprintf(stderr,"WooFCreate: must init system\n");
		fflush(stderr);
		exit(1);
	}

	memset(local_name,0,sizeof(local_name));
	strncpy(local_name,WooF_dir,sizeof(local_name));
	if(local_name[strlen(local_name)-1] != '/') {
		strncat(local_name,"/",1);
	}


	if(name != NULL) {
		strncat(local_name,name,sizeof(local_name));
	} else {
		sprintf(fname,"woof-%10.0",drand48()*399999999);
		strncat(local_name,fname,sizeof(fname));
	}
	mio = MIOOpen(local_name,"w+",space);
	if(mio == NULL) {
		return(-1);
	}
#ifdef DEBUG
	printf("WooFCreate: opened %s\n",local_name);
	fflush(stdout);
#endif

	wfs = (WOOF_SHARED *)MIOAddr(mio);
	memset(wfs,0,sizeof(WOOF_SHARED));

	if(name != NULL) {
		strncpy(wfs->filename,name,sizeof(wfs->filename));
	} else {
		strncpy(wfs->filename,fname,sizeof(wfs->filename));
	}

	wfs->history_size = history_size;
	wfs->element_size = element_size;
	wfs->seq_no = 1;

	InitSem(&wfs->mutex,1);
	InitSem(&wfs->tail_wait,history_size);

	MIOClose(mio);

	return(1);
}

WOOF *WooFOpen(char *name)
{
	WOOF *wf;
	WOOF_SHARED *wfs;
	MIO *mio;
	char local_name[4096];

	if(name == NULL) {
		return(NULL);
	}

	if(WooF_dir[0] == 0) {
		fprintf(stderr,"WooFOpen: must init system\n");
		fflush(stderr);
		exit(1);
	}

	memset(local_name,0,sizeof(local_name));
	strncpy(local_name,WooF_dir,sizeof(local_name));
	if(local_name[strlen(local_name)-1] != '/') {
		strncat(local_name,"/",1);
	}


	strncat(local_name,name,sizeof(local_name));
#ifdef DEBUG
	printf("WooFOpen: trying to open %s\n",local_name);
	fflush(stdout);
#endif
	mio = MIOReOpen(local_name);
	if(mio == NULL) {
		return(NULL);
	}
#ifdef DEBUG
	printf("WooFOpen: opened %s\n",local_name);
	fflush(stdout);
#endif

	wf = (WOOF *)malloc(sizeof(WOOF));
	if(wf == NULL) {
		MIOClose(mio);
		return(NULL);
	}
	memset(wf,0,sizeof(WOOF));

	wf->shared = (WOOF_SHARED *)MIOAddr(mio);
	wf->mio = mio;

	return(wf);
}

void WooFFree(WOOF *wf)
{
	MIOClose(wf->mio);
	free(wf);

	return;
}

int WooFAppend(WOOF *wf, char *hand_name, void *element)
{
	MIO *mio;
	MIO *lmio;
	WOOF_SHARED *wfs;
	char woof_name[2048];
	char log_name[4096];
	pthread_t tid;
	unsigned long next;
	unsigned char *buf;
	unsigned char *ptr;
	ELID *el_id;
	unsigned long seq_no;
	unsigned long ndx;
	int err;
	char launch_string[4096];
	char *namelog_seq_no;
	unsigned long my_log_seq_no;
	EVENT *ev;
	unsigned long ls;
#ifdef DEBUG
	printf("WooFAppend: called %s %s\n",wf->shared->filename,hand_name);
	fflush(stdout);
#endif

	wfs = wf->shared;

	/*
	 * if called from within a handler, env variable carries cause seq_no
	 * for logging
	 *
	 * when called for first time on namespace to start, should be NULL
	 */
	namelog_seq_no = getenv("WOOF_NAMELOG_SEQNO");
	if(namelog_seq_no != NULL) {
		my_log_seq_no = (unsigned long)atol(namelog_seq_no);
	} else {
		my_log_seq_no = 1;
	}

	if(hand_name != NULL) {
		ev = EventCreate(TRIGGER,Name_id);
		if(ev == NULL) {
			fprintf(stderr,"WooFAppend: couldn't create log event\n");
			exit(1);
		}
		ev->cause_host = Name_id;
		ev->cause_seq_no = my_log_seq_no;
	}

#ifdef DEBUG
	printf("WooFAppend: checking for empty slot\n");
	fflush(stdout);
#endif

	P(&wfs->tail_wait);

#ifdef DEBUG
	printf("WooFAppend: got empty slot\n");
	fflush(stdout);
#endif
	

#ifdef DEBUG
	printf("WooFAppend: adding element\n");
	fflush(stdout);
#endif
	/*
	 * now drop the element in the object
	 */
	P(&wfs->mutex);
#ifdef DEBUG
	printf("WooFAppend: in mutex\n");
	fflush(stdout);
#endif
	/*
	 * find the next spot
	 */
	next = (wfs->head + 1) % wfs->history_size;
	ndx = next;

	/*
	 * write the data and record the indices
	 */
	buf = (unsigned char *)(((void *)wfs) + sizeof(WOOF_SHARED));
	ptr = buf + (next * (wfs->element_size + sizeof(ELID)));
	el_id = (ELID *)(ptr + wfs->element_size);

	/*
	 * tail is the last valid place where data could go
	 * check to see if it is allocated to a function that
	 * has yet to complete
	 */
#ifdef DEBUG
	printf("WooFAppend: element in\n");
	fflush(stdout);
#endif


#if 0
	while((el_id->busy == 1) && (next == wfs->tail)) {
#ifdef DEBUG
	printf("WooFAppend: busy at %lu\n",next);
	fflush(stdout);
#endif
		V(&wfs->mutex);
		P(&wfs->tail_wait);
		P(&wfs->mutex);
		next = (wfs->head + 1) % wfs->history_size;
		ptr = buf + (next * (wfs->element_size + sizeof(ELID)));
		el_id = (ELID *)(ptr+wfs->element_size);
	}
#endif

	/*
	 * write the element
	 */
#ifdef DEBUG
	printf("WooFAppend: writing element 0x%x\n",el_id);
	fflush(stdout);
#endif
	memcpy(ptr,element,wfs->element_size);
	/*
	 * and elemant meta data after it
	 */
	el_id->seq_no = wfs->seq_no;
	el_id->busy = 1;

	/*
	 * update circular buffer
	 */
	ndx = wfs->head = next;
	if(next == wfs->tail) {
		wfs->tail = (wfs->tail + 1) % wfs->history_size;
	}
	seq_no = wfs->seq_no;
	wfs->seq_no++;
	V(&wfs->mutex);
#ifdef DEBUG
	printf("WooFAppend: out of element mutex\n");
	fflush(stdout);
#endif

	/*
	 * if there is no handler, we are done (no need to generate log record)
	 * However, since there is no handler, woofc-shperherd can't V the counting
	 * semaphore for the WooF.  Without a handler, the tail is immediately available since
	 * we don't know whether the WooF will be consumed -- V it in this case
	 */
	if(hand_name == NULL) {
		V(&wfs->tail_wait);
		return(1);
	}

	ev->woofc_ndx = ndx;
#ifdef DEBUG
	printf("WooFAppend: ndx: %lu\n",ev->woofc_ndx);
	fflush(stdout);
#endif
	ev->woofc_seq_no = seq_no;
#ifdef DEBUG
	printf("WooFAppend: seq_no: %lu\n",ev->woofc_seq_no);
	fflush(stdout);
#endif
	ev->woofc_element_size = wfs->element_size;
#ifdef DEBUG
	printf("WooFAppend: element_size %lu\n",ev->woofc_element_size);
	fflush(stdout);
#endif
	ev->woofc_history_size = wfs->history_size;
#ifdef DEBUG
	printf("WooFAppend: history_size %lu\n",ev->woofc_history_size);
	fflush(stdout);
#endif
	memset(ev->woofc_name,0,sizeof(ev->woofc_name));
	strncpy(ev->woofc_name,wfs->filename,sizeof(ev->woofc_name));
#ifdef DEBUG
	printf("WooFAppend: name %s\n",ev->woofc_name);
	fflush(stdout);
#endif
	memset(ev->woofc_handler,0,sizeof(ev->woofc_handler));
	strncpy(ev->woofc_handler,hand_name,sizeof(ev->woofc_handler));
#ifdef DEBUG
	printf("WooFAppend: handler %s\n",ev->woofc_handler);
	fflush(stdout);
#endif

	/*
	 * log the event so that it can be triggered
	 */
	memset(log_name,0,sizeof(log_name));
	sprintf(log_name,"%s/%s",WooF_dir,Namelog_name);
#ifdef DEBUG
	printf("WooFAppend: logging event to %s\n",log_name);
	fflush(stdout);
#endif
	ls = LogEvent(Name_log,ev);
	if(ls == 0) {
		fprintf(stderr,"WooFAppend: couldn't log event to log %s\n",
			log_name);
		fflush(stderr);
		return(-1);
	}

#ifdef DEBUG
	printf("WooFAppend: logged %lu for woof %s %s\n",
		ls,
		ev->woofc_name,
		ev->woofc_handler);
	fflush(stdout);
#endif

	EventFree(ev);
	V(&Name_log->tail_wait);
	return(1);
}
		
int WooFPut(char *wf_name, char *hand_name, void *element)
{
	WOOF *wf;
	char woof_name[2048];
	int err;

#ifdef DEBUG
	printf("WooFPut: called %s %s\n",wf_name,hand_name);
	fflush(stdout);
#endif

	if(WooF_dir[0] == 0) {
		fprintf(stderr,"WooFPut: must init system\n");
		fflush(stderr);
		return(-1);
	}
#ifdef DEBUG
	printf("WooFPut: WooF_dir: %s\n",WooF_dir);
	fflush(stdout);
#endif

	memset(woof_name,0,sizeof(woof_name));
	strncpy(woof_name,WooF_dir,sizeof(woof_name));
	strncat(woof_name,"/",sizeof(woof_name)-strlen("/"));
	strncat(woof_name,wf_name,sizeof(woof_name)-strlen(woof_name));

	wf = WooFOpen(wf_name);

	if(wf == NULL) {
		return(-1);
	}

#ifdef DEBUG
	printf("WooFPut: WooF %s open\n",wf_name);
	fflush(stdout);
#endif
	err = WooFAppend(wf,hand_name,element);

	WooFFree(wf);
	return(err);
}

int WooFGetTail(WOOF *wf, void *elements, int element_count)
{
	int i;
	unsigned long ndx;
	unsigned char *buf;
	unsigned char *ptr;
	unsigned char *lp;
	WOOF_SHARED *wfs;

	wfs = wf->shared;

	buf = (unsigned char *)(((void *)wfs) + sizeof(WOOF_SHARED));
	
	i = 0;
	lp = (unsigned char *)elements;
	P(&wfs->mutex);
		ndx = wfs->head;
		while(i < element_count) {
			ptr = buf + (ndx * (wfs->element_size + sizeof(ELID)));
			memcpy(lp,ptr,wfs->element_size);
			lp += wfs->element_size;
			i++;
			ndx = ndx - 1;
			if(ndx >= wfs->history_size) {
				ndx = 0;
			}
			if(ndx == wfs->tail) {
				break;
			}
		}
	V(&wfs->mutex);

	return(i);

}

int WooFGet(WOOF *wf, void *element, unsigned long ndx)
{
	unsigned char *buf;
	unsigned char *ptr;
	WOOF_SHARED *wfs;

	wfs = wf->shared;

	/*
	 * must be a valid index
	 */
	if(ndx >= wfs->history_size) {
		return(-1);
	}
	buf = (unsigned char *)(((void *)wfs) + sizeof(WOOF_SHARED));
	ptr = buf + (ndx * (wfs->element_size + sizeof(ELID)));
	memcpy(element,ptr,sizeof(wfs->element_size));
	return(1);
}

unsigned long WooFEarliest(WOOF *wf)
{
	unsigned long earliest;
	WOOF_SHARED *wfs;

	wfs = wf->shared;

	earliest = (wfs->tail + 1) % wfs->history_size;
	return(earliest);
}

unsigned long WooFLatest(WOOF *wf)
{
	return(wf->shared->head);
}

unsigned long WooFNext(WOOF *wf, unsigned long ndx)
{
	unsigned long next;

	next = (ndx + 1) % wf->shared->history_size;

	return(next);
}

unsigned long WooFBack(WOOF *wf, unsigned long elements)
{
	WOOF_SHARED *wfs = wf->shared;
	unsigned long remainder = elements % wfs->history_size;
	unsigned long new;
	unsigned long wrap;

	if(elements == 0) {
		return(wfs->head);
	}

	new = wfs->head - remainder;

	/*
	 * if we need to wrap around
	 */
	if(new >= wfs->history_size) {
		wrap = remainder - wfs->head;
		new = wfs->history_size - wrap;
	}

	return(new);
}
		

