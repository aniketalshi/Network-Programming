#ifndef __CONTROLS_H
#define __CONTROLS_H

#define "structs.h"

typedef struct window_pckt {
    unsigned long  time_st;     // Time Stamp
    uint32_t       seq_num;     // Seq num on pckt
    uint32_t       sent_cnt;    // cont of retransmits
    void*          body;        // body of packet
    msg_hdr_t*     header;      // header of packet
}wndw_pckt_t;

typedef struct sender_window {
    wndw_pckt_t *buff [SEND_WINDOW_SIZE]; // Circular buff
    int    win_head;                      // Head of buff
    int    win_tail;                      // Tail of buff 
    int    free_sz;                       // free slots avail
}snd_wndw_t;


/* window init */
void window_init (struct sender_window *snd_wndw);

/* to add a new pckt to window */
int add_window (struct sender_window *snd_wndw,
                struct window_pckt *pckt);

/* remove pckt from window */
int rem_window(struct sender_window *snd_wndw);

void sending_func (void *read_buf);

/* To get next seq num */
long int get_seq_num();

#endif /* __CONTROLS_H */
