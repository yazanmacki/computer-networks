#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_NAME 256
#define MAX_DATA 256

enum MessageType { LOGIN = 1, JOIN = 2, LEAVE_SESS = 3, MESSAGE = 4, OTHER };

int sockfd = -1; // Socket file descriptor
int n;

pthread_mutex_t ack_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ack_cond = PTHREAD_COND_INITIALIZER;
char ack_buffer[MAX_DATA + 1]; // Buffer to store the latest ACK message
int ack_received = 0; // Flag to indicate if an ACK has been received


enum AckStatus { NO_MSG, ACK, NACK };
enum AckStatus ack_status = NO_MSG; // Initial state: No message received


struct message {
	unsigned int type;
	unsigned int size;
	unsigned char source[MAX_NAME];
	unsigned char data[MAX_DATA];
};

// Function to continuously listen for messages from the server
void* receive_messages(void* arg) {
    char buffer[MAX_DATA + 1];
    while(1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t len = recv(sockfd, buffer, MAX_DATA, 0);
        if (len <= 0) {
            perror("recv");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        buffer[len] = '\0';

        pthread_mutex_lock(&ack_mutex);
        if (strncmp(buffer, "ACK", 3) == 0) {
            ack_status = ACK;
            strncpy(ack_buffer, buffer, MAX_DATA);
        } else if (strncmp(buffer, "NACK", 4) == 0) {
            ack_status = NACK;
            strncpy(ack_buffer, buffer, MAX_DATA);
        } else {
            printf("Server: %s\n", buffer);
            pthread_mutex_unlock(&ack_mutex);
            continue;
        }
        pthread_cond_signal(&ack_cond);
        pthread_mutex_unlock(&ack_mutex);
    }
    return NULL;
}


int wait_for_ack() {
    int result = 0; // Default to timeout or error

    pthread_mutex_lock(&ack_mutex);
    while (ack_status == NO_MSG) {
        pthread_cond_wait(&ack_cond, &ack_mutex);
    }

    if (ack_status == ACK) {
        printf("Received ACK: %s\n", ack_buffer);
        result = 1; // Success
    } else if (ack_status == NACK) {
        printf("Received NACK: %s\n", ack_buffer);
        result = 0; // Failure
    }

    ack_status = NO_MSG; // Reset status for next use
    pthread_mutex_unlock(&ack_mutex);

    return result;
}


// Function to connect to the server
int connect_to_server(const char* hostname, int port) {
    struct sockaddr_in serv_addr;
    struct hostent* server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return -1;
    }

    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        return -1;
    }

    return 0;
}

