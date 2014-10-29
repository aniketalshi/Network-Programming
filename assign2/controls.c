#include "controls.h"
#include "structs.h"
#include "unprtt.h"
#include <setjmp.h>

#define PRINT_S(a)  print_s((snd_wndw_t *)a)
#define PRINT_R(a)  print_r((recv_wndw_t *)a)

#define PRINT_BUF(a,b) do {if((b) == SEND_BUF)  PRINT_S(a);\
                           if((b) == RECV_BUF)  PRINT_R(a);\
                        } while(0)

static sigjmp_buf jmp;

void sigalarm_handler(int signo){
    siglongjmp(jmp, 1);
}

void
print_s (snd_wndw_t *s) {
    int iter;
    for (iter = s->win_tail; iter < s->win_head;) {
        printf("\n SPckt %d, Seq num: %d", iter, s->buff[iter]->seq_num);
        iter = (iter + 1)%SEND_WINDOW_SIZE;
    }
}

void
print_r (recv_wndw_t *s) {
    int iter;
    for (iter = s->win_tail; iter < s->win_head; ) {
        printf("\n Pckt %d, Seq num: %d", iter, s->buff[iter]->seq_num);
        iter = (iter + 1)%RECV_WINDOW_SIZE;
    }
}

/* To get next seq num */
long int 
get_seq_num() {
    static long int seq_num = 111;
    return ++seq_num;
}

/* sending window init */
snd_wndw_t *
s_window_init () {
    snd_wndw_t *snd_wndw = (snd_wndw_t *)malloc(sizeof(snd_wndw_t));
    
    memset (snd_wndw->buff, 0, (size_t)SEND_WINDOW_SIZE);
    snd_wndw->win_head = 0;
    snd_wndw->win_tail = 0;
    snd_wndw->free_sz  = SEND_WINDOW_SIZE;

    return snd_wndw;
}

/* Receving window init */
recv_wndw_t *
r_window_init () {
    
    recv_wndw_t *recv_wndw = (recv_wndw_t *)malloc(sizeof(recv_wndw_t));

    memset (recv_wndw->buff, 0, RECV_WINDOW_SIZE);
    recv_wndw->win_head  = 0;
    recv_wndw->win_tail  = 0;
    recv_wndw->last_ack  = 0;
    recv_wndw->free_slt  = RECV_WINDOW_SIZE;
    recv_wndw->num_occ   = 0;

    return recv_wndw;
}

/* to add a new pckt to window */
int 
s_add_window (struct sender_window *snd_wndw, struct window_pckt *pckt) {

    if (snd_wndw->free_sz > 0) {
       snd_wndw->buff[snd_wndw->win_head] = pckt;
       snd_wndw->win_head = (snd_wndw->win_head + 1) % 
                                       SEND_WINDOW_SIZE;
       snd_wndw->free_sz--;
       return 1;
    }
    return 0;
}

/* to add a new pckt to recv window */
int  
r_add_window (struct recv_window *recv_wndw,    
                   struct window_pckt *pckt) {
    int num = 0; 
    
    // if queue is empty
    if (!recv_wndw->buff[recv_wndw->win_head]) {
        recv_wndw->buff[recv_wndw->win_head] = pckt;
        num = 1;
    
    } else {
        // check diff of seq nums between current packt and pckt at head
        // is less than equal to avail free slots
        num = recv_wndw->buff[recv_wndw->win_head]->seq_num - pckt->seq_num;
        if (recv_wndw->free_slt < num) {
            printf ("\n Discarding packet with seq num %d,"
                                "no free slots avail", pckt->seq_num);
            return 0; 
        }
    }
    
    recv_wndw->win_head = (recv_wndw->win_head + num) % 
                                               RECV_WINDOW_SIZE;
    recv_wndw->free_slt -= num;
    recv_wndw->num_occ--;
}

/* remove pckt from sending window */
int 
s_rem_window (struct sender_window *snd_wndw) {
    if (snd_wndw->free_sz < SEND_WINDOW_SIZE) {
        // printf("\n Removing pckt %d", snd_wndw->buff[snd_wndw->win_tail]->seq_num);

        snd_wndw->buff[snd_wndw->win_tail] = NULL;
        snd_wndw->win_tail = (snd_wndw->win_tail + 1) %
                                        SEND_WINDOW_SIZE;
        snd_wndw->free_sz++;
        return 1;
    }
    return 0;
}

/* remove pckt from recv window */
int 
r_rem_window (struct recv_window *recv_wndw) {
    if (recv_wndw->free_slt < RECV_WINDOW_SIZE) {
        recv_wndw->buff[recv_wndw->win_tail] = NULL;
        recv_wndw->win_tail = (recv_wndw->win_tail + 1) %
                                        RECV_WINDOW_SIZE;
        recv_wndw->free_slt++;
        return 1;
    }
    return 0;
}

