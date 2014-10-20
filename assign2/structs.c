#include <stdlib.h>
#include "structs.h"

struct sock_struct * 
get_sock_struct (int sockfd, 
		 struct sockaddr *ip_addr,
	         struct sockaddr *net_mask ) {


    struct sock_struct * sk = (struct sock_struct *) malloc
					(sizeof(struct sock_struct));
    sk->sockfd     = sockfd;
    sk->ip_addr    = ip_addr;
    sk->net_mask   = net_mask;
    //sk->subnetaddr = ip_addr->sa_data & net_mask->sa_data;
    sk->nxt_struct = NULL;
    return sk;
}

void print_sock_struct (struct sock_struct *st) {
    printf("\n Array of of sock structures");
    printf ("\n socket\t\t   ipaddr\t    net_mask\t\t  subnet");
    printf ("\n=======================================================================\n");
    
    while (st) {
	printf ("%5d%20s%20s%20s\n", st->sockfd,
			    Sock_ntop_host(st->ip_addr, sizeof(*(st->ip_addr))), 
			    Sock_ntop_host(st->net_mask, sizeof(*(st->net_mask))),
			    st->ip_addr->sa_data);

	st = st->nxt_struct;
    }
}
