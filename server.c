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
#include <stdbool.h>

#define MAXBUFLEN 1000

// get sockaddr, IPv4 or IPv6:

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    
    if (argc != 2) {
        fprintf(stderr,"usage: listener port\n");
        exit(1);
    }
    
    char* MYPORT = argv[1];

    if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("listener: waiting to recvfrom...\n");

    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
            (struct sockaddr *) &their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    printf("listener: got packet from %s\n",
            inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *) &their_addr),
            s, sizeof s));
    printf("listener: packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);
    
    


    FILE *file = NULL;
    int ptr = 0;
    int total_frag;
    int frag_num = 1;

    // read total frag 

    char total_frag_str[256];
    int i = 0;

    while (buf[ptr] != ':') {
        total_frag_str[i] = buf[ptr];
        i++; 
        ptr++;
    }

    printf("\n");

    ptr++;

    total_frag = atoi(total_frag_str);

    bool done = false;

    int start_ptr = ptr;

    while (!done) {

      //  printf("BUFFER: \"%s\"\n", buf);
        
        ptr = start_ptr;

     //   printf("PTR: %d", ptr);
     //   printf("\n");

        // read frag num
        
        char frag_num_str[256];
        int k = 0;

        while (buf[ptr] != ':') {
            frag_num_str[k] = buf[ptr];
            k++; 
            ptr++;
        }

        frag_num = atoi(frag_num_str);

     //   printf("FRAG NUM: %s", frag_num_str);
     //   printf("\n");

        printf("\n");

        ptr++;


        // read data size
        char num_bytes_str[256];
        int index = 0;

        while (buf[ptr] != ':') {
            num_bytes_str[index] = buf[ptr];
            index++;  
            ptr++;
        }
    
    //    printf("%s", num_bytes_str);

        //printf("%d", num_bytes);
  //     printf("\n");

        ptr++;


        // read file name
        
        char file_name[256];
        int index_2 = 0;

        while (buf[ptr] != ':' && ptr <= numbytes) {
            file_name[index_2] = buf[ptr];
            index_2++;
            ptr++;
        }
        

     //   printf("%s", file_name);
     //   printf("\n");
    

        

        ptr++;


        // read file content

        char file_content[1000];
        int index_3 = 0;

        while (index_3 <= numbytes) {
            file_content[index_3] = buf[ptr];
            index_3++;
            ptr++;
        }
        

      //  printf("%s", file_content);
      //  printf("\n");


        if (file == NULL) {
            file = fopen(file_name, "ab");
        }

        if (file == NULL) {
            perror("Error opening file");
            return -1;
        }

     //   printf("FILE CONTENT: %s" , file_content);
     //   printf("NUM BYTES = %s", num_bytes_str);

        fwrite(file_content, sizeof(char), atoi(num_bytes_str), file);

      //  printf("Frag num: %d", frag_num);
      //  printf("\n");

        if (sendto(sockfd, "ack", strlen("ack"), 0, (struct sockaddr *)&their_addr, addr_len) == -1) {
            perror("sendto ack");
            exit(1);
        }


        if (frag_num == total_frag) {

            done = true;

        } else {

            addr_len = sizeof their_addr;
            if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
                    (struct sockaddr *) &their_addr, &addr_len)) == -1) {
                perror("recvfrom");
                exit(1);
            }

            printf("listener: got packet from %s\n",
                    inet_ntop(their_addr.ss_family,
                    get_in_addr((struct sockaddr *) &their_addr),
                    s, sizeof s));
            printf("listener: packet is %d bytes long\n", numbytes);
            buf[numbytes] = '\0';
            printf("listener: packet contains \"%s\"\n", buf);
        }

    }



    fclose(file);



    // if (strcmp(buf, "ftp") == 0) {
    //     if ((numbytes = sendto(sockfd, "yes", strlen("yes"), 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
    //         perror("listener: sendto");
    //         exit(1);
    //     }
    // } else {
    //     if ((numbytes = sendto(sockfd, "no", strlen("no"), 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
    //         perror("listener: sendto");
    //         exit(1);
    //     }
    // }
     
    close(sockfd);

    return 0;
}