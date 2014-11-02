#include <stdlib.h>
#include "structs.h"
#include "unprtt.h"

#define SRV_FILE    "server.in"
#define CLI_FILE    "client.in"

struct rtt_info rtts;

/* utility function */

/* To print ip and port */
char *
print_ip_port(struct sockaddr_in *st) {
    
    char *buf = (char *)malloc(100);
    char str_ip[IPLEN];
    inet_ntop (AF_INET, &(st->sin_addr), str_ip, IPLEN);
    sprintf(buf, "IP : %s, Port : %d",str_ip, ntohs(st->sin_port));
    return buf;
}

/* Take input from server.in */
void
server_input(int *server_port, int *max_win_size){
    char temp[MAXLINE];
    
    FILE *fp = NULL;
    if((fp = fopen(SRV_FILE, "rt")) == NULL) {
	printf("No such file: server.in\n");
	exit(1);
    }

    fscanf(fp, "%s", temp);
    *server_port = atoi(temp);

    fscanf(fp, "%s", temp);
    *max_win_size = atoi(temp);

    printf("=============== Read from server.in ===============\n");
    printf("Server Port Number          : %d\n", *server_port);
    printf("Maximum sliding window size : %d\n", *max_win_size);
    printf("===================================================\n");
}

/* To get input from client.in */
void 
client_input () {
    char temp[MAXLINE];

    FILE *fp = NULL;
    if((fp = fopen(CLI_FILE, "rt")) == NULL) {
	printf("No such file: client.in\n");
	exit(1);
    }
    
    fscanf(fp, "%s", cli_params.server_ip);

    fscanf(fp, "%s", temp);
    cli_params.server_port = atoi(temp);
		    
    fscanf(fp, "%s", cli_params.file_name);

    fscanf(fp, "%s", temp);
    cli_params.window_size = atoi(temp);
				        
    fscanf(fp, "%s", temp);
    cli_params.seed_val = atoi(temp);
						    
    fscanf(fp, "%s", temp);
    cli_params.prob_loss = atof(temp);

    fscanf(fp, "%s", temp);
    cli_params.read_rate = atoi(temp);

    printf("=============== Read from client.in ===============\n");
    printf("Server IP Address   : %s\n", cli_params.server_ip);
    printf("Server Port Number  : %d\n", cli_params.server_port);
    printf("File name           : %s\n", cli_params.file_name);
    printf("Window size         : %d\n", cli_params.window_size);
    printf("Seed Value          : %d\n", cli_params.seed_val);
    printf("Probability of loss : %f\n", cli_params.prob_loss); 
    printf("Read Rate           : %d\n", cli_params.read_rate);
    printf("===================================================\n");
}

/* To print port */
char *
print_ip(struct sockaddr_in *st) {
    
    char *buf = (char *)malloc(100);
    char str_ip[IPLEN];
    inet_ntop (AF_INET, &(st->sin_addr), str_ip, IPLEN);
    sprintf(buf, "IP : %s",str_ip);
    return buf;
}

/* To populate sock struct */
sock_struct_t * 
get_sock_struct (int sockfd, 
		 struct sockaddr_in *ip_addr,
	         struct sockaddr_in *net_mask,
		 int is_loopback) {

    if (!ip_addr || !net_mask)
	return NULL;

    sock_struct_t * sk = (struct sock_struct *) malloc
				(sizeof(struct sock_struct));
    
    sk->sockfd			 = sockfd;
    sk->ip_addr.sin_addr.s_addr  = ip_addr->sin_addr.s_addr;
    sk->net_mask.sin_addr.s_addr = net_mask->sin_addr.s_addr;
    sk->is_loopback		 = is_loopback;
    sk->nxt_struct		 = NULL;
    
    // Subnet mask - logical and of ip and netmask
    sk->subnetaddr.sin_addr.s_addr  = ip_addr->sin_addr.s_addr &
					   net_mask->sin_addr.s_addr;
    return sk;
}

