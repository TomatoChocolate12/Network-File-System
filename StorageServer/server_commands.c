#include "ss.h"
#include "../NamingServer/ns.h"

int PORT = 0;

void handle_client(int client_socket)
{
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_socket, buffer, BUFFER_SIZE);
    buffer[bytes_read] = '\0';
    printf("%s\n", buffer);

    // Parse command and filename
    char command[256], filename[256], content[BUFFER_SIZE], flag[256];

    sscanf(buffer, "%s %s %s %[^\n]", command, filename, content, flag); // fail

    printf("%s\n", command);
    printf("%s\n", filename);

    if (strcmp(command, "READ") == 0)
    {
        send_file_content(client_socket, filename);

    }
    else if (strcmp(command, "WRITE") == 0)
    {
        if (strcmp(flag, "SYNC") == 0 || strlen(content) < 50)
        {
            write_to_file(client_socket, filename, content);
        }
        else
        {
            async_write(client_socket, filename, content, NM_IP);
        }
    }
    else if (strcmp(command, "INFO") == 0)
    {
        send_file_info(client_socket, filename);
    }
    else if (strcmp(command, "STREAM") == 0)
    {
        stream_audio(client_socket, filename);
    }
    else if (strcmp(command, "COPY_FILE") == 0)
    {
        if (strcmp(filename, "DEST") == 0)
        {
            printf("Destination Port, receiving file from server at point: %s\n", content);
            receive_file(client_socket, content);
        }
        else if (strcmp(filename, "SRC") == 0)
        {
            printf("Source Port, sending file from server at point: %s\n", content);
            send_file_content(client_socket, content);
        }
        printf("COPY FILE correctly parsed on Server\n");
    }
    else if (strcmp(command, "COPY_DIR") == 0)
    {
        if (strcmp(filename, "DEST") == 0)
        {
            printf("Destination Port, receiving file from server at point: %s\n", content);
            char current_dir[BUFFER_SIZE];
            if (getcwd(current_dir, sizeof(current_dir)) == NULL)
            {
                perror("Failed to get current directory");
                return;
            }
            receive_file(client_socket, content); // Change directory to the content path
            if (chdir(content) == -1)
            {
                perror("Failed to change directory to content path");
                return;
            }

            char *flag_new = strdup(flag);
            char *flag_n = basename(flag_new);

            printf("%s\n", flag_new);

            // Extract the tar archive
            char command[BUFFER_SIZE];
            snprintf(command, sizeof(command), "tar -xzf %s.tar.gz", flag_n);
            int result = system(command);
            if (result == -1)
            {
                perror("Failed to extract tar archive");
                return;
            }
            char tar_filepath[BUFFER_SIZE];
            snprintf(tar_filepath, sizeof(tar_filepath), "%s.tar.gz", flag_n);
            remove(tar_filepath);
            // Change back to the original directory
            if (chdir(current_dir) == -1)
            {
                perror("Failed to change back to the original directory");
                return;
            }
        }
        else if (strcmp(filename, "SRC") == 0)
        {
            printf("Source Port, sending file from server at point: %s\n", content);
            // Tar the directory
            char tar_command[BUFFER_SIZE];
            snprintf(tar_command, sizeof(tar_command), "tar -czf %s.tar.gz %s", content, content);
            int tar_result = system(tar_command);
            if (tar_result == -1)
            {
                perror("Failed to tar directory");
                return;
            }

            // Send the tarred file
            char tar_filepath[BUFFER_SIZE];
            snprintf(tar_filepath, sizeof(tar_filepath), "%s.tar.gz", content);
            send_file_content(client_socket, tar_filepath);

            // Remove the tarred file after sending
            remove(tar_filepath);
            printf("COPY DIRECTORY correctly parsed on Server\n");
        }
        else
        {
            write(client_socket, "Invalid Command\n", 16);
        }
    }
    else if (strcmp("CREATE", command) == 0)
    {
        int type = atoi(content);
        if (type == 0)
        {
            int fd = open(filename, O_CREAT | O_TRUNC, 0644);
            if (fd < 0)
            {
                perror("File not created");
            }
            else
            {
                printf("File created");
            }
        }
        else if (type == 1)
        {
            if (mkdir(filename, 0755) == 0)
            {
                printf("Directory created");
            }
            else
            {
                perror("Directory not created");
            }
        }
    }
    else if (strcmp(command, "DELETE") == 0)
    {
        int type = atoi(content);
        if (type == 0)
        {
            if (unlink(filename) == 0)
            {
                printf("File deleted");
            }
            else
            {
                perror("File not deleted");
            }
            return;
        }
        if (type == 1)
        {
            char *args[] = {"rm", "-r", (char *)filename, NULL};

            // Fork a new process to execute the command
            pid_t pid = fork();
            if (pid == -1)
            {
                perror("Failed to fork");
                exit(EXIT_FAILURE);
            }
            else if (pid == 0)
            {
                // Child process: execute the rm command
                execvp("rm", args);
                // If execvp fails, print an error and exit
                perror("execvp failed");
                exit(EXIT_FAILURE);
            }
            else
            {
                // Parent process: wait for the child process to complete
                int status;
                if (waitpid(pid, &status, 0) == -1)
                {
                    perror("waitpid failed");
                }
                else if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                {
                    printf("Directory '%s' deleted successfully.\n", filename);
                }
                else
                {
                    printf("Failed to delete directory '%s'.\n", filename);
                }
            }
            return;
        }
        return;
    }
    else if (strcmp(command, "LIST") == 0)
    {
        // list_directory(filename);
    }
}

