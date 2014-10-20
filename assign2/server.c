#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"

extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern void free_ifi_info_plus(struct ifi_info *ifihead);

int main(int argc, char* argv[]) {
	
    struct ifi_info *ifi, *ifihead;
    struct sock_struct *sock_struct_head, *prev, *curr;
    struct sockaddr_in *sa;
    int sockfd;
    const int on = 1;

    for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
            ifi != NULL; ifi = ifi->ifi_next) {
	
	if (ifi->ifi_addr && ifi->ifi_ntmaddr) {
	     
	    sockfd = Socket(AF_INET, SOCK_DGRAM, 0); 
	    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    
	    sa             = (struct sockaddr_in *)ifi->ifi_addr;
	    sa->sin_family = AF_INET;
	    sa->sin_port   = htons(SERV_PORT);
	    
	    if (bind(sockfd, (SA *)sa, sizeof(*sa)) < 0)
		perror("Bind error");
	     
	    curr = get_sock_struct (sockfd, ifi->ifi_addr, ifi->ifi_ntmaddr); 
	   
	    if (!sock_struct_head) {
	        sock_struct_head = curr;
	        prev             = curr;
	        continue;
	    }

	    prev->nxt_struct = curr;
	    prev = curr;
	}
    
    }
    
    print_sock_struct (sock_struct_head);
    
    free_ifi_info_plus(ifihead);
    return 0;
}
