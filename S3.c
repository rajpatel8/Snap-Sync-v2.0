/*
 * Name - 1    : Rajkumar Patel - 110184076
 * Name - 2    : Vansh Patel    - 110176043
 * 
 * Project     : W25_Project - Distributed File System
 * File        : S3.c
 * Author      : lord_rajkumar
 * Co-Author   : vansh7388
 * GitHub      : https://github.com/rajpatel8/Snap-Sync-v2.0
 * Description : Server S3. Receives and stores .txt files that are transferred from S1.
 * License     : MIT License
 *
 * (c) 2025 lord_rajkumar. All rights reserved.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>

#define BUFSIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void ensure_directory(const char *path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
    system(cmd);
}

void send_all(int sock, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = buf;
    while(total < len) {
        ssize_t n = send(sock, p+total, len-total, 0);
        if(n <= 0) break;
        total += n;
    }
}

ssize_t recv_all(int sock, void *buf, size_t len) {
    size_t total = 0;
    char *p = buf;
    while(total < len) {
        ssize_t n = recv(sock, p+total, len-total, 0);
        if(n <= 0) return n;
        total += n;
    }
    return total;
}

void prcclient(int sock) {
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    int n = recv(sock, buffer, BUFSIZE-1, 0);
    if(n <= 0) { close(sock); return; }
    buffer[n] = '\0';

    char cmd[32];
    sscanf(buffer, "%s", cmd);
    

    char base[256] = "./S3";
    
    if (strcasecmp(cmd, "storef") == 0) {
        char dest[256], filename[256];
        if (sscanf(buffer, "%*s %s %s", dest, filename) != 2) {
            send(sock, "Invalid command syntax\n", 23, 0);
            close(sock);
            return;
        }
        send(sock, "READY", 5, 0);
        uint32_t net_filesize;
        if (recv(sock, &net_filesize, sizeof(net_filesize), 0) != sizeof(net_filesize)) {
            close(sock);
            return;
        }
        int filesize = ntohl(net_filesize);
        char *filebuf = malloc(filesize);
        if(!filebuf) { close(sock); return; }
        int received = 0;
        while(received < filesize) {
            n = recv(sock, filebuf+received, filesize-received, 0);
            if(n <= 0) break;
            received += n;
        }
        char fullpath[512];
        char *subpath = strstr(dest, "~S1");
        if(subpath)
            subpath += 3;
        else
            subpath = dest;
        snprintf(fullpath, sizeof(fullpath), "%s%s", base, subpath);
        ensure_directory(fullpath);
        char filepath[600];
        snprintf(filepath, sizeof(filepath), "%s/%s", fullpath, filename);
        FILE *fp = fopen(filepath, "wb");
        if(fp) {
            fwrite(filebuf, 1, filesize, fp);
            fclose(fp);
            send(sock, "File stored successfully\n", 27, 0);
        } else {
            send(sock, "Error writing file\n", 19, 0);
        }
        free(filebuf);
    }
    else if (strcasecmp(cmd, "downlf") == 0) {
        char filepath_rel[512];
        if(sscanf(buffer, "%*s %s", filepath_rel) != 1) {
            send(sock, "Invalid command syntax\n", 23, 0);
            close(sock);
            return;
        }
        char fullpath[600];
        char *subpath = strstr(filepath_rel, "~S1");
        if(subpath)
            subpath += 3;
        else
            subpath = filepath_rel;
        snprintf(fullpath, sizeof(fullpath), "%s%s", base, subpath);
        FILE *fp = fopen(fullpath, "rb");
        if(!fp) {
            send(sock, "ERROR", 5, 0);
            close(sock);
            return;
        }
        fseek(fp, 0, SEEK_END);
        int filesize = ftell(fp);
        rewind(fp);
        uint32_t net_filesize = htonl(filesize);
        send(sock, &net_filesize, sizeof(net_filesize), 0);
        char filebuf[BUFSIZE];
        while((n = fread(filebuf, 1, BUFSIZE, fp)) > 0) {
            send(sock, filebuf, n, 0);
        }
        fclose(fp);
    }
    else if (strcasecmp(cmd, "removef") == 0) {
        char filepath_rel[512];
        if(sscanf(buffer, "%*s %s", filepath_rel) != 1) {
            send(sock, "Invalid command syntax\n", 23, 0);
            close(sock);
            return;
        }
        char fullpath[600];
        char *subpath = strstr(filepath_rel, "~S1");
        if(subpath)
            subpath += 3;
        else
            subpath = filepath_rel;
        snprintf(fullpath, sizeof(fullpath), "%s%s", base, subpath);
        if(remove(fullpath)==0)
            send(sock, "File removed successfully\n", 28, 0);
        else
            send(sock, "Error removing file\n", 21, 0);
    }
    else if (strcasecmp(cmd, "downltar") == 0) {
        char filetype[10];
        if(sscanf(buffer, "%*s %s", filetype) != 1) {
            send(sock, "Invalid command syntax\n", 23, 0);
            close(sock);
            return;
        }
        if(strcasecmp(filetype, ".txt") != 0) {
            send(sock, "Invalid filetype for tar\n", 26, 0);
            close(sock);
            return;
        }
        char tarname[20];
        strcpy(tarname, "txtfiles.tar");
        char cmdline[600];
        snprintf(cmdline, sizeof(cmdline), "tar -cf %s %s >/dev/null 2>&1", tarname, base);
        system(cmdline);
        FILE *fp = fopen(tarname, "rb");
        if(!fp) {
            send(sock, "ERROR creating tar\n", 21, 0);
            close(sock);
            return;
        }
        fseek(fp, 0, SEEK_END);
        int filesize = ftell(fp);
        rewind(fp);
        uint32_t net_filesize = htonl(filesize);
        send(sock, &net_filesize, sizeof(net_filesize), 0);
        char filebuf[BUFSIZE];
        while((n = fread(filebuf, 1, BUFSIZE, fp)) > 0) {
            send(sock, filebuf, n, 0);
        }
        fclose(fp);
        remove(tarname);
    }
    else if (strcasecmp(cmd, "dispfnames") == 0) {

        char pathname[512];
        if (sscanf(buffer, "%*s %s", pathname) != 1) {
            send(sock, "Invalid command syntax\n", 23, 0);
            close(sock);
            return;
        }

        char subpath[512] = "";
        char *p = strstr(pathname, "~S1");
        if (p != NULL) {
            p += 3; // Skip "~S1"
            if (strcmp(p, "/") != 0 && strlen(p) > 0)
                strncpy(subpath, p, sizeof(subpath)-1);
        } else {
            strncpy(subpath, pathname, sizeof(subpath)-1);
        }

        char fullpath[600];
        snprintf(fullpath, sizeof(fullpath), "%s%s", base, subpath);
        
        char find_cmd[700];
        snprintf(find_cmd, sizeof(find_cmd), "find %s -maxdepth 1 -type f | sort", fullpath);
        FILE *fp = popen(find_cmd, "r");
        char output[4096];
        output[0] = '\0';
        if (fp != NULL) {
            char temp[256];
            while (fgets(temp, sizeof(temp), fp)) {
                strncat(output, temp, sizeof(output) - strlen(output) - 1);
            }
            pclose(fp);
        } else {
            strncpy(output, "Error listing files\n", sizeof(output)-1);
        }
        if (strlen(output) == 0)
            send(sock, "No files found\n", 15, 0);
        else
            send(sock, output, strlen(output), 0);
        close(sock);
    }
    else {
        send(sock, "Invalid command\n", 16, 0);
    }
    close(sock);
}

int main() {
    int sockfd, newsockfd, portno;
    pid_t pid;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    portno = 9003;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
         error("ERROR opening socket");
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
         error("ERROR on binding");
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    while(1) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if(newsockfd < 0)
            error("ERROR on accept");
        pid = fork();
        if(pid < 0)
            error("ERROR on fork");
        if(pid == 0) {
            close(sockfd);
            prcclient(newsockfd);
            exit(0);
        } else {
            close(newsockfd);
        }
    }
    close(sockfd);
    return 0;
}
