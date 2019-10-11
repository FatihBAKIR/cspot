#ifndef LINK_H
#define LINK_H

struct Link{

    unsigned long dw_seq_no;
    unsigned long lw_seq_no;
    char type;
    unsigned long version_stamp;

};

typedef struct Link LINK;

void LINK_display(LINK link);

#endif