#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <string.h>
#include <arpa/inet.h>
#include "unp.h"
#include "readline.h"



void *echo_client(void *value) {

    struct sockaddr_in cli_addr;
    int	connfd, n;
    int listenfd = *(int *)value;


    char buf[MAXLINE];
    time_t ticks;
    socklen_t len;
    pid_t pid;

    fd_set fdset;
    FD_ZERO(&fdset);

    len = sizeof(cli_addr);
    memset (&cli_addr, 0, sizeof(cli_addr));
    
    if ((connfd = Accept (listenfd, (SA *) &cli_addr, &len)) < 0) {
        perror("Error accepting connection \n");
        exit(EXIT_FAILURE);
    }

    while(1) {
        while((n = read(connfd, buf, MAXLINE)) > 0) 
            Writen(connfd, buf, n);

        if (n < 0 && errno == EINTR) {
            continue;
        } else if (n < 0) {
            fprintf(stderr, "Error reading %s", strerror(errno));
            break;
        }
    }
    pthread_exit(value);

}



void *time_client(void* value) {
    
    struct sockaddr_in cli_addr;
    int	connfd;
    int listenfd = *(int *)value;

    char buff[MAXLINE], recv_line[MAXLINE];
    time_t ticks;
    socklen_t len;
    struct timeval tt;
    pid_t pid;

    fd_set fdset;
    FD_ZERO(&fdset);
    
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
                pthread_exit(value);
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
        
    pthread_exit(value);
}


int main(int argc, char **argv)
{
    int	echo_listenfd, echo_connfd;
    int	time_listenfd, time_connfd, res;
    struct sockaddr_in	echo_servaddr, time_servaddr, cli_addr;
    
    fd_set fdset;
    FD_ZERO(&fdset);
    
    char buff[MAXLINE];
    time_t ticks;
    socklen_t len;
    
    pthread_t echo_thread, time_thread;
    pthread_attr_t attr;
    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    echo_listenfd = socket (AF_INET, SOCK_STREAM, 0);
    memset (&echo_servaddr, 0, sizeof(echo_servaddr));
    
    echo_servaddr.sin_family      = AF_INET;
    echo_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echo_servaddr.sin_port        = htons(5200);	/* Echo server */

    time_listenfd = socket (AF_INET, SOCK_STREAM, 0);
    memset (&time_servaddr, 0, sizeof(time_servaddr));
    
    time_servaddr.sin_family      = AF_INET;
    time_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    time_servaddr.sin_port        = htons(5300);	/* Time server */
    
    bind (time_listenfd, (SA *)&time_servaddr, sizeof(time_servaddr));
    listen(time_listenfd, LISTENQ);
    bind (echo_listenfd, (SA *)&echo_servaddr, sizeof(echo_servaddr));
    listen(echo_listenfd, LISTENQ);
    
    while(1) {
        FD_SET (time_listenfd, &fdset);
        FD_SET (echo_listenfd, &fdset);

        select (max(time_listenfd, echo_listenfd) + 1,
                                    &fdset, NULL, NULL, NULL);
        
        if (FD_ISSET(time_listenfd, &fdset)) {
            res = pthread_create(&time_thread, 
                                &attr, time_client, (void *)&time_listenfd);
            if(res != 0) {
                perror("Thread creation failed");
                exit(EXIT_FAILURE);
            }
        }

        if (FD_ISSET(echo_listenfd, &fdset)) {
            
            res = pthread_create(&echo_thread, 
                                &attr, echo_client, (void *)&echo_listenfd);
            if(res != 0) {
                perror("Thread creation failed");
                exit(EXIT_FAILURE);
            }
        }
    }
    pthread_attr_destroy(&attr);
    return 0;
}

