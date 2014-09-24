#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include "unp.h"


int main(int argc, char* argv[]) {

    printf("Hello world");
    
    int sockt_fd;
    socklen_t sklen;
    struct sockaddr_in server_addr;

    if(argc < 2)
        err_quit("Please enter server addr");
    
    if(sockt_fd = socket(AF_INET, SOCK_STREAM, 0) < 0)
        err_sys("socket error");

    return  0;
}









