/*
 *  _                  _ 
 * | |    ___  _ __ __| |
 * | |   / _ \| '__/ _` |
 * | |__| (_) | | | (_| |
 * |_____\___/|_|  \__,_|
 * 
 * Project     : Reciver_Router
 * Author      : lord_rajkumar
 * GitHub      : https://github.com/rajpatel8/Snap-Sync-v2.0
 * Description : Basic TCP server written in C
 * License     : MIT License
 * 
 * (c) 2025 lord rajkumar. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

void prcclient (int sock) ;

void setup() ;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]){

    struct sockaddr_in server_addr, client_addr ;
    socklen_t client ;
    int new_sock_fd,  pid ;

    int socket_fd = socket(AF_INET ,SOCK_STREAM , 0) ;

    if (socket_fd<0)
    {
        printf("Failed to create Socket") ;
    }

    bzero((char *) &server_addr, sizeof(server_addr));

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

    while (1) {
        new_sock_fd = accept(socket_fd, 
              (struct sockaddr *) &client_addr, &client);
        if (new_sock_fd < 0) 
            error("ERROR on accept");
        pid = fork();
        if (pid < 0)
            error("ERROR on fork");
        if (pid == 0)  {
            close(socket_fd);
            prcclient(new_sock_fd);
            exit(0);
        }
        else close(new_sock_fd);
    } /* end of while */
    close(socket_fd);
    return 0;

}

void setup_S1(char * path){
    struct stat st = {0};
    char temp[256] = "~";
    strcat(temp, path); // assuming user enters path with '/'
    strcpy(path, temp);
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
}

void prcclient (int sock)
{
   // TODO : ROUTER LOGIC
   // FIRST : Client will share firl type.
   // Will read it and direct the traffic to designated server

   int n ;
   char buffer[3] ;
   n = read(sock , buffer ,3) ;

   

}