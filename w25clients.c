/*
    w25clients.c
    A sample client for the distributed file system.
    Usage: ./w25clients <S1_hostname> <S1_port>
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <errno.h>

#define BUFSIZE 1024

/* Utility routines */
void error(const char *msg) {
    perror(msg);
    exit(1);
}

/* Checks that the filename exists and has a valid extension */
int is_valid_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

/* A simple command sanitization function. In this example, we
   only minimally check for the valid commands. */
int sanitize_command(char *command) {
    // Acceptable commands: uploadf, downlf, removef, downltar, dispfnames
    char *token = strtok(command, " ");
    if (!token)
        return -1;
    if (strcasecmp(token, "uploadf") == 0 ||
        strcasecmp(token, "downlf") == 0 ||
        strcasecmp(token, "removef") == 0 ||
        strcasecmp(token, "downltar") == 0 ||
        strcasecmp(token, "dispfnames") == 0) {
        return 0;
    }
    return -1;
}

/* send_all ensures that all N bytes are sent */
ssize_t send_all(int sockfd, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = buf;
    while(total < len) {
        ssize_t n = send(sockfd, p+total, len-total, 0);
        if(n <= 0) {
            return n;
        }
        total += n;
    }
    return total;
}

/* recv_all ensures that all N bytes are received */
ssize_t recv_all(int sockfd, void *buf, size_t len) {
    size_t total = 0;
    char *p = buf;
    while(total < len) {
        ssize_t n = recv(sockfd, p+total, len-total, 0);
        if(n <= 0) {
            return n;
        }
        total += n;
    }
    return total;
}

