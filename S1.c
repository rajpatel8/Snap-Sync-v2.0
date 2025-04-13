/*
    S1.c
    S1 acts as the front‐end server. It accepts client connections and processes commands.
    
    For uploads:
      - .c files are stored locally under ./S1.
      - Other file types (.pdf, .txt, .zip) are forwarded to the appropriate remote server.
    
    For downloads, removals, tar archives, and file-name display:
      - If the file (or file type) is .c, S1 handles it locally.
      - Otherwise, S1 forwards the entire command to the proper remote server and relays the response.
    
    Compile with: gcc -o S1 S1.c
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

/*---------------- Utility Functions ----------------*/

/* Print error message and exit */
void error(const char *msg) {
    perror(msg);
    exit(1);
}

/* Ensure a directory exists, if not, create it using a system call to mkdir -p */
void ensure_directory(const char *path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
    system(cmd);
}

/* send_all ensures that all len bytes from buf are sent through sockfd */
ssize_t send_all(int sockfd, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = buf;
    while(total < len) {
        ssize_t n = send(sockfd, p + total, len - total, 0);
        if(n <= 0) return n;
        total += n;
    }
    return total;
}

/* recv_all ensures that all len bytes are received from sockfd into buf */
ssize_t recv_all(int sockfd, void *buf, size_t len) {
    size_t total = 0;
    char *p = buf;
    while(total < len) {
        ssize_t n = recv(sockfd, p + total, len - total, 0);
        if(n <= 0) return n;
        total += n;
    }
    return total;
}

/* Forward an uploaded non-.c file from S1 to the appropriate remote server.
   The function connects to the remote server (given by server_port), sends a
   "storef" command along with destination and filename, then sends the filesize
   header and raw file data. */
int forward_file(int server_port, const char *dest, const char *filename, char *filebuf, int filesize) {
    int sockfd;
    struct sockaddr_in serv_addr;
    char buf[BUFSIZE];
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror("ERROR opening socket to forward");
        return -1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    // Assuming remote servers are on localhost
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting to forward server");
        close(sockfd);
        return -1;
    }
    // Build command: "storef <destination> <filename>"
    snprintf(buf, sizeof(buf), "storef %s %s", dest, filename);
    if(send(sockfd, buf, strlen(buf), 0) < 0) {
        perror("Error sending store command");
        close(sockfd);
        return -1;
    }
    // Wait for remote server to respond with "READY"
    memset(buf, 0, sizeof(buf));
    if(recv(sockfd, buf, sizeof(buf)-1, 0) <= 0) {
        perror("No READY response from forwarding server");
        close(sockfd);
        return -1;
    }
    if(strncmp(buf, "READY", 5) != 0) {
        fprintf(stderr, "Forwarding server not ready\n");
        close(sockfd);
        return -1;
    }
    // Send file size (4 bytes)
    uint32_t net_filesize = htonl(filesize);
    if(send(sockfd, &net_filesize, sizeof(net_filesize), 0) < (ssize_t)sizeof(net_filesize)) {
        perror("Error sending filesize to forwarding server");
        close(sockfd);
        return -1;
    }
    // Send file data
    int sent = 0;
    while(sent < filesize) {
        int n = send(sockfd, filebuf + sent, filesize - sent, 0);
        if(n <= 0)
            break;
        sent += n;
    }
    // Optionally, read acknowledgement (if any) before closing.
    memset(buf, 0, sizeof(buf));
    recv(sockfd, buf, sizeof(buf)-1, 0);
    close(sockfd);
    return 0;
}

/*---------------- S1 Main Client-Handler Function ----------------*/

