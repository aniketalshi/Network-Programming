#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include "unp.h"


int main(int argc, char **argv)
{
    int	listenfd, connfd;
    struct sockaddr_in	servaddr, cli_addr;
    
    char buff[MAXLINE], recv_line[MAXLINE];
    time_t ticks;
    socklen_t len;
    struct timeval tt;
    pid_t pid;

    fd_set fdset;
    FD_ZERO(&fdset);

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
        
        while(1) {
            tt.tv_sec = 1;
            FD_SET (connfd, &fdset);
            
            /* Select call to check if FIN is received from client
             * Else sit in loop writing time on socket
             */
            select (connfd + 1,&fdset, NULL, NULL, &tt);
            
            if (FD_ISSET(connfd, &fdset)) {
                if (Readline(connfd, recv_line, MAXLINE) == 0) {
                    printf("Received FIN \n");
                    close(connfd);
                    exit(0);
                }
            }
            
            /* TODO: sleep not working here
             * Displaying 20 ticks before checking for FIN 
             */
            int tim = 20;
            while(tim) {
                ticks = time(NULL);
                snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
                write(connfd, buff, strlen(buff));
                tim--;
            }
        }
    }
    return 0;
}
