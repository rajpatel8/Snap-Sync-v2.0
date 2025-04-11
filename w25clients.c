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

int Mode = 0 ;

bool is_valid_destination(const char *dest) ;

bool sanitize_command(char * command) ;

bool is_valid_file(const char *filename) ;

void error(const char *msg);

void reset_val(int value) ;

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

    printf("\n------ Connected to Lord ------\n") ;

    while (true)
    {
        
        printf("Delta ~ $ ") ;

        char command[256] ;

        bzero(command,256);
        fgets(command,sizeof(command),stdin) ;
        command[strcspn(command, "\n")] = '\0'; // triming \n bcoz it causes error in later part
        
        char * command_copy = malloc(sizeof(command)+1) ;

        if (command_copy == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }

        strcpy(command_copy, command) ;

        if(!sanitize_command(command_copy)){
            Mode = 0 ;
            free(command_copy) ;
            error("Invalid Command\n");
        }

        // TODO : Implemet Dis 
        // switch(Mode){
        //     case 1 :

        // }

    }
    
    
    return 0;
}

void error(const char *msg){
    perror(msg);
    // exit(0);
}

bool sanitize_command(char * command){
    char * token = strtok(command , " ") ;
    // printf(command) ;                        // DEBUG  
                 
    if (strcasecmp(token,"uploadf")==0)
    {
        Mode = 1 ;
        token = strtok(NULL," ") ;
        if (token != NULL) {
            token[strcspn(token, "\n")] = '\0';
        }

        if (is_valid_file(token))
        {
            printf("File Exists\n") ;
            char * ext = strrchr(token,'.') ;

            if (strcasecmp(ext, ".c") != 0 && strcasecmp(ext, ".pdf") != 0 && strcasecmp(ext, ".txt") != 0 && strcasecmp(ext, ".zip") != 0)
            {
                // printf(ext) ;                // DEBUG
                printf("Invalid File Extension\n") ;
                return false ;
            }

            token = strtok(NULL," ") ;
            // printf(token) ;                     // DEBUG
            if(!is_valid_destination(token)) {
                printf("Please enter a valid Destination Path\n") ;
                reset_val(Mode);
                return false ;
            }
            return true ;
        }else{
            reset_val(Mode);
            return false ;
        }
    }else if(strcasecmp(token,"downlf")==0){
        Mode = 2 ;
        token = strtok(NULL," ") ;
        if (token != NULL) {
            token[strcspn(token, "\n")] = '\0';
        }else{
            reset_val(Mode) ;
            return false ;
        }
        // printf(token) ;                             // DEBUG
    }
    
    return true ;
}

bool is_valid_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

bool is_valid_destination(const char *dest) {

    if (dest == NULL || dest[0] == '\0')
        return false;

    size_t len = strlen(dest);
    if (len > 4096)
        return false;

    for (size_t i = 0; i < len; i++) {
        char c = dest[i];

        if (c < 32 || c > 126)
            return false;

        if (c == '\\')
            return false;

        if (!((c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '/' ||
              c == '-' ||
              c == '_' ||
              c == '.' ||
              c == ' '))
        {
            return false;
        }
    }

    if (strcmp(dest, "/") != 0) { 
        for (size_t i = 0; i < len - 1; i++) {
            if (dest[i] == '/' && dest[i + 1] == '/')
                return false;
        }
    }

    return true;
}

void reset_val(int value){
    value = 0 ;
}