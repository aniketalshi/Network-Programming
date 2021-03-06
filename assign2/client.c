#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"
#include "controls.h"
#include "rtt.c"

#define BYTE 8
extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern void free_ifi_info_plus(struct ifi_info *ifihead);
struct sock_struct *sock_struct_head;

pthread_t cons_thread;
pthread_t prod_thread;
/* get the total number of preceding bits set in a number */
int
get_bits(long number){
    int count = 0;
    long mask = 0x80000000;

    while((number & mask) == mask){
        count++;
        number = number << 1;
    }
    return count;
}

void 
get_client_ip (struct sock_struct *sock_struct_head, 
		    struct sockaddr_in *cli_addr,
		    char *srv_ip, int *local_ip) {
    
    struct sockaddr_in srv, ntmsk, subnet;
    struct sock_struct *st = sock_struct_head;
    int is_local = 0;
    unsigned int max_bits = 0;
    inet_pton(AF_INET, srv_ip, &(srv.sin_addr));
   
    while(st) {
	ntmsk  = st->net_mask;
	subnet = st->subnetaddr;

        /* check if an IP is local */
        if(subnet.sin_addr.s_addr == (srv.sin_addr.s_addr & ntmsk.sin_addr.s_addr)){
            is_local = 1;
            
	    /* return, if current ip address loopback address */
            if(st->is_loopback == 1) { 
                cli_addr->sin_addr = st->ip_addr.sin_addr;
		*local_ip          = 1;
                return;
            }

            if(get_bits(ntmsk.sin_addr.s_addr) > max_bits){
                max_bits = get_bits(ntmsk.sin_addr.s_addr); 
                cli_addr->sin_addr = st->ip_addr.sin_addr; 
            }
        }
        
        if( !is_local && (st->is_loopback != 1)){
            cli_addr->sin_addr = st->ip_addr.sin_addr;
        }

	st = st->nxt_struct;
    }
    
    return;
}


void
print_srvr_addr (int sockfd) {

    int len_inet; /* length */

    struct sockaddr_in adr_inet;/* AF_INET */
    int z;
    len_inet = sizeof adr_inet;
    
    //TODO: Print out server info calling getpeername
    z = getpeername(sockfd, (struct sockaddr *)&adr_inet, &len_inet);
    if ( z == -1) {
        printf ("getpeername failed.\n"); /* Failed */
    }

    /*
     * Convert address into a string
     * form that can be displayed:
     */
    printf("Server address: %s:%u \n",
            inet_ntoa(adr_inet.sin_addr),
            (unsigned)ntohs(adr_inet.sin_port));
}

void 
cli_func (int sockfd, SA *srv_addr, socklen_t len, char* file_name, int window_size) { 
    char buf[MAXLINE]; 
    char sendline[MAXLINE];
    msg_hdr_t *msg_hdr;
    Connect(sockfd, (SA *)srv_addr, len);

    print_srvr_addr (sockfd);
    write(sockfd, file_name, strlen(file_name));
    
    /* Second Hand-shake receive new connection port from server
     * connect to this new port
     */
    read(sockfd, buf, MAXLINE);
    
    printf("Second hand-shake. ");

    ((struct sockaddr_in *)srv_addr)->sin_port = htons(atoi(buf));
   
    msg_hdr = get_hdr(__MSG_ACK, 0, window_size);

    /* connect to this new port */
    Connect(sockfd, (SA *)srv_addr, len);
    
    memset(sendline, 0, sizeof(sendline));
    sprintf(sendline, "%d", window_size);
    
    /* Third Hand shake */
    Write(sockfd, (void *) msg_hdr, sizeof(msg_hdr_t));
    
    printf ("New server port: ");
    print_srvr_addr (sockfd);
    /* Start reading data from client */
    r_win = r_window_init(); 
    
    pthread_create(&prod_thread, NULL, receiving_func, (void *)&sockfd);
    pthread_create(&cons_thread, NULL, consumer_func, NULL);
    
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);
    //receiving_func((void *)&sockfd);
}

int 
main(int argc, char* argv[]) {
    struct ifi_info *ifi, *ifihead;
    struct sockaddr_in *sa, cli_addr, temp_addr, sock, srv_addr;
    struct sock_struct *prev, *curr;
    int is_loopback = 0, connect_fd, socklen, is_local = 0;
    char str[INET_ADDRSTRLEN], str1[INET_ADDRSTRLEN];
    char buf[MAXLINE];
    socklen_t len;
    char server_ip[MAXLINE], file_name[MAXLINE];
    int server_port, window_size, read_rate, buffer_cap, on = 1;
    
    /* read input from client.in file */
    client_input();

    for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
            ifi != NULL; ifi = ifi->ifi_next) {

	is_loopback    = 0;
	sa             = (struct sockaddr_in *)ifi->ifi_addr;
	sa->sin_family = AF_INET;
	sa->sin_port   = htons(SERV_PORT);


	if (ifi->ifi_flags & IFF_LOOPBACK)
            is_loopback = 1;
	    
	curr = get_sock_struct (0, (struct sockaddr_in *)ifi->ifi_addr,
				   (struct sockaddr_in *)ifi->ifi_ntmaddr, is_loopback); 
	   
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
    
    get_client_ip(sock_struct_head, &temp_addr, cli_params.server_ip, &is_local); 
    
    printf ("\nIp choosen by client : %s\n", inet_ntoa(temp_addr.sin_addr)); 
    
    connect_fd = Socket(AF_INET, SOCK_DGRAM, 0); 

    /* set socket options */
    /* set buffer size for socket and socket options */
    buffer_cap = BUFF_SIZE;
    setsockopt(connect_fd, SOL_SOCKET, SO_SNDBUF, 
			&buffer_cap, sizeof(buffer_cap));
    
    setsockopt(connect_fd, SOL_SOCKET, SO_RCVBUF, 
			&buffer_cap, sizeof(buffer_cap));
    
    if (is_local) {
	setsockopt(connect_fd, SOL_SOCKET, 
		          SO_DONTROUTE, &on, sizeof(on));
    }
    
    memset(&cli_addr, 0, sizeof(cli_addr));
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
    
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family  = AF_INET;
    srv_addr.sin_port    = htons(cli_params.server_port);
    inet_pton (AF_INET, cli_params.server_ip, &srv_addr.sin_addr);

    // First Hand-shake
    cli_func (connect_fd, (SA *)&srv_addr, sizeof(srv_addr), cli_params.file_name, cli_params.window_size); 

    return 0;
}

