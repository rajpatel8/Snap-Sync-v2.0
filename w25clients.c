#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define bool int
#define true 1
#define false 0

// Global variables to store server information
char server_hostname[256];
char server_ip[256];

int upload_file(const char *filename, const char *dest);

bool is_valid_file(const char *filename) ;

int sanitize_input(char * buffer); 

void error(const char *msg);

// Parse server hostname and IP address from welcome message
void parse_server_info(const char *buffer, char *hostname, char *ip_address) {
    char *hostname_ptr = strstr(buffer, "Hostname: ");
    char *ip_ptr = strstr(buffer, "IP Address: ");
    
    if (hostname_ptr && ip_ptr) {
        hostname_ptr += 10; // Skip "Hostname: "
        char *end = strchr(hostname_ptr, '\n');
        if (end) {
            size_t len = end - hostname_ptr;
            strncpy(hostname, hostname_ptr, len);
            hostname[len] = '\0';
        }
        
        ip_ptr += 12; // Skip "IP Address: "
        end = strchr(ip_ptr, '\n');
        if (end) {
            size_t len = end - ip_ptr;
            strncpy(ip_address, ip_ptr, len);
            ip_address[len] = '\0';
        }
    }
}

int main(int argc, char *argv[])
{
    // signal(SIGCHLD,SIG_IGN) ;
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
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

    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;

    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);

    serv_addr.sin_port = htons(portno);

    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    // Receive initial welcome message with server information
    bzero(buffer, 256);
    n = read(sockfd, buffer, 255);
    if (n < 0) 
        error("ERROR reading from socket");
    
    // Display welcome message
    printf("%s\n", buffer);
    
    // Parse and store server hostname and IP
    parse_server_info(buffer, server_hostname, server_ip);
    
    // Print confirmation that we've connected to the server
    if (strlen(server_hostname) > 0 && strlen(server_ip) > 0) {
        printf("Connected to server: %s (%s)\n", server_hostname, server_ip);
    }

    while (1)
    {
        printf("Please enter Command : \n");

        bzero(buffer,256);
        fgets(buffer,256,stdin);

        // copy the buffer to a new string
        char * buffer_copy = malloc(strlen(buffer) + 1);
        if (buffer_copy == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }
        strcpy(buffer_copy, buffer);

        int mode_bit = sanitize_input(buffer_copy) ;

        // print buffer
        printf("Command : %s\n", buffer);

        switch (mode_bit)
        {
            case 1 :
            {
                // Skip the command name (uploadf) and get the actual filename and destination
                char *command = strtok(buffer, " ");
                char *filename = strtok(NULL, " ");
                char *destination = strtok(NULL, " ");
                if (filename && destination) {
                    upload_file(filename, destination);
                }
                break;
            }
        }

    /*
    TODO : uploadf filename deatination_path
           downlf filename
           removef fielname
           dispfname pathname 
            - c , pdf , txt and zip files
            - In-oder, chronlogical order
            
    */


    }
        
    close(sockfd);
    return 0;
}

int sanitize_input(char * buffer){
    // check the first word, and set the function bit accordingly
    int mode_bit = 0; // default mode is disabled
    char *pch;
    pch = strtok (buffer," ");
    mode_bit = (strcasecmp(pch,"uploadf") == 0) ? 1 : 
              (strcasecmp(pch,"downlf") == 0) ? 2 : 
              (strcasecmp(pch,"removef") == 0) ? 3 : 
              (strcasecmp(pch,"dispfname") == 0) ? 4 : 
              (strcasecmp(pch,"exit") == 0) ? 5 : -1;

    // Now caheck the rest of the string accordingly
    switch (mode_bit)
    {
        case -1:
            printf("Invalid command\n");
            return 0;
        
        case 1:
            // uploadf, check if the filename file exists in pwd
            pch = strtok (NULL, " ");
            if (pch == NULL) {
                printf("Please provide a filename\n");
                break;
            }
            if (is_valid_file(pch)) {
                printf("File %s exists\n", pch);
                // check if the file is a valid .c , .pdf , .txt or .zip file
                char *ext = strrchr(pch, '.');
                if (ext == NULL) {
                    printf("File %s does not have a valid extension\n", pch);
                    // reset the bit
                    return 0;
                }
                if (strcasecmp(ext, ".c") != 0 && strcasecmp(ext, ".pdf") != 0 && strcasecmp(ext, ".txt") != 0 && strcasecmp(ext, ".zip") != 0) {
                    printf("File %s does not have a valid extension\n", pch);
                    // reset the bit
                    return 0;
                }
                return mode_bit ;
            } else {
                printf("File %s does not exist\n", pch);
                // reset the bit
                return 0;
            }
            break;
        case 2:
            // downlf command code will go here
            break;
            
        case 3:
            // removef command code will go here
            break;
            
        case 4:
            // dispfname command code will go here
            break;
            
        case 5:
            // exit command
            printf("Disconnecting from server: %s (%s)\n", server_hostname, server_ip);
            exit(0);
            break;
    }
    
    return mode_bit;
}

bool is_valid_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int upload_file(const char *filename, const char *dest) {
    char command[1024];
    int status;
    
    // Trim any newline characters from destination
    char clean_dest[256];
    strncpy(clean_dest, dest, sizeof(clean_dest) - 1);
    clean_dest[sizeof(clean_dest) - 1] = '\0';
    char *newline = strchr(clean_dest, '\n');
    if (newline) {
        *newline = '\0';
    }
    
    // Create the rsync command
    snprintf(command, sizeof(command), 
             "rsync -avz %s %s:%s", 
             filename, server_ip, clean_dest);
    
    printf("Executing: %s\n", command);
    
    // Execute the command
    status = system(command);
    
    if (status == 0) {
        printf("File '%s' successfully uploaded to %s (%s):%s\n", 
               filename, server_hostname, server_ip, clean_dest);
        return 0;
    } else {
        fprintf(stderr, "Failed to upload file '%s' to server\n", filename);
        return -1;
    }
}
