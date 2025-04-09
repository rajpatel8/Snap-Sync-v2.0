#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
int main(int argc, char *argv[]){

    struct sockaddr_in server_addr, client_addr;
    socklen_t client ;
    int socket_fd = socket(AF_INET ,SOCK_STREAM , 0) ;

    if (socket_fd<0)
    {
        printf("Failed to create Socket") ;
    }

    // If compiler might optimize this bzero
    bzero((char *) &server_addr, sizeof(server_addr));

    // so using explicite_bzero
    // explicite_bzero((char *) &server_addr, sizeof(server_addr));

    int port = atoi(argv[1]) ;

    server_addr.sin_family = AF_INET ;
    server_addr.sin_addr.s_addr = INADDR_ANY ;
    server_addr.sin_port = htons(port) ;

    if(bind(socket_fd, (struct sockaddr *) &server_addr ,sizeof(server_addr))<0){
        printf("Failed to Bind Socket") ;
    }

    listen(socket_fd,5);

    client = sizeof(client_addr);

    int new_sock_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &client) ;

    if(new_sock_fd<0){
        printf("Fail to Accept") ;
    }

    char buffer[256] ;

    bzero(buffer,256) ;

}