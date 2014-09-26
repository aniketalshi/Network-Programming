#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <string.h>
#include "unp.h"

void echo_fun(FILE *fp, int sockt_fd) {

    char send_line[MAXLINE], recv_line[MAXLINE];
    
    while(Fgets(send_line, MAXLINE, fp) != NULL) {
        
        Writen(sockt_fd, send_line, strlen(send_line));
        if(Readline(sockt_fd, recv_line, MAXLINE) == 0)
            err_quit("Server terminated");
    
        Fputs(recv_line, stdout);
    }
}

int main(int argc, char* argv[]) {

    int sockt_fd, r;
    socklen_t sklen;
    struct sockaddr_in server_addr;
    char recv_line[MAXLINE + 1];

    // check if user entered ip address 
    if (argc < 2)
        err_quit("IP Address expected");
    
    // open a ipv4, stream socket  
    if ( (sockt_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error creating socket %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
   
    // zero out the serveraddr struct and initialize it
    memset(&server_addr, 0, sizeof(server_addr));
    memset(recv_line, 0, sizeof(recv_line));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(5200); 
    
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
        err_quit("Invalid address %s specified", argv[1]);
    
    //connect to server
    Connect (sockt_fd, (SA *) &server_addr, sizeof(server_addr));

    echo_fun(stdin, sockt_fd);
    
    return 0;
}
    
