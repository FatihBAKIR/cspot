#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#include "log.h"
#include "woofc.h"

static char *WooF_dir;
static char Host_log_name[2048];
static unsigned long Host_id;
static LOG *Host_log;

static int WooFDone;

void *WooFLauncher(void *arg);

int WooFInit(unsigned long host_id)
{
	struct timeval tm;
	int err;
	char log_name[2048];
	char log_path[2048];
	pthread_t tid;

	gettimeofday(&tm,NULL);
	srand48(tm.tv_sec+tm.tv_usec);

	WooF_dir = getenv("WOOFC_DIR");
	if(WooF_dir == NULL) {
		WooF_dir = DEFAULT_WOOF_DIR;
	}

	if(strcmp(WooF_dir,"/") == 0) {
		fprintf(stderr,"WooFInit: WOOFC_DIR can't be %s\n",
				WooF_dir);
		exit(1);
	}

	err = mkdir(WooF_dir,0600);
	if((err < 0) && (errno != EEXIST)) {
		perror("WooFInit");
		return(-1);
	}

	sprintf(Host_log_name,"cspot-log.%10.0d",host_id);
	memset(log_name,0,sizeof(log_name));
	sprintf(log_name,"%s%s",WooF_dir,Host_log_name);

	Host_log = LogCreate(log_name,host_id,DEFAULT_WOOF_LOG_SIZE);

	if(Host_log == NULL) {
		fprintf(stderr,"WooFInit: couldn't open log as %s, size %d\n",log_name,DEFAULT_WOOF_LOG_SIZE);
		fflush(stderr);
		exit(1);
	}

	Host_id = host_id;

	err = pthread_create(&tid,NULL,WooFLauncher,NULL);
	if(err < 0) {
		fprintf(stderr,"couldn't start launcher thread\n");
		exit(1);
	}
	pthread_detach(tid);
	return(1);
}

void WooFExit()
{
	WooFDone = 1;
	pthread_exit(NULL);
}
	

WOOF *WooFCreate(char *name,
	       unsigned long element_size,
	       unsigned long history_size)
{
	WOOF *wf;
	MIO *mio;
	unsigned long space;
	char local_name[4096];
	char fname[20];

	/*
	 * each element gets a seq_no and log index so we can handle
	 * function cancel if we wrap
	 */
	space = ((history_size+1) * (element_size +sizeof(ELID))) + 
			sizeof(WOOF);

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
		return(NULL);
	}

	wf = (WOOF *)MIOAddr(mio);
	memset(wf,0,sizeof(WOOF));
	wf->mio = mio;

	if(name != NULL) {
		strncpy(wf->filename,name,sizeof(wf->filename));
	} else {
		strncpy(wf->filename,fname,sizeof(wf->filename));
	}

	memset(wf->handler_name,0,sizeof(wf->handler_name));
	strncpy(wf->handler_name,name,sizeof(wf->handler_name));
	strncat(wf->handler_name,".handler",sizeof(wf->handler_name)-strlen(wf->handler_name));
	wf->history_size = history_size;
	wf->element_size = element_size;
	wf->seq_no = 1;

	InitSem(&wf->mutex,1);
	InitSem(&wf->tail_wait,0);

	return(wf);
}

WOOF *WooFOpen(char *name, unsigned long element_size, unsigned long history_size)
{
	WOOF *wf;
	MIO *mio;
	unsigned long space;
	char local_name[4096];
	char fname[20];

	if(name == NULL) {
		return(NULL);
	}

	/*
	 * each element gets a seq_no and log index so we can handle
	 * function cancel if we wrap
	 */
	space = ((history_size+1) * (element_size +sizeof(ELID))) + 
			sizeof(WOOF);

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


	strncat(local_name,name,sizeof(local_name));
	mio = MIOOpen(local_name,"a+",space);
	if(mio == NULL) {
		return(NULL);
	}

	wf = (WOOF *)MIOAddr(mio);

	return(wf);
}

void WooFFree(WOOF *wf)
{
	MIOClose(wf->mio);

	return;
}

		
int WooFPut(WOOF *wf, void *element)
{
	pthread_t tid;
	unsigned long next;
	unsigned char *buf;
	unsigned char *ptr;
	ELID *el_id;
	unsigned long seq_no;
	unsigned long ndx;
	int err;
	char launch_string[4096];
	char *host_log_seq_no;
	unsigned long my_log_seq_no;
	EVENT *ev;
	unsigned long ls;

	host_log_seq_no = getenv("WOOF_HOST_LOG_SEQNO");
	if(host_log_seq_no == NULL) {
		my_log_seq_no = atol(host_log_seq_no);
	} else {
		my_log_seq_no = 1;
	}

	ev = EventCreate(TRIGGER,Host_id);
	if(ev == NULL) {
		fprintf(stderr,"WooFPut: couldn't create log event\n");
		exit(1);
	}
	ev->cause_host = Host_id;
	ev->cause_seq_no = my_log_seq_no;
	

	/*
	 * now drop the element in the object
	 */
	P(&wf->mutex);
	/*
	 * find the next spot
	 */
	next = (wf->head + 1) % wf->history_size;
	ndx = next;

	/*
	 * write the data and record the indices
	 */
	buf = (unsigned char *)(MIOAddr(wf->mio)+sizeof(WOOF));
	ptr = buf + (next * (wf->element_size + sizeof(ELID)));
	el_id = (ELID *)(ptr+wf->element_size);

	/*
	 * tail is the last valid place where data could go
	 * check to see if it is allocated to a function that
	 * has yet to complete
	 */
	while((el_id->busy == 1) && (next == wf->tail)) {
		V(&wf->mutex);
		P(&wf->tail_wait);
		P(&wf->mutex);
		next = (wf->head + 1) % wf->history_size;
		ptr = buf + (next * (wf->element_size + sizeof(ELID)));
		el_id = (ELID *)(ptr+wf->element_size);
	}

	/*
	 * write the element
	 */
	memcpy(ptr,element,wf->element_size);
	/*
	 * and elemant meta data after it
	 */
	el_id->seq_no = wf->seq_no;
	el_id->busy = 1;

	/*
	 * update circular buffer
	 */
	ndx = wf->head = next;
	if(next == wf->tail) {
		wf->tail = (wf->tail + 1) % wf->history_size;
	}
	seq_no = wf->seq_no;
	wf->seq_no++;
	V(&wf->mutex);

	ev->woofc_ndx = ndx;
	ev->woofc_seq_no = seq_no;
	ev->woofc_element_size = wf->element_size;
	ev->woofc_history_size = wf->history_size;
	memset(ev->woofc_name,0,sizeof(ev->woofc_name));
	strncpy(ev->woofc_name,wf->filename,sizeof(ev->woofc_name));

	/*
	 * log the event so that it can be triggered
	 */
	ls = LogEvent(Host_log,ev);
	if(ls == 0) {
		fprintf(stderr,"WooFPut: couldn't log event\n");
		exit(1);
	}

	EventFree(ev);
	V(&Host_log->tail_wait);
	return(1);
}

