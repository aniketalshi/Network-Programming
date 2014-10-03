#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include "unp.h"

enum type {ECHO_CLI, TIME_CLI};

void start_echocli (int type, char *str) {
    int fd[2], status;
    pipe(fd);
   
    pid_t pid;
    
    char buf[5];
    snprintf(buf, sizeof(buf), "%d",fd[1]);

    fd_set fdset;
    FD_ZERO(&fdset);
    char recv_buf[MAXLINE];

    switch(pid = fork()) {
        case 0 : { // child process
            close(fd[0]);
            
            if (type == TIME_CLI) { // exec the time client in xterm window
                if ( (execlp("xterm", "xterm", "-e", 
                                    "./time_cli", str, (char *)buf, (char *) 0)) < 0)  {
                    perror("Exec error"); 
                    exit(EXIT_FAILURE);
                }
            } else if (type == ECHO_CLI) { // exec the echo client in xterm window
                if ( (execlp("xterm", "xterm", "-e", 
                                    "./echo_cli", str, (char *)buf, (char *) 0)) < 0)  {
                    perror("Exec error"); 
                    exit(EXIT_FAILURE);
                }
            }
            break;
        }

        default : { // parent process
            close(fd[1]);
            while(1) { 
                FD_SET(fileno(stdin), &fdset);                
                FD_SET(fd[0], &fdset);

                // monitor stdin and status message from client
                select (max(fileno(stdin) ,fd[0]) + 1, &fdset,
                                                NULL, NULL, NULL);
                
                if (FD_ISSET(fileno(stdin), &fdset)) {
                    memset(recv_buf, 0, MAXLINE); 
                    Readline (fileno(stdin), recv_buf, MAXLINE);
                    printf("Usage: Type in client window \n");
                }

                if (FD_ISSET(fd[0], &fdset)) {
                    memset(recv_buf, 0, MAXLINE); 
                    /* If Ctrl+C is entered, we will receive Done on pipe
                     * on client crashing, readline will return zero
                     */
                    if ((Readline (fd[0], recv_buf, MAXLINE) == 0) || 
                                        (strcmp(recv_buf, "Done") == 0)) {
                        if ((pid = waitpid(-1, &status, 0)) > 0)
                            printf("Child %d terminated successfully.\n", pid);
                        break;
                    }
                    // print out status message by client
                    fputs (recv_buf, stdout);
                } 
            }
            close(fd[0]);
            break;
        }

        case -1 : {
            perror("Error in fork");
            exit(EXIT_FAILURE);
        }
    }
}


int main(int argc, char* argv[]) {
    
    int inp = 0;
    if (argc < 2) {
        printf("Usage <ip address>");
        exit(EXIT_FAILURE);
    }

    printf ("\n=================TCP Client Server=======================\n");
    
    while(1) {
        
        printf ("Choose one of the option :\n Echo Client : Enter 1"
                            "\n Time Server : Enter 2\n Exit : Enter 3\n");
        scanf("%d", &inp); 
        switch(inp) {
            case 1: {
                start_echocli(ECHO_CLI, argv[1]);
                break;
            }

            case 2: {
                start_echocli(TIME_CLI, argv[1]);
                break;
            }

            case 3: {
               return 0; 
            }
            
            default: {
                break;
            }
        }

    }
    
    return 0;
}
