#ifndef __CONTROLS_H
#define __CONTROLS_H
#include "structs.h"

#define SEND_WINDOW_SIZE        10
#define RECV_WINDOW_SIZE        10
#define MAX_ACK_CNT             3

enum TYPE {SEND_BUF, RECV_BUF};
extern int      seed_val;
extern float    prob_loss;

/* paratmeters received from server.in */
extern int      max_sending_win_size;

typedef struct window_pckt {
    unsigned long  time_st;     // Time Stamp
    uint32_t       seq_num;     // Seq num on pckt
    uint32_t       sent_cnt;    // cont of retransmits
    void*          body;        // body of packet
    uint32_t       data_len;    // lenght of body
    msg_hdr_t*     header;      // header of packet
}wndw_pckt_t;

typedef struct sender_window {
    wndw_pckt_t *buff [SEND_WINDOW_SIZE]; // Circular buff
    int win_head;                         // Head of buff
    int win_tail;                         // Tail of buff 
    int free_sz;                          // free slots avail
}snd_wndw_t;

typedef struct buff_entry {
    int is_valid;
    wndw_pckt_t *entry;
}buff_entry_t;

typedef struct recv_window {
    buff_entry_t buff [RECV_WINDOW_SIZE]; // Circular buff
    int win_head;                         // Head of buff
    int win_tail;                         // Tail of buff
    int free_slt;
    int last_ack;                         // index of cell with last ack sent
    int num_occ;                          // num of cells occupied
    long int exp_seq; 
}recv_wndw_t;

struct recv_window *r_win;

snd_wndw_t  *s_window_init  ();  /* window init */
recv_wndw_t *r_window_init  ();

int  s_add_window   (struct sender_window *snd_wndw,   
                     struct window_pckt *pckt);        /* to add a new pckt to send window */
int r_add_window    (int sockfd, struct recv_window *recv_wndw,    
                     struct window_pckt *pckt);




int  s_rem_window   (struct sender_window *snd_wndw);  /* remove pckt from send window */
int  r_rem_window   (struct recv_window *recv_wndw);   /* remove pckt from recv window */

/* Main sending logic */
void sending_func (int sockfd, void *read_buf, int bytes_rem, int sending_win_size);


/* To get next seq num */
long int get_seq_num ();

void* receiving_func (void* data);

void* consumer_func ();
#endif /* __CONTROLS_H */
