#include "controls.h"
#include "structs.h"
#include "unprtt.h"
#include <setjmp.h>
#include <assert.h>

#define PRINT_S(a)  print_s((snd_wndw_t *)a)
#define PRINT_R(a)  print_r((recv_wndw_t *)a)

#define PRINT_BUF(a,b) do {if((b) == SEND_BUF)  PRINT_S(a);\
                           if((b) == RECV_BUF)  PRINT_R(a);\
                        } while(0)

float   prob_loss;
int     seed_val;

int     max_sending_win_size;

pthread_mutex_t fastmutex = PTHREAD_MUTEX_INITIALIZER;
volatile    should_terminate = 0;
static      sigjmp_buf jmp;
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
        printf("\n Pckt %d, Seq num: %d", iter, s->buff[iter].entry->seq_num);
        iter = (iter + 1)%RECV_WINDOW_SIZE;
    }
}

/* To get next seq num */
long int 
get_seq_num() {
    static long int seq_num = 110;
    return ++seq_num;
}

/* sending window init */
snd_wndw_t *
s_window_init () {
    snd_wndw_t *snd_wndw = (snd_wndw_t *)malloc(sizeof(snd_wndw_t));
    
    memset (snd_wndw->buff, 0, (size_t)max_sending_win_size);
    snd_wndw->win_head = 0;
    snd_wndw->win_tail = 0;
    //TODO: initialise with receiving sending window size?
    snd_wndw->free_sz  = SEND_WINDOW_SIZE;

    return snd_wndw;
}

