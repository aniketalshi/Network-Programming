#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"

extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern void free_ifi_info_plus(struct ifi_info *ifihead);

void get_client_ip (struct sock_struct *sock_struct_head, struct sockaddr_in *cli_addr, char *srv_ip) {
    struct sockaddr_in srv, *ntmsk;
    struct in_addr subnet;
    struct sock_struct *st = sock_struct_head;
    int is_loopback = 0;
    char str[INET_ADDRSTRLEN], str1[INET_ADDRSTRLEN];

    inet_pton(AF_INET, srv_ip, &(srv.sin_addr));
    
    while(st) {
	/* if there is perfect match between (server ip & netmask) 
	 * and subnet_addr then server is local
	 */
	ntmsk = ((struct sockaddr_in *)st->net_mask);
	subnet = st->subnetaddr;
	
	if ((srv.sin_addr.s_addr & ntmsk->sin_addr.s_addr) == subnet.s_addr) {
	    cli_addr = ((struct sockaddr_in *)st->ip_addr);
	    if(st->is_loopback == 1) {
		is_loopback = 1;
	    } 
	    
	    inet_ntop(AF_INET, &(cli_addr->sin_addr), str, INET_ADDRSTRLEN); 
	    inet_ntop(AF_INET, &(srv.sin_addr), str1, INET_ADDRSTRLEN); 
    	    printf ("\n Server IP: %s \t Ip choosen by client : %s\n", str1, str); 
	    break;
	}
	
	st = st->nxt_struct;
    }
}

int main(int argc, char* argv[]) {

    struct ifi_info *ifi, *ifihead;
    struct sockaddr_in *sa, cli_addr;
    struct sock_struct *sock_struct_head, *prev, *curr;
    int is_loopback = 0;

    for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
            ifi != NULL; ifi = ifi->ifi_next) {

	is_loopback    = 0;
	sa             = (struct sockaddr_in *)ifi->ifi_addr;
	sa->sin_family = AF_INET;
	sa->sin_port   = htons(SERV_PORT);


	if (ifi->ifi_flags & IFF_LOOPBACK)
            is_loopback = 1;
	    
	curr = get_sock_struct (0, ifi->ifi_addr, ifi->ifi_ntmaddr, is_loopback); 
	   
	if (!sock_struct_head) {
	    sock_struct_head = curr;
	    prev             = curr;
	    continue;
	}

	prev->nxt_struct = curr;
	prev = curr;
    }

    print_sock_struct (sock_struct_head);
    free_ifi_info_plus(ifihead);
   
    get_client_ip(sock_struct_head, &cli_addr, "130.245.1.100"); 

    return 0;
}
