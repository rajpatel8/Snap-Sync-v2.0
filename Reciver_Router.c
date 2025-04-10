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
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

void prcclient(int sock);
void error(const char *msg);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int socket_fd, new_sock_fd, pid;

    // Create socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        error("ERROR opening socket");
    }

    // Zero out the server address structure
    bzero((char *)&server_addr, sizeof(server_addr));

    // Set up the server address
    int port = atoi(argv[1]);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind the socket
    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("ERROR on binding");
    }

    // Retrieve and print the server's actual non-loopback IPv4 address
    char ip[INET_ADDRSTRLEN];
    int found = 0;
    
    // Get list of network interfaces
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        error("ERROR getting interface addresses");
    }
    
    // Find non-loopback IPv4 address
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
            
        if (ifa->ifa_addr->sa_family == AF_INET) { // IPv4
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            // Skip loopback address (127.0.0.1)
            if (ntohl(addr->sin_addr.s_addr) != INADDR_LOOPBACK) {
                inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
                printf("Server IP Address: %s\n", ip);
                found = 1;
                break;
            }
        }
    }
    
    if (!found) {
        // Fallback to loopback address if no other interface is found
        printf("Server IP Address: 127.0.0.1\n");
        strcpy(ip, "127.0.0.1");
    }
    
    freeifaddrs(ifaddr);

    // Listen for connections
    listen(socket_fd, 5);

    client_len = sizeof(client_addr);

    while (1) {
        // Accept a new connection
        new_sock_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &client_len);
        if (new_sock_fd < 0) {
            error("ERROR on accept");
        }

        // Fork a new process to handle the client
        pid = fork();
        if (pid < 0) {
            error("ERROR on fork");
        }
        if (pid == 0) {
            // Child process
            close(socket_fd);
            prcclient(new_sock_fd);
            exit(0);
        } else {
            // Parent process
            close(new_sock_fd);
        }
    }

    close(socket_fd);
    return 0;
}

void prcclient(int sock) {
    char hostname[256];
    char buffer[256];
    char server_ip[INET_ADDRSTRLEN];
    
    // Get the hostname
    if (gethostname(hostname, sizeof(hostname)) < 0) {
        perror("ERROR getting hostname");
        return;
    }
    
    // Get the server's actual IP address (non-loopback)
    int found = 0;
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("ERROR getting interface addresses");
        return;
    }
    
    // Find non-loopback IPv4 address
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
            
        if (ifa->ifa_addr->sa_family == AF_INET) { // IPv4
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            // Skip loopback address (127.0.0.1)
            if (ntohl(addr->sin_addr.s_addr) != INADDR_LOOPBACK) {
                inet_ntop(AF_INET, &addr->sin_addr, server_ip, sizeof(server_ip));
                found = 1;
                break;
            }
        }
    }
    
    if (!found) {
        // Fallback to loopback address if no other interface is found
        strcpy(server_ip, "127.0.0.1");
    }
    
    freeifaddrs(ifaddr);

    // Send the welcome message, hostname and IP address to the client
    snprintf(buffer, sizeof(buffer), 
             "Welcome to Snap-Sync Server v2.0\n"
             "-----------------------------\n"
             "Connected to server:\n"
             "Hostname: %s\n"
             "IP Address: %s\n"
             "-----------------------------\n", 
             hostname, server_ip);
             
    if (send(sock, buffer, strlen(buffer), 0) < 0) {
        perror("ERROR sending data to client");
    }
}

void error(const char *msg) {
    perror(msg);
    exit(1);
}