#ifndef	__structs_h
#define	__structs_h

#include <sys/socket.h>
#include <netinet/in.h> 
#include <ctype.h>
#include "unp.h"
#include "unpifiplus.h"

#define IPLEN INET_ADDRSTRLEN
#define MAXFD 256

/* sock_struct : stores all information related to a socket
 * socket number, ip_addr, netmask
 */

typedef struct sock_struct {
    int sockfd;			    // socket num
    int is_loopback;		    // if the ip is the loopback ip
    struct sockaddr_in ip_addr;	    // ip addr associated with this sock
    struct sockaddr_in net_mask;    // Network mask corresponding to this ip
    struct sockaddr_in subnetaddr;  // Calculates subnet
    struct sock_struct *nxt_struct; // pointer to next struct
}sock_struct_t;


/* struct stores all information about a connection */
typedef struct conn_struct {
    int conn_sockfd;		    // connection socket fd associated with this
    int list_sockfd;		    // listen socket fd associated with this
    struct sockaddr_in serv;	    // server ip and port no
    struct sockaddr_in cli;	    // client ip and port no
    struct conn_struct *nxt_struct; // pointer to next struct
}conn_struct_t; 

/* head of list of all socket_structs */
sock_struct_t *sock_head;
/* head of list of connection structs */
conn_struct_t *conn_head;

/* allocate new sock struct */
sock_struct_t * 
get_sock_struct (int sockfd, 
		 struct sockaddr_in *ip_addr,
	         struct sockaddr_in *net_mask,
		 int is_loopback);

/* find sock struct based on sockfd */
sock_struct_t *
find_sock_struct (int sockfd);

#endif /* __structs_h */
