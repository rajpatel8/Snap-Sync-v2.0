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

bool is_valid_file(const char *filename) ;

int sanitize_input(char * buffer); 

void error(const char *msg);

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

    while (1)
    {
        printf("Please enter Command : \n");

        bzero(buffer,256);
        fgets(buffer,256,stdin);

        sanitize_input(buffer) ;

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
                // get the destination path
                char * dest = strtok (NULL, " ");
                // upload_file(pch, dest);
            } else {
                printf("File %s does not exist\n", pch);
            }
            break;
            
            
    }
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