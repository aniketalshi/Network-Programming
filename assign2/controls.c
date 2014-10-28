#define "controls.c"

struct sender_window *snd_wndw;


/* To get next seq num */
long int 
get_seq_num() {
    static long int seq_num = 111;
    return ++seq_num;
}

/* window init */
void
window_init () {
    
    memset (snd_wndw->buff, NULL, SEND_WINDOW_SIZE);
    snd_wndw->win_head = 0;
    snd_wndw->win_tail = 0;
    snd_wndw->free_sz  = SEND_WINDOW_SIZE;
}

/* to add a new pckt to window */
int
add_window (struct window_pckt *pckt) {

    if (snd_wndw->free_sz > 0) {
       snd_wndw->buff[snd_wndw->win_head] = pckt;
       snd_wndw->win_head = (snd_wndw->win_head + 1) % 
                                            SEND_WINDOW_SIZE;
       snd_wndw->free_sz--;
       return 1;
    }
    return 0;
}

/* remove pckt from window */
int 
rem_window() {

    if (snd_wndw->free_sz < SEND_WINDOW_SIZE) {
        snd_wndw->buff[snd_wndw->win_tail] = NULL;
        snd_wndw->win_tail = (snd_wndw->win_tail + 1) %
                                            SEND_WINDOW_SIZE;
        snd_wndw->free_sz++;
        return 1;
    }
    return 0;
}

/* Function to handle sending logic */
void
sending_func (void *read_buf, int bytes_rem) {
    
    int acks_pending = 0;
    int seqnum       = 0;
    int offset       = 0;
    msg_hdr_t *hdr   = NULL;
    int expected_ack = 0;
    wndw_pckt_t wndw_pckt;
    char buf_temp[CHUNK_SIZE];
    
    window_init();
    
    while (bytes_rem || acks_pending) {
                
        // fill in the buffer till we can
        while (snd_wndw->free_sz > 0) {

            bytes_to_copy = min(bytes_rem, CHUNK_SIZE);
            bytes_rem    -= bytes_to_copy;
            
            memset(buf_temp, 0, CHUNK_SIZE);
            memcpy(buf_temp, read_buf + offset, bytes_to_copy);
            memset(wndw_pckt, NULL, sizeof(struct window_pckt));
                
            seqnum              = get_seq_num()
            wndw_pckt->seq_num  = seqnum;
            wndw_pckt->sent_cnt = 0;
            wndw_pckt->body     = buf_temp;
            wndw_pckt->header   = get_hdr(__MSG_FILE_DATA, seqnum, 0);

            add_window(wndw_pckt);
        }
        
        expected_ack = seqnum + 1;
        
        // wait to receive acks

        // process the acks


    }
}


