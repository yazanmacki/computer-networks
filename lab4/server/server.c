#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_NAME 256
#define MAX_DATA 256
#define MAX_SESSIONS 50
#define MAX_SESSION_ID_LENGTH 256
#define MAX_CLIENT_ID_LENGTH 256
#define MAX_CONNECTED_USERS 100
#define MAX_CLIENTS 256 

struct User {
    char client_ID[MAX_NAME];
    char password[MAX_DATA];
};

struct ClientInfo {
    char client_ID[MAX_CLIENT_ID_LENGTH];
    int sockfd;
    char session_id[MAX_SESSION_ID_LENGTH]; // Track the session a client is part of
};


struct client {
    int sockfd; // Socket file descriptor
    char name[MAX_NAME]; // Client's name
};

struct client connected_clients[MAX_CLIENTS];
int num_connected_clients = 0;



struct Session {
    char session_id[MAX_SESSION_ID_LENGTH];
    struct ClientInfo connected_users[MAX_CONNECTED_USERS];
    int num_connected_users;
};

struct Session sessions[MAX_SESSIONS];
int num_sessions = 0;
struct User users[2] = {{"yazan", "pass"}, {"michael", "xyz"}};

int authenticate(struct User users[], int num_users, char* client_ID, char* password) {
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].client_ID, client_ID) == 0 && strcmp(users[i].password, password) == 0) {
            return 1; // Authentication successful
        }
    }
    return 0; // Authentication failed
}

char* listSessions() {
    char *message = malloc(8192); // Allocate a large enough buffer for the message
    if (!message) {
        perror("Failed to allocate memory for session list");
        return NULL;
    }
    strcpy(message, "Available sessions:\n");
    
    for (int i = 0; i < num_sessions; i++) {
        if (sessions[i].num_connected_users > 0) { // Check if the session is active
            strcat(message, "[Session name]: ");
            strcat(message, sessions[i].session_id);
            strcat(message, "\nConnected Users:\n");
            for (int j = 0; j < sessions[i].num_connected_users; j++) {
                strcat(message, "[User_client_id]: ");
                strcat(message, sessions[i].connected_users[j].client_ID);
                strcat(message, "\n");
            }
            strcat(message, "\n"); // Add an extra newline for better readability
        }
    }
    return message;
}


void removeClientFromAnySession(char *clientID) {
    for (int i = 0; i < num_sessions; i++) {
        for (int j = 0; j < sessions[i].num_connected_users; j++) {
            if (strcmp(sessions[i].connected_users[j].client_ID, clientID) == 0) {
                // Shift the rest of the clients up in the list
                for (int k = j; k < sessions[i].num_connected_users - 1; k++) {
                    sessions[i].connected_users[k] = sessions[i].connected_users[k + 1];
                }
                sessions[i].num_connected_users--;
                // Update the client's session_id to indicate they're not in a session
                strcpy(sessions[i].connected_users[j].session_id, "");
                return; // Assuming a client can only be in one session at a time
            }
        }
    }
}


