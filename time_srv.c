#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <string.h>
#include <arpa/inet.h>
#include "unp.h"


int main(int argc, char **argv)
{
    int	listenfd, connfd;
    struct sockaddr_in	servaddr, cli_addr;
    
    char buff[MAXLINE];
    time_t ticks;
    socklen_t len;

    listenfd = socket (AF_INET, SOCK_STREAM, 0);
    memset (&servaddr, 0, sizeof(servaddr));
    memset (buff, 0, sizeof(buff));
    
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(5000);	/* daytime server */

    bind (listenfd, (SA *)&servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);

    for ( ; ; ) {
        len = sizeof(cli_addr);
        memset (&cli_addr, 0, sizeof(cli_addr));
        
        if ((connfd = Accept (listenfd, (SA *) &cli_addr, &len)) < 0) {
            fprintf(stderr, "Error accepting connection %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        // printf("DEBUG connfd: %d\n", connfd);
        
        ticks = time(NULL);
        snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
        write(connfd, buff, strlen(buff));

        close(connfd);
        sleep(5);
    }
    return 0;
}
