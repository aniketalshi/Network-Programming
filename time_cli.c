#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <string.h>
#include "unp.h"

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
        perror("Error creating socket \n");
        exit(EXIT_FAILURE);
    }
    
    // zero out the serveraddr struct and initialize it
    memset(&server_addr, 0, sizeof(server_addr));
    memset(recv_line, 0, sizeof(recv_line));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(5000); 
    
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
        err_quit("Invalid address %s specified", argv[1]);
    
    //connect to server
    Connect (sockt_fd, (SA *) &server_addr, sizeof(server_addr));

    //read from server
    while ((r = read (sockt_fd, recv_line, sizeof(recv_line)-1)) > 0) {
        recv_line[r] = '0';
        if (fputs(recv_line, stdout) == EOF) 
            err_sys("output error");
    }

    if (r < 0)
        err_sys("read error");

    return  0;
}

