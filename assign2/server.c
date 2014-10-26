#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"

#define TOTALFD 256

extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern void free_ifi_info_plus(struct ifi_info *ifihead);

struct sock_struct *sock_struct_head;
struct conn_struct *conn_head;

/* Send file requested by client 
 * Invoked after tcp handshake */
void 
send_file (conn_struct_t *conn, char *filename) {
    
    int file_d, n_bytes;
    char send_buf[CHUNK_SIZE];
    
    memset(send_buf, 0, CHUNK_SIZE);
    printf("\n File Requested by client: %s\n", filename);
    
    /* open file for reading */
    file_d = open(filename, O_RDONLY);
    
    //TODO: Handle case where no file present 
    if (file_d == -1) {
	printf("No such file present !");
	return;
    }
    
    /* read chunks of file */
    while((n_bytes = read (file_d, send_buf, CHUNK_SIZE)) > 0) {
	send_data(conn->conn_sockfd, (void *)send_buf, 
				    n_bytes, __MSG_FILE_DATA);
	memset(send_buf, 0, CHUNK_SIZE);
    }

    /* Send final FIN */
    memset(send_buf, 0, CHUNK_SIZE);
    send_data(conn->conn_sockfd, (void *)send_buf, 0, __MSG_FIN);


    close(file_d);
}

/* Handle each client request */
void 
service_client_req(sock_struct_t *curr, 
	    struct sockaddr_in *cli_addr,
	    char *bufname) {
   
    struct sockaddr_in srv_addr, udpsock;
    conn_struct_t *conn;
    int i, n, connect_fd, socklen, is_client_local = 0;
    int buffer_cap, on = 1;
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
    
    /* set buffer size for socket and socket options */
    buffer_cap = BUFFSIZE;
    setsockopt(connect_fd, SOL_SOCKET, SO_SNDBUF, 
			&buffer_cap, sizeof(buffer_cap));
    
    if (is_client_local) {
	setsockopt(connect_fd, SOL_SOCKET, 
		          SO_DONTROUTE, &on, sizeof(on));
    }

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

    /* Server connects now to client port */
    Connect(connect_fd, (SA *)cli_addr, sizeof(struct sockaddr_in));
    
    /* Second hand shake 
     * send message to client giving the new port number 
     * using the listening socket. 
     */
    /* TODO: Add TCP Time out mechanism here for this packet */
    snprintf(msg, sizeof(msg), "%d", htons(udpsock.sin_port));
    sendto(curr->sockfd, (void *)msg, 
	    sizeof(msg)+1, 0, (struct sockaddr *)cli_addr, sizeof(struct sockaddr));
    
    /* Receive final ack on new UDP port */
    n = recvfrom(connect_fd, msg, MAXLINE, 0, 
    		    (struct sockaddr *)cli_addr, &len);
    
    printf("\n Received final ack from client on new port");

    /* Now we can close listening socket */
    close(curr->sockfd);

    /* insert in conn_struct */
    conn = insert_conn_struct (connect_fd, &srv_addr , cli_addr, &conn_head);
    
    /* start sending file */
    send_file (conn, bufname);
}

void
listen_reqs (struct sock_struct *sock_struct_head) {

    struct sock_struct *curr;
    struct sockaddr_in srv_addr, sock, cli_addr;
    
    int n = 0, sockfd, maxfd = 0, i, is_client_local = 0, retval;
    pid_t pid;
    char msg[MAXLINE];
    socklen_t len;
    
    fd_set fdset, tempset;
    FD_ZERO(&fdset);
    FD_ZERO(&tempset);
    
    len = sizeof(struct sockaddr_in);
   
    for(curr = sock_struct_head; curr != NULL; curr = curr->nxt_struct) {
        FD_SET(curr->sockfd, &tempset);
        if (curr->sockfd > maxfd)
            maxfd = curr->sockfd;
    }

    while(1) {
	fdset = tempset;
	printf ("aaa\n");	
	retval = select (maxfd + 1, &fdset, NULL, NULL, NULL);
	if (retval < 0) {
	    if (errno == EINTR)
		continue;
	    perror("select error");
	}

	for(curr = sock_struct_head; curr != NULL; curr = curr->nxt_struct) {
	    if (FD_ISSET(curr->sockfd, &fdset)) {
		
		bzero(&cli_addr, sizeof(cli_addr));
		n = recvfrom(curr->sockfd, msg, MAXLINE, 0, 
				    (struct sockaddr *)&cli_addr, &len);
		
		printf("\n Client : %s", print_ip_port(&cli_addr));
		
		// If request with same ip/port already exist, ignore it
		if (check_new_conn (&cli_addr, conn_head))
		    continue;

		if ( (pid = fork()) == 0) { //child proc
		    service_client_req(curr, &cli_addr, msg);
		
		} else {
		    //TODO: store this pid of child in our table
		}

		return;
	    }
	}
    }
}

int 
main(int argc, char* argv[]) {
   
    struct ifi_info *ifi, *ifihead;
    struct sock_struct *prev, *curr;
    struct sockaddr_in *sa;
    const int on = 1;
    int is_loopback = 0, sockfd;
    
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
