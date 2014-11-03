#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "structs.h"
#include "controls.h"
#include "rtt.c"

#define TOTALFD 256

extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern void free_ifi_info_plus(struct ifi_info *ifihead);

struct sock_struct *sock_struct_head;
struct conn_struct *conn_head;

static sigjmp_buf jmp;
static struct rtt_info rtt_s;

void timer_signal_handler(int signo){
    siglongjmp(jmp, 1);
}

void sigchild_handler(int signo) {
    printf("\n got in sigchild handler\n");
    pid_t pid;
    int   status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
       delete_conn_struct(&conn_head, pid); 
    }
}

/* Send file requested by client 
 * Invoked after tcp handshake */
void 
send_file (conn_struct_t *conn, char *filename, int client_win_size) {
    
    int file_d, n_bytes = 0;
    char send_buf[CHUNK_SIZE * 10];
    
    memset(send_buf, 0, CHUNK_SIZE * 10);
    printf("\n File Requested by client: %s\n", filename);
    
    /* open file for reading */
    file_d = open(filename, O_RDONLY);
    
    //TODO: Handle case where no file present 
    if (file_d == -1) {
	printf("No such file present !");
	return;
    }
    
    printf("\n==========Starting File Transfer =============\n");
    
    /* read chunks of file */
    //while((n_bytes = read (file_d, send_buf, CHUNK_SIZE * 10)) > 0) {
    //    printf("\n read %d bytes\n", n_bytes); 
    //    
    //    sending_func(conn->conn_sockfd, (void *)send_buf, n_bytes, client_win_size);
    //    memset(send_buf, 0, CHUNK_SIZE * 10);
    //}

    sending_func_new(conn->conn_sockfd, file_d);
    /* send the fin packet */ 
    send_fin (conn->conn_sockfd, (void *)send_buf);
    printf("\n==========File Transfer complete =============\n");

    close(file_d);
}

/* Handle each client request */
void 
service_client_req(sock_struct_t *curr, 
	           struct sockaddr_in *cli_addr,
                   conn_struct_t *conn,
	           char *bufname) {
   
    struct sockaddr_in srv_addr, udpsock;
    int i, n, connect_fd, socklen, is_client_local = 0;
    int buffer_cap, on = 1;
    char msg[MAXLINE];
    socklen_t len;
    msg_hdr_t *msg_hdr;

    signal(SIGALRM, timer_signal_handler);
    memset(&rtt_s, 0, sizeof(struct rtt_info));
    rtt_init( &rtt_s );
    
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
    snprintf(msg, sizeof(msg), "%d", htons(udpsock.sin_port));
    sendto(curr->sockfd, (void *)msg, 
    	    sizeof(msg)+1, 0, (struct sockaddr *)cli_addr, sizeof(struct sockaddr));
   
    /* Initialize the retransmit count to 0. */
    rtt_newpack (&rtt_s);

    /* Start the timer. */
    rtt_start_timer (3000);
    
    /* Handle the timer signal, and retransmit. */
    if (sigsetjmp(jmp, 1) != 0) {
        
        /* Check if maximum retransmits have already been sent. */
        if (rtt_timeout( &rtt_s ) == 0) {
            printf("\n\n No communication from client; Sending packet again.\n");
            rtt_start_timer( 3000 );
            
            /* Send the packet again. */
            sendto(curr->sockfd, (void *)msg, 
	        sizeof(msg)+1, 0, (struct sockaddr *)cli_addr, sizeof(struct sockaddr));
            
            /* Send on the old port as well. */
            sendto(connect_fd, (void *)msg, 
	        sizeof(msg)+1, 0, (struct sockaddr *)cli_addr, sizeof(struct sockaddr));
        
        } else{
            /* Maximum number of retransmits sent; give up. */
            printf("\n Client not responding. Terminating connection\n");
            return;
        }
    }
   
    msg_hdr = get_hdr(__MSG_ACK, 0, 0);
    /* Receive final ack on new UDP port */
    n = recvfrom(connect_fd, (msg_hdr_t *) msg_hdr, sizeof(msg_hdr_t) , 0, 
    		    (struct sockaddr *)cli_addr, &len);
    
    int client_win_size = msg_hdr->win_size;
    /* Turn off the alarm */
    rtt_start_timer(0);
    
    printf("\n Received final ack from client on new port");

    /* Now we can close listening socket */
    close(curr->sockfd);

    /* insert in conn_struct */
    //conn = insert_conn_struct (connect_fd, &srv_addr , cli_addr, &conn_head);
    insert_values_conn_struct(cli_addr, &srv_addr, connect_fd, conn);
    
    /* start sending file */
    send_file (conn, bufname, client_win_size);
}

