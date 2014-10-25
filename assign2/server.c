#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"

#define TOTALFD 256

extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern void free_ifi_info_plus(struct ifi_info *ifihead);

void 
client_func(sock_struct_t *curr, 
	    struct sockaddr_in *cli_addr) {
   
    struct sockaddr_in srv_addr, udpsock;
    int i, n, connect_fd, socklen, is_client_local = 0;
    char msg[MAXLINE];
    socklen_t len;

    /* Starting with fd:3, close all fds except the one on listening */
    for (i = 3; i < MAXFD; ++i) {
        if (i == curr->sockfd)
            continue;
        close(i);
    }
    
    /* check if this is on local ip or loopback */
    if(curr->is_loopback == 1) {
        
        printf("\n This request is on loopback interface");
    } else if ((curr->net_mask.sin_addr.s_addr &
    	cli_addr->sin_addr.s_addr) == curr->subnetaddr.sin_addr.s_addr) {
    	
        is_client_local = 1;
    	printf("\n The client is on same local subnet as the server");
    }
    
    /* Create a new UDP socket and bind to ephemeral port */
    connect_fd = Socket(AF_INET, SOCK_DGRAM, 0); 
    
    bzero(&srv_addr, sizeof(srv_addr));
    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_port        = htons(0);
    srv_addr.sin_addr.s_addr = curr->ip_addr.sin_addr.s_addr;
    
    if (bind(connect_fd, (SA *)&srv_addr, sizeof(srv_addr)) < 0)
        perror("Bind error");

    socklen = sizeof(struct sockaddr_in);
    
    // get the ephemeral port no assigned to this socket
    if (getsockname(connect_fd, (void *)&udpsock, &socklen) == -1) 
        perror("getsockname error");
    
    printf ("\n Server : %s", print_ip_port(&udpsock));
    
    /* Second hand shake 
     * send message to client giving the new port number 
     * using the listening socket. 
     */
    snprintf(msg, sizeof(msg), "%d", htons(udpsock.sin_port));
    sendto(curr->sockfd, (void *)msg, 
	    sizeof(msg)+1, 0, (struct sockaddr *)cli_addr, sizeof(struct sockaddr));
    
    /* Receive final ack on new UDP port */
    n = recvfrom(connect_fd, msg, MAXLINE, 0, 
    		    (struct sockaddr *)cli_addr, &len);
    
    printf("\n Received final ack from client on new port");

}

void
listen_reqs (struct sock_struct *sock_struct_head) {

    struct sock_struct *curr;
    struct sockaddr_in srv_addr, sock, cli_addr;
    
    int n = 0, sockfd, maxfd = 0, i, is_client_local = 0;
    pid_t pid;
    char msg[MAXLINE];
    socklen_t len;
    
    fd_set fdset, tempset;
    FD_ZERO(&fdset);
    FD_ZERO(&tempset);
    
    len = sizeof(struct sockaddr_in);
    bzero(&cli_addr, sizeof(cli_addr));
   
    for(curr = sock_struct_head; curr != NULL; curr = curr->nxt_struct) {
        FD_SET(curr->sockfd, &fdset);
        if (curr->sockfd > maxfd)
            maxfd = curr->sockfd;
    }
    tempset = fdset;

    while(1) {
	fdset = tempset;
	
	if (select (maxfd + 1, &fdset, NULL, NULL, NULL) < 0) {
	    if (errno == EINTR)
		continue;
	    perror("select error");
	}

	for(curr = sock_struct_head; curr != NULL; curr = curr->nxt_struct) {
	    if (FD_ISSET(curr->sockfd, &fdset)) {
		
		n = recvfrom(curr->sockfd, msg, MAXLINE, 0, 
				    (struct sockaddr *)&cli_addr, &len);
		
		printf("\n Client : %s", print_ip_port(&cli_addr));
		
		// If request with same ip/port already exist, ignore it
		if (check_new_conn (&cli_addr))
		    continue;

		if ( (pid = fork()) == 0) { //child proc
		    client_func(curr, &cli_addr);
		
		} else {
		    //TODO: store this pid of child in our table
		}

	    }
	}
    }
}

int 
main(int argc, char* argv[]) {
    
    struct sock_struct *sock_struct_head;
    struct ifi_info *ifi, *ifihead;
    struct sock_struct *prev, *curr;
    struct sockaddr_in *sa;
    const int on = 1;
    int is_loopback = 0, sockfd;
    int server_port = 0, max_win_size = 0;                                                                                                                                     
    
    /* read input from server.in input file */
    server_input(&server_port, &max_win_size);

    /* Iterate over all interfaces and store values in struct */
    for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
            ifi != NULL; ifi = ifi->ifi_next) {
		
	if (ifi->ifi_addr != NULL && ifi->ifi_ntmaddr != NULL) {
	    is_loopback = 0;
	     
	    sockfd = Socket(AF_INET, SOCK_DGRAM, 0); 
	    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	    
	    sa             = (struct sockaddr_in *)ifi->ifi_addr;
	    sa->sin_family = AF_INET;
	    sa->sin_port   = htons(5500);
	    
	    if (bind(sockfd, (SA *)sa, sizeof(*sa)) < 0)
		perror("Bind error");
	    
	    if (ifi->ifi_flags & IFF_LOOPBACK)
		is_loopback = 1;
	    
	    curr = get_sock_struct (sockfd, (struct sockaddr_in *)ifi->ifi_addr, 
					    (struct sockaddr_in *)ifi->ifi_ntmaddr, is_loopback); 
	    if (!curr) {
		fprintf(stderr, "Failed to get subnet mask and ip");
		return;
	    }
	    
	    if (!prev) {
	        sock_struct_head = curr;
	        prev = curr;
	        continue;
	    }

	    prev->nxt_struct = curr;
	    prev = curr;
	}
    }
    print_sock_struct(sock_struct_head);

    listen_reqs (sock_struct_head);
    return 0;
}
