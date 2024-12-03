#include "ns.h"

int connect_to_storage(const char *ip_address, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        char err_mess[50];
        get_error_message(9, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_address, &server_addr.sin_addr) <= 0)
    {
        char err_mess[50];
        get_error_message(9, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Invalid address");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        char err_mess[50];
        get_error_message(9, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Connection failed");
        close(sock);
        return -1;
    }

    return sock;
}

void send_create(const char *ip_address, int port, const char *path, int type)
{
    // printf("called");
    // Connect to the storage server
    // printf("1\n");
    int sock = connect_to_storage(ip_address, port);
    if (sock < 0)
    {
        char err_mess[50];
        get_error_message(10, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        fprintf(stderr, "Failed to connect to storage server\n");
        return;
    }

    // Prepare the command string
    char tosend[PATH_MAX];
    snprintf(tosend, sizeof(tosend), "CREATE %s %d", path, type);

    // Send command to storage server
    if (send(sock, tosend, strlen(tosend), 0) < 0)  // Use strlen instead of sizeof
    {
        char err_mess[50];
        get_error_message(10, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Failed to send create command");
        close(sock);
        return;
    }

    // Clean up
    close(sock);
}

// Modified delete function to work over network
void send_delete(const char *ip_address, int port, const char *path, int type)
{
    int sock = connect_to_storage(ip_address, port);
    if (sock < 0)
    {
        char err_mess[50];
        get_error_message(10, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        fprintf(stderr, "Failed to connect to storage server\n");
        return;
    }

    // Send command to storage server
    char tosend[PATH_MAX];
    snprintf(tosend, sizeof(tosend), "DELETE %s %d", path, type);

    // Send command to storage server
    if (send(sock, tosend, strlen(tosend), 0) < 0)  // Use strlen instead of sizeof
    {
        char err_mess[50];
        get_error_message(10, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Failed to send delete command");
        close(sock);
        return;
    }

    // Clean up
    close(sock);
}

// Function to send a file over network
int send_file(const char *filepath, int sock)
{
    struct stat st;
    if (stat(filepath, &st) < 0)
    {
        char err_mess[50];
        get_error_message(1, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Failed to get file info");
        return -1;
    }

    // Extract the base name of the file from the file path
    char *base_name = basename((char *)filepath);
    if (!base_name)
    {
        char err_mess[50];
        get_error_message(1, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        fprintf(stderr, "Failed to extract base name from filepath\n");
        return -1;
    }

    // Prepare and send header
    file_header header;

    strncpy(header.filename, base_name, 255);
    header.filename[255] = '\0'; // Ensure null-termination

    header.filesize = st.st_size;
    header.type = S_ISDIR(st.st_mode) ? 0 : 1;

    if (send(sock, &header, sizeof(header), 0) < 0)
    {
        char err_mess[50];
        get_error_message(9, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Failed to send header");
        return -1;
    }

    // If it's a regular file, send its contents
    if (header.type == 1)
    {
        int fd = open(filepath, O_RDONLY);
        if (fd < 0)
        {
            char err_mess[50];
            get_error_message(14, err_mess, sizeof(err_mess));
            log_message(2, err_mess);
            perror("Failed to open file");
            return -1;
        }

        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0)
        {
            if (send(sock, buffer, bytes_read, 0) < 0)
            {
                char err_mess[50];
            get_error_message(9, err_mess, sizeof(err_mess));
            log_message(2, err_mess);
                perror("Failed to send file data");
                close(fd);
                return -1;
            }
        }
        close(fd);
    }
    return 0;
}

// Function to route a file over network and forward it
int route_file_ns(int sock, int dest_sock, const char *dest_path)
{
    file_header header;
    if (recv(sock, &header, sizeof(header), 0) < 0)
    {
        perror("Failed to receive header");
        return -1;
    }

    printf("Header Received\n");
    // Send header to destination server
    if (send(dest_sock, &header, sizeof(header), 0) < 0)
    {
        perror("Failed to send header to destination server");
        return -1;
    }

    printf("Header Sent\n");

    size_t remaining = header.filesize;
    char buffer[BUFFER_SIZE];
    while (remaining > 0)
    {
        ssize_t bytes_received = recv(sock, buffer,
                                      MIN(BUFFER_SIZE, remaining), 0);
        if (bytes_received <= 0)
        {
            perror("Failed to receive file data");
            char err_mess[50];
            get_error_message(9, err_mess, sizeof(err_mess));
            log_message(2, err_mess);
            return -1;
        }

        if (send(dest_sock, buffer, bytes_received, 0) < 0)
        {
            perror("Failed to send file data to destination server");
            char err_mess[50];
            get_error_message(9, err_mess, sizeof(err_mess));
            log_message(2, err_mess);
            return -1;
        }
        remaining -= bytes_received;
    }
    printf("File Info Sent\n");
    return 0;
}


// Modified copy_file function to use network transfer
void copy_file_network(const char *src, const char *dest, const char *src_server_ip, const char *dest_server_ip, int src_port, int dest_port)
{
    printf("Source Server IP: %s, Port: %d\n", src_server_ip, src_port);
    printf("Destination Server IP: %s, Port: %d\n", dest_server_ip, dest_port);

    // Connect to source server
    int src_sock = connect_to_storage(src_server_ip, src_port);

    if (src_sock < 0)
    {
        fprintf(stderr, "Failed to connect to source server\n");
        char err_mess[50];
        get_error_message(10, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        return;
    }

    // Connect to destination server
    int dest_sock = connect_to_storage(dest_server_ip, dest_port);

    if (dest_sock < 0)
    {
        fprintf(stderr, "Failed to connect to destination server\n");
        char err_mess[50];
        get_error_message(10, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        close(src_sock);
        return;
    }

    // Send COPY_FILE command to destination server
    // IT wil now wait for a file to be made here

    char copy_dest[PATH_MAX];
    snprintf(copy_dest, sizeof(copy_dest), "COPY_FILE DEST %s", dest);

    if (send(dest_sock, copy_dest, strlen(copy_dest), 0) < 0)
    {
        perror("Failed to send COPY_FILE command");
        close(src_sock);
        close(dest_sock);
        return;
    }

    // Send COPY_FILE Command to Source Server, Receive File data back.

    char copy_src[PATH_MAX];
    snprintf(copy_src, sizeof(copy_src), "COPY_FILE SRC %s", src);

    if (send(src_sock, copy_src, strlen(copy_src), 0) < 0)
    {
        perror("Failed to send COPY_FILE command");
        close(src_sock);
        close(dest_sock);
        return;
    }

    route_file_ns(src_sock, dest_sock, dest);

    //uncomment to insert into trie of dest_server
    insert_path(file_trie, dest, dest_server_ip, dest_port);

    close(src_sock);
    close(dest_sock);
}

// Modified copy_directory function to use network transfer
void copy_directory_network(const char *src, const char *dest, const char *src_server_ip, const char *dest_server_ip, int src_port, int dest_port,int flag)
{

    printf("Source Server IP: %s, Port: %d\n", src_server_ip, src_port);
    printf("Destination Server IP: %s, Port: %d\n", dest_server_ip, dest_port);

    // Step 1: Connect to source and destination server
    // Connect to source server
    int src_sock = connect_to_storage(src_server_ip, src_port);
    if (src_sock < 0)
    {
        fprintf(stderr, "Failed to connect to source server\n");
        return;
    }

    // Connect to destination server
    printf("%s %d",dest_server_ip,dest_port);
    int dest_sock = connect_to_storage(dest_server_ip, dest_port);
    if (dest_sock < 0)
    {
        fprintf(stderr, "Failed to connect to destination server\n");
        close(src_sock);
        return;
    }

    // Send COPY_DIR command to destination server
    // It should make a directory and await information.

    char copy_dest[PATH_MAX];
    snprintf(copy_dest, sizeof(copy_dest), "COPY_DIR DEST %s %s", dest, src);

    if (send(dest_sock, copy_dest, strlen(copy_dest), 0) < 0)
    {
        perror("Failed to send COPY_FILE command");
        close(src_sock);
        close(dest_sock);
        return;
    }

    // Send COPY_DIR Command to Source Server, Receive File data back.
    // Should be send recursively until some acknowledgment.

    char copy_src[PATH_MAX];
    snprintf(copy_src, sizeof(copy_src), "COPY_DIR SRC %s", src);

    if (send(src_sock, copy_src, strlen(copy_src), 0) < 0)
    {
        perror("Failed to send COPY_FILE command");
        close(src_sock);
        close(dest_sock);
        return;
    }

    // ROUTE DIRECTORY information over network until acknowledgement of a stop bit.
    // route_dir_ns(src_sock, dest_sock, dest);
    route_file_ns(src_sock, dest_sock, dest);
    reroute_prefix(file_trie,src,dest,src_server_ip,dest_server_ip);
    if(flag==1){
    close(src_sock);

    close(dest_sock);
    }
}

// void list_directory(const char *path) 
// {
//     DIR *dir = opendir(path); // Open the directory
//     if (dir == NULL) {
//         perror("Failed to open directory");
//         return;
//     }

//     struct dirent *entry;
//     printf("Contents of directory '%s':\n", path);

//     while ((entry = readdir(dir)) != NULL) { // Read entries
//         if (entry->d_type == DT_DIR) {
//             printf("[DIR]  %s\n", entry->d_name); // Directory
//         } else if (entry->d_type == DT_REG) {
//             printf("[FILE] %s\n", entry->d_name); // Regular file
//         } else {
//             printf("[OTHER] %s\n", entry->d_name); // Other types (e.g., symlinks)
//         }
//     }

//     closedir(dir); // Close the directory
// }
