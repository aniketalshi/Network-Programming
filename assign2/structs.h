#ifndef	__structs_h
#define	__structs_h

#include <sys/socket.h>
#include <netinet/in.h> 
#include <ctype.h>
#include "unp.h"
#include "unpifiplus.h"

#define IPLEN		        INET_ADDRSTRLEN
#define MAXFD		        256
#define BUFF_SIZE	        1024*1024
#define CHUNK_SIZE	        512

/* FILE TYPE MACROS */
#define __MSG_ACK		1  // Acknowledgement packet
#define __MSG_FILE_DATA		2  // Packets containing file data
#define __MSG_INFO		3  // Info regarding port nos etc
#define __MSG_FIN		4  // Final packet

/* sock_struct : stores all information related to a socket
 * socket number, ip_addr, netmask
 */
typedef struct sock_struct {
    int    sockfd;		    // socket num
    int    is_loopback;		    // if the ip is the loopback ip
    struct sockaddr_in ip_addr;	    // ip addr associated with this sock
    struct sockaddr_in net_mask;    // Network mask corresponding to this ip
    struct sockaddr_in subnetaddr;  // Calculates subnet
    struct sock_struct *nxt_struct; // pointer to next struct
}sock_struct_t;


/* struct stores all information about a connection */
typedef struct conn_struct {
    int    conn_sockfd;		    // connection socket fd associated with this
    pid_t  pid;			    // pid of proccess
    struct sockaddr_in *serv;	    // server ip and port no
    struct sockaddr_in *cli;	    // client ip and port no
    struct conn_struct *nxt_struct; // pointer to next struct
}conn_struct_t; 


/* structure of packet header */
typedef struct msg_hdr {
    uint32_t msg_type;	    // Type of message
    uint32_t seq_num;	    // Sequence num of the message
    uint32_t timestamp;	    // Timestamp on message 
    uint32_t win_size;	    // Windown size
}msg_hdr_t;

/* allocate new sock struct */
sock_struct_t * get_sock_struct (int sockfd, 
		 struct sockaddr_in *ip_addr,
	         struct sockaddr_in *net_mask,
		 int is_loopback);

/* find sock struct based on sockfd */
sock_struct_t * find_sock_struct (int sockfd, sock_struct_t *head);

/* Check if this is new connection */
int 
check_new_conn (struct sockaddr_in *cli, 
		conn_struct_t *conn_head);

/* Insert new conn struct in list */
conn_struct_t * insert_conn_struct (int conn_sockfd,
			 struct sockaddr_in *srv, 
			 struct sockaddr_in *cli, 
			 conn_struct_t **conn_head);

/* To construct a msg hdr */
msg_hdr_t *get_hdr (int msg_type, uint32_t seq_num, uint32_t ws);

/* To construct ack msg hdr */
msg_hdr_t *get_ack_hdr (uint32_t seq_num, unsigned long ts, uint32_t ws);

/* Logic too send data */
void  send_data (int sockfd, void *buf, int len, int ftype);

/* Logic to read data */
//void  read_data (int sockfd);
int
read_data (int sockfd, int *seqnum, msg_hdr_t *recv_msg_hdr, void *body, int *len);

#endif /* __structs_h */