/* find sock_struct based on sockfd */
sock_struct_t *
find_sock_struct (int sockfd, sock_struct_t *sock_head) {
    
    sock_struct_t *curr;
    for (curr = sock_head; curr != NULL;
				curr = curr->nxt_struct) {
	if (curr->sockfd == sockfd)
	    return curr;

    }
    return NULL;
}

/* To print sock struct */
void 
print_sock_struct (sock_struct_t *st) {
    printf("\n List of Interfaces, IP and ");
    printf ("\n socket\t\t   ipaddr\t    net_mask\t\t  subnet");
    printf ("\n=======================================================================\n");
    char str[IPLEN], str1[IPLEN], str2[IPLEN];
    
    while (st) {
	inet_ntop (AF_INET, &(st->subnetaddr.sin_addr), str, IPLEN);
	inet_ntop (AF_INET, &(st->ip_addr.sin_addr), str1, IPLEN);
	inet_ntop (AF_INET, &(st->net_mask.sin_addr), str2, IPLEN);
	
	printf ("%5d%20s%20s%20s\n", st->sockfd, str1, str2, str);
	st = st->nxt_struct;
    }
}

/* To get new connection struct */
conn_struct_t *
get_conn_struct (int conn_sockfd,
		 struct sockaddr_in *srv,
		 struct sockaddr_in *cli) {
    
    if (!srv || !cli)
	return NULL;
    
    conn_struct_t *connr = (conn_struct_t *)
				 malloc (sizeof(struct conn_struct));
    connr->serv         = srv;
    connr->cli          = cli;
    connr->conn_sockfd  = conn_sockfd;
    connr->nxt_struct	= NULL;

    return connr;
}

/* To check if this connection exists */
int 
check_new_conn (struct sockaddr_in *cli, 
		conn_struct_t *conn_head) {

    conn_struct_t *curr;
    for (curr = conn_head; curr != NULL; curr = curr->nxt_struct) {
	
	// if a connection exists with same client ip and port, done
	if (curr->cli->sin_port == cli->sin_port &&
		curr->serv->sin_addr.s_addr == cli->sin_addr.s_addr) {
	    printf ("\n Duplicate connection received ");	
	    return 1;
	}
    }
    
    return 0;
}

/* To insert in conn struct */
conn_struct_t *
insert_conn_struct (int conn_sockfd,
		    struct sockaddr_in *srv,
		    struct sockaddr_in *cli,
		    conn_struct_t **conn_head) {
    
    conn_struct_t *curr = *conn_head;
    
    if (!curr) {
	*conn_head = get_conn_struct(conn_sockfd, srv, cli);
	return *conn_head;
    }

    // Go to the end of list 
    for (curr = *conn_head; curr->nxt_struct; curr = curr->nxt_struct);

    curr->nxt_struct = get_conn_struct (conn_sockfd, srv, cli);
    return curr->nxt_struct;
}

/* Function to construct msg header */
msg_hdr_t 
*get_hdr (int msg_type, uint32_t seq_num, uint32_t ws) {

    msg_hdr_t *msg = (msg_hdr_t *)malloc(sizeof(struct msg_hdr));
    msg->msg_type  = msg_type;
    msg->seq_num   = seq_num;
    msg->timestamp = htonl(rtt_ts(&rtts));
    msg->win_size  = ws;

    return msg;
}

msg_hdr_t 
*get_ack_hdr (uint32_t seq_num, unsigned long ts, uint32_t ws) {

    msg_hdr_t *msg = (msg_hdr_t *)malloc(sizeof(struct msg_hdr));
    msg->msg_type  = __MSG_ACK;
    msg->seq_num   = seq_num;
    msg->timestamp = ts;
    msg->win_size  = ws;

    return msg;
}

/* Function to construct packet 
 * and send packet */
