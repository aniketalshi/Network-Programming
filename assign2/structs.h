#ifndef	__structs_h
#define	__structs_h

#include <sys/socket.h>
#include <netinet/in.h> 
#include <ctype.h>
#include "unp.h"
#include "unpifiplus.h"

#define IPLEN INET_ADDRSTRLEN

struct sock_struct {
	int sockfd;
	int is_loopback;
	struct sockaddr_in ip_addr;
	struct sockaddr_in net_mask;
        struct sockaddr_in subnetaddr;
	struct sock_struct *nxt_struct;
};

struct sock_struct * 
get_sock_struct (int sockfd, 
		 struct sockaddr_in *ip_addr,
	         struct sockaddr_in *net_mask,
		 int is_loopback);

struct sock_struct *sock_head;


#endif /* __structs_h */
