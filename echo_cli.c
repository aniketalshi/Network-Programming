#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <string.h>
#include "unp.h"

int parent_fd;

void echo_fun(FILE *fp, int sockt_fd) {

    char send_line[MAXLINE], recv_line[MAXLINE];
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
                err_quit("Error reading input\n");
            // write the input from buf to socket stream
            Writen (sockt_fd, send_line, strlen(send_line));
        }

        // check if input is received on socket 
        if (FD_ISSET(sockt_fd, &fdset)) {
            // Read the input from socket into buf
            if (Readline(sockt_fd, recv_line, MAXLINE) == 0)
                err_quit("Server terminated, Recvd FIN\n");
            // write output from buf to stdout
            Fputs (recv_line, stdout);
        }
    }
}

void sig_int_handler (int signo){
    write (parent_fd, "Done", 4);
    exit(EXIT_SUCCESS); 
}

int main(int argc, char* argv[]) {

    int sockt_fd, r;
    socklen_t sklen;
    struct sockaddr_in server_addr;
    char recv_line[MAXLINE + 1];

    // check if user entered ip address 
    if (argc < 2) {
        perror("IP Address expected");
        exit(EXIT_FAILURE);
    }
    
    if(argv[2])
        parent_fd = atoi(argv[2]);
    
    // open a ipv4, stream socket  
    if ( (sockt_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // zero out the serveraddr struct and initialize it
    memset(&server_addr, 0, sizeof(server_addr));
    memset(recv_line, 0, sizeof(recv_line));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(5200); 

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
        err_quit("Invalid address %s specified", argv[1]);

    // register signal handler to catch SIGINT 
    Signal (SIGINT, sig_int_handler);
    
    // connect to server
    Connect (sockt_fd, (SA *) &server_addr, sizeof(server_addr));

    echo_fun(stdin, sockt_fd);

    return 0;
}

