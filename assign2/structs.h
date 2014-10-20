#ifndef	__structs_h
#define	__structs_h

#include <sys/socket.h>
#include <netinet/in.h> 
#include <ctype.h>
#include "unp.h"
#include "unpifiplus.h"

struct sock_struct {
	int sockfd;
	int is_loopback;
	struct sockaddr *ip_addr;
	struct sockaddr	*net_mask;
        struct in_addr subnetaddr;
	struct sock_struct *nxt_struct;
};

struct sock_struct * 
get_sock_struct (int sockfd, 
		 struct sockaddr *ip_addr,
	         struct sockaddr *net_mask,
		 int is_loopback);

#endif /* __structs_h */