int main(int argc, char *argv[]) {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[BUFSIZE];

    if (argc < 3) {
       fprintf(stderr, "usage %s hostname port\n", argv[0]);
       exit(0);
    }
    
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    
    if (connect(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    
    printf("\n------ Connected to S1 ------\n");

    while (1) {
        printf("Delta ~ $ ");
        memset(buffer, 0, BUFSIZE);
        if (fgets(buffer, BUFSIZE, stdin) == NULL)
            break;
        // Remove trailing newline.
        buffer[strcspn(buffer, "\n")] = '\0';
        
        // Make a copy for validation.
        char command_copy[BUFSIZE];
        strncpy(command_copy, buffer, BUFSIZE);
        if (sanitize_command(command_copy) != 0) {
            fprintf(stderr, "Invalid command\n");
            continue;
        }
        
        // Send the command string to S1.
        if(send_all(sockfd, buffer, strlen(buffer)) < (ssize_t)strlen(buffer)) {
            error("ERROR sending command");
        }
        
        // Determine command type.
        char cmd[32];
        sscanf(buffer, "%s", cmd);
        if (strcasecmp(cmd, "uploadf") == 0) {
            // Expected syntax: uploadf <filename> <destination_path>
            char filename[256], dest[256];
            if (sscanf(buffer, "%*s %s %s", filename, dest) != 2) {
                fprintf(stderr, "Invalid uploadf syntax\n");
                continue;
            }
            if (!is_valid_file(filename)) {
                fprintf(stderr, "File does not exist.\n");
                continue;
            }
            // Open the file.
            FILE *fp = fopen(filename, "rb");
            if (!fp) {
                perror("Error opening file");
                continue;
            }
            fseek(fp, 0, SEEK_END);
            long filesize = ftell(fp);
            rewind(fp);
            char *filebuf = malloc(filesize);
            if (!filebuf) {
                fprintf(stderr, "Memory allocation error\n");
                fclose(fp);
                continue;
            }
            fread(filebuf, 1, filesize, fp);
            fclose(fp);
            
            // Wait for S1 to reply with "READY"
            memset(buffer, 0, BUFSIZE);
            n = recv(sockfd, buffer, BUFSIZE-1, 0);
            if(n <= 0 || strncmp(buffer, "READY", 5) != 0) {
                fprintf(stderr, "Server not ready for file data\n");
                free(filebuf);
                continue;
            }
            // Send file size as a 4-byte integer.
            uint32_t net_filesize = htonl(filesize);
            if(send_all(sockfd, &net_filesize, sizeof(net_filesize)) < sizeof(net_filesize)) {
                error("Error sending filesize");
            }
            // Send the file data.
            if(send_all(sockfd, filebuf, filesize) < filesize) {
                error("Error sending file data");
            }
            free(filebuf);
            // Get server acknowledgment.
            memset(buffer, 0, BUFSIZE);
            n = recv(sockfd, buffer, BUFSIZE-1, 0);
            if(n > 0)
                printf("Server: %s\n", buffer);
        }
        else if (strcasecmp(cmd, "downlf") == 0) {
            // Expected syntax: downlf <filepath>
            // S1 will send first a 4-byte filesize, then the raw file data.
            uint32_t net_filesize;
            n = recv_all(sockfd, &net_filesize, sizeof(net_filesize));
            if(n != sizeof(net_filesize)) {
                fprintf(stderr, "Error receiving filesize\n");
                continue;
            }
            int filesize = ntohl(net_filesize);
            if(filesize <= 0) {
                fprintf(stderr, "Server returned error or empty file\n");
                continue;
            }
            char *filebuf = malloc(filesize);
            if(!filebuf) {
                fprintf(stderr, "Memory allocation error\n");
                continue;
            }
            if(recv_all(sockfd, filebuf, filesize) != filesize) {
                fprintf(stderr, "Error receiving file data\n");
                free(filebuf);
                continue;
            }
            // Save the downloaded file locally.
            // For simplicity, we extract the filename from the path.
            char filepath[256];
            sscanf(buffer, "%*s %s", filepath);
            char *fname = strrchr(filepath, '/');
            if(fname)
                fname++; 
            else
                fname = filepath;
            FILE *fp = fopen(fname, "wb");
            if(fp) {
                fwrite(filebuf, 1, filesize, fp);
                fclose(fp);
                printf("Downloaded file saved as %s\n", fname);
            } else {
                fprintf(stderr, "Error writing downloaded file\n");
            }
            free(filebuf);
        }else if (strcasecmp(cmd, "downltar") == 0) {
            // Expected syntax: downltar <filetype>
            char filetype[10];
            sscanf(buffer, "%*s %s", filetype);
            
            // Determine output filename based on filetype
            char tarname[20];
            if (strcasecmp(filetype, ".c") == 0)
                strcpy(tarname, "cfiles.tar");
            else if (strcasecmp(filetype, ".pdf") == 0)
                strcpy(tarname, "pdffiles.tar");
            else if (strcasecmp(filetype, ".txt") == 0)
                strcpy(tarname, "txtfiles.tar");
            else if (strcasecmp(filetype, ".zip") == 0)
                strcpy(tarname, "zipfiles.tar");
            else {
                fprintf(stderr, "Invalid filetype for tar\n");
                continue;
            }
            
            // Receive filesize header
            uint32_t net_filesize;
            n = recv_all(sockfd, &net_filesize, sizeof(net_filesize));
            if(n != sizeof(net_filesize)) {
                fprintf(stderr, "Error receiving tar filesize\n");
                continue;
            }
            
            int filesize = ntohl(net_filesize);
            if(filesize <= 0) {
                fprintf(stderr, "Server returned error or empty tar file\n");
                continue;
            }
            
            // Allocate buffer and receive tar data
            char *filebuf = malloc(filesize);
            if(!filebuf) {
                fprintf(stderr, "Memory allocation error\n");
                continue;
            }
            
            if(recv_all(sockfd, filebuf, filesize) != filesize) {
                fprintf(stderr, "Error receiving tar file data\n");
                free(filebuf);
                continue;
            }
            
            // Save the tar file
            FILE *fp = fopen(tarname, "wb");
            if(fp) {
                fwrite(filebuf, 1, filesize, fp);
                fclose(fp);
                printf("Downloaded tar file saved as %s\n", tarname);
            } else {
                fprintf(stderr, "Error writing tar file\n");
            }
            free(filebuf);
        }
        else {
            // For removef, dispfnames, simply print the response
            memset(buffer, 0, BUFSIZE);
            n = recv(sockfd, buffer, BUFSIZE-1, 0);
            if(n > 0)
                printf("Server: %s\n", buffer);
        }
    }
    
    close(sockfd);
    return 0;
}
