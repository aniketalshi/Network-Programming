#include "controls.h"
#include "structs.h"
#include "unprtt.h"
#include <setjmp.h>
#include <assert.h>
#include <math.h>

#define PRINT_S(a)  print_s((snd_wndw_t *)a)
#define PRINT_R(a)  print_r((recv_wndw_t *)a)

#define PRINT_BUF(a,b) do {if((b) == SEND_BUF)  PRINT_S(a);\
                           if((b) == RECV_BUF)  PRINT_R(a);\
                          } while(0)

int            max_sending_win_size;
volatile int   should_terminate = 0;
static         sigjmp_buf jmp;

/* signal handler to jump to sending packets again */
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

/* To print the entire sender window. */
/*
void
print_s_win (snd_wndw_t *s) {
    int iter;
    for (iter = 0; iter < SEND_WINDOW_SIZE; iter++) {
        
        if (s->buff[iter].is_valid) {
            printf ("| %d ", s->buff[iter].entry->seq_num);
        }
        else {
            printf ("|  X  ");
        }
    }
    printf ("|\n");
}
*/

/* To print the entire receiver window. */
void
print_r_win (recv_wndw_t *s) {
    int iter;
    for (iter = 0; iter < RECV_WINDOW_SIZE; iter++) {
        
        if (s->buff[iter].is_valid) {
            printf ("| %d ", s->buff[iter].entry->seq_num);
        }
        else {
            printf ("|  X  ");
        }
    }
    printf ("|\n");
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
    
    recv_wndw->win_head  = -1;
    recv_wndw->win_tail  = 0;
    recv_wndw->last_ack  = 0;
    recv_wndw->free_slt  = RECV_WINDOW_SIZE;
    recv_wndw->num_occ   = 0;
    recv_wndw->exp_seq   = 111;
    pthread_mutex_init(&recv_wndw->mut, NULL);

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
    int win_size = 0; 
    /* If packet in middle sequence is received, traverse all contiguous
     * packets till we have packets, and update expcted seq and last ack
     */

    while(recv_wndw->buff[recv_wndw->last_ack].is_valid) {
        recv_wndw->exp_seq = 
                  recv_wndw->buff[recv_wndw->last_ack].entry->seq_num + 1;
        
        recv_wndw->last_ack = (recv_wndw->last_ack + 1) %
                                             RECV_WINDOW_SIZE;
        if (recv_wndw->free_slt == 0)
            break;
    }

    // TEST SENDING 0 AS CLIENT WINDOW SIZE
    /*if(recv_wndw->free_slt < 9) {
        printf ("here\n");
        hdr = get_ack_hdr(recv_wndw->exp_seq, ts, 0); 
    }
    else
    */
    //printf(" inside ack funcc, expc seq %d\n", recv_wndw->exp_seq);

    assert(recv_wndw->buff[recv_wndw->win_tail].is_valid);
    win_size = RECV_WINDOW_SIZE - (recv_wndw->exp_seq - 
                        recv_wndw->buff[recv_wndw->win_tail].entry->seq_num);
    send_ack(sockfd, get_ack_hdr(recv_wndw->exp_seq, ts, win_size));
}

