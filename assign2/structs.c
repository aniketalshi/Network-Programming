#include <stdlib.h>
#include "structs.h"

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
find_sock_struct (int sockfd) {
    
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
    printf("\n Array of of sock structures");
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
		 int list_sockfd,
		 struct sockaddr_in *srv,
		 struct sockaddr_in *cli) {
    
    if (!srv || !cli)
	return NULL;
    
    conn_struct_t *connr = (conn_struct_t *)
				 malloc (sizeof(struct conn_struct));
    
    connr->serv.sin_port        = srv->sin_port;
    connr->cli.sin_port         = cli->sin_port;
    connr->conn_sockfd          = conn_sockfd;
    connr->list_sockfd          = list_sockfd;
    connr->serv.sin_addr.s_addr = srv->sin_addr.s_addr;
    connr->cli.sin_addr.s_addr  = cli->sin_addr.s_addr;
    connr->nxt_struct		= NULL;

    return connr;
}

/* To check if this connection exists */
int check_new_conn (struct sockaddr_in *cli) {

    conn_struct_t *curr;
    for (curr = conn_head; curr != NULL; curr = curr->nxt_struct) {
	
	// if a connection exists with same client ip and port, done
	if (curr->cli.sin_port == cli->sin_port &&
		curr->serv.sin_addr.s_addr == cli->sin_addr.s_addr) {
	    printf ("\n Duplicate connection received ");	
	    return 1;
	}
    }
    
    return 0;
}

/* To insert in conn struct */
void
insert_conn_struct (int conn_sockfd,
		    int list_sockfd,
		    struct sockaddr_in *srv,
		    struct sockaddr_in *cli) {
    
    conn_struct_t *curr = conn_head;
    if (!curr) {
	conn_head = get_conn_struct(conn_sockfd, list_sockfd, srv, cli);
	return;
    }

    // Go to the end of list 
    for (curr = conn_head; curr->nxt_struct; curr = curr->nxt_struct);

    curr->nxt_struct = get_conn_struct 
			(conn_sockfd, list_sockfd, srv, cli);
    return;
}
