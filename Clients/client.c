#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 7070
#define BUFFER_SIZE 1024
#define NAMING_SERVER_PORT 5000

#include "../Utils/headers.h"
#include "../Utils/typedefs.h"
#include "../Utils/constants.h"
#include "../NamingServer/ns.h"

void send_request(const char *request);

int main()
{
    char command[BUFFER_SIZE];

    while (1)
    {
        fflush(stdout);
        printf("Enter command: ");
        fgets(command, BUFFER_SIZE, stdin);
        command[strcspn(command, "\n")] = '\0'; // Remove newline character

        printf("%s\n", command);
        if (strcmp(command, "exit") == 0)
        {
            send_request(command);
            break;
        }

        send_request(command);
    }
    return 0;
}

void send_request(const char *request)
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char server_ip[INET_ADDRSTRLEN];
    int server_port;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket creation error\n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(NAMING_SERVER_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(NM_IP);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection to naming server failed");
        return;
    }

    // Get storage server details
    // changes to be made here regarding on what basis the storage server details need to be recieved and sent
    // changes to also be made regarding sending the request commands
    printf("%s\n", request);

    send(sock, request, strlen(request), 0);

    // Create a temporary copy of request
    char temp_request[BUFFER_SIZE];
    strncpy(temp_request, request, BUFFER_SIZE);
    temp_request[BUFFER_SIZE - 1] = '\0'; // Ensure null-termination

    // Tokenize the temporary copy to get the command
    char *token = strtok(temp_request, " ");
    char *command = token;

    if (strcmp(command, "CREATE") == 0 || strcmp(command, "COPY") == 0 || strcmp(command, "DELETE") == 0)
    {
        // printf("HI");
        close(sock);
        return;
    }
    else if(strcmp(command, "LIST") == 0){
        printf("Received file paths:\n");
        while (1)
        {
            int bytes_read = recv(sock, buffer, BUFFER_SIZE, 0);
            buffer[bytes_read] = '\0';
            if (strcmp(buffer, "END") == 0)
            {
            break;
            }
            printf("%s\n", buffer);
        }
    }
    else if (strcmp(command, "READ") == 0)
    {
        // Handle READ command
        int bytes_read = recv(sock, buffer, BUFFER_SIZE, 0);
        buffer[bytes_read] = '\0';
        printf("Connected to Storage Server: %s\n", buffer);

        sscanf(buffer, "%[^:]:%d", server_ip, &server_port);

        // Close connection to naming server
        close(sock);

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("Socket creation error\n");
            return;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(server_port);
        serv_addr.sin_addr.s_addr = inet_addr(server_ip);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            printf("Connection Failed\n");
            return;
        }

        // printf("%s\n", request);
        send(sock, request, strlen(request), 0);

        file_header header;
        if (recv(sock, &header, sizeof(header), 0) < 0)
        {
            perror("Failed to receive header");
            return -1;
        }

        // Check if header printed correctly.
        // printf("File Name: %s\n", header.filename);
        // printf("File Size: %zu bytes\n", header.filesize);

        size_t remaining = header.filesize;
        char buffer[BUFFER_SIZE];
        while (remaining > 0)
        {
            ssize_t bytes_received = recv(sock, buffer,
                                          MIN(BUFFER_SIZE, remaining), 0);
            if (bytes_received <= 0)
            {
                perror("Failed to receive file data");
                return -1;
            }

            printf("%s", buffer);
            remaining -= bytes_received;
        }
    }
    else
    {
        recv(sock, buffer, BUFFER_SIZE, 0);
        printf("Connected to Storage Server: %s\n", buffer);

        sscanf(buffer, "%[^:]:%d", server_ip, &server_port);

        // Close connection to naming server
        close(sock);

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("Socket creation error\n");
            return;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(server_port);
        serv_addr.sin_addr.s_addr = inet_addr(server_ip);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            printf("Connection Failed\n");
            return;
        }

        printf("%s\n", request);
        send(sock, request, strlen(request), 0);
        if (strncmp(command, "STREAM", 6) == 0)
        {
            FILE *mpv = popen("mpv --no-video --really-quiet -", "w");
            if (mpv == NULL)
            {
                perror("Failed to open mpv");
                close(sock);
                return;
            }
            int bytes_read;
            while ((bytes_read = read(sock, buffer, BUFFER_SIZE)) > 0)
            {
                fwrite(buffer, 1, bytes_read, mpv); // Write data to mpv
                fflush(mpv);                        // Ensure data is sent to mpv immediately
            }

            // Cleanup
            pclose(mpv);
        }
        else
        {
            int bytes_read = read(sock, buffer, BUFFER_SIZE);
            buffer[bytes_read] = '\0';
            printf("%s\n", buffer);
        }

        close(sock);
    }
    return;
}
