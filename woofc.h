#ifndef WOOFC
#define WOOFC

#include "mio.h"
#include "lsema.h"
#include "log.h"

struct woof_stc
{
	char filename[2048];
	char handler_name[2048];
	MIO *mio;
	sema mutex;
	sema tail_wait;
	unsigned long long seq_no;
	unsigned long history_size;
	unsigned long head;
	unsigned long tail;
	unsigned long element_size;
};

typedef struct woof_stc WOOF;

struct element_stc
{
	unsigned long busy;
	unsigned long long seq_no;
};

typedef struct element_stc ELID;

WOOF *WooFCreate(char *name,
               unsigned long element_size,
               unsigned long history_size);

WOOF *WooFOpen(char *name,
               unsigned long element_size,
               unsigned long history_size);

int WooFPut(WOOF *wf, void *element);

void WooFExit();

#define DEFAULT_WOOF_DIR "cspot"
#define DEFAULT_WOOF_LOG_SIZE (10000)

#endif

