#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"

#define TOTALFD 256

extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern void free_ifi_info_plus(struct ifi_info *ifihead);

void create_child (struct sock_struct *sk) {
    pid_t pid;
    int i, n, connect_fd, socklen;
    char msg[MAXLINE];
    socklen_t len;
    struct sockaddr_in cli_addr, srv_addr, sock;
    struct sockaddr_in *sockaddr;

    if ( (pid = fork()) == 0) { //child proc	
	// close all socket descp except sockfd
     	for (i = 0; i < TOTALFD; ++i) {
     	    if (i == sk->sockfd)
     	        continue;
     	    close(i);
     	}
	
	// Read the request
	n = Recvfrom (sk->sockfd, msg, MAXLINE, 0, (SA *)&cli_addr, &len);
     	sockaddr = ((struct sockaddr_in *) sk->ip_addr);
	
	// check if this is loopback / local addr
     	if (sk->is_loopback) {
	    printf("\n Client connected on loopback interface");

	} else {
	   if ((cli_addr.sin_addr.s_addr & sockaddr->sin_addr.s_addr)
					== sk->subnetaddr.s_addr)
	    printf("\n Client connected on local ip");
	}

	connect_fd = Socket(AF_INET, SOCK_DGRAM, 0); 
	
	bzero(&srv_addr, sizeof(srv_addr));
	srv_addr.sin_family      = AF_INET;
	srv_addr.sin_port        = htons(0);
	srv_addr.sin_addr.s_addr = sockaddr->sin_addr.s_addr;
	
	if (bind(connect_fd, (SA *)&srv_addr, sizeof(srv_addr)) < 0)
	    perror("Bind error");
	
	socklen = sizeof(srv_addr);
	// get the port no assigned to this socket
	if (getsockname(connect_fd, (void *)&sock, &socklen) == -1) 
	    perror("getsockname error");
	
	printf("\n Connection information :");
	printf("\n Client ip: %s\t Client port: %s\t", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
	printf("\n Server ip: %s\t Server port: %s\t", inet_ntoa(sock.sin_addr), ntohs(sock.sin_port));
	
    } else {
	return;
    }
}


int main(int argc, char* argv[]) {
	
    struct ifi_info *ifi, *ifihead;
    struct sock_struct *sock_struct_head, *prev, *curr;
    struct sockaddr_in *sa;
    int sockfd, maxfd = 0;
    const int on = 1;
    int is_loopback = 0;
    fd_set fdset;
    FD_ZERO(&fdset);

    for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
            ifi != NULL; ifi = ifi->ifi_next) {
	
	is_loopback = 0;
	
	if (ifi->ifi_addr != NULL && ifi->ifi_ntmaddr != NULL) {
	     
	    sockfd = Socket(AF_INET, SOCK_DGRAM, 0); 
	    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	    
	    sa             = (struct sockaddr_in *)ifi->ifi_addr;
	    sa->sin_family = AF_INET;
	    sa->sin_port   = htons(SERV_PORT);
	    
	    if (bind(sockfd, (SA *)sa, sizeof(*sa)) < 0)
		perror("Bind error");
	    
	    if (ifi->ifi_flags & IFF_LOOPBACK)
		is_loopback = 1;
	    
	    curr = get_sock_struct (sockfd, ifi->ifi_addr, ifi->ifi_ntmaddr, is_loopback); 
	   
	    if (!sock_struct_head) {
	        sock_struct_head = curr;
	        prev             = curr;
	        continue;
	    }

	    prev->nxt_struct = curr;
	    prev = curr;
	}
    }
    
    print_sock_struct (sock_struct_head);
    free_ifi_info_plus(ifihead);
    
    while(1) {
	for(curr = sock_struct_head; curr != NULL; curr = curr->nxt_struct) {
	    FD_SET(curr->sockfd, &fdset);
	    if (curr->sockfd > maxfd)
		maxfd = curr->sockfd;
	}
	
	if (select (maxfd + 1, &fdset, NULL, NULL, NULL) < 0) {
	    if (errno == EINTR)
		continue;
	    perror("select error");
	}

	for(curr = sock_struct_head; curr != NULL; curr = curr->nxt_struct) {
	    if (FD_ISSET(curr->sockfd, &fdset)) {
		create_child (curr);
	    }
	}
    }
    
    return 0;
}
