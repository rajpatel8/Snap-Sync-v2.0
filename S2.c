/*
    S2.c
    S2 stores PDF files. It listens on port 9002.
    It supports the following commands:
      - storef <destination> <filename>
      - downlf <filepath>
      - removef <filepath>
      - downltar <filetype>   (for PDF files, expected filetype is ".pdf")
      - dispfnames <pathname>
    Compile with: gcc -o S2 S2.c
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

/* Simple error-handling routine */
void error(const char *msg) {
    perror(msg);
    exit(1);
}

/* ensure_directory creates the directory if it does not exist */
void ensure_directory(const char *path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
    system(cmd);
}

/* Helper functions for complete send/recv */
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

/* prcclient handles an incoming connection from S1 */
void prcclient(int sock) {
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    int n = recv(sock, buffer, BUFSIZE-1, 0);
    if(n <= 0) { close(sock); return; }
    buffer[n] = '\0';
    
    char cmd[32];
    sscanf(buffer, "%s", cmd);
    
    /* Base directory for S2: it stores PDF files */
    char base[256] = "./S2";
    
    if (strcasecmp(cmd, "storef") == 0) {
        // Expected: storef <destination> <filename>
        char dest[256], filename[256];
        if (sscanf(buffer, "%*s %s %s", dest, filename) != 2) {
            send(sock, "Invalid command syntax\n", 23, 0);
            close(sock);
            return;
        }
        // Send READY to S1
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
        // Build full storage path. Remove "~S1" prefix if present.
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
        // Expected: downlf <filepath>
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
        // Expected: removef <filepath>
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
        // Expected: downltar <filetype> (for S2, should be ".pdf")
        char filetype[10];
        if(sscanf(buffer, "%*s %s", filetype) != 1) {
            send(sock, "Invalid command syntax\n", 23, 0);
            close(sock);
            return;
        }
        if(strcasecmp(filetype, ".pdf") != 0) {
            send(sock, "Invalid filetype for tar\n", 26, 0);
            close(sock);
            return;
        }
        char tarname[20];
        strcpy(tarname, "pdffiles.tar");
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
        // Expected: dispfnames <pathname>
        char pathname[512];
        if (sscanf(buffer, "%*s %s", pathname) != 1) {
            send(sock, "Invalid command syntax\n", 23, 0);
            close(sock);
            return;
        }
        // Normalize the sub-path by stripping "~S1"
        char subpath[512] = "";
        char *p = strstr(pathname, "~S1");
        if (p != NULL) {
            p += 3; // Skip "~S1"
            if (strcmp(p, "/") != 0 && strlen(p) > 0)
                strncpy(subpath, p, sizeof(subpath)-1);
        } else {
            strncpy(subpath, pathname, sizeof(subpath)-1);
        }
        // Assume the base directory has already been defined; for instance:
        // For S2: char base[256] = "./S2";
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
    portno = 9002;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
         error("ERROR opening socket");
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
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
        }
        else {
            close(newsockfd);
        }
    }
    close(sockfd);
    return 0;
}
