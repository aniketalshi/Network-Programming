#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"

#define BYTE 8
extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern void free_ifi_info_plus(struct ifi_info *ifihead);

int get_match (unsigned long num1, unsigned long num2){
    int i, cnt = 0; 
    for (i = 0; i < sizeof(unsigned long); ++i) {
	int offset = (3-i)*BYTE;
	
	if (((num1 << (i*BYTE)) >> offset) == 0)
	    break;

	if(~((num1 << (i*BYTE) >> offset)^(num2 << (i*BYTE)>> offset)) == 0xFFFFFFFF) {
	    cnt++;
	    continue;
	}
	break;
    }
    return cnt;
}


void get_client_ip (struct sock_struct *sock_struct_head, struct sockaddr_in *cli_addr, char *srv_ip) {
    struct sockaddr_in srv, *ntmsk;
    struct in_addr subnet;
    struct sock_struct *st = sock_struct_head;
    int is_loopback = 0, max_match = 0, match;
    char str[INET_ADDRSTRLEN], str1[INET_ADDRSTRLEN];

    inet_pton(AF_INET, srv_ip, &(srv.sin_addr));
    
    while(st) {
	ntmsk  = ((struct sockaddr_in *)st->net_mask);
	subnet = st->subnetaddr;
	match = get_match((srv.sin_addr.s_addr & ntmsk->sin_addr.s_addr), subnet.s_addr);
	//printf("match %d\n", match);
	
	if (match > max_match) {
	    max_match = match; 
	    cli_addr  = ((struct sockaddr_in *)st->ip_addr);
	    if(st->is_loopback == 1) {
		is_loopback = 1;
	    } 
	}
	st = st->nxt_struct;
    }
	    
    inet_ntop(AF_INET, &(cli_addr->sin_addr), str, INET_ADDRSTRLEN); 
    inet_ntop(AF_INET, &(srv.sin_addr), str1, INET_ADDRSTRLEN); 
    printf ("\n Server IP: %s \t Ip choosen by client : %s\n", str1, str); 
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
   
    get_client_ip(sock_struct_head, &cli_addr, "130.245.1.182"); 

    return 0;
}