void prcclient(int client_sock) {
    char buffer[BUFSIZE];
    int n;
    while (1) {
        memset(buffer, 0, BUFSIZE);
        n = recv(client_sock, buffer, BUFSIZE-1, 0);
        if(n <= 0) break;
        buffer[n] = '\0';
        
        // Determine the command.
        char cmd[32];
        sscanf(buffer, "%s", cmd);
        
        if(strcasecmp(cmd, "uploadf") == 0) {
            // Expected: uploadf <filename> <destination_path>
            char filename[256], dest[256];
            if(sscanf(buffer, "%*s %s %s", filename, dest) != 2) {
                send(client_sock, "Invalid command syntax\n", 23, 0);
                continue;
            }
            // Check file extension.
            char *ext = strrchr(filename, '.');
            if(!ext) {
                send(client_sock, "Invalid file extension\n", 23, 0);
                continue;
            }
            // Tell client "READY" to send file data.
            send(client_sock, "READY", 5, 0);
            // Receive filesize header.
            uint32_t net_filesize;
            n = recv(client_sock, &net_filesize, sizeof(net_filesize), 0);
            if(n != sizeof(net_filesize)) {
                send(client_sock, "Error receiving filesize\n", 26, 0);
                continue;
            }
            int filesize = ntohl(net_filesize);
            char *filebuf = malloc(filesize);
            if(!filebuf) {
                send(client_sock, "Memory allocation error\n", 24, 0);
                continue;
            }
            int received = 0;
            while(received < filesize) {
                n = recv(client_sock, filebuf+received, filesize-received, 0);
                if(n <= 0) break;
                received += n;
            }
            // If file is a .c file, store it locally.
            if(strcasecmp(ext, ".c") == 0) {
                char base[256] = "./S1";
                char fullpath[512];
                // Remove "~S1" if present.
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
                    send(client_sock, "File uploaded successfully\n", 29, 0);
                } else {
                    send(client_sock, "Error writing file\n", 19, 0);
                }
            }
            else {
                // Non-.c files are forwarded.
                int target_port = 0;
                if(strcasecmp(ext, ".pdf") == 0)
                    target_port = 9002;
                else if(strcasecmp(ext, ".txt") == 0)
                    target_port = 9003;
                else if(strcasecmp(ext, ".zip") == 0)
                    target_port = 9004;
                else {
                    send(client_sock, "Unsupported file type\n", 23, 0);
                    free(filebuf);
                    continue;
                }
                if(forward_file(target_port, dest, filename, filebuf, filesize) == 0)
                    send(client_sock, "File forwarded successfully\n", 30, 0);
                else
                    send(client_sock, "Error forwarding file\n", 23, 0);
            }
            free(filebuf);
        }
        else if(strcasecmp(cmd, "downlf") == 0) {
            // Expected: downlf <filepath>
            char filepath[512];
            if(sscanf(buffer, "%*s %s", filepath) != 1) {
                send(client_sock, "Invalid command syntax\n", 23, 0);
                continue;
            }
            char *ext = strrchr(filepath, '.');
            if(!ext) {
                send(client_sock, "Invalid file extension\n", 23, 0);
                continue;
            }
            if(strcasecmp(ext, ".c") == 0) {
                // Local download from ./S1.
                char localpath[600];
                snprintf(localpath, sizeof(localpath), "./S1%s", (strstr(filepath, "~S1") ? filepath+3 : filepath));
                FILE *fp = fopen(localpath, "rb");
                if(!fp) {
                    send(client_sock, "ERROR", 5, 0);
                    continue;
                }
                fseek(fp, 0, SEEK_END);
                int filesize = ftell(fp);
                rewind(fp);
                uint32_t net_filesize = htonl(filesize);
                send(client_sock, &net_filesize, sizeof(net_filesize), 0);
                char filebuf[BUFSIZE];
                int bytes;
                while((bytes = fread(filebuf, 1, BUFSIZE, fp)) > 0)
                    send(client_sock, filebuf, bytes, 0);
                fclose(fp);
            }
            else {
                // Forward the download request to the correct remote server.
                int target_port = 0;
                if(strcasecmp(ext, ".pdf") == 0)
                    target_port = 9002;
                else if(strcasecmp(ext, ".txt") == 0)
                    target_port = 9003;
                else if(strcasecmp(ext, ".zip") == 0)
                    target_port = 9004;
                else {
                    send(client_sock, "Unsupported file type\n", 23, 0);
                    continue;
                }
                int sock_remote = socket(AF_INET, SOCK_STREAM, 0);
                if(sock_remote < 0) {
                    perror("ERROR opening socket to remote server");
                    continue;
                }
                struct sockaddr_in remote_addr;
                memset(&remote_addr, 0, sizeof(remote_addr));
                remote_addr.sin_family = AF_INET;
                remote_addr.sin_port = htons(target_port);
                remote_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                if(connect(sock_remote, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
                    perror("ERROR connecting to remote server");
                    close(sock_remote);
                    continue;
                }
                // Send the same downlf command received from the client.
                if(send_all(sock_remote, buffer, strlen(buffer)) < (ssize_t)strlen(buffer)) {
                    perror("Error sending remote downlf command");
                    close(sock_remote);
                    continue;
                }
                // Receive file size from remote server and relay it.
                uint32_t net_filesize_remote;
                if(recv_all(sock_remote, &net_filesize_remote, sizeof(net_filesize_remote)) != sizeof(net_filesize_remote)) {
                    send(client_sock, "Error receiving filesize from remote server\n", 45, 0);
                    close(sock_remote);
                    continue;
                }
                send(client_sock, &net_filesize_remote, sizeof(net_filesize_remote), 0);
                int remote_filesize = ntohl(net_filesize_remote);
                char tempbuf[BUFSIZE];
                int total_received = 0;
                while(total_received < remote_filesize) {
                    int rec = recv(sock_remote, tempbuf, BUFSIZE, 0);
                    if(rec <= 0) break;
                    send(client_sock, tempbuf, rec, 0);
                    total_received += rec;
                }
                close(sock_remote);
            }
        }
        else if(strcasecmp(cmd, "removef") == 0) {
            // Expected: removef <filepath>
            char filepath[512];
            if(sscanf(buffer, "%*s %s", filepath) != 1) {
                send(client_sock, "Invalid command syntax\n", 23, 0);
                continue;
            }
            char *ext = strrchr(filepath, '.');
            if(!ext) {
                send(client_sock, "Invalid file extension\n", 23, 0);
                continue;
            }
            if(strcasecmp(ext, ".c") == 0) {
                char localpath[600];
                snprintf(localpath, sizeof(localpath), "./S1%s", (strstr(filepath, "~S1") ? filepath+3 : filepath));
                if(remove(localpath) == 0)
                    send(client_sock, "File removed successfully\n", 28, 0);
                else
                    send(client_sock, "Error removing file\n", 21, 0);
            }
            else {
                int target_port = 0;
                if(strcasecmp(ext, ".pdf") == 0)
                    target_port = 9002;
                else if(strcasecmp(ext, ".txt") == 0)
                    target_port = 9003;
                else if(strcasecmp(ext, ".zip") == 0)
                    target_port = 9004;
                else {
                    send(client_sock, "Unsupported file type\n", 23, 0);
                    continue;
                }
                int sock_remote = socket(AF_INET, SOCK_STREAM, 0);
                if(sock_remote < 0) { perror("ERROR opening remote socket"); continue; }
                struct sockaddr_in remote_addr;
                memset(&remote_addr, 0, sizeof(remote_addr));
                remote_addr.sin_family = AF_INET;
                remote_addr.sin_port = htons(target_port);
                remote_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                if(connect(sock_remote, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
                    perror("ERROR connecting to remote server");
                    close(sock_remote);
                    continue;
                }
                if(send_all(sock_remote, buffer, strlen(buffer)) < (ssize_t)strlen(buffer)) {
                    perror("Error sending remote removef command");
                    close(sock_remote);
                    continue;
                }
                memset(buffer, 0, BUFSIZE);
                n = recv(sock_remote, buffer, BUFSIZE-1, 0);
                if(n > 0)
                    send(client_sock, buffer, n, 0);
                close(sock_remote);
            }
        }
        else if(strcasecmp(cmd, "downltar") == 0) {
            // Expected: downltar <filetype>
            char filetype[10];
            if(sscanf(buffer, "%*s %s", filetype) != 1) {
                send(client_sock, "Invalid command syntax\n", 23, 0);
                continue;
            }
            if(strcasecmp(filetype, ".c") == 0) {
                // Create tar archive from ./S1 for C files
                char tarname[20];
                strcpy(tarname, "cfiles.tar");
                char cmdline[600];
                snprintf(cmdline, sizeof(cmdline), "tar -cf %s ./S1 >/dev/null 2>&1", tarname);
                system(cmdline);
                FILE *fp = fopen(tarname, "rb");
                if(!fp) {
                    send(client_sock, "ERROR creating tar\n", 21, 0);
                    continue;
                }
                fseek(fp, 0, SEEK_END);
                int filesize = ftell(fp);
                rewind(fp);
                uint32_t net_filesize = htonl(filesize);
                send(client_sock, &net_filesize, sizeof(net_filesize), 0);
                char filebuf[BUFSIZE];
                while((n = fread(filebuf, 1, BUFSIZE, fp)) > 0)
                    send(client_sock, filebuf, n, 0);
                fclose(fp);
                remove(tarname);
            }
            else {
                int target_port = 0;
                if(strcasecmp(filetype, ".pdf") == 0)
                    target_port = 9002;
                else if(strcasecmp(filetype, ".txt") == 0)
                    target_port = 9003;
                else if(strcasecmp(filetype, ".zip") == 0)
                    target_port = 9004;
                else {
                    send(client_sock, "Unsupported file type for tar\n", 31, 0);
                    continue;
                }
                int sock_remote = socket(AF_INET, SOCK_STREAM, 0);
                if(sock_remote < 0) { perror("ERROR opening remote socket"); continue; }
                struct sockaddr_in remote_addr;
                memset(&remote_addr, 0, sizeof(remote_addr));
                remote_addr.sin_family = AF_INET;
                remote_addr.sin_port = htons(target_port);
                remote_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                if(connect(sock_remote, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
                    perror("ERROR connecting to remote server");
                    close(sock_remote);
                    continue;
                }
                if(send_all(sock_remote, buffer, strlen(buffer)) < (ssize_t)strlen(buffer)) {
                    perror("Error sending remote downltar command");
                    close(sock_remote);
                    continue;
                }
                uint32_t net_filesize_remote;
                if(recv_all(sock_remote, &net_filesize_remote, sizeof(net_filesize_remote)) != sizeof(net_filesize_remote)) {
                    send(client_sock, "Error receiving tar filesize from remote server\n", 50, 0);
                    close(sock_remote);
                    continue;
                }
                send(client_sock, &net_filesize_remote, sizeof(net_filesize_remote), 0);
                int remote_filesize = ntohl(net_filesize_remote);
                char tempbuf[BUFSIZE];
                int total_received = 0;
                while(total_received < remote_filesize) {
                    int rec = recv(sock_remote, tempbuf, BUFSIZE, 0);
                    if(rec <= 0)
                        break;
                    send(client_sock, tempbuf, rec, 0);
                    total_received += rec;
                }
                close(sock_remote);
            }
        }
        else if (strcasecmp(cmd, "dispfnames") == 0) {
            char pathname[512];
            if (sscanf(buffer, "%*s %s", pathname) != 1) {
                send(client_sock, "Invalid command syntax\n", 23, 0);
                continue;
            }
            // Normalize the sub-path: remove "~S1" and if only "/" remains, use an empty string.
            char subpath[512] = "";
            char *p = strstr(pathname, "~S1");
            if (p != NULL) {
                p += 3; // Skip "~S1"
                // If the remaining string is "/" or empty, set subpath to empty.
                if (strcmp(p, "/") != 0 && strlen(p) > 0)
                    strncpy(subpath, p, sizeof(subpath) - 1);
            } else {
                strncpy(subpath, pathname, sizeof(subpath) - 1);
            }
            // Construct local path: S1's base directory is "./S1"
            char local_path[600];
            snprintf(local_path, sizeof(local_path), "./S1%s", subpath);
            
            // Execute the find command using popen().
            char find_cmd[700];
            snprintf(find_cmd, sizeof(find_cmd), "find %s -maxdepth 1 -type f | sort", local_path);
            FILE *fp = popen(find_cmd, "r");
            char combined[4096];
            combined[0] = '\0';
            if (fp != NULL) {
                char temp[256];
                while (fgets(temp, sizeof(temp), fp)) {
                    strncat(combined, temp, sizeof(combined) - strlen(combined) - 1);
                }
                pclose(fp);
            } else {
                strncat(combined, "Error listing local files\n", sizeof(combined)-strlen(combined)-1);
            }
            
            // Query remote servers (S2, S3, and S4)
            int remote_ports[3] = {9002, 9003, 9004};
            for (int i = 0; i < 3; i++) {
                int sock_remote = socket(AF_INET, SOCK_STREAM, 0);
                if (sock_remote < 0)
                    continue;
                struct sockaddr_in remote_addr;
                memset(&remote_addr, 0, sizeof(remote_addr));
                remote_addr.sin_family = AF_INET;
                remote_addr.sin_port = htons(remote_ports[i]);
                remote_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                if (connect(sock_remote, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
                    close(sock_remote);
                    continue;
                }
                // Forward the same dispfnames command from the client.
                if (send_all(sock_remote, buffer, strlen(buffer)) < (ssize_t)strlen(buffer)) {
                    close(sock_remote);
                    continue;
                }
                char remote_buf[1024];
                int r;
                while ((r = recv(sock_remote, remote_buf, sizeof(remote_buf) - 1, 0)) > 0) {
                    remote_buf[r] = '\0';
                    strncat(combined, remote_buf, sizeof(combined)-strlen(combined)-1);
                }
                close(sock_remote);
            }
            
            if (strlen(combined) == 0)
                send(client_sock, "No files found\n", 15, 0);
            else
                send(client_sock, combined, strlen(combined), 0);
        }
        else {
            send(client_sock, "Invalid command\n", 16, 0);
        }
    }
    close(client_sock);
}

/*---------------- Main Function: Accept and Fork ----------------*/

int main(int argc, char *argv[]){
    int sockfd, newsockfd, portno;
    pid_t pid;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    
    if(argc < 2) {
       fprintf(stderr, "Usage: %s port\n", argv[0]);
       exit(1);
    }
    portno = atoi(argv[1]);
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
       if(pid == 0) { // Child process
          close(sockfd);
          prcclient(newsockfd);
          exit(0);
       }
       else {
          close(newsockfd);
       }
    }
    close(sockfd);
    return 0;
}