// Main function
int main() {
    char command[MAX_NAME];
    pthread_t recv_thread;

	char client_id[256];
	strcpy(client_id, "");

	char curr_session[256];
	strcpy(curr_session, "");

    while (1) {
		
        if (scanf("%s", command) != 1) {
            fprintf(stderr, "Failed to read command\n");
            continue;
        }
		 
		// if (!fgets(command, sizeof(command), stdin)) {
		// 	fprintf(stderr, "Failed to read command\n");
		// 	continue;
		// }
		// // Remove the newline character at the end if present
		// size_t len = strlen(command);
		// if (len > 0 && command[len - 1] == '\n') {
		// 	command[len - 1] = '\0';
		// }
		
        if (strcmp(command, "login") == 0) { 

			char clientName[MAX_NAME];
			char clientPassword[MAX_DATA];
			char hostname[256];
			int portno;

			// Since it's a login command, now read the additional arguments
			scanf("%s %s %s %d", clientName, clientPassword, hostname, &portno);

			struct sockaddr_in serv_addr;
			struct hostent *server;

			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if (sockfd < 0) {
				perror("ERROR opening socket");
				exit(EXIT_FAILURE);
			}

			server = gethostbyname(hostname);
			if (server == NULL) {
				fprintf(stderr, "ERROR, no such host\n");
				exit(EXIT_FAILURE);
			}

			memset(&serv_addr, 0, sizeof(serv_addr));
			serv_addr.sin_family = AF_INET;
			bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
			serv_addr.sin_port = htons(portno);

			if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
				perror("ERROR connecting");
				exit(EXIT_FAILURE);
			}

			if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
                perror("pthread_create");
                close(sockfd);
                exit(EXIT_FAILURE);
            }
            pthread_detach(recv_thread); // Detach thread

			struct message loginMessage;
			loginMessage.type = LOGIN;
			loginMessage.size = strlen(clientPassword);
			strncpy((char *)loginMessage.source, clientName, MAX_NAME);
			strncpy((char *)loginMessage.data, clientPassword, MAX_DATA);

			// Prepare the packet for sending
			char packet[1024];
			sprintf(packet, "LOGIN:%d:%s:%s", loginMessage.size, loginMessage.source, loginMessage.data);

			// Send the packet
			n = send(sockfd, packet, strlen(packet), 0);
			if (n < 0) {
				perror("ERROR writing to socket");
				exit(EXIT_FAILURE);
			}

			// char buffer[1024];
			// memset(buffer, 0, 1024);
			// ssize_t read_size = recv(sockfd, buffer, 1023, 0);
			// if (read_size < 0) {
			// 	perror("ERROR reading from socket"); 
			// 	close(sockfd);
			// } else {
			// 	buffer[read_size] = '\0';
			// }

			// if (strcmp(buffer, "ACK") == 0) { 
			// 	printf("login successful\n"); 
			// 	strcpy(client_id, loginMessage.source);
			// } else {
			// 	printf("login failed\n");
			// }

			if (wait_for_ack() == 1) {
				printf("login successful\n"); 
				strcpy(client_id, loginMessage.source);  //set cur user ID to client ID
			} else {
				printf("login failed\n"); 
			}
			
			
			
        } else { 
			
			 

			if (strcmp(command, "createsession") == 0) { 

				char session_id[MAX_NAME]; 

				scanf("%s", session_id); 

				struct message createMessage;
				createMessage.type = JOIN;
				createMessage.size = strlen(session_id);
				strncpy((char *)createMessage.source, client_id, MAX_NAME);
				strncpy((char *)createMessage.data, session_id, MAX_DATA);

				// Prepare the packet for sending
				char packet[1024];
				sprintf(packet, "NEW_SESS:%d:%s:%s", createMessage.size, createMessage.source, createMessage.data);

				// Send the packet
				n = send(sockfd, packet, strlen(packet), 0);
				if (n < 0) {
					perror("ERROR writing to socket");
					exit(EXIT_FAILURE);
				}

				if (wait_for_ack() == 1) {
					printf("createsession successful\n"); 
					strcpy(curr_session, createMessage.data);
				} else {
					printf("createsession failed\n"); 
				}

				// char buffer[1024];
				// memset(buffer, 0, 1024);
				// ssize_t read_size = recv(sockfd, buffer, 1023, 0);
				// if (read_size < 0) {
				// 	perror("ERROR reading from socket"); 
				// 	close(sockfd);
				// } else {
				// 	buffer[read_size] = '\0';
				// }

				// printf("%s\n", buffer);

				// if (strcmp(buffer, "ACK") == 0) { 
				// 	printf("create session successful\n"); 
				// 	strcpy(curr_session, createMessage.data);
				// } else {
				// 	printf("create session failed\n");
				// } 

			} else if (strcmp(command, "joinsession") == 0) { 

				char session_id[MAX_NAME]; 

				// Since it's a login command, now read the additional arguments
				scanf("%s", session_id); 


				struct message joinMessage;
				joinMessage.type = JOIN;
				joinMessage.size = strlen(session_id);
				strncpy((char *)joinMessage.source, client_id, MAX_NAME);
				strncpy((char *)joinMessage.data, session_id, MAX_DATA);

				// Prepare the packet for sending
				char packet[1024];
				sprintf(packet, "JOIN:%d:%s:%s", joinMessage.size, joinMessage.source, joinMessage.data);

				// Send the packet
				n = send(sockfd, packet, strlen(packet), 0);
				if (n < 0) {
					perror("ERROR writing to socket");
					exit(EXIT_FAILURE);
				}

				if (wait_for_ack() == 1) {
					printf("joinsession successful\n"); 
					strcpy(curr_session, joinMessage.data);
				} else {
					printf("joinsession failed\n"); 
				}

				// char buffer[1024];
				// memset(buffer, 0, 1024);
				// ssize_t read_size = recv(sockfd, buffer, 1023, 0);
				// if (read_size < 0) {
				// 	perror("ERROR reading from socket"); 
				// 	close(sockfd);
				// } else {
				// 	buffer[read_size] = '\0';
				// }

				// printf("%s\n", buffer);

				// if (strcmp(buffer, "ACK") == 0) { 
				// 	printf("join session successful\n"); 
				// 	strcpy(curr_session, joinMessage.data);
				// } else {
				// 	printf("join session failed\n");
				//} 

			} else if (strcmp(command, "list") == 0) { 

				char packet[1024];
				sprintf(packet, "LIST:::");

				// Send the packet
				n = send(sockfd, packet, strlen(packet), 0);
				if (n < 0) {
					perror("ERROR writing to socket");
					exit(EXIT_FAILURE);
				}
			
			} else if (strcmp(command, "logout") == 0) {

				strcpy(client_id, "");
				strcpy(curr_session, "");

				// Prepare the packet for sending
				char packet[1024];
				sprintf(packet, "EXIT:::");

				// Send the packet
				n = send(sockfd, packet, strlen(packet), 0);

				if (n < 0) {
					perror("ERROR writing to socket");
					exit(EXIT_FAILURE);
				}
			
			} else if (strcmp(command, "leavesession") == 0) {
				
				struct message joinMessage;
				joinMessage.type = LEAVE_SESS;
				joinMessage.size = 0;
				strncpy((char *)joinMessage.source, client_id, MAX_NAME); 

				// Prepare the packet for sending
				char packet[1024];
				sprintf(packet, "LEAVE_SESS:%d:%s:", joinMessage.size, joinMessage.source);

				// Send the packet
				n = send(sockfd, packet, strlen(packet), 0);
				if (n < 0) {
					perror("ERROR writing to socket");
					exit(EXIT_FAILURE);
				}

				if (wait_for_ack() == 1) {
					printf("leavesession successful\n"); 
					strcpy(curr_session, "");
				} else {
					printf("leavesession failed\n"); 
				}

				// char buffer[1024];
				// memset(buffer, 0, 1024);
				// ssize_t read_size = recv(sockfd, buffer, 1023, 0);
				// if (read_size < 0) {
				// 	perror("ERROR reading from socket"); 
				// 	close(sockfd);
				// } else {
				// 	buffer[read_size] = '\0';
				// }

				// printf("%s\n", buffer);

				// if (strcmp(buffer, "ACK") == 0) { 
				// 	printf("left session\n"); 
				// 	strcpy(curr_session, "");
				// } else {
				// 	printf("leave session failed\n");
				// } 

			} else if (strcmp(command, "quit") == 0) {
				
				return 0;

			} else if (strcmp(command, "text") == 0) {

				char message[MAX_DATA]; // Buffer to store the message
				memset(message, 0, sizeof(message)); // Ensure the buffer is clean 

				ssize_t bytesRead = read(STDIN_FILENO, message, sizeof(message) - 1); // Leave space for null terminator
				if (bytesRead > 0) {

					message[bytesRead] = '\0'; // Null terminate the string

					// Now `message` contains the entire line input by the user
					struct message textMessage;
					textMessage.type = MESSAGE; // Assuming MESSAGE is the correct enum value for message sending
					textMessage.size = strlen(message);
					strncpy((char *)textMessage.source, client_id, MAX_NAME);
					strncpy((char *)textMessage.data, message, MAX_DATA);

					// Prepare the packet for sending
					char packet[1024];
					sprintf(packet, "MESSAGE:%d:%s:%s", textMessage.size, textMessage.source, textMessage.data);

					// Send the packet
					n = send(sockfd, packet, strlen(packet), 0);
					if (n < 0) {
						perror("ERROR writing to socket");
						exit(EXIT_FAILURE);
					}
				} else if (bytesRead < 0) {
					// Handle read error
					perror("read failed");
				} else {
					// EOF or no input
					printf("No input detected.\n");
				}
			}

        }
    }

    if (sockfd != -1) {
		printf("error, closing fd");
        close(sockfd);
    }

    return 0;
}