/* Function to handle sending logic */
void
sending_func (int sockfd, void *read_buf, int bytes_rem) {
    
    int acks_pending = 0, seqnum = 0, offset = 0, iter;
    msg_hdr_t *hdr   = NULL;
    int bytes_to_copy = 0, snum = 0;
    
    int n_bytes, count = 0, expected_ack = 0;
    static int latest_ack = 0;
    struct msghdr pcktmsg;
    struct iovec recvvec[1];
    msg_hdr_t recv_msg_hdr;
    struct rtt_info rtt_s;
    
    signal (SIGALRM, sigalarm_handler);
    memset (&rtt_s, 0, sizeof(rtt_s));
    rtt_init (&rtt_s);
    
    /* Initialize the retransmit count to 0. */
    rtt_newpack (&rtt_s);
    
    // initialise the buffer
    snd_wndw_t *snd_wndw = s_window_init();
    
    while (bytes_rem) {
                
        // fill in the buffer till we can
        while (snd_wndw->free_sz > 0 && bytes_rem > 0) {

            wndw_pckt_t *wndw_pckt = (wndw_pckt_t *)malloc(sizeof(wndw_pckt_t));
            char *buf_temp         = (char *)malloc(CHUNK_SIZE);
            
            bytes_to_copy = min(bytes_rem, CHUNK_SIZE);
            bytes_rem    -= bytes_to_copy;
            
            memset(buf_temp, 0, CHUNK_SIZE);
            bzero(wndw_pckt, sizeof(struct window_pckt));
            
            memcpy(buf_temp, read_buf + offset, bytes_to_copy);
            offset += bytes_to_copy;
                
            seqnum              = get_seq_num();
            wndw_pckt->seq_num  = seqnum;
            wndw_pckt->sent_cnt = 0;
            wndw_pckt->body     = buf_temp;
            wndw_pckt->data_len = bytes_to_copy;
            wndw_pckt->header   = get_hdr(__MSG_FILE_DATA, seqnum, 0);

            s_add_window(snd_wndw, wndw_pckt);
        }
        
        expected_ack = snd_wndw->buff[snd_wndw->win_head]->seq_num;

        // DEBUG
        //PRINT_BUF(snd_wndw, SEND_BUF);
    
send:
        count = 0; 
        // send the packets
        for (iter = snd_wndw->win_tail; iter < snd_wndw->win_head;) {
            if (send_packet(sockfd, snd_wndw->buff[iter]->header, 
                                    snd_wndw->buff[iter]->body, 
                                    snd_wndw->buff[iter]->data_len) < 0) {
                fprintf(stderr, "Error sending packet");	
                return;
            }
            printf("\n %d Seq Num: %d, Bytes send: %d", iter, snd_wndw->buff[iter]->seq_num, 
                                                snd_wndw->buff[iter]->data_len);

            iter = (iter + 1)%SEND_WINDOW_SIZE;
            count++;
        }

        /* Start the timer. */
        rtt_start_timer (rtt_start(&rtt_s));
       

        /* Handle the timer signal, and retransmit. */
        if (sigsetjmp(jmp, 1) != 0) {
            
            /* Check if maximum retransmits have already been sent. */
            if (rtt_timeout(&rtt_s) == 0) {
                printf("\nTime out. Retransmitting\n");
                rtt_start_timer (rtt_start(&rtt_s));
                
                goto process_acks;
            } else{
                /* Maximum number of retransmits sent; give up. */
                printf("\n Client not responding. Terminating connection\n");
                return;
            }
        }
    
        // wait to receive acks
        while (count) {
            memset (&pcktmsg, 0, sizeof(struct msghdr));
            memset (&recv_msg_hdr, 0, sizeof(msg_hdr_t));
            
            pcktmsg.msg_name     = NULL;
            pcktmsg.msg_namelen  = 0;
            pcktmsg.msg_iov      = recvvec;
            pcktmsg.msg_iovlen   = 1;

            recvvec[0].iov_len   = sizeof(msg_hdr_t);
            recvvec[0].iov_base  = (void *)&recv_msg_hdr;

            n_bytes = recvmsg(sockfd, &pcktmsg, 0);
        
            if (n_bytes <= 0) {
                printf ("\n Connection Terminated");
                break; 
            }

            printf("\n Rcvd ACK. Seq Num : %d", recv_msg_hdr.seq_num);
            /* If type of message is ACK, check seq num 
            *  TODO: if same ack no is received increment ackcnt
            *        if it reaches 3, we can do fast retransmit 
            */
            if (recv_msg_hdr.msg_type ==  __MSG_ACK &&
                recv_msg_hdr.seq_num >= latest_ack) {
                
                latest_ack = recv_msg_hdr.seq_num;
            }
            count--;
        }
        // stop the timer here
        /* Turn off the alarm */
        rtt_start_timer(0);

process_acks:
        // process the acks 
        // start from tail and delete packets upto seqnum = latest_ack - 1
        
        while(1) {        
            if (snd_wndw->free_sz < SEND_WINDOW_SIZE) {
                snum = snd_wndw->buff[snd_wndw->win_tail]->seq_num;
                
                //printf("\n DEBUG %d, Rcvd ACK Seq Num : %d", latest_ack, snum);
                if(snum >= latest_ack)
                    break;

                //printf("\n Rcvd ACK Seq Num : %d", snum);
                s_rem_window(snd_wndw);
                continue;
            }
            break;
        }

        if (snd_wndw->free_sz < SEND_WINDOW_SIZE) {
            expected_ack = latest_ack;
            goto send;
        }
    }

    free(snd_wndw);
}
