#include <stdio.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h> 
#include <string.h>
#include <arpa/inet.h>
#include "unp.h"
#include "readline.h"

void *echo_client(void *value) {

    struct sockaddr_in cli_addr;
    int	connfd, n, conn_flags;
    int listenfd = *(int *)value;

    char buf[MAXLINE];
    time_t ticks;
    socklen_t len;
    pid_t pid;

    len = sizeof(cli_addr);
    memset (&cli_addr, 0, sizeof(cli_addr));
   
    /* accept will fail if no connection as socket is 
     * nonblocking. So retry if failed with EWOULDBLOCK
     */
    while(1) { 
        if ((connfd = accept (listenfd, (SA *) &cli_addr, &len)) < 0) {
            if(errno == EWOULDBLOCK || errno == EAGAIN) {
                continue;
           } else {
                perror("Error accepting connection \n");
                exit(EXIT_FAILURE);
            }
        }
        break;
    }

    if ((conn_flags = fcntl(connfd, F_GETFL, 0) == -1)) {
        perror("fcntl F_GETFL Error");
        exit(EXIT_FAILURE);
    }
    /* listen socket is non-blocking. Connection socket 
     * will inherit its properties. We need to make it blocking
     */
    conn_flags &= ~O_NONBLOCK;    
    
    if (fcntl(connfd, F_SETFL, conn_flags) == -1 ) {
        perror("fcntl F_SETFL Error");
        exit(EXIT_FAILURE);
    }

    while(1) {
        while((n = read(connfd, buf, MAXLINE)) > 0) 
            Writen(connfd, buf, n);

        if (n < 0 && errno == EINTR) {
            continue;
        } else if (n < 0) {
            perror("Error reading"); 
            break;
        }
    }
    close(connfd);
    pthread_exit(value);

}

void *time_client(void* value) {
    
    struct sockaddr_in cli_addr;
    int	connfd, conn_flags;
    time_t ticks;
    struct timeval tt;
    pid_t pid;

    char buff[MAXLINE], recv_line[MAXLINE], str[INET_ADDRSTRLEN];

    fd_set fdset;
    FD_ZERO(&fdset);
    
    int listenfd   = *(int *)value;
    socklen_t len  = sizeof(cli_addr);
    memset (&cli_addr, 0, sizeof(cli_addr));
    
    /* accept will fail if no connection as socket is 
     * nonblocking. So retry if failed with EWOULDBLOCK
     */
    while(1) { 
        if ((connfd = accept (listenfd, (SA *) &cli_addr, &len)) < 0) {
            if(errno == EWOULDBLOCK || errno == EAGAIN) {
                continue;
           } else {
                perror("Error accepting connection \n");
                exit(EXIT_FAILURE);
            }
        }
        break;
    }
    
    if ((conn_flags = fcntl(listenfd, F_GETFL, 0) == -1)) {
        perror("fcntl F_GETFL Error");
        exit(EXIT_FAILURE);
    }
    /* listen socket is non-blocking. Connection socket 
     * will inherit its properties. We need to make it blocking
     */
    conn_flags &= ~O_NONBLOCK;    
    
    if (fcntl(listenfd, F_SETFL, conn_flags) == -1 ) {
        perror("fcntl F_SETFL Error");
        exit(EXIT_FAILURE);
    }
   
    // get client ip addr
    inet_ntop (AF_INET, &(cli_addr.sin_addr), str, INET_ADDRSTRLEN);
    
    while(1) {
        tt.tv_sec = 2;
        FD_SET (connfd, &fdset);
        
        /* Select call to check if FIN is received from client
         * Else sit in loop writing time on socket
         */
        select (connfd + 1,&fdset, NULL, NULL, &tt);
        
        /* if recieved a zero length message on socket
         *  client sent the fin, close the connection
         */
        if (FD_ISSET(connfd, &fdset)) {
            if (Readline(connfd, recv_line, MAXLINE) == 0) {
                printf("connection closed by the client %s \n", str);
                close(connfd);
                pthread_exit(value);
                exit(EXIT_SUCCESS);
            }
        }
        
        // write the time on the socket
        ticks = time(NULL);
        snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
        write(connfd, buff, strlen(buff));
    }
    close(connfd);    
    pthread_exit(value);
}

int main(int argc, char **argv)
{
    int	echo_listenfd, echo_connfd;
    int	time_listenfd, time_connfd, res;
    struct sockaddr_in	echo_servaddr, time_servaddr, cli_addr;
    
    int echo_flags, time_flags;

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
   
    // Make the listening sockets non blocking
    if ((echo_flags = fcntl(echo_listenfd, F_GETFL, 0) == -1) ||
        (time_flags = fcntl(time_listenfd, F_GETFL, 0) == -1)) {
        perror("fcntl F_GETFL Error");
        exit(EXIT_FAILURE);
    }
    
    if((fcntl(echo_listenfd, F_SETFL, echo_flags | O_NONBLOCK) == -1) ||
       (fcntl(time_listenfd, F_SETFL, time_flags | O_NONBLOCK) == -1 )) {
        perror("fcntl F_SETFL Error");
        exit(EXIT_FAILURE);
    }
    
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