/* Receving window init */
recv_wndw_t *
r_window_init () {
    
    recv_wndw_t *recv_wndw = (recv_wndw_t *)calloc(1, sizeof(recv_wndw_t));
    
    pthread_mutex_init(&fastmutex, NULL);
    //memset (recv_wndw->buff, 0, RECV_WINDOW_SIZE);
    recv_wndw->win_head  = -1;
    recv_wndw->win_tail  = 0;
    recv_wndw->last_ack  = 0;
    recv_wndw->free_slt  = RECV_WINDOW_SIZE;
    recv_wndw->num_occ   = 0;
    recv_wndw->exp_seq   = 111;

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

void
send_ack_func(int sockfd, recv_wndw_t *recv_wndw, unsigned long ts) {
    
    while(recv_wndw->buff[recv_wndw->last_ack].is_valid) {
        recv_wndw->exp_seq = 
                  recv_wndw->buff[recv_wndw->last_ack].entry->seq_num + 1;
        
        recv_wndw->last_ack = (recv_wndw->last_ack + 1) %
                                             RECV_WINDOW_SIZE;
    }

    msg_hdr_t *hdr;
    // TEST SENDING 0 AS CLIENT WINDOW SIZE
    /*if(recv_wndw->free_slt < 9) {
        printf ("here\n");
        hdr = get_ack_hdr(recv_wndw->exp_seq, ts, 0); 
    }
    else
    */
        hdr = get_ack_hdr(recv_wndw->exp_seq, ts, recv_wndw->free_slt); 
    send_ack(sockfd, hdr);
}

int
r_add_window (int sockfd, struct recv_window *recv_wndw,    
                   struct window_pckt *pckt) {
    int diff = 0; 

    
    if(pthread_mutex_lock(&fastmutex) != 0)
        printf("Error in pthread_mutex_lock\n");

    printf ("Inserting packet with seq: %d", 
                    pckt->seq_num);
    //printf ("head - %d, tail - %d", recv_wndw->win_head,
    //                                recv_wndw->win_tail);
    // If head is empty
    if (recv_wndw->free_slt == RECV_WINDOW_SIZE) {
        //printf ("head - %d, tail - %d", recv_wndw->win_head,
        //                            recv_wndw->win_tail);
        recv_wndw->win_head = (recv_wndw->win_head + 1) %
                                             RECV_WINDOW_SIZE;


        recv_wndw->buff[recv_wndw->win_head].is_valid = 1; 
        recv_wndw->buff[recv_wndw->win_head].entry = pckt;
        
        recv_wndw->free_slt--;
        send_ack_func(sockfd, recv_wndw, pckt->time_st);
        if(pthread_mutex_unlock(&fastmutex) != 0)
            printf ("Error while unlocking pthread_mutex\n");
        return 1;
    }

    //printf ("head seq %d, pckt seq %d", 
    //            recv_wndw->buff[recv_wndw->win_head].entry->seq_num,
    //            pckt->seq_num); 
    // if this is new beyond head
    if (recv_wndw->buff[recv_wndw->win_head].entry->seq_num <
                                pckt->seq_num) {
        diff = pckt->seq_num - 
                recv_wndw->buff[recv_wndw->win_head].entry->seq_num;
        
        if (diff > recv_wndw->free_slt) {
            fprintf(stderr,"\n Sorry no place in buffer ");
            if(pthread_mutex_unlock(&fastmutex) != 0)
                printf ("Error while unlocking pthread_mutex\n");
            return 0;
        }
        
        recv_wndw->free_slt -= diff;
        recv_wndw->win_head = (recv_wndw->win_head + diff) %
                                             RECV_WINDOW_SIZE;

        recv_wndw->buff[recv_wndw->win_head].is_valid = 1; 
        recv_wndw->buff[recv_wndw->win_head].entry = pckt;
        
        send_ack_func(sockfd, recv_wndw, pckt->time_st);
        if(pthread_mutex_unlock(&fastmutex) != 0)
            printf ("Error while unlocking pthread_mutex\n");
        return 1;
    } else {
       
       if (recv_wndw->win_tail == recv_wndw->last_ack) { 
            recv_wndw->buff[recv_wndw->win_tail].is_valid = 1;
            recv_wndw->buff[recv_wndw->win_tail].entry = pckt;
            send_ack_func(sockfd, recv_wndw, pckt->time_st);
            if(pthread_mutex_unlock(&fastmutex) != 0)
                printf ("Error while unlocking pthread_mutex\n");
            return 1;
        }
        // if diff is negative
        diff = (recv_wndw->win_tail + (pckt->seq_num - 
                        recv_wndw->buff[recv_wndw->win_tail].entry->seq_num)) 
                        % RECV_WINDOW_SIZE;
        
        if (pckt->seq_num < recv_wndw->exp_seq) {
            fprintf(stderr,"\n Duplicated Packet. Already Acked");
            if(pthread_mutex_unlock(&fastmutex) != 0)
                printf ("Error while unlocking pthread_mutex\n");
            return 0;
        }
        
        if(recv_wndw->buff[diff].is_valid == 0) {
            recv_wndw->buff[diff].is_valid = 1;
            recv_wndw->buff[diff].entry = pckt;
        }
        send_ack_func(sockfd, recv_wndw, pckt->time_st);
        if(pthread_mutex_unlock(&fastmutex) != 0)
            printf ("Error while unlocking pthread_mutex\n");
        return 1;
    }
}


/* remove pckt from sending window */
int 
s_rem_window (struct sender_window *snd_wndw) {
    if (snd_wndw->free_sz < RECV_WINDOW_SIZE) {
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
        // printf("Removed packet with seq_num: %d from window\n", 
        //                recv_wndw->buff[recv_wndw->win_tail].entry->seq_num);
        recv_wndw->buff[recv_wndw->win_tail].is_valid = 0;
        recv_wndw->buff[recv_wndw->win_tail].entry = NULL;
        recv_wndw->win_tail = (recv_wndw->win_tail + 1) %
                                        RECV_WINDOW_SIZE;
        recv_wndw->free_slt++;
        return 1;
    }
    return 0;
}


void* consumer_func () {
    char buf[CHUNK_SIZE+1];
    memset(buf, 0, sizeof(buf));
    int len = 0;

        
    /* iterate from tail to last ack and remove them from buff */
    while(should_terminate == 0 || r_win->buff[r_win->win_tail].is_valid == 1) {
        
        if(pthread_mutex_lock(&fastmutex) != 0)
            printf("Error in pthread_mutex_lock\n");
        
        while ( r_win->buff[r_win->win_tail].is_valid == 1 &&
           
            r_win->buff[r_win->win_tail].entry->seq_num < r_win->exp_seq) {
            len = r_win->buff[r_win->win_tail].entry->data_len;
            
            // read the entry at tail
            if(!r_win->buff[r_win->win_tail].entry->body) {
                fprintf(stderr, "No content in packet to be read");
                break;
            }
            memcpy(buf, r_win->buff[r_win->win_tail].entry->body, len);
            printf("%s\n",buf);
            
            // remove entry from tail
            r_rem_window(r_win);
        }

        if(pthread_mutex_unlock(&fastmutex) != 0)
            printf ("Error while unlocking pthread_mutex\n");
    }
    pthread_exit(NULL);
}

/* Function to handle sending logic */
void
sending_func (int sockfd, void *read_buf, int bytes_rem, int client_win_size) {
    
    int acks_pending = 0, seqnum = 0, offset = 0, iter;
    msg_hdr_t *hdr   = NULL;
    int bytes_to_copy = 0, snum = 0;
    
    int n_bytes, count = 0, expected_ack = 0, ack_cnt = 0;
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
        static int flag = 1; 
send:
        count   = 0; 
        ack_cnt = 0;
        // Shouldnt we send the next packet if we receive an ack for the 
        // first time as well?

        printf ("\nPackets sent: ");
        // send the packets
        for (iter = snd_wndw->win_tail; iter < snd_wndw->win_head
                                        && client_win_size != 0;) {
            


            /* Test Code */
            if(snd_wndw->buff[iter]->seq_num == 112 && flag) {
                iter = (iter + 1)%SEND_WINDOW_SIZE;
                count++;
                flag = 0;
                continue;
            }
            client_win_size--;
            if (send_packet(sockfd, snd_wndw->buff[iter]->header, 
                                    snd_wndw->buff[iter]->body, 
                                    snd_wndw->buff[iter]->data_len) < 0) {
                fprintf(stderr, "Error sending packet");	
                return;
            }
            printf("%d\t", snd_wndw->buff[iter]->seq_num);

            iter = (iter + 1)%SEND_WINDOW_SIZE;
            count++;
        }
        
        printf ("\n");
        
        printf ("sending win size %d", client_win_size);
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
                recv_msg_hdr.seq_num > latest_ack) {
                
                latest_ack      = recv_msg_hdr.seq_num;
                ack_cnt         = 1;
                client_win_size = recv_msg_hdr.win_size;
                // printf("client win size recvd %d\n", client_win_size);

            } else if (recv_msg_hdr.seq_num == latest_ack) {
                ack_cnt++;
                
                if (ack_cnt > MAX_ACK_CNT) {
                    printf ("\n Received third ack for Seq Num %d."
                            "Doing fast retransmit", latest_ack);
                    
                    while ((snd_wndw->buff[snd_wndw->win_tail]->seq_num < 
                                                    recv_msg_hdr.seq_num) && 
                                                    snd_wndw->free_sz < SEND_WINDOW_SIZE) {
                        
                        s_rem_window(snd_wndw);
                    }
                    rtt_start_timer(0); 
                    goto send;
                }

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

int
drop_probability(){
    srand48(seed_val);
    float rand = drand48();
    int i;
    if(rand < prob_loss){
        return 1;
    }
    return 0;
}

void*
receiving_func (void* data) {
    
    int sockfd = *(int *)data;
    wndw_pckt_t *r_win_pckt;
    char *body;
    int seqnum = 0, len = 0;

    msg_hdr_t *recv_msg_hdr = (msg_hdr_t *)malloc(sizeof(msg_hdr_t));
   
    //printf(" Inside receiving func\n");
    while(1) {
        r_win_pckt = (wndw_pckt_t *)calloc (1, sizeof(wndw_pckt_t));
        body = (char *)calloc(1, CHUNK_SIZE + 1);
        
        memset(recv_msg_hdr, 0, sizeof(msg_hdr_t));

        /* Drop packets according to the given probability */
        if (drop_probability()){
            printf ("Packet dropped!\n");
            continue;
        }
        
        read_data (sockfd, &seqnum, recv_msg_hdr, body, &len);
        
        //Keep this?
        //body[len-1] = '\0';
        
        //printf ("\n seq %d, %d, %d\n",seqnum, recv_msg_hdr->seq_num, len);
        
        /* IF type of message is FIN, we are done */
	if (recv_msg_hdr->msg_type ==  __MSG_FIN) {
	    printf ("\n\n****** File transfer completed *****\n");
                
            //TODO: handle this more gracefully.
            should_terminate = 1;
	    break;
	}
        
        // Insert the packet in receiving window
	if (recv_msg_hdr->msg_type ==  __MSG_FILE_DATA) {

            r_win_pckt->seq_num  = recv_msg_hdr->seq_num;
            r_win_pckt->time_st  = recv_msg_hdr->timestamp;
            r_win_pckt->data_len = CHUNK_SIZE;
            r_win_pckt->body     = body;

            r_add_window (sockfd, r_win, r_win_pckt);    
        }
    }
    
    // TODO: Consumer func works here without threading
    //consumer_func();
    pthread_exit(data);
}
