#include <stdlib.h>
#include "structs.h"

struct sock_struct * 
get_sock_struct (int sockfd, 
		 struct sockaddr *ip_addr,
	         struct sockaddr *net_mask,
		 int is_loopback) {

    struct sock_struct * sk = (struct sock_struct *) malloc
					(sizeof(struct sock_struct));
    sk->sockfd      = sockfd;
    sk->ip_addr     = ip_addr;
    sk->net_mask    = net_mask;
    sk->is_loopback = is_loopback;
    sk->nxt_struct  = NULL;
    
    sk->subnetaddr.s_addr  = (((struct sockaddr_in *) ip_addr)->sin_addr.s_addr) &
		      (((struct sockaddr_in *) net_mask)->sin_addr.s_addr);
    return sk;
}

void print_sock_struct (struct sock_struct *st) {
    printf("\n Array of of sock structures");
    printf ("\n socket\t\t   ipaddr\t    net_mask\t\t  subnet");
    printf ("\n=======================================================================\n");
    char str[INET_ADDRSTRLEN], str1[INET_ADDRSTRLEN], str2[INET_ADDRSTRLEN];
    
    while (st) {

	inet_ntop (AF_INET, &(st->subnetaddr), str, INET_ADDRSTRLEN);
	inet_ntop (AF_INET, &(((struct sockaddr_in *)st->ip_addr)->sin_addr), str1, INET_ADDRSTRLEN);
	inet_ntop (AF_INET, &(((struct sockaddr_in *)st->net_mask)->sin_addr), str2, INET_ADDRSTRLEN);
	
	printf ("%5d%20s%20s%20s\n", st->sockfd, str1, str2, str);
	st = st->nxt_struct;
    }
}
