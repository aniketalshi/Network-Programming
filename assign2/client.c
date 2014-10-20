#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"

#define BYTE 8
extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern void free_ifi_info_plus(struct ifi_info *ifihead);

/* compare two long num by each byte and check on how many bytes they match */
int get_match (unsigned long num1, unsigned long num2){
    int i, j, cnt = 0; 
    for (i = 0; i < sizeof(unsigned long); ++i) {
	int offset = (3-i)*BYTE;
	char n1    = (num1 << (i*BYTE) >> offset);
	char n2    = (num2 << (i*BYTE) >> offset);
	
	/* if byte is zero, we are done */
	if (n1 == 0)
	    break;
	
	/* if they match completely */
	if(~(n1^n2) == 0xFF) {
	    cnt = cnt + 8;
	    continue;
	}

	/* check how many bits match */
	for(j = 0; j < sizeof(char); ++j) {
	    if(~((n1 << j >> BYTE-1-j) ^ (n2 << j >> BYTE-1-j))){
		cnt++;
	    } else {
		break;
	    }
	}
	
	/* if they dont match break */
	break;
    }
    return cnt;
}

void get_client_ip (struct sock_struct *sock_struct_head, 
		    struct sockaddr_in *cli_addr,
		    char *srv_ip) {
    
    struct sockaddr_in srv, *ntmsk;
    struct in_addr subnet;
    struct sock_struct *st = sock_struct_head;
    int is_loopback = 0, max_match = 0, match;
    char str[INET_ADDRSTRLEN], str1[INET_ADDRSTRLEN];

    inet_pton(AF_INET, srv_ip, &(srv.sin_addr));
    
    while(st) {
	ntmsk  = ((struct sockaddr_in *)st->net_mask);
	subnet = st->subnetaddr;
	match  = get_match((srv.sin_addr.s_addr & ntmsk->sin_addr.s_addr), subnet.s_addr);
	//printf("match %d\n", match);
	
	if (match > max_match) {
	    max_match           = match; 
	    cli_addr->sin_addr.s_addr  = ((struct sockaddr_in *)st->ip_addr)->sin_addr.s_addr;
	    if(st->is_loopback == 1) {
		is_loopback = 1;
	    } 
	}
	st = st->nxt_struct;
    }
    
    return;
}

int main(int argc, char* argv[]) {

    struct ifi_info *ifi, *ifihead;
    struct sockaddr_in *sa, cli_addr, temp_addr, sock;
    struct sock_struct *sock_struct_head, *prev, *curr;
    int is_loopback = 0, connect_fd, socklen;
    char str[INET_ADDRSTRLEN], str1[INET_ADDRSTRLEN];
    socklen_t len;

    for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
            ifi != NULL; ifi = ifi->ifi_next) {

	is_loopback    = 0;
	sa             = (struct sockaddr_in *)ifi->ifi_addr;
	sa->sin_family = AF_INET;
	sa->sin_port   = htons(SERV_PORT);


	if (ifi->ifi_flags & IFF_LOOPBACK)
            is_loopback = 1;
	    
	curr = get_sock_struct (0, ifi->ifi_addr, ifi->ifi_ntmaddr, is_loopback); 
	   
	if (!sock_struct_head && !prev) {
	    sock_struct_head = curr;
	    prev             = curr;
	    continue;
	}

	prev->nxt_struct = curr;
	prev = curr;
    }

    print_sock_struct (sock_struct_head);
    free_ifi_info_plus(ifihead);
    
    get_client_ip(sock_struct_head, &temp_addr, "130.245.1.182"); 
    
    printf ("\nIp choosen by client : %s\n", inet_ntoa(temp_addr.sin_addr)); 
    
    memset(&cli_addr, 0, sizeof(cli_addr));
    connect_fd           = Socket(AF_INET, SOCK_DGRAM, 0); 
    cli_addr.sin_addr    = temp_addr.sin_addr;
    cli_addr.sin_family  = AF_INET;
    cli_addr.sin_port    = htons(0);
    
    if (bind(connect_fd, (SA *)&cli_addr, sizeof(cli_addr)) < 0)
       perror("Bind error");
    
    socklen = sizeof(cli_addr);
    // get the port no assigned to this socket
    if (getsockname(connect_fd, (SA *)&sock, &socklen) == -1) 
       perror("getsockname error");
    
    printf("client port          : %d\n", ntohs(sock.sin_port));

    return 0;
}