int
r_add_window (int sockfd, struct recv_window *recv_wndw,    
                   struct window_pckt *pckt) {
    int diff = 0, flag = 0;
    //printf("\n Producer strarts\n");
    pthread_mutex_lock(&recv_wndw->mut);
    
    //printf("entering producer %d, freeslt %d\n", pckt->seq_num, recv_wndw->free_slt);

    // If head is empty
    if (recv_wndw->free_slt == RECV_WINDOW_SIZE) {
        recv_wndw->win_head = (recv_wndw->win_head + 1) %
                                             RECV_WINDOW_SIZE;

        recv_wndw->buff[recv_wndw->win_head].is_valid = 1; 
        recv_wndw->buff[recv_wndw->win_head].entry = pckt;
        
        recv_wndw->free_slt--;
        send_ack_func(sockfd, recv_wndw, pckt->time_st);
        goto end;
    }

    // if this is new beyond head
    if (recv_wndw->buff[recv_wndw->win_head].entry->seq_num <
                                pckt->seq_num) {

        //printf("\n head seqnum %d\n", recv_wndw->buff[recv_wndw->win_head].entry->seq_num);

        diff = pckt->seq_num - 
                recv_wndw->buff[recv_wndw->win_head].entry->seq_num;
        
        if (diff > recv_wndw->free_slt) {
            fprintf(stderr,"\n Sorry no place in buffer\n");
            flag = 1;
            goto end;
        }
        
        //printf("\n head %d, seq %d\n", recv_wndw->win_head, pckt->seq_num);

        recv_wndw->free_slt -= diff;
        recv_wndw->win_head = (recv_wndw->win_head + diff) %
                                             RECV_WINDOW_SIZE;
    
        recv_wndw->buff[recv_wndw->win_head].is_valid = 1; 
        recv_wndw->buff[recv_wndw->win_head].entry = pckt;
        
        send_ack_func(sockfd, recv_wndw, pckt->time_st);
        goto end;
    
    } else {
        // if this packet is less than head
        diff = pckt->seq_num - 
                recv_wndw->buff[recv_wndw->win_head].entry->seq_num;
        diff = (recv_wndw->win_head + diff + RECV_WINDOW_SIZE) % RECV_WINDOW_SIZE;
        
        if (pckt->seq_num < recv_wndw->exp_seq) {
            fprintf(stderr,"Duplicate Packet with Seq num %d\n", pckt->seq_num);
            send_ack_func(sockfd, recv_wndw, pckt->time_st);
            goto end;
        }
   
        if(recv_wndw->buff[diff].is_valid == 0) {
            recv_wndw->buff[diff].is_valid = 1;
            recv_wndw->buff[diff].entry = pckt;
        }

        send_ack_func(sockfd, recv_wndw, pckt->time_st);
        goto end;
    }

end:
    //printf("\n Producer end\n");
    pthread_mutex_unlock(&recv_wndw->mut);
    if (flag)
        return 0;
    
    return 1;
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
    pthread_mutex_lock(&recv_wndw->mut); 
    //printf("\n consumer func starts\n");    
    if (recv_wndw->free_slt < RECV_WINDOW_SIZE) {

        //printf("\n Consumer read %d\n", recv_wndw->buff[recv_wndw->win_tail].entry->seq_num);    
        recv_wndw->buff[recv_wndw->win_tail].is_valid = 0;
        recv_wndw->buff[recv_wndw->win_tail].entry = NULL;
        recv_wndw->win_tail = (recv_wndw->win_tail + 1) %
                                        RECV_WINDOW_SIZE;
        
        recv_wndw->free_slt++;
        pthread_mutex_unlock(&recv_wndw->mut);
        return 1;
    }
   
    //printf("\n consumer func ends\n");    
    pthread_mutex_unlock(&recv_wndw->mut);
    return 0;
}

int 
get_sleep_secs () {
int sleep = -1 * (cli_params.read_rate * (log (drand48()) / log (2)));
printf ("Consumer sleeping for %d secs.\n", sleep);
return sleep;
}

void* consumer_func () {
    char buf[CHUNK_SIZE+1];
    memset(buf, 0, sizeof(buf));
    int len = 0, flag = 0;
   
    int out_fd = open (OUTFILE, O_WRONLY | O_CREAT | O_TRUNC);
    
    if (out_fd < 0) {
        fprintf(stderr, "Unable to open outfile\n");
    }
    
    /* iterate from tail to last ack and remove them from buff */
    while (should_terminate == 0) {
        flag = 0;
        sleep (get_sleep_secs ());
        
        while ( r_win->buff[r_win->win_tail].is_valid == 1 &&
                r_win->buff[r_win->win_tail].entry->seq_num < r_win->exp_seq) {
            if (!flag)
                printf ("Consumer read: ");
            flag = 1;
            len = r_win->buff[r_win->win_tail].entry->data_len;
            
            // read the entry at tail
            if(!r_win->buff[r_win->win_tail].entry->body) {
                fprintf(stderr, "No content in packet to be read");
                break;
            }
            printf ("%d\t",  r_win->buff[r_win->win_tail].entry->seq_num);   
            // write to the outfile
            write(out_fd, r_win->buff[r_win->win_tail].entry->body, len);
            
            // remove entry from tail
            r_rem_window(r_win);
        }
        if (flag){
            printf ("\n"); 
            print_r_win (r_win);
        }
    }
    
    close(out_fd);
    pthread_exit(NULL);
}