/*
 * FIX ME: it would be better if the TRIGGER seq_no gets retired after the launch is successful
 *         Right now, the last_seq_no in the launcher is set before the launch actually happens
 *         which means a failure will nt be retried
 */
void *WooFDockerThread(void *arg)
{
	char *launch_string = (char *)arg;

	system(launch_string);
	free(launch_string);

	return(NULL);
}

void *WooFLauncher(void *arg)
{
	unsigned long last_seq_no = 1;
	unsigned long first;
	LOG *log_tail;
	EVENT *ev;
	char *launch_string;
	char *pathp;
	WOOF *wf;
	char woof_shepherd_dir[2048];
	int err;
	pthread_t tid;

	/*
	 * wait for things to show up in the log
	 */

	while(WooFDone != 0) {
		P(&Host_log->tail_wait);

		/*
		 * must lock to extract tail
		 */
		P(&Host_log->mutex);
		log_tail = LogTail(Host_log,last_seq_no,Host_log->size);
		V(&Host_log->mutex);
		if(log_tail == NULL) {
			LogFree(log_tail);
			continue;
		}
		if(log_tail->head == log_tail->tail) {
			LogFree(log_tail);
			continue;
		}

		ev = (EVENT *)(MIOAddr(log_tail->m_buf) + sizeof(LOG));

		/*
		 * find the first TRIGGER we havn't seen yet
		 */
		first = log_tail->head;
		while(ev[first].type != TRIGGER) {
			first++;
			if(first == log_tail->tail) {
				break;
			}
		}  

		/*
		 * if no TRIGGERS found
		 */
		if(log_tail->tail == first) {
			LogFree(log_tail);
			continue;
		}
		/*
		 * otherwise, fire this event
		 */


		wf = WooFOpen(ev[first].woofc_name,
			      ev[first].woofc_element_size,
			      ev[first].woofc_history_size);

		if(wf == NULL) {
			fprintf(stderr,"WooFLauncher: open failed for WooF at %s, %lu %lu\n",
				ev[first].woofc_name,
				ev[first].woofc_element_size,
				ev[first].woofc_history_size);
			fflush(stderr);
			exit(1);
		}

		/*
		 * find the last directory in the path
		 */
		pathp = strrchr(WooF_dir,'/');
		if(pathp == NULL) {
			fprintf(stderr,"couldn't find leaf dir in %s\n",
				WooF_dir);
			exit(1);
		}

		strncpy(woof_shepherd_dir,pathp,sizeof(woof_shepherd_dir));

		launch_string = (char *)malloc(2048);
		if(launch_string == NULL) {
			exit(1);
		}

		memset(launch_string,0,2048);

		sprintf(launch_string, "docker run -it\
			 -e WOOF_SHEPHERD_DIR=%s\
			 -e WOOF_SHEPHERD_NAME=%s\
			 -e WOOF_SHEPHERD_SIZE=%lu\
			 -e WOOF_SHEPHERD_NDX=%lu\
			 -e WOOF_SHEPHERD_SEQNO=%lu\
			 -e WOOF_HOST_ID=%lu\
			 -e WOOF_HOST_LOG_NAME=%s\
			 -e WOOF_HOST_LOG_SIZE=%lu\
			 -e WOOF_HOST_LOG_SEQNO=%lu\
			 -v %s:%s\
			 centos:7\
			 %s",
				woof_shepherd_dir,
				wf->filename,
				wf->mio->size,
				ev[first].woofc_ndx,
				ev[first].woofc_seq_no,
				Host_id,
				Host_log->filename,
				Host_log->size,
				ev[first].seq_no,
				WooF_dir,pathp,
				wf->handler_name);

		/*
		 * remember its sequence number for next time
		 */
		last_seq_no = ev[first].seq_no; 		/* log seq_no */
		LogFree(log_tail);
		err = pthread_create(&tid,NULL,WooFDockerThread,(void *)launch_string);
		if(err < 0) {
			/* LOGGING
			 * log thread create failure here
			 */
			exit(1);
		}
		pthread_detach(tid);
	}

	pthread_exit(NULL);
}

	


	
