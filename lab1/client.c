#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVERPORT "4951" // the port users will be connecting to

#define MAXBUFLEN 100

// get sockaddr, IPv4 or IPv6:

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}


void trim_newline(char *str) {
    int length = strlen(str);
    if (length > 0 && str[length - 1] == '\n') {
        str[length - 1] = '\0';
    }
}  
    

int main(int argc, char *argv[])
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    char buf[MAXBUFLEN];
    struct sockaddr_storage their_addr; 
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];

    if (argc != 3) {
        fprintf(stderr,"usage: talker hostname port\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM; //UDP

    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }

    printf("Please input ftp <file name>\n");

    char buffer[256];
    ssize_t bytes_read = read(STDIN_FILENO, buffer, 256);

    if (bytes_read == -1) {
        perror("Read error");
        return 1;
    }

    buffer[bytes_read] = '\0';
    trim_newline(buffer); 

    char* file_name;
    
    //check input formatting
    if (strncmp(buffer, "ftp ", 4) == 0) {

        file_name = buffer + 4;
        //printf("?%s?", file_name); 

        FILE *file = fopen(file_name, "r"); 

        if (file == NULL) { 

            perror("Error opening file");
            exit(1);

        } else { 

            if ((numbytes = sendto(sockfd, "ftp", strlen("ftp"), 0, p->ai_addr, p->ai_addrlen)) == -1) {
                perror("talker: sendto");
                exit(1);
            }
             
            fclose(file);
        }

    } else {
        printf("Invalid input\n");
        exit(1);
    }


    freeaddrinfo(servinfo);
    printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);



    printf("talker: waiting to recvfrom...\n");

    addr_len = sizeof their_addr;
    // wait and receive answer from server
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
            (struct sockaddr *) &their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    printf("talker: got packet from %s\n",
            inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *) &their_addr),
            s, sizeof s));

    printf("talker: packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("talker: packet contains \"%s\"\n", buf);

    if (strcmp(buf, "yes") == 0) {
        printf("A file transfer can start\n");
    }
    

    close(sockfd);

    return 0;
}