int
send_packet (int sockfd, msg_hdr_t *header, 
		void *buff, uint32_t data_len) {

    struct msghdr pcktmsg;
    struct iovec sendvec[2];
    int nbytes;

    memset(&pcktmsg, 0, sizeof(struct msghdr));
    pcktmsg.msg_name     = NULL;
    pcktmsg.msg_namelen  = 0;
    pcktmsg.msg_iov      = sendvec;
    pcktmsg.msg_iovlen   = 2;
    
    sendvec[0].iov_len   = sizeof(msg_hdr_t);
    sendvec[0].iov_base  = (void *)header;
    sendvec[1].iov_len   = data_len;
    sendvec[1].iov_base  = buff;
    
    // send the packet
    if ((nbytes = sendmsg(sockfd, &pcktmsg, 0)) < 0) {
	fprintf(stderr, "Error sending packet");	
	return nbytes;
    }
    return nbytes;
}

/* send probe packet */
int 
send_probe_packet (int sockfd) {

    struct msghdr pcktmsg;
    struct iovec sendvec[1];
    int nbytes;

    memset(&pcktmsg, 0, sizeof(struct msghdr));
    pcktmsg.msg_name     = NULL;
    pcktmsg.msg_namelen  = 0;
    pcktmsg.msg_iov      = sendvec;
    pcktmsg.msg_iovlen   = 1;
    
    sendvec[0].iov_len   = sizeof(msg_hdr_t);
    sendvec[0].iov_base  = (void *)get_hdr (__MSG_PROBE, 0, 0);
    
    // send the packet
    if ((nbytes = sendmsg(sockfd, &pcktmsg, 0)) < 0) {
	fprintf(stderr, "Error sending packet");	
	return nbytes;
    }
    
    return nbytes;

}
/* Data sending logic will go here */
void
send_data (int sockfd, void *buf, int len, int ftype) {
    rtt_init(&rtts);
    rtt_newpack(&rtts);
    int n_bytes = 0;

    msg_hdr_t *header = get_hdr (ftype, 1, 0);
    if ((n_bytes = send_packet(sockfd, header, buf, len)) <= 0)
	printf("Error sending data ");
}

/* Data reading logic will go here */
int
read_data (int sockfd, int *seqnum, msg_hdr_t *recv_msg_hdr, void *body, int *len) {

    int n_bytes;
    struct msghdr pcktmsg;
    //wndw_pckt_t *r_win_pckt;
    struct iovec recvvec[2];
        
    memset (&pcktmsg, 0, sizeof(struct msghdr));
    
    pcktmsg.msg_name     = NULL;
    pcktmsg.msg_namelen  = 0;
    pcktmsg.msg_iov      = recvvec;
    pcktmsg.msg_iovlen   = 2;

    recvvec[0].iov_len   = sizeof(msg_hdr_t);
    recvvec[0].iov_base  = (msg_hdr_t *)recv_msg_hdr;
    recvvec[1].iov_len   = CHUNK_SIZE;
    recvvec[1].iov_base  = (void *)body;

    n_bytes = recvmsg(sockfd, &pcktmsg, 0);
    
    if (n_bytes <= 0) {
        printf ("\n Connection Terminated");
        return 0;
    }
    
    *seqnum = recv_msg_hdr->seq_num;
    *len    = CHUNK_SIZE;
    return 1;
}

/* Function to construct packet and send packet */
int
send_ack (int sockfd, msg_hdr_t *header) {
    
    struct msghdr pcktmsg;
    struct iovec sendvec[1];
    int nbytes;

    memset(&pcktmsg, 0, sizeof(struct msghdr));
    pcktmsg.msg_name     = NULL;
    pcktmsg.msg_namelen  = 0;
    pcktmsg.msg_iov      = sendvec;
    pcktmsg.msg_iovlen   = 1;
    
    sendvec[0].iov_len   = sizeof(msg_hdr_t);
    sendvec[0].iov_base  = (void *)header;
    
    // send the packet
    if ((nbytes = sendmsg(sockfd, &pcktmsg, 0)) < 0) {
	fprintf(stderr, "Error sending packet");	
	return nbytes;
    }
    
    printf("\n Ack Sent %d, win size %d\n", header->seq_num, header->win_size);
    return nbytes;
}