void *client_handler(void *socket_desc) {
    int newsockfd = *(int*)socket_desc;
    free(socket_desc);

    int session_active = 1;

    while (session_active) {
        char buffer[1024];
        memset(buffer, 0, 1024);
        ssize_t read_size = recv(newsockfd, buffer, 1023, 0);
        if (read_size < 0) {
            perror("ERROR reading from socket");
            session_active = 0; // End session due to read error
            continue;
        } else if (read_size == 0) {
            // Client has closed the connection
            session_active = 0;
            continue;
        }

        printf("Received packet: %s\n", buffer);

        char type[6], source[MAX_NAME], data[MAX_DATA];
        unsigned int size;
        sscanf(buffer, "%[^:]:%u:%[^:]:%[^:]", type, &size, source, data);

        if (strcmp(type, "LOGIN") == 0) {
            // Check if client is already logged in
            int i;
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (connected_clients[i].sockfd != -1 && strcmp(connected_clients[i].name, source) == 0) {
                    // Client is already connected
                    send(newsockfd, "NACK", strlen("NACK"), 0);
                    break;
                }
            }
            
            if (i == MAX_CLIENTS) { // Client name not found in connected_clients
                if (authenticate(users, 2, source, data)) {
                    // Authentication successful
                    // Find an empty slot to save the client's info
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (connected_clients[i].sockfd == -1) { // Empty slot found
                            connected_clients[i].sockfd = newsockfd; // Save socket fd
                            strncpy(connected_clients[i].name, source, MAX_NAME); // Save client name
                            send(newsockfd, "ACK", strlen("ACK"), 0);
                            break;
                        }
                    }
                } else {
                    // Authentication failed
                    send(newsockfd, "NACK", strlen("NACK"), 0);
                }
            }

            
        } else if (strcmp(type, "NEW_SESS") == 0) {
            
            removeClientFromAnySession(source);

            int found = 0;

            for (int i = 0; i < num_sessions; i++) {
                if (strcmp(sessions[i].session_id, data) == 0) { // Session exists
                    found = 1;
                    // Check if user is already added to avoid duplicates
                    int user_found = 0;
                    for (int j = 0; j < sessions[i].num_connected_users; j++) {
                        if (strcmp(sessions[i].connected_users[j].client_ID, source) == 0) {
                            user_found = 1;
                            break;
                        }
                    }
                    if (!user_found && sessions[i].num_connected_users < MAX_CONNECTED_USERS) {
                        strcpy(sessions[i].connected_users[sessions[i].num_connected_users].client_ID, source);
                        sessions[i].connected_users[sessions[i].num_connected_users].sockfd = newsockfd;
                        sessions[i].num_connected_users++;
                    }
                    break;
                }
            }

            if (!found && num_sessions < MAX_SESSIONS) { // Create new session
                strcpy(sessions[num_sessions].session_id, data);
                strcpy(sessions[num_sessions].connected_users[0].client_ID, source);
                sessions[num_sessions].connected_users[0].sockfd = newsockfd;
                sessions[num_sessions].num_connected_users = 1;
                num_sessions++;
            }

            send(newsockfd, "ACK", strlen("ACK"), 0); 

        } else if (strcmp(type, "JOIN") == 0) {
            
            removeClientFromAnySession(source);

            int found = 0;

            for (int i = 0; i < num_sessions; i++) {
                if (strcmp(sessions[i].session_id, data) == 0) { // Check if session exists
                    found = 1;
                    // Check if client is already in the session to avoid duplicates
                    int already_in_session = 0;
                    for (int j = 0; j < sessions[i].num_connected_users; j++) {
                        if (strcmp(sessions[i].connected_users[j].client_ID, source) == 0) {
                            already_in_session = 1;
                            break;
                        }
                    }

                    if (!already_in_session) {
                        if (sessions[i].num_connected_users < MAX_CONNECTED_USERS) {
                            // Add client to the session
                            strcpy(sessions[i].connected_users[sessions[i].num_connected_users].client_ID, source);
                            sessions[i].connected_users[sessions[i].num_connected_users].sockfd = newsockfd; // Store sockfd for future communication
                            sessions[i].num_connected_users++;

                            send(newsockfd, "ACK", strlen("ACK"), 0); // Send ACK
                        } else {
                            // Session is full
                            send(newsockfd, "NACK", strlen("NACK"), 0); // Send NACK
                        }
                    } else {
                        // Client already in session
                        send(newsockfd, "ACK", strlen("ACK"), 0); // Optionally, send ACK again or a different message
                    }
                    break; // Exit the loop after handling the session
                }
            }

            if (!found) {
                // Session not found, send NACK
                send(newsockfd, "NACK", strlen("NACK"), 0);
            }

        } else if (strcmp(type, "LIST") == 0) {
            char* sessionList = listSessions();
            if (sessionList) {
                send(newsockfd, sessionList, strlen(sessionList), 0); // Send the session list back to the client
                free(sessionList); // Remember to free the allocated memory
            }
        } else if (strcmp(type, "LEAVE_SESS") == 0) {
            removeClientFromAnySession(source);
            send(newsockfd, "ACK", strlen("ACK"), 0); 
    	} else if (strcmp(type, "EXIT") == 0) { 

            removeClientFromAnySession(source);
            
            // Assuming `sockfd` is the socket file descriptor of the disconnecting client
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (connected_clients[i].sockfd == newsockfd) {
                    connected_clients[i].sockfd = -1; // Mark as available
                    memset(connected_clients[i].name, 0, MAX_NAME); // Clear the name
                    break;
                }
            }

        } else if (strcmp(type, "MESSAGE") == 0) {
        // Locate the session of the sender

            //send(newsockfd, data, strlen(data), 0);

            for (int i = 0; i < num_sessions; i++) {
                for (int j = 0; j < sessions[i].num_connected_users; j++) {
                    if (strcmp(sessions[i].connected_users[j].client_ID, source) == 0) {
                        // Broadcast the message to all clients in the session except the sender
                        for (int k = 0; k < sessions[i].num_connected_users; k++) {
                            if (strcmp(sessions[i].connected_users[k].client_ID, source) != 0) {
                                send(sessions[i].connected_users[k].sockfd, data, strlen(data), 0);
                            }
                        }
                        break; // Break out of the loop once the session is found and message is sent
                    }
                }
            } 

    	} else {
            //
        }
        
    }

    close(newsockfd); // Close the client socket at the end
    printf("server closing fd");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd, portno;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(EXIT_FAILURE);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(EXIT_FAILURE);
    }


    for (int i = 0; i < MAX_CLIENTS; i++) {
        connected_clients[i].sockfd = -1; 
        memset(connected_clients[i].name, 0, MAX_NAME);
    }


    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while (1) {
        int *new_sock = malloc(sizeof(int));
        *new_sock = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (*new_sock < 0) {
            perror("ERROR on accept");
            free(new_sock);
            continue; // Skip to the next iteration for a new connection
        }

        pthread_t sniffer_thread;
        if (pthread_create(&sniffer_thread, NULL, client_handler, (void*) new_sock) < 0) {
            perror("could not create thread");
            free(new_sock);
        }
        pthread_detach(sniffer_thread); 
    }

    close(sockfd);
    return 0;
}
