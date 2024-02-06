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
#include <time.h>

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

    char* packet_string;

    struct timespec start, end;
    double elapsed_time;

    //check input formatting
    if (strncmp(buffer, "ftp ", 4) == 0) {

        file_name = buffer + 4;
        //printf("?%s?", file_name); 

        FILE *file = fopen(file_name, "rb"); 

        if (file == NULL) { 

            perror("Error opening file");
            exit(1);

        } else { 

            //time 1
            clock_gettime(CLOCK_MONOTONIC, &start);


            long file_size;
            fseek(file, 0, SEEK_END);
            file_size = ftell(file);
            rewind(file);

            const int chunk_size = 1000; // Maximum payload size
            const int max_header_size = 50; // Adjust based on your needs
            int total_fragments = (file_size / (chunk_size - max_header_size)) + (file_size % (chunk_size - max_header_size) != 0); // Calculate total number of fragments

            char ack[4]; // To receive "ack" message
            ssize_t numbytes;

            for (int i = 0; i < total_fragments; i++) {
                size_t remaining_size = file_size - i * (chunk_size - max_header_size);
                size_t current_chunk_size = remaining_size > (chunk_size - max_header_size) ? (chunk_size - max_header_size) : remaining_size;
                char current_chunk_size_str[20]; // To hold the current_chunk_size in string format
                sprintf(current_chunk_size_str, "%ld", current_chunk_size); // Convert current_chunk_size to string

                size_t packet_size = max_header_size + current_chunk_size; // Adjust packet_size calculation if necessary

                char *packet_buffer = malloc(packet_size);
                if (packet_buffer == NULL) {
                    fprintf(stderr, "Error: Memory allocation failed.\n");
                    fclose(file);
                    exit(1);
                }

                // Construct the packet header with fragment info "total_fragments:current_fragment_index:current_chunk_size:file_name:"
                int offset = sprintf(packet_buffer, "%d:%d:%s:", total_fragments, i + 1, current_chunk_size_str);
                memcpy(packet_buffer + offset, file_name, strlen(file_name));
                offset += strlen(file_name);
                packet_buffer[offset++] = ':';

                // Read the current chunk from the file
                fseek(file, i * (chunk_size - max_header_size), SEEK_SET);
                fread(packet_buffer + offset, sizeof(char), current_chunk_size, file);

                // Send the current packet
                if ((numbytes = sendto(sockfd, packet_buffer, offset + current_chunk_size, 0, p->ai_addr, p->ai_addrlen)) == -1) {
                    perror("talker: sendto");
                    free(packet_buffer);
                    fclose(file);
                    exit(1);
                }

                printf("Sent %zd bytes for fragment %d\n", numbytes, i + 1);

                // Wait for "ack" from server
                if ((numbytes = recv(sockfd, ack, sizeof(ack) - 1, 0)) == -1) {
                    perror("listener: recv");
                    free(packet_buffer);
                    fclose(file);
                    exit(1);
                }
                ack[numbytes] = '\0'; // Null-terminate the received data
                if (strcmp(ack, "ack") != 0) {
                    printf("ACK not received, stopping.\n");
                    free(packet_buffer);
                    fclose(file);
                    exit(1);
                }

                free(packet_buffer); // Free the packet buffer after each fragment is sent
            }

            fclose(file);


        }

    } else {
        printf("Invalid input\n");
        exit(1);
    }


    freeaddrinfo(servinfo);
    // printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);



    // printf("talker: waiting to recvfrom...\n");

    // addr_len = sizeof their_addr;
    // // wait and receive answer from server
    // if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
    //         (struct sockaddr *) &their_addr, &addr_len)) == -1) {
    //     perror("recvfrom");
    //     exit(1);
    // }

    //time 2

    // printf("talker: got packet from %s\n",
    //         inet_ntop(their_addr.ss_family,
    //         get_in_addr((struct sockaddr *) &their_addr),
    //         s, sizeof s));

    // printf("talker: packet is %d bytes long\n", numbytes);
    // buf[numbytes] = '\0';
    // printf("talker: packet contains \"%s\"\n", buf);

    // if (strcmp(buf, "yes") == 0) {
    //     printf("A file transfer can start\n");
    // }


    // Get the current time at the end
    clock_gettime(CLOCK_MONOTONIC, &end);

    // Calculate the elapsed time in seconds
    elapsed_time = end.tv_sec - start.tv_sec;
    elapsed_time += (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("Elapsed time: %.5f seconds\n", elapsed_time);
    

    close(sockfd);

    return 0;
}
