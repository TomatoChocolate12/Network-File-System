#include "ss.h"

void copy_async_file(const char *source, const char *destination)
{
    FILE *src, *dest;
    char buffer[1024]; // Buffer for copying
    size_t bytes_read;

    // Open source file for reading
    src = fopen(source, "rb");
    if (src == NULL)
    {
        perror("Failed to open source file");
        exit(EXIT_FAILURE);
    }

    // Open destination file for writing
    dest = fopen(destination, "wb");
    if (dest == NULL)
    {
        perror("Failed to open destination file");
        fclose(src);
        exit(EXIT_FAILURE);
    }

    // Copy content in chunks
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        fwrite(buffer, 1, bytes_read, dest);
    }

    // Close files
    fclose(src);
    fclose(dest);
    // printf("File copied successfully.\n");
}

void get_parent_directory(const char *filename, char *parent_directory)
{
    // Copy the original filename to a modifiable string
    strcpy(parent_directory, filename);

    // Find the last occurrence of '/'
    char *last_slash = strrchr(parent_directory, '/');

    if (last_slash != NULL)
    {
        // Terminate the string at the last '/'
        *last_slash = '\0';
    }
    else
    {
        // If no '/' is found, the file is in the current directory
        strcpy(parent_directory, ".");
    }
}

void *async_write(int client_socket, const char *filename, char *content, const char *server_ip)
{
    char health[50];
    int pid = fork();
    if (pid < 0)
    {
        perror("Fork failed");
        return NULL;
    }
    else if (pid == 0)
    {
        char parent[300];
        get_parent_directory(filename, parent);
        char temp[300];
        sprintf(temp, "%s/temp", parent);
        copy_async_file(filename, temp);
        // Child process: perform asynchronous writing
        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
        {
            perror("File open failed");
            exit(EXIT_FAILURE);
        }

        size_t content_len = strlen(content);
        size_t written = 0;
        // printf("%s async command here", content);
        // printf("file desc: %d\n", fd);
        while (written < content_len)
        {
            // printf("1");
            size_t to_write = (content_len - written > BUFFER_SIZE) ? BUFFER_SIZE : (content_len - written);
            ssize_t result = write(fd, content + written, to_write);

            if (result < 0)
            {
                perror("Write failed");
                close(fd);
                exit(EXIT_FAILURE);
            }

            written += result;
        }

        close(fd);

        // Notify the naming server about the completion
        int ns_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (ns_socket < 0)
        {
            perror("Naming server socket creation failed");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in ns_addr;
        ns_addr.sin_family = AF_INET;
        ns_addr.sin_port = htons(NM_PORT);
        ns_addr.sin_addr.s_addr = inet_addr(NM_IP); // Replace with actual naming server port
        // inet_pton(AF_INET, server_ip, &ns_addr.sin_addr);

        if (connect(ns_socket, (struct sockaddr *)&ns_addr, sizeof(ns_addr)) < 0)
        {
            // printf("1");
            perror("Naming server connection failed");
            close(ns_socket);
            exit(EXIT_FAILURE);
        }

        char hostbuffer[256];
        char *IPbuffer;
        struct hostent *host_entry;

        // Retrieve the hostname
        if (gethostname(hostbuffer, sizeof(hostbuffer)) == -1)
        {
            perror("gethostname");
            close(ns_socket);
            return;
        }

        // Retrieve host information
        host_entry = gethostbyname(hostbuffer);
        if (host_entry == NULL)
        {
            perror("gethostbyname");
            close(ns_socket);
            return;
        }
        char buffer[BUFFER_SIZE];

        // Convert an Internet network address into ASCII string
        IPbuffer = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0]));

        // Step 4: Send IP address and PORT to Naming Server
        snprintf(buffer, sizeof(buffer), "Write completed at %s:%d, %d", IPbuffer, PORT, client_socket);
        send(ns_socket, buffer, strlen(buffer), 0);
        printf("Sent IP and port to Naming Server: %s\n", buffer);
        recv(ns_socket, health, sizeof(health), NULL);
        printf("Health: %s\n", health);
        if (strcmp(health, "Unhealthy") == 0)
        {
            copy_async_file(temp, filename);
            unlink(temp);
            write(ns_socket, "ASYNC: Could not write as Storage server collapsed", 43);
            // write(client_socket, "ASYNC: Could not write as Storage server collapsed", 43);
        }
        else if (strcmp(health, "Healthy") == 0)
        {
            printf("1...\n");
            sleep(2);
            printf("2...\n");
            unlink(temp);
            write(ns_socket, "ASYNC: Written", 15);
            printf("Written\n");
        }
        close(ns_socket); // Close the naming server socket
        // close(client_socket);
    }
    else
    {
        // Parent process: send immediate acknowledgment to the client
        write(client_socket, "Request accepted\n", 17);
        close(client_socket); // Close the connection with the client

        // waitpid(pid, NULL, WNOHANG|WUNTRACED);
    }
    printf("Returning");
    return NULL;
}