/* send probe packet */
int
send_probe (int sockfd, int *client_win_size) {
    
    msg_hdr_t recv_msg_hdr;
    struct msghdr pcktmsg;
    struct iovec recvvec[1];
    int n_bytes, flags;
    
    // make sockfd non blocking
    if ((flags = fcntl(sockfd, F_GETFL, 0)) == -1) {
        perror("fcntl F_GETFL Error");
        exit(EXIT_FAILURE);
    }
   
    if((fcntl(sockfd, F_SETFL, flags | O_NONBLOCK)) == -1) {
        perror("fcntl F_SETFL Error");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // probe client and check
        printf ("\nsending probe\n");
        send_probe_packet (sockfd);
        
        memset (&pcktmsg, 0, sizeof(struct msghdr));
        memset (&recv_msg_hdr, 0, sizeof(msg_hdr_t));
        
        pcktmsg.msg_name     = NULL;
        pcktmsg.msg_namelen  = 0;
        pcktmsg.msg_iov      = recvvec;
        pcktmsg.msg_iovlen   = 1;

        recvvec[0].iov_len   = sizeof(msg_hdr_t);
        recvvec[0].iov_base  = (void *)&recv_msg_hdr;

        while((n_bytes = recvmsg(sockfd, &pcktmsg, 0)) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;    
        }
        
        
        if (recv_msg_hdr.msg_type ==  __MSG_PROBE_RESP && 
                                    recv_msg_hdr.win_size > 0) {
            printf ("\n probing client");
            *client_win_size = recv_msg_hdr.win_size;
            break;
        }
        // sleep for 3 seconds before sending
        sleep(3);
    } 
    
    // make socket blocking again
    flags &= ~O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags) == -1 ) {
        perror("fcntl F_SETFL Error");
        exit(EXIT_FAILURE);
    }

    return 1;
}


