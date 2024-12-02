#include "ss.h"

// int main()
// {
//     int ss_socket;
//     struct sockaddr_in nm_address;
//     char buffer[BUFFER_SIZE];

//     // Create socket for Storage Server
//     ss_socket = socket(AF_INET, SOCK_STREAM, 0);
//     if (ss_socket == -1)
//     {
//         perror("Failed to create socket");
//         exit(EXIT_FAILURE);
//     }

//     // Configure Naming Server address
//     nm_address.sin_family = AF_INET;
//     nm_address.sin_addr.s_addr = inet_addr(NM_IP);
//     nm_address.sin_port = htons(NM_PORT);

//     // Connect to the Naming Server
//     if (connect(ss_socket, (struct sockaddr *)&nm_address, sizeof(nm_address)) == -1)
//     {
//         perror("Connection to Naming Server failed");
//         close(ss_socket);
//         exit(EXIT_FAILURE);
//     }
//     printf("Connected to Naming Server at %s:%d\n", NM_IP, NM_PORT);

//     // Prepare metadata for the Naming Server (IP, port, accessible paths)
//     char *ss_metadata = "IP:NM_IP, NM_Port:5000, Client_Port:5001, Paths:/path1,/path2";
//     send(ss_socket, ss_metadata, strlen(ss_metadata), 0);

//     printf("Metadata sent to Naming Server: %s\n", ss_metadata);

//     int close_connection = 0;
//     while(!close_connection){
//         int bytes_received = recv(ss_socket, buffer, BUFFER_SIZE - 1, 0);
//         if (bytes_received == -1)
//         {
//             perror("Failed to receive data");
//             close(ss_socket);
//             exit(EXIT_FAILURE);
//         }
//         buffer[bytes_received] = '\0';

//         if (strcmp(buffer, "exit") == 0)
//         {
//             printf("Exit command received. Shutting down.\n");
//             close_connection = 1;
//         }
//         else
//         {
//             printf("Received command: %s\n", buffer);
//             // Handle other commands here
//         }
//     }

//     close(ss_socket);
//     return 0;
// }