void
listen_reqs (struct sock_struct *sock_struct_head) {

    struct sock_struct *curr;
    struct sockaddr_in srv_addr, sock, cli_addr;
    
    int n = 0, sockfd, maxfd = 0, i, is_client_local = 0, retval;
    pid_t pid;
    char msg[MAXLINE];
    socklen_t len;
    conn_struct_t *conn;
    sigset_t signal_set;
    
    fd_set fdset, tempset;
    FD_ZERO(&fdset);
    FD_ZERO(&tempset);
    
    len = sizeof(struct sockaddr_in);
    maxfd = -1; 

    for(curr = sock_struct_head; curr != NULL; curr = curr->nxt_struct) {
        FD_SET(curr->sockfd, &tempset);
        if (curr->sockfd > maxfd)
            maxfd = curr->sockfd;
    }
    FD_ZERO(&fdset);
    
    while(1) {
	fdset = tempset;
        
        /* block the sigchild signal */
        sigemptyset(&signal_set);
        sigaddset(&signal_set, SIGCHLD);

	retval = select (maxfd + 1, &fdset, NULL, NULL, NULL);
	if (retval < 0) {
	    if (errno == EINTR)
		continue;
	    perror("select error");
	}
	
        for(curr = sock_struct_head; curr != NULL; curr = curr->nxt_struct) {
            
            //sigprocmask(SIG_BLOCK, &signal_set, NULL);
	    
            if (FD_ISSET(curr->sockfd, &fdset)) {
		
		bzero(&cli_addr, sizeof(cli_addr));
		bzero(msg, MAXLINE);
		
		n = recvfrom(curr->sockfd, msg, MAXLINE, 0, 
				    (struct sockaddr *)&cli_addr, &len);
		
		printf("\n Client : %s", print_ip_port(&cli_addr));
		msg[n] = '\0';
		
		// If request with same ip/port already exist, ignore it
		if (check_new_conn (&cli_addr, conn_head))
		    continue;

                conn = insert_conn_struct (&cli_addr, &conn_head);
		
                if ((pid = fork()) == 0) { //child proc
		    service_client_req(curr, &cli_addr, conn, msg);
	            exit(0);	
		} else {
		    //TODO: store this pid of child in our table
                    printf("\n %d came in parent\n", pid);
                    conn->pid = pid;
                    //sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
		}
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
    int server_port = 0;                                                                                                                                     
    /* read input from server.in input file */
    server_input(&server_port, &max_sending_win_size);

    signal(SIGCHLD, sigchild_handler);
    /* Iterate over all interfaces and store values in struct */
    for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
            ifi != NULL; ifi = ifi->ifi_next) {
		
	if (ifi->ifi_addr != NULL && ifi->ifi_ntmaddr != NULL) {
	    is_loopback = 0;
	     
	    sockfd = Socket(AF_INET, SOCK_DGRAM, 0); 
	    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	    
	    sa             = (struct sockaddr_in *)ifi->ifi_addr;
	    sa->sin_family = AF_INET;
	    sa->sin_port   = htons(server_port);
	    
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