/* Function to handle sending logic */
void
sending_func (int sockfd, void *read_buf, int bytes_rem, int client_win_size_s) {
    
    static int client_win_size =  10;
    int acks_pending = 0, seqnum = 0, offset = 0, iter, winsize_iter = 0;
    msg_hdr_t *hdr   = NULL;
    int bytes_to_copy = 0, snum = 0, cli_term = 0;
    
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
   
    while (bytes_rem && cli_term != 1) {
                
        // fill in the buffer till we can
        while (snd_wndw->free_sz > 0 && bytes_rem > 0) {
        
            wndw_pckt_t *wndw_pckt = (wndw_pckt_t *)calloc(1, sizeof(wndw_pckt_t));
            char *buf_temp         = (char *)calloc(1, CHUNK_SIZE + 1);
            
            bytes_to_copy = min(bytes_rem, CHUNK_SIZE);
            bytes_rem    -= bytes_to_copy;
            
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
        printf("\n client win size %d", client_win_size);
        
        /* if window size is zero, send probe packet
         * It will return when client win size is greater than zero 
         */
        if (client_win_size == 0) {
            
            /* Send the probe packet to check if client window size has increased
             * Break if client has terminated while sending probes
             */
            if (!send_probe(sockfd, &client_win_size)) {
                cli_term = 1;
                break;
            }
        }

        printf("Packets sent: ");
        // send the packets
        for (iter = snd_wndw->win_tail, winsize_iter = client_win_size; winsize_iter!= 0;) {
            
            winsize_iter--;
            if (send_packet(sockfd, snd_wndw->buff[iter]->header, 
                                    snd_wndw->buff[iter]->body, 
                                    snd_wndw->buff[iter]->data_len) < 0) {
                fprintf(stderr, "Error sending packet");	
                return;
            }
            printf("\t %d", snd_wndw->buff[iter]->seq_num);

            iter = (iter + 1)%SEND_WINDOW_SIZE;
            count++;
            
            /* iter goes from tail to head, if its equal to head
             * no more packets to send 
             */
            if (iter == snd_wndw->win_head)
                break;
        }
        printf("\n");
        /* Start the timer. */
        rtt_start_timer (rtt_start(&rtt_s));
       
        /* Handle the timer signal, and retransmit. */
        if (sigsetjmp(jmp, 1) != 0) {
            
            //Handle probe msg timeouts here?
            /* Check if maximum retransmits have already been sent. */
            if (rtt_timeout(&rtt_s) == 0) {
                printf("Time out. Retransmitting\n");
                rtt_start_timer (rtt_start(&rtt_s));
                
                goto process_acks;
            } else{
                /* Maximum number of retransmits sent; give up. */
                printf("Client not responding. Terminating connection\n");
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
                printf ("Connection Terminated\n");
                cli_term = 1;
                break; 
            }

            printf("Rcvd ACK. Seq Num : %d, cli win_size %d\n", 
                                                recv_msg_hdr.seq_num, recv_msg_hdr.win_size);
            
            /* If type of message is ACK, check seq num 
            *  TODO: if same ack no is received increment ackcnt
            *        if it reaches 3, we can do fast retransmit 
            */
            if (recv_msg_hdr.msg_type ==  __MSG_ACK &&
                recv_msg_hdr.seq_num > latest_ack) {
                
                latest_ack      = recv_msg_hdr.seq_num;
                ack_cnt         = 1;
                client_win_size = recv_msg_hdr.win_size;
                printf("client win size recvd %d\n", client_win_size);

            } else if (recv_msg_hdr.seq_num == latest_ack) {
                ack_cnt++;
                
                if (ack_cnt > MAX_ACK_CNT) {
                    printf ("Received third ack for Seq Num %d."
                            "Doing fast retransmit\n", latest_ack);
                    
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
                
                //printf("\n DEBUG latest ack %d, Rcvd ACK Seq Num : %d", latest_ack, snum);
                if(snum >= latest_ack)
                    break;

                //printf("\n Rcvd ACK Seq Num : %d", snum);
                s_rem_window(snd_wndw);
                continue;
            }
            break;
        }
        
        //printf("\n freesize %d , last ack: %d, win size %d", snd_wndw->free_sz, latest_ack, client_win_size);
        if (snd_wndw->free_sz < SEND_WINDOW_SIZE) {
            expected_ack = latest_ack;
            goto send;
        }
    }
    free(snd_wndw);
}

int
drop_probability(){
    
    return (drand48() < cli_params.prob_loss) ? 1: 0;
}

void*
receiving_func (void* data) {
    
    int sockfd = *(int *)data;
    wndw_pckt_t *r_win_pckt;
    char *body;
    int seqnum = 0, len = 0, winsize = 0;
    int flag_1 = 1;
    
    /* init the random function */
    srand48 (cli_params.seed_val);

    msg_hdr_t *recv_msg_hdr = (msg_hdr_t *)malloc(sizeof(msg_hdr_t));
   
    while(1) {
        r_win_pckt = (wndw_pckt_t *)calloc (1, sizeof(wndw_pckt_t));
        body = (char *)calloc(1, CHUNK_SIZE + 1);
        
        memset(recv_msg_hdr, 0, sizeof(msg_hdr_t));

        read_data (sockfd, &seqnum, recv_msg_hdr, body, &len);
        
        /* Drop packets according to the given probability */
        if (drop_probability()){
            printf ("Packet dropped with seq num %d\n", recv_msg_hdr->seq_num);
            continue;
        }
        
        //printf ("\n seq %d, %d, %d\n",seqnum, recv_msg_hdr->seq_num, len);
        
        /* IF type of message is FIN, we are done */
	if (recv_msg_hdr->msg_type ==  __MSG_FIN) {
            
            send_fin_ack(sockfd);
            printf("\n Client Terminating"); 
            
            // TODO: handle this more gracefully.
            should_terminate = 1;
	    break;
	}
        
        // Insert the packet in receiving window
	if (recv_msg_hdr->msg_type ==  __MSG_FILE_DATA) {
            //printf("\n Data Seq Num %d, Timestamp %ld\n", 
            //                recv_msg_hdr->seq_num, recv_msg_hdr->timestamp);
            
            r_win_pckt->seq_num  = recv_msg_hdr->seq_num;
            r_win_pckt->time_st  = recv_msg_hdr->timestamp;
            r_win_pckt->data_len = CHUNK_SIZE;
            r_win_pckt->body     = body;

            r_add_window (sockfd, r_win, r_win_pckt);    
            print_r_win (r_win);
        }

        // If probe packet is received
	if (recv_msg_hdr->msg_type ==  __MSG_PROBE) {
            if (flag_1 == 1) {
                flag_1 = 0;
                continue;
            }


            pthread_mutex_lock(&r_win->mut);
            
            if (r_win->buff[r_win->win_tail].is_valid == 1) {
                winsize = RECV_WINDOW_SIZE - (r_win->exp_seq - 
                               r_win->buff[r_win->win_tail].entry->seq_num);
            } else {
                winsize = RECV_WINDOW_SIZE;
            }

            printf ("\n Probe received %d\n", winsize);
            send_ack(sockfd, get_hdr(__MSG_PROBE_RESP, 0, winsize)); 
            
            pthread_mutex_unlock(&r_win->mut);
        }
    }
    
    // TODO: Consumer func works here without threading
    //consumer_func();
    pthread_exit(NULL);
}