void send_file_content(int client_socket, const char *filepath)
{

    struct stat st;
    if (stat(filepath, &st) < 0)
    {
        perror("Failed to get file info");
        char err_mess[50];
        get_error_message(1, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        return;
    }

    // Extract the base name of the file from the file path
    char *base_name = basename((char *)filepath);
    if (!base_name)
    {
        fprintf(stderr, "Failed to extract base name from filepath\n");
        return;
    }

    // Prepare and send header
    file_header header;

    strncpy(header.filename, base_name, 255);
    header.filename[255] = '\0'; // Ensure null-termination

    header.filesize = st.st_size;
    header.type = S_ISDIR(st.st_mode) ? 0 : 1;

    if (send(client_socket, &header, sizeof(header), 0) < 0)
    {
        perror("Failed to send header");
        return;
    }

    // Check if the file is binary
    int is_binary = 0;
    FILE *file_check = fopen(filepath, "rb");
    if (file_check != NULL)
    {
        char ch;
        while (fread(&ch, 1, 1, file_check) == 1)
        {
            if (ch == '\0')
            {
                is_binary = 1;
                break;
            }
        }
        fclose(file_check);
    }

    // Send file content with appropriate encoding
    if (is_binary)
    {
        FILE *file = fopen(filepath, "rb");
        if (file == NULL)
        {
            char err_mess[50];
            write(client_socket, "File not found\n", 15);
            get_error_message(1, err_mess, sizeof(err_mess));
            log_message(2, err_mess);
            return;
        }

        char buffer[BUFFER_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
        {
            if (send(client_socket, buffer, bytes_read, 0) < 0)
            {
                perror("Failed to send file data");
                fclose(file);
                return;
            }
        }
        fclose(file);
    }
    else
    {
        FILE *file = fopen(filepath, "r");
        if (file == NULL)
        {
            char err_mess[50];
            write(client_socket, "File not found\n", 15);
            get_error_message(1, err_mess, sizeof(err_mess));
            log_message(2, err_mess);
            return;
        }

        char buffer[BUFFER_SIZE];
        while (fgets(buffer, BUFFER_SIZE, file) != NULL)
        {
            write(client_socket, buffer, strlen(buffer));
        }
        fclose(file);
    }
    FILE *file = fopen(filepath, "r");
    if (file == NULL)
    {
        char err_mess[50];
        write(client_socket, "File not found\n", 15);
        get_error_message(1, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        return;
    }

    char buffer[BUFFER_SIZE];
    while (fgets(buffer, BUFFER_SIZE, file) != NULL)
    {
        write(client_socket, buffer, strlen(buffer));
    }
    fclose(file);
}

void write_to_file(int client_socket, const char *filename, const char *content)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
    {
        char err_mess[50];
        get_error_message(16, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        write(client_socket, "Cannot open file\n", 17);
        return;
    }

    printf("%s\n", content);
    fprintf(file, "%s", content);
    fclose(file);
    write(client_socket, "Write successful\n", 17);
}

void send_file_info(int client_socket, const char *filename)
{
    struct stat file_stat;
    if (stat(filename, &file_stat) < 0)
    {
        char err_mess[50];
        write(client_socket, "File not found\n", 15);
        get_error_message(1, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        return;
    }
    char response[BUFFER_SIZE];
    snprintf(response, BUFFER_SIZE, "Size: %ld bytes\nPermissions: %o\n", file_stat.st_size, file_stat.st_mode & 0777);
    write(client_socket, response, strlen(response));
}

// Function to receive a file over network
int receive_file(int sock, const char *dest_path)
{
    file_header header;
    if (recv(sock, &header, sizeof(header), 0) < 0)
    {
        perror("Failed to receive header");
        return -1;
    }

    printf("Received header successfully\n");

    char full_path[PATH_MAX];

    snprintf(full_path, PATH_MAX, "%s/%s", dest_path, header.filename);

    if (header.type == 0)
    { // Directory
        if (mkdir(full_path, 0755) < 0 && errno != EEXIST)
        {
            char err_mess[50];
            get_error_message(11, err_mess, sizeof(err_mess));
            log_message(2, err_mess);
            perror("Failed to create directory");
            return -1;
        }
    }
    else
    { // Regular file
        int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
        {
            char err_mess[50];
            get_error_message(11, err_mess, sizeof(err_mess));
            log_message(2, err_mess);
            perror("Failed to create file");
            return -1;
        }

        size_t remaining = header.filesize;
        char buffer[BUFFER_SIZE];
        while (remaining > 0)
        {
            ssize_t bytes_received = recv(sock, buffer,
                                          MIN(BUFFER_SIZE, remaining), 0);
            if (bytes_received <= 0)
            {
                perror("Failed to receive file data");
                close(fd);
                return -1;
            }

            if (write(fd, buffer, bytes_received) < 0)
            {
                perror("Failed to write file data");
                close(fd);
                return -1;
            }
            remaining -= bytes_received;
        }
        close(fd);
    }
    return 0;
}

// // Receives directory on the Destination Storage Server.
// int receive_dir_ss(client_socket)
// {
//     char buffer[MAX_PATH_LENGTH];

//     int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
//     if (bytes_received <= 0)
//     {
//         close(client_socket);
//         return NULL;
//     }
//     buffer[bytes_received] = '\0';
//     char directory_path[BUFFER_SIZE];
//     strncpy(directory_path, buffer, BUFFER_SIZE);

//     // Step 3: Receive the file structure from the Naming Server
//     while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0)
//     {
//         buffer[bytes_received] = '\0';

//         // Check if it's the end of the file list
//         if (strcmp(buffer, "END") == 0)
//         {
//             break;
//         }
//     }
// }

void stream_audio(int client_socket, const char *filename)
{
    printf("%s\n", filename);
    int file = open(filename, O_RDONLY);
    if (file < 0)
    {
        write(client_socket, "Audio file not found\n", 21);
        return;
    }
    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = read(file, buffer, BUFFER_SIZE)) > 0)
    {
        write(client_socket, buffer, bytes);
    }
    close(file);
}

void register_with_naming_server(int PORT, const char *storage_path)
{
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    // Create a socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(NM_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(NM_IP);

    // Connect to the naming server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        char err_mess[50];
        get_error_message(10, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Connection to naming server failed");
        close(sock);
        return;
    }

    // Step 1: Send initial command to register as a storage server
    char *initial_command = "STORAGE";
    send(sock, initial_command, strlen(initial_command), 0);
    printf("Command sent to Naming Server: %s\n", initial_command);

    // Step 2: Receive acknowledgment from Naming Server
    int ack_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (ack_received == -1)
    {
        char err_mess[50];
        get_error_message(9, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Failed to receive acknowledgment");
        close(sock);
        exit(EXIT_FAILURE);
    }
    buffer[ack_received] = '\0';

    if (strcmp(buffer, "ACK") != 0)
    {
        char err_mess[50];
        get_error_message(9, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        fprintf(stderr, "Did not receive ACK from Naming Server. Received: %s\n", buffer);
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Acknowledgment received from Naming Server: %s\n", buffer);

    // Step 3: Get the Storage Server's IP address
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;

    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        close(sock);
        return;
    }

    // Loop through linked list of interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        // Check for IPv4 address
        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                perror("getnameinfo");
                close(sock);
                freeifaddrs(ifaddr);
                return;
            }

            // Skip loopback address
            if (strcmp(host, "127.0.0.1") != 0) {
                IPbuffer = strdup(host);
                break;
            }
        }
    }

    freeifaddrs(ifaddr);

    // Step 4: Send IP address and PORT to Naming Server
    snprintf(buffer, sizeof(buffer), "%s:%d", IPbuffer, PORT);
    send(sock, buffer, strlen(buffer), 0);
    printf("Sent IP and port to Naming Server: %s\n", buffer);

    // Step 5: Send the storage path to Naming Server
    usleep(10000);
    send(sock, storage_path, strlen(storage_path), 0);
    printf("Sent storage path to Naming Server: %s\n", storage_path);

    usleep(10000);
    // Step 6: Send the contents of the storage directory to Naming Server

    // Close the connection
    char storage_path_copy[BUFFER_SIZE];
    strncpy(storage_path_copy, storage_path, sizeof(storage_path_copy) - 1);
    storage_path_copy[sizeof(storage_path_copy) - 1] = '\0'; // Ensure null termination

    // Get the parent directory
    char *parent_dir = dirname(storage_path_copy);
    send_directory_contents(sock, storage_path, parent_dir, 0);
    close(sock);
    if (chdir(parent_dir) == -1)
    {
        perror("Failed to change directory to parent of storage path");
        return;
    }

    // printf("Changed directory to parent: %s\n", parent_dir);
}

int main(int argc, char *argv[])
{
    if (argc != 3) // Expecting two arguments: PORT and storage path
    {
        char err_mess[50];
        get_error_message(4, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        fprintf(stderr, "Usage: %s <PORT> <STORAGE_PATH>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    PORT = atoi(argv[1]);
    const char *storage_path = argv[2];

    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    ss_duplicate(storage_path);
    // Register with Naming Server with both PORT and storage path
    register_with_naming_server(PORT, storage_path);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        char err_mess[50];
        get_error_message(9, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        char err_mess[50];
        get_error_message(9, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        char err_mess[50];
        get_error_message(9, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Storage Server listening on port %d\n", PORT);

    // Server loop to accept and handle incoming connections
    while (1)
    {
        // Accept a new client connection
        char temp_buffer[BUFFER_SIZE];
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        if ((client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0)
        {
            char err_mess[50];
            get_error_message(9, err_mess, sizeof(err_mess));
            log_message(2, err_mess);
            perror("Accept failed");
            continue;
        }

        int bytes_received = recv(client_socket, temp_buffer, sizeof(temp_buffer) - 1, MSG_PEEK);
        if (bytes_received > 0)           // Handle client requests
            handle_client(client_socket); // fail

        // Close the client socket after handling
        close(client_socket);
    }

    // Close the server socket before exiting
    close(server_fd);
    return 0;
}

void send_directory_contents(int sock, const char *path, char *parent_dir, int send_content)
{
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    if ((dir = opendir(path)) == NULL)
    {

        perror("Unable to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        // Skip "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Construct the full path of the file/directory
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        char *new_path = full_path + strlen(parent_dir) + 1;

        // Get file/directory information
        if (stat(full_path, &file_stat) == -1)
        {
            perror("stat");
            continue;
        }

        if (S_ISDIR(file_stat.st_mode))
        {
            // It's a directory, send directory name and recurse
            snprintf(buffer, sizeof(buffer), "%s", new_path);
            send(sock, buffer, strlen(buffer), 0);
            usleep(100000);
            send_directory_contents(sock, full_path, parent_dir, send_content); // Recursive call for subdirectories
        }
        else if (S_ISREG(file_stat.st_mode))
        {

            if (!send_content)
            {
                // It's a regular file, send file name
                printf("Sending\n");
                snprintf(buffer, sizeof(buffer), "%s", new_path);
                send(sock, buffer, strlen(buffer), 0);
                usleep(100000);
            }
            else
            {
                printf("Sending informationn about file at %s\n", full_path);
                send_file_content(sock, full_path);
            }
        }
    }

    closedir(dir);
}

// Function to create a directory if it doesn't exist
int create_directory(const char *path)
{
    struct stat st = {0};
    if (stat(path, &st) == -1)
    {
        if (mkdir(path, 0755) == -1)
        {
            char err_mess[50];
            get_error_message(11, err_mess, sizeof(err_mess));
            log_message(2, err_mess);
            perror("mkdir failed");
            return -1;
            // exit(EXIT_FAILURE);
        }
    }
    return 0;
}

// Function to copy a file from source to destination
void copy_file(const char *source, const char *destination)
{
    FILE *src_file = fopen(source, "rb");
    if (!src_file)
    {
        char err_mess[50];
        get_error_message(1, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Error opening source file");
        return;
    }

    FILE *dest_file = fopen(destination, "wb");
    if (!dest_file)
    {
        char err_mess[50];
        get_error_message(1, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Error opening destination file");
        fclose(src_file);
        return;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0)
    {
        fwrite(buffer, 1, bytes, dest_file);
    }

    fclose(src_file);
    fclose(dest_file);
}

// Recursive function to duplicate a directory
// Recursive function to duplicate a directory
void duplicate_directory(const char *src_dir, const char *dest_dir)
{
    DIR *dir = opendir(src_dir);
    if (!dir)
    {
        char err_mess[50];
        get_error_message(1, err_mess, sizeof(err_mess));
        log_message(2, err_mess);
        perror("Error opening source directory");
        return;
    }

    struct dirent *entry;
    struct stat entry_stat;

    while ((entry = readdir(dir)) != NULL)
    {
        // Skip special directories "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full source and destination paths
        char src_path[1024];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);

        char dest_path[1024];
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, entry->d_name);

        // Skip the backup directory to prevent infinite recursion
        if (strcmp(entry->d_name, "backup") == 0)
            continue;

        // Get entry details
        if (stat(src_path, &entry_stat) == -1)
        {
            char err_mess[50];
            get_error_message(5, err_mess, sizeof(err_mess));
            log_message(2, err_mess);
            perror("Error getting file status");
            continue;
        }

        // If entry is a directory, recursively copy it
        if (S_ISDIR(entry_stat.st_mode))
        {
            create_directory(dest_path);
            duplicate_directory(src_path, dest_path);
        }
        else if (S_ISREG(entry_stat.st_mode)) // If entry is a regular file, copy it
        {
            copy_file(src_path, dest_path);
        }
    }

    closedir(dir);
}

void ss_duplicate(const char *storage_path)
{
    char current_dir[PATH_MAX];
    snprintf(current_dir,PATH_MAX,"%s",storage_path);
    
    char backup_dir[PATH_MAX];
    snprintf(backup_dir, PATH_MAX, "%s/backup", storage_path);
    
    // Ensure the backup directory exists or try an alternative name
    if (create_directory(backup_dir) != 0) {
        // If the creation of 'backup' fails, try creating 'backup_new'
        snprintf(backup_dir, PATH_MAX, "%s/backup_new", storage_path);
        if (create_directory(backup_dir) != 0) {
            // If the creation of 'backup_new' also fails, report an error and exit
            fprintf(stderr, "Error: Unable to create backup directory at %s or %s.\n", 
                    storage_path, backup_dir);
            return;
        }
    }

    // Start duplicating the directory
    duplicate_directory(current_dir, backup_dir);

    printf("All files and folders have been duplicated into the 'backup' directory.\n");
    
}