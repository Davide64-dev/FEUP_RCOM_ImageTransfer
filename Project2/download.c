#include "download.h"

int parse(const char *input, struct URL *url){

    regex_t regex;
    
    regcomp(&regex, "@", 0);

    if (regexec(&regex, input, 0, NULL, 0) != 0)
    {
        if (sscanf(input, "%*[^/]//%[^/]/%s", url->host, url->path) == 2) {
            strcpy(url->user, USER_DEFAULT);
            strcpy(url->password, PASS_DEFAULT);
            char *ip = get_ip_addr(url);
            strcpy(url->ip, ip);
            return 0;
        }
        return -1;
    }
    else {
            sscanf(input, USER_REGEX, url->user);
            sscanf(input, PASSWORD_REGEX, url->password);
            sscanf(input, HOST_REGEX, url->host);
            sscanf(input, URL_REGEX, url->path);
            char *ip = get_ip_addr(url);
            strcpy(url->ip, ip);
            return 0;
    }
}

char* get_ip_addr(const struct URL* url){
    struct hostent *h;
    char* ip = (char*) malloc(MAX_LENGTH);
    memset(ip, 0, MAX_LENGTH);


    if ((h=gethostbyname(url->host)) == NULL) {
        herror("gethostbyname");
        exit(1);
    }
    ip = inet_ntoa(*((struct in_addr *)h->h_addr));
    return ip;
}

int createSocket(char *ip, int port)
{
    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip); /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);          /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket()");
        return -1;
    }
    /*connect to the server*/
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect()");
        return -1;
    }

    return sockfd;
}


int readResponse(const int socket, char* buffer) {

    char byte;

    int index = 0, responseCode;
    ResponseState state = START;
    memset(buffer, 0, MAX_LENGTH);

    while (state != END) {
        
        read(socket, &byte, 1);
        printf("%c", byte);
        switch (state) {
            case START:
                if (byte == ' ') state = SINGLE;
                else if (byte == '-') state = MULTIPLE;
                else if (byte == '\n') state = END;
                else buffer[index++] = byte;
                break;
            case SINGLE:
                if (byte == '\n') state = END;
                else buffer[index++] = byte;
                break;
            case MULTIPLE:
                if (byte == '\n') {
                    memset(buffer, 0, MAX_LENGTH);
                    state = START;
                    index = 0;
                }
                else buffer[index++] = byte;
                break;
            case END:
                break;
            default:
                break;
        }
        
    }

    sscanf(buffer, "%d", &responseCode);
    return responseCode;
}



int authenticate(const int socket, const char* user, const char* pass) {


    char userCommand[5+strlen(user)+1]; sprintf(userCommand, "user %s\n", user);
    char passCommand[5+strlen(pass)+1]; sprintf(passCommand, "pass %s\n", pass);
    char answer[MAX_LENGTH];
    
    write(socket, userCommand, strlen(userCommand));
    if (readResponse(socket, answer) != 331) {
        printf("Unknown user '%s'. Abort.\n", user);
        exit(-1);
    }

    write(socket, passCommand, strlen(passCommand));
    return readResponse(socket, answer);
    
}

int changePath(const int socket, const char* path){
    char path1[strlen(path)];
    char path1[strlen(path) + 1];

    char answer[MAX_LENGTH];
    strcpy(path1, path);

    int size = strlen(path1);
    int found = 0;

    int count = 0;
    for (int i = size - 1; i >= 0; i--){
        if (path1[i] == '/') {
            path1[i] = '\0';
            found = 1;
            break;    
        }
    }

    if (found == 0) return 1;
    char command[4+strlen(path1)+2]; sprintf(command, "cwd %s\r\n", path1);

    write(socket, command, strlen(command));

    printf("%s", command);


    readResponse(socket, answer);

    printf("The path after the changePath is: %s\n", path);
}

int putPASV(const int socket){
    char answer[MAX_LENGTH];
    char command[7];
    sprintf(command, "pasv\r\n");

    write(socket, command, 7);

    readResponse(socket, answer);
}

int getFile(const int socket, const char* path){

    printf("The path is: %s\n", path);
    char *path1[len(path)];
    strcpy(path1, path);
    char answer[MAX_LENGTH];


    int size = strlen(path);
    int found = 0;

    for (int i = size - 1; i >= 0; i--) {
        if (path[i] == '/') {
            strcpy(path1, path[i]);
            break;
        }
    }

    printf("File name: %s\n", answer);
}



int main(int argc, char *argv[]) {

    char *input = argv[1];

    struct URL* url = malloc(sizeof(struct URL));
    parse(input, url);

    printf("Input: %s\n", input);
    printf("User: %s\n", url->user);
    printf("Password: %s\n", url->password);
    printf("Host: %s\n", url->host);
    printf("Path: %s\n", url->path);
    printf("IP: %s\n", url->ip);

    int socketA = createSocket(url->ip, 21);
    char* answer = malloc(100);

    if (socketA < 0 || readResponse(socketA, answer) != 220){
        return -1;
    }

    authenticate(socketA, url->user, url->password);

    changePath(socketA, url->path);

    putPASV(socketA);

    getFile(socketA, url->path);

    //printf("Socket Number: %d\n", socketA);


    free(url);
    return 0;
}