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

void sig_int_handler (int signo){
    write (parent_fd, "Done", 4);
    exit(EXIT_SUCCESS); 
}


int main(int argc, char* argv[]) {

    int sockt_fd, r;
    socklen_t sklen;
    struct sockaddr_in server_addr;
    char recv_line[MAXLINE + 1], buf[MAXLINE];
    struct hostent *hnet;
    struct in_addr **addr_list;

    // check if user entered ip address 
    if (argc < 2)
        err_quit("IP Address expected");
  
    if(argv[2])
        parent_fd = atoi(argv[2]);
    
    
    // open a ipv4, stream socket  
    if ( (sockt_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating socket \n");
        exit(EXIT_FAILURE);
    }
    
    // zero out the serveraddr struct and initialize it
    memset(&server_addr, 0, sizeof(server_addr));
    memset(recv_line, 0, sizeof(recv_line));
    server_addr.sin_family  = AF_INET;
    server_addr.sin_port    = htons(5300); 

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
    
    // register signal handler to catch SIGINT 
    if(signal (SIGINT, sig_int_handler) == SIG_ERR)
        perror("signal error");
    
    //connect to server
    connect (sockt_fd, (SA *) &server_addr, sizeof(server_addr));

    //read from server
    while ((r = read (sockt_fd, recv_line, sizeof(recv_line)-1)) > 0) {
        if (fputs(recv_line, stdout) == EOF) 
            err_sys("output error");
    }

    if (r <= 0)
        err_write("Server terminated, Recvd FIN\n");

    return  0;
}

