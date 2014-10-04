#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <string.h>
#include "unp.h"

int parent_fd;

void err_write(char *str) {
    write (parent_fd, str, strlen(str));
    exit(EXIT_SUCCESS); 
}

void echo_fun(FILE *fp, int sockt_fd, char *srv_ip) {

    char send_line[MAXLINE], recv_line[MAXLINE], buf[100];
    fd_set fdset;
    FD_ZERO(&fdset);
    
    while(1) {
        FD_SET (fileno(fp), &fdset);
        FD_SET (sockt_fd, &fdset);
       
        /* Blocking I/O on stdin and socket simultaneously
         * Returns with number of fd's set 
         */
        select ((max(fileno(fp), sockt_fd) + 1), 
                        &fdset, NULL, NULL, NULL);
        
        // check if the input is received on stdin
        if (FD_ISSET(fileno(fp), &fdset)) {
            // get the input from stdin into buf
            if (Fgets (send_line, MAXLINE, fp) == NULL)
                err_write("EOF typed by user\n");
            // write the input from buf to socket stream
            Writen (sockt_fd, send_line, strlen(send_line));
        }

        // check if input is received on socket 
        if (FD_ISSET(sockt_fd, &fdset)) {
            // Read the input from socket into buf
            if (Readline(sockt_fd, recv_line, MAXLINE) == 0)
                err_write("Server terminated, Recvd FIN\n");
            // write output from buf to stdout
            fputs (recv_line, stdout);
        }
    }
}

void sig_int_handler (int signo){
    write (parent_fd, "Done", 4);
    exit(EXIT_SUCCESS); 
}

int main(int argc, char* argv[]) {

    int sockt_fd, sock_flags, r;
    socklen_t sklen;
    struct sockaddr_in server_addr;
    struct in_addr ipv4addr;
    struct in_addr **addr_list;
    struct hostent *hnet;

    // check if user entered ip address 
    if (argc < 2) {
        err_write("IP Address expected");
    }
    
    // if pipe is not set by parent exit
    if(argv[2] == NULL) 
        exit(EXIT_FAILURE);
        
    if(argv[2])
        parent_fd = atoi(argv[2]);
   
    // open a ipv4, stream socket  
    if ( (sockt_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        err_write("Error creating socket");
    }
    

    // zero out the serveraddr struct and initialize it
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family  = AF_INET;
    server_addr.sin_port    = htons(5200); 
    //server_addr.sin_addr    = ipv4addr;

    if (inet_pton(AF_INET, argv[1], &(server_addr.sin_addr)) == 1) {
           
         hnet = gethostbyaddr(&(server_addr.sin_addr), sizeof(struct in_addr), AF_INET);
    } else {
        if (hnet = gethostbyname(argv[1])) {
            addr_list = (struct in_addr **)hnet->h_addr_list; 
            inet_pton(AF_INET, inet_ntoa(*addr_list[0]), &(server_addr.sin_addr));
        } else {
            perror("Error with ip hostname");
            exit(EXIT_FAILURE);
        }
    }
   
    printf("The Server is %s \t ip address: %s\n", 
                                        hnet->h_name, inet_ntoa(server_addr.sin_addr));
   
   // register signal handler to catch SIGINT 
    signal (SIGINT, sig_int_handler);
   
    //inet_ntop(AF_INET, &(server_addr.sin_addr), str, INET_ADDRSTRLEN); 
    //printf("ip addr: %s\n", str);
    
    
    // Non blocking connect call
    if (connect (sockt_fd, (SA *) &server_addr, sizeof(server_addr)) < 0)
        if(errno != EINPROGRESS)
            err_write("Connect Error");

    echo_fun(stdin, sockt_fd, argv[1]);
    return 0;
}

