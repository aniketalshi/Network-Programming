#include "controls.h"
#include "structs.h"

/* To get next seq num */
long int 
get_seq_num() {
    static long int seq_num = 111;
    return ++seq_num;
}

/* sending window init */
void 
s_window_init (struct sender_window *snd_wndw) {
    
    memset (snd_wndw->buff, 0, (size_t)SEND_WINDOW_SIZE);
    snd_wndw->win_head = 0;
    snd_wndw->win_tail = 0;
    snd_wndw->free_sz  = SEND_WINDOW_SIZE;
}

/* Receving window init */
void 
r_window_init (struct recv_window *recv_wndw) {
    
    memset (recv_wndw->buff, 0, RECV_WINDOW_SIZE);
    recv_wndw->win_head  = 0;
    recv_wndw->win_tail  = 0;
    recv_wndw->last_ack  = 0;
    recv_wndw->free_slt  = RECV_WINDOW_SIZE;
    recv_wndw->num_occ   = 0;
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
    wndw_pckt_t wndw_pckt;
    int bytes_to_copy = 0;
    char buf_temp[CHUNK_SIZE];
    
    int n_bytes, latest_ack = 0, count = 0, expected_ack = 0;
    struct msghdr pcktmsg;
    struct iovec recvvec[1];
    msg_hdr_t recv_msg_hdr;
    snd_wndw_t *snd_wndw;
    
    // initialise the buffer
    s_window_init(snd_wndw);
    
    while (bytes_rem) {
                
        // fill in the buffer till we can
        while (snd_wndw->free_sz > 0) {

            bytes_to_copy = min(bytes_rem, CHUNK_SIZE);
            bytes_rem    -= bytes_to_copy;
            
            memset(buf_temp, 0, CHUNK_SIZE);
            memcpy(buf_temp, read_buf + offset, bytes_to_copy);
            bzero(&wndw_pckt, sizeof(struct window_pckt));
                
            seqnum             = get_seq_num();
            wndw_pckt.seq_num  = seqnum;
            wndw_pckt.sent_cnt = 0;
            wndw_pckt.body     = buf_temp;
            wndw_pckt.data_len = bytes_to_copy;
            wndw_pckt.header   = get_hdr(__MSG_FILE_DATA, seqnum, 0);

            s_add_window(snd_wndw, &wndw_pckt);
        }
        
        expected_ack = seqnum + 1;
        
send:
        // send the packets
        for (iter = snd_wndw->win_tail; iter <= snd_wndw->win_head; ++iter) {
            if ((send_packet(sockfd, snd_wndw->buff[iter]->header, 
                                     snd_wndw->buff[iter]->body, 
                                     snd_wndw->buff[iter]->data_len)) < 0) {
                fprintf(stderr, "Error sending packet");	
                return;
            }
            printf("\n Seq Num: %d, Bytes send: %d", snd_wndw->buff[iter]->seq_num, 
                                                snd_wndw->buff[iter]->data_len);
            count++;
        }
         
        // TODO:start the timer

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

            /* If type of message is ACK, check seq num 
            *  TODO: if same ack no is received increment ackcnt
            *        if it reaches 3, we can do fast retransmit 
            */
            if (recv_msg_hdr.msg_type ==  __MSG_ACK &&
                recv_msg_hdr.seq_num > latest_ack) {
                
                latest_ack = recv_msg_hdr.seq_num;
	    }
            count--;
        }
        
        // stop the timer here

process_acks:
        // process the acks 
        // start from tail and delete packets upto seqnum = latest_ack - 1
        while(1) {        
            if (snd_wndw->free_sz < SEND_WINDOW_SIZE) {
                if(snd_wndw->buff[snd_wndw->win_tail]->seq_num >= latest_ack)
                    break;
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
}



