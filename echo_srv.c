#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <string.h>
#include <arpa/inet.h>
#include "unp.h"
#include "readline.h"

void echo_fun(int sock_fd) {
    int n;
    char buf[MAXLINE];
    

    while(1) {
        while((n = read(sock_fd, buf, MAXLINE)) > 0) 
            Writen(sock_fd, buf, n);

        if (n < 0 && errno == EINTR) {
            continue;
        } else if (n < 0) {
            fprintf(stderr, "Error reading %s", strerror(errno));
            break;
        }
    }
}

void sig_child_handler(int sig_no) {
    pid_t pid;
    int status;
    
    /* call waitpid to wait on any exiting process 
     * WNOHANG says the call should not be blocking
     */
    while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Child %d terminated \n", pid);
    }
    return;
}


int main(int argc, char **argv)
{
    int	listenfd, connfd;
    struct sockaddr_in	servaddr, cli_addr;
    pid_t pid;
    
    char buff[MAXLINE];
    time_t ticks;
    socklen_t len;

    listenfd = socket (AF_INET, SOCK_STREAM, 0);
    memset (&servaddr, 0, sizeof(servaddr));
    memset (buff, 0, sizeof(buff));
    
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(5200);	/* Echo server */

    bind (listenfd, (SA *)&servaddr, sizeof(servaddr));
    listen(listenfd, LISTENQ);
    
    Signal (SIGCHLD, sig_child_handler);

    for ( ; ; ) {
        len = sizeof(cli_addr);
        memset (&cli_addr, 0, sizeof(cli_addr));
        
        if ((connfd = Accept (listenfd, (SA *) &cli_addr, &len)) < 0) {
            /* If child exits and SIGCHILD signal handler returns in middle
             * of Accept syscall, EINTR is recieved, then restart the syscall
             */
            if (errno == EINTR) 
                continue;
            
            perror("Error accepting connection \n");
            exit(EXIT_FAILURE);
        }
        
        if((pid = Fork()) == 0) {  // child process
            // close the listening socket and proces connection
            close(listenfd); 
            echo_fun(connfd);
            exit(0);
        }
        close(connfd);
    }
    return 0;
}
