#include "ns.h"
typedef struct FileNode
{
    char path[BUFFER_SIZE];
    struct FileNode *next;
} FileNode;

typedef struct serdest
{
    char dir_path[BUFFER_SIZE];
} serdest;

serdest src_dest[MAX_STORAGE_SERVERS];

FileNode *create_file_node(const char *path)
{
    FileNode *new_node = (FileNode *)malloc(sizeof(FileNode));
    if (new_node)
    {
        strncpy(new_node->path, path, BUFFER_SIZE);
        new_node->next = NULL;
    }
    return new_node;
}

TrieNode *file_trie;
LRUCache *cache;
int async_writing[] = {0};

// Function to insert a file path into the file list
void insert_file(FileNode **head, const char *path)
{
    FileNode *new_node = create_file_node(path);
    if (new_node)
    {
        new_node->next = *head;
        *head = new_node;
    }
}

void remove_prefix(char *str, const char *prefix) {
    char *pos = strstr(str, prefix);
    if (pos == str) { // Check if the prefix is at the beginning
        memmove(pos, pos + strlen(prefix), strlen(pos + strlen(prefix)) + 1);
    }
}

void initialize_backup_directory_network(int server_id, int n, const char *src_dir, const char *dest_dir1, const char *dest_dir2)
{
    int backup1 = (server_id + 1) % n;
    int backup2 = (server_id + 2) % n;

    char dest_dir_backup1[PATH_MAX], dest_dir_backup2[PATH_MAX];
    snprintf(dest_dir_backup1, PATH_MAX, "SS%d/backup_s%d_s%d", backup1 + 1, backup1, server_id);
    snprintf(dest_dir_backup2, PATH_MAX, "SS%d/backup_s%d_s%d", backup2 + 1, backup2, server_id);
    char src_dir_final[PATH_MAX];
    snprintf(src_dir_final, PATH_MAX, "SS%d/backup", server_id + 1);
    printf("%s\n", src_dir_final);

    printf("Initializing network backup directories for Server %d\n", server_id + 1);

    // Use network-based copy for backup initialization
    send_create(storage_servers[backup1].ip, storage_servers[backup1].port, dest_dir_backup1, 1);
    copy_directory_network(
        src_dir_final,
        dest_dir_backup1,
        storage_servers[server_id].ip,
        storage_servers[backup1].ip,
        storage_servers[server_id].port,
        storage_servers[backup1].port,
        0);
    sleep(10);
    insert_path(file_trie, dest_dir_backup1, storage_servers[backup1].ip, storage_servers[backup1].port);
    printf("%s\n", src_dir_final);
    send_create(storage_servers[backup2].ip, storage_servers[backup2].port, dest_dir_backup2, 1);
    copy_directory_network(
        src_dir_final,
        dest_dir_backup2,
        storage_servers[server_id].ip,
        storage_servers[backup2].ip,
        storage_servers[server_id].port,
        storage_servers[backup2].port,
        0);
    insert_path(file_trie, dest_dir_backup2, storage_servers[backup2].ip, storage_servers[backup2].port);
}

StorageServer storage_servers[MAX_STORAGE_SERVERS];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t health_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_count = 0;
ServerHealth server_health[MAX_STORAGE_SERVERS];

// Initialize health monitoring for a server
void init_server_health(int server_index, const char *ip, int port)
{
    pthread_mutex_lock(&health_mutex);
    strncpy(server_health[server_index].ip, ip, INET_ADDRSTRLEN);
    server_health[server_index].port = port;
    server_health[server_index].is_active = true;
    server_health[server_index].last_seen = time(NULL);
    server_health[server_index].failed_checks = 0;
    pthread_mutex_init(&server_health[server_index].health_mutex, NULL);
    pthread_mutex_unlock(&health_mutex);
}

// Check if a server is responsive
bool check_server_health(const char *ip, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = 2; // 2 second timeout
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof tv);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        close(sock);
        return false;
    }

    close(sock);
    return true;
}

// Monitor thread function
void *health_monitor(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&health_mutex);
        for (int i = 0; i < server_count; i++)
        {
            pthread_mutex_lock(&server_health[i].health_mutex);

            bool current_status = check_server_health(server_health[i].ip, server_health[i].port);

            if (!current_status)
            {
                server_health[i].failed_checks++;

                if (server_health[i].failed_checks >= MAX_RETRIES && server_health[i].is_active)
                {
                    // Server just went down
                    server_health[i].is_active = false;
                    log_message(LOG_WARNING, "Storage server %s:%d is DOWN after %d failed health checks",
                                server_health[i].ip, server_health[i].port, MAX_RETRIES);

                    if (async_writing[i] == 1)
                    {
                        async_writing[i] = 2;
                    }
                }
            }
            else
            {
                if (!server_health[i].is_active)
                {
                    // Server just came back up
                    log_message(LOG_INFO, "Storage server %s:%d is back ONLINE",
                                server_health[i].ip, server_health[i].port);
                }
                server_health[i].is_active = true;
                server_health[i].failed_checks = 0;
                server_health[i].last_seen = time(NULL);
            }

            pthread_mutex_unlock(&server_health[i].health_mutex);
        }
        pthread_mutex_unlock(&health_mutex);

        sleep(HEALTH_CHECK_INTERVAL);
    }
    return NULL;
}


bool register_server(const char *ip,int port){
    for(int i=0;i<server_count;i++){
        printf("port=%d",server_health[i].port);
        if(server_health[i].port==port){
            server_health[i].is_active=true;
            printf("server with port %d back online\n",port);
            printf("%d %d\n",server_count,i);
            if(server_count>=3){
                sleep(10);
                char src_path1[MAX_PATH_LENGTH],src_path2[MAX_PATH_LENGTH];
                snprintf(src_path1,MAX_PATH_LENGTH,"%s/backup",src_dest[(i-1+server_count)%server_count].dir_path);
                snprintf(src_path2,MAX_PATH_LENGTH,"%s/backup",src_dest[(i-2+server_count)%server_count].dir_path);
                
                char dest_path1[MAX_PATH_LENGTH],dest_path2[MAX_PATH_LENGTH];
                snprintf(dest_path1,MAX_PATH_LENGTH,"%s/backup_s%d_s%d",src_dest[i].dir_path,i,(i-1+server_count)%server_count);
                snprintf(dest_path2,MAX_PATH_LENGTH,"%s/backup_s%d_s%d",src_dest[i].dir_path,i,(i-2+server_count)%server_count);

                remove_prefix(src_path1,"Test/");
                remove_prefix(src_path2,"Test/");
                remove_prefix(dest_path1,"Test/");
                remove_prefix(dest_path2,"Test/");

                copy_directory_network(src_path1,dest_path1,storage_servers[(i-1+server_count)%server_count].ip,storage_servers[i].ip,storage_servers[(i-1+server_count)%server_count].port,storage_servers[i].port,0);
                sleep(5);
                printf("works\n");
                copy_directory_network(src_path2,dest_path2,storage_servers[(i-2+server_count)%server_count].ip,storage_servers[i].ip,storage_servers[(i-2+server_count)%server_count].port,storage_servers[i].port,0);
                
                char src_path3[MAX_PATH_LENGTH]; // for the server coming up
                snprintf(src_path3,MAX_PATH_LENGTH,"%s/backup",src_dest[i].dir_path);
                char dest_path3[MAX_PATH_LENGTH], dest_path4[MAX_PATH_LENGTH]; // for backups coming up
                snprintf(dest_path3,MAX_PATH_LENGTH,"%s/backup_s%d_s%d",src_dest[(i+1)%server_count].dir_path,(i+1)%server_count,i);
                snprintf(dest_path4,MAX_PATH_LENGTH,"%s/backup_s%d_s%d",src_dest[(i+2)%server_count].dir_path,(i+2)%server_count,i);
                remove_prefix(src_path3,"Test/");
                remove_prefix(dest_path3,"Test/");
                remove_prefix(dest_path4,"Test/");

                copy_directory_network(src_path3,dest_path3,storage_servers[i].ip,storage_servers[(i+1)%server_count].ip,storage_servers[i].port,storage_servers[(i+1)%server_count].port,0);
                sleep(5);
                printf("works\n");
                copy_directory_network(src_path3,dest_path4,storage_servers[i].ip,storage_servers[(i+2)%server_count].ip,storage_servers[i].port,storage_servers[(i+2)%server_count].port,0);
                
            }
            return true;
        }
    }
    return false;
}

void *handle_client(void *client_socket)
{
    int sock = *(int *)client_socket;
    char buffer[BUFFER_SIZE] = {0};

    // Provide the first available storage server to the client
    pthread_mutex_lock(&mutex);
    if (server_count > 0)
    {
        snprintf(buffer, sizeof(buffer), "%s:%d", storage_servers[MAX_STORAGE_SERVERS - 1].ip, storage_servers[MAX_STORAGE_SERVERS - 1].port);
    }
    else
    {
        strcpy(buffer, "No storage servers available");
    }
    pthread_mutex_unlock(&mutex);

    send(sock, buffer, strlen(buffer), 0);
    printf("Sent storage server info to client: %s\n", buffer);

    close(sock);
    return NULL;
}

void *handle_ss_registration(void *client_socket_ptr)
{
    int client_socket = *(int *)client_socket_ptr;
    free(client_socket_ptr);

    // Get client address information
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    getpeername(client_socket, (struct sockaddr *)&peer_addr, &peer_addr_len);

    char buffer[BUFFER_SIZE];
    char server_ip[INET_ADDRSTRLEN];
    int server_port;

    // fflush(buffer);
    // pthread_mutex_lock(&mutex);
    // printf("Before Receive: %s", buffer);
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    // printf("Received After: %s\n", buffer);
    // pthread_mutex_unlock(&mutex);

    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';
        log_message(LOG_INFO, "Received registration request from %s:%d - Data: %s",
                    inet_ntoa(peer_addr.sin_addr),
                    ntohs(peer_addr.sin_port),
                    buffer);

        // Parse the received IP and port
        if (sscanf(buffer, "%[^:]:%d", server_ip, &server_port) != 2)
        {
            log_message(LOG_ERROR, "Failed to parse IP:Port from storage server data: %s", buffer);
            close(client_socket);
            return NULL;
        }
        bool flag1 = register_server(server_ip, server_port);
        if (flag1 == true)
        {
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        int flag = 0;
        for (int i = 0; i < server_count; i++)
        {
            if (storage_servers[i].port == server_port && !strcmp(storage_servers[i].ip, server_ip))
            {
                flag = 1;
            }
        }
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            log_message(LOG_ERROR, "Error receiving directory path from %s:%d",
                        server_ip, server_port);
            close(client_socket);
            return NULL;
        }
        buffer[bytes_received] = '\0';

        char directory_path[BUFFER_SIZE];
        strncpy(directory_path, buffer, BUFFER_SIZE);
        log_message(LOG_INFO, "Storage Server %s:%d - Directory Path: %s",
                    server_ip, server_port, directory_path);
        insert_path(file_trie, buffer, server_ip, server_port);

        // Step 3: Receive the file structure from the Storage Server
        while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0)
        {
            buffer[bytes_received] = '\0';

            // Check if it's the end of the file list
            if (strcmp(buffer, "END") == 0)
            {
                break;
            }

            log_message(LOG_INFO, "Received file: %s", buffer);
            insert_path(file_trie, buffer, server_ip, server_port);
        }
        print_all_paths(file_trie);
        if (!flag)
        {
            // Add storage server to the list with proper synchronization
            pthread_mutex_lock(&mutex);

            if (server_count < MAX_STORAGE_SERVERS)
            {
                // Add to array

                strcpy(storage_servers[server_count].ip, server_ip);
                storage_servers[server_count].port = server_port;
                init_server_health(server_count, server_ip, server_port);
                strncpy(src_dest[server_count].dir_path, directory_path, BUFFER_SIZE);
                server_count++;

                // Create new server node
                StorageServernew *new_server = malloc(sizeof(StorageServernew));
                if (!new_server)
                {
                    log_message(LOG_ERROR, "Failed to allocate memory for StorageServer at %s:%d",
                                server_ip, server_port);
                    pthread_mutex_unlock(&mutex);
                    close(client_socket);
                    return NULL;
                }

                // Initialize new server
                strcpy(new_server->ip, server_ip);
                new_server->port = server_port;
                new_server->file_structure = create_node();

                // Uncomment and modify if path processing is needed
                /*
                char *path = strtok(paths, ",");
                while (path) {
                    insert_path(new_server->file_structure, path);
                    path = strtok(NULL, ",");
                }
                new_server->next = server_list;
                server_list = new_server;
                */

                log_message(LOG_INFO, "Successfully registered storage server %s:%d (Server Count: %d)",
                            server_ip, server_port, server_count);
            }
            else
            {
                log_message(LOG_WARNING, "Maximum storage server limit reached. Rejected server %s:%d",
                            server_ip, server_port);
            }

            pthread_mutex_unlock(&mutex);

            if (server_count == 3)
            { // major urgent: add path to trie and delete it from trie
                for (int i = 0; i < 3; i++)
                {
                    initialize_backup_directory_network(i, server_count, src_dest[i % server_count].dir_path, src_dest[(i + 1) % server_count].dir_path, src_dest[(i + 2) % server_count].dir_path);
                    sleep(10);
                }
            }
            else if (server_count > 3)
            {
                initialize_backup_directory_network(server_count - 1, server_count, src_dest[(server_count - 1) % server_count].dir_path, src_dest[(server_count) % server_count].dir_path, src_dest[(server_count + 1) % server_count].dir_path);
                sleep(5);
                initialize_backup_directory_network(server_count - 2, server_count, src_dest[(server_count - 2) % server_count].dir_path, src_dest[(server_count - 1) % server_count].dir_path, src_dest[(server_count) % server_count].dir_path);
                sleep(5);
                initialize_backup_directory_network(server_count - 3, server_count, src_dest[(server_count - 3) % server_count].dir_path, src_dest[(server_count - 2) % server_count].dir_path, src_dest[(server_count - 1) % server_count].dir_path);
                // sleep(5);
                char del_dir_path1[PATH_MAX];
                char del_dir_path2[PATH_MAX];
                snprintf(del_dir_path1, PATH_MAX, "SS1/backup_s0_s%d", server_count - 3);
                snprintf(del_dir_path2, PATH_MAX, "SS2/backup_s1_s%d", server_count - 2);
                send_delete(storage_servers[0].ip, storage_servers[0].port, del_dir_path1, 1);
                send_delete(storage_servers[1].ip, storage_servers[1].port, del_dir_path2, 1);
                delete_path(file_trie, del_dir_path1, storage_servers[0].ip, storage_servers[0].port);
                delete_path(file_trie, del_dir_path2, storage_servers[1].ip, storage_servers[1].port);

                // del backup_s2_s%d(server_count-1)
                // del backup_s1_s%d(server_count-2)
            }
        }
    }
    else if (bytes_received == 0)
    {
        log_message(LOG_WARNING, "Connection closed by Storage Server at %s:%d",
                    inet_ntoa(peer_addr.sin_addr),
                    ntohs(peer_addr.sin_port));
    }
    else
    {
        log_message(LOG_ERROR, "Error receiving data from %s:%d - %s",
                    inet_ntoa(peer_addr.sin_addr),
                    ntohs(peer_addr.sin_port),
                    strerror(errno));
    }

    close(client_socket);
    return NULL;
}

void handle_ns_commands(char *command_buffer)
{
    // printf("1");
    int ARGUMENT_BUFFER_SIZE = 256;
    char command[ARGUMENT_BUFFER_SIZE];
    char path1[ARGUMENT_BUFFER_SIZE];
    char path2[ARGUMENT_BUFFER_SIZE];
    int type = -1;

    // Initialize the variables to empty strings
    memset(command, 0, sizeof(command));
    memset(path1, 0, sizeof(path1));
    memset(path2, 0, sizeof(path2));

    // Parse the command and arguments
    int args = sscanf(command_buffer, "%s %s %s %d", command, path1, path2, &type);
    // Print out all the commands and arguments space separated
    printf("Command: %s\n", command);
    // printf("%d\n", args);

    if (args >= 2)
    {
        printf("Argument 1: %s\n", path1);
    }
    if (args >= 3)
    {
        printf("Argument 2: %s\n", path2);
    }
    if (args == 4)
    {
        printf("Argument 3: %d\n", type);
    }

    // Handle commands
    // printf("here\n");
    // if(file_trie == NULL)
    // {
    //     printf("null");
    // }
    if ((strcmp(command, "CREATE") == 0) && (args == 4))
    {
        // printf("here");
        // printf("%d", type);

        StorageServer *cr = find_storage_server(file_trie, path1);
        if (cr == NULL)
        {
            char err_mess[50];
            get_error_message(4, err_mess, sizeof(err_mess));
            log_message(3, err_mess);
            return;
        }
        StorageServer cre = *cr;
        send_create(cre.ip, cre.port, path2, type);
        // create(path1, type);
        printf("CREATE command executed successfully.\n");
    }
    else if (strcmp(command, "DELETE") == 0 && args == 4)
    {
        // list_directory(path1);
        StorageServer *cre_ptr = find_storage_server(file_trie, path1);
        if (cre_ptr == NULL) {
            char err_mess[50];
            get_error_message(4, err_mess, sizeof(err_mess));
            log_message(3, err_mess);
            return;
        }
        StorageServer cre = *cre_ptr;
        if(&cre == NULL)
        {
            char err_mess[50];
            get_error_message(4, err_mess, sizeof(err_mess));
            log_message(3, err_mess);
            return;
        }
        printf("path2=%s\n", path2);
        send_delete(cre.ip, cre.port, path2, type);
        delete_path(file_trie,path2,cre.ip, cre.port);
        // delete (path1, type);
        printf("DELETE command executed successfully.\n");
    }
    else if (strcmp(command, "COPY") == 0 && args >= 3)
    {
        // copy_file(path1, path2);
        StorageServer *src_ptr = find_storage_server(file_trie, path1);
        StorageServer *dest_ptr = find_storage_server(file_trie, path2);

        if (src_ptr == NULL || dest_ptr == NULL) {
            char err_mess[50];
            get_error_message(4, err_mess, sizeof(err_mess));
            log_message(3, err_mess);
            return;
        }

        StorageServer src = *src_ptr;
        StorageServer dest = *dest_ptr;

        printf("Source Server Port: %d\n", src.port);
        printf("Destination Server Port: %d\n", dest.port);

        if (args == 3)
        {
            copy_file_network(path1, path2, src.ip, dest.ip, src.port, dest.port);
            printf("COPY_FILE command executed successfully.\n");
            char combined_path[PATH_MAX];
            snprintf(combined_path, PATH_MAX, "%s/%s", path2, path1);
            insert_path(file_trie, combined_path, dest.ip, dest.port);
        }
        else if (args == 4 && type == 1)
        {
            // copy_directory(path1, path2);

            copy_directory_network(path1, path2, src.ip, dest.ip, src.port, dest.port, 1);
            printf("COPY_DIR command executed successfully.\n");

        }
        else
        {
            printf("Usage:\n");
            printf("COPY <source_path> <destination_path> <0/1>\n");
        }
    }
    else if (strcmp(command, "LIST") == 0)
    {
        // Send "hi" to the client
        print_all_paths(file_trie);
    }
    else if (strcmp(command, "QUIT") == 0)
    {
        printf("QUIT command received. Exiting command handler.\n");
    }
    else
    {
        printf("Unknown command: %s\n", command);
    }
}

int main()
{
    // Register cleanup function
    atexit(cleanup);
    file_trie = create_node();
    int server_fd, nm_socket, new_socket, client_socket;
    struct sockaddr_in address, nm_address, client_address;
    socklen_t addrlen = sizeof(address);
    socklen_t client_addr_len = sizeof(client_address);
    int opt = 1;

    // Initialize LRU Cache
    cache = create_lru_cache(10); // Cache size of 10 entries

    // Create socket for the naming server
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Naming server socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("Failed to set SO_REUSEADDR");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Configure naming server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(NM_PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, 5) == -1)
    {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Naming server is running on port %d...\n", NM_PORT);

    pthread_t health_thread;
    if (pthread_create(&health_thread, NULL, health_monitor, NULL) != 0)
    {
        log_message(LOG_ERROR, "Failed to create health monitoring thread");
        exit(EXIT_FAILURE);
    }
    pthread_detach(health_thread);

    // Accept and process connections
    while (1)
    {
        char temp_buffer[PATH_MAX];
        memset(temp_buffer, 0, sizeof(temp_buffer));
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket == -1)
        {
            perror("Accept failed");
            continue;
        }

        // Check if the connection is from a client or a storage server
        pthread_t thread_id;
        // pthread_mutex_lock(&mutex);
        int bytes_peeked = recv(new_socket, temp_buffer, sizeof(temp_buffer), NULL);
        printf("%d\n", bytes_peeked);
        temp_buffer[bytes_peeked] = '\0';
        char ssip[20];
        char reci[100];
        int ssport;
        int client;
        char temp[15];
        int ind = 0;
        printf("Temp Buffer: %s\n", temp_buffer);
        if (strncmp(temp_buffer, "Write completed", 15) != 0 && strncmp(temp_buffer, "ASYNC", 5) != 0)
        {
            printf("Temp Buffer: %s\n", temp_buffer);
        }
        else if (strncmp(temp_buffer, "Write completed", 15) == 0)
        {
            // printf("%s", temp_buffer);
            sscanf(temp_buffer, "Write completed at %s %d", reci, &client);
            printf("temp: %s", temp_buffer);
            printf("reci %s", reci);
            int flag = 0;
            for (int i = 0; i < strlen(reci); i++)
            {
                if (reci[i] == ':')
                {
                    flag = 1;
                }
                if (flag == 0)
                {
                    ssip[ind++] = reci[i];
                }
                if (flag == 2)
                {
                    temp[ind++] = reci[i];
                }
                if (flag == 1)
                {
                    ssip[ind] = '\0';
                    ind = 0;
                    flag = 2;
                }
            }
            temp[ind] = '\0';
            ssport = atoi(temp);
            // printf("ip address: %s:%d\n", ssip, ssport);
            struct sockaddr_in server_address;
            int ss_sock = socket(AF_INET, SOCK_STREAM, 0);
            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(ssport); // Convert port to network byte order
            if (inet_pton(AF_INET, ssip, &server_address.sin_addr) <= 0)
            {
                perror("Invalid IP address");
                close(ss_sock);
                exit(EXIT_FAILURE);
            }

            // Connect to the server
            if (connect(ss_sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
            {
                perror("Connection failed");
                close(ss_sock);
                exit(EXIT_FAILURE);
            }
            int health = 0;
            for (int i = 0; i < server_count; i++)
            {
                if (storage_servers[i].port == ssport && !strcmp(storage_servers[i].ip, ssip))
                    health = 1;
            }
            if (!health)
            {
                send(ss_sock, "Unhealthy", 9, 0);
            }
            else
            {
                send(new_socket, "Healthy", 7, 0);
                // write(new_socket, "Write completed\n", 17);
            }
            continue;
        }
        else if (strncmp(temp_buffer, "ASYNC", 5) == 0)
        {
            if (strlen(temp_buffer) > 20)
            {
            }
            continue;
        }

        // pthread_mutex_unlock(&mutex);

        char command_buffer[100];
        strncpy(command_buffer, temp_buffer, sizeof(command_buffer) - 1);
        command_buffer[sizeof(command_buffer) - 1] = '\0';

        char *token = strtok(temp_buffer, " ");
        if (token != NULL)
        {
            if (strcmp(token, "STORAGE") == 0)
            {
                // Send acknowledgment to storage server
                char ack_msg[] = "ACK";
                send(new_socket, ack_msg, strlen(ack_msg), 0);

                // Handle storage server registration
                int *new_socket_ptr = malloc(sizeof(int));
                if (!new_socket_ptr)
                {
                    perror("Failed to allocate memory for socket pointer");
                    close(new_socket);
                    continue;
                }
                *new_socket_ptr = new_socket;
                if (pthread_create(&thread_id, NULL, handle_ss_registration, new_socket_ptr) != 0)
                {
                    perror("Failed to create thread for storage server");
                    free(new_socket_ptr);
                    close(new_socket);
                    continue;
                }
                pthread_detach(thread_id);
            }
            else if (strcmp(token, "CREATE") == 0 || strcmp(token, "COPY") == 0 || strcmp(token, "DELETE") == 0 || strcmp(token, "LIST") == 0)
            {
                handle_ns_commands(command_buffer);
                if (strcmp(token, "LIST") == 0) {
                    int saved_stdout = dup(STDOUT_FILENO);
                    dup2(new_socket, STDOUT_FILENO);
                    print_all_paths(file_trie);
                    dup2(saved_stdout, STDOUT_FILENO);
                    close(saved_stdout);
                    send(new_socket, "END", strlen("END"), 0);
                }
            }
            else
            {
                // Handle client requests
                char *command = strtok(command_buffer, " ");
                char *path = strtok(NULL, " ");
                printf("Command: %s, Path: %s\n", command, path);
                StorageServer *found_server = find_storage_server(file_trie, path);
                int *client_socket_ptr = malloc(sizeof(int));
                if (found_server == NULL) {
                    const char *error_msg = "Error: Storage server not found for the given path";
                    send(new_socket, error_msg, strlen(error_msg), 0);
                    close(new_socket);
                    free(client_socket_ptr);
                    continue;
                }
                storage_servers[MAX_STORAGE_SERVERS - 1] = *find_storage_server(file_trie, path);
                if (!client_socket_ptr)
                {
                    perror("Failed to allocate memory for socket pointer");
                    close(new_socket);
                    continue;
                }
                *client_socket_ptr = new_socket;
                if (pthread_create(&thread_id, NULL, handle_client, client_socket_ptr) != 0)
                {
                    perror("Failed to create thread for client");
                    free(client_socket_ptr);
                    close(new_socket);
                    continue;
                }
                pthread_detach(thread_id);
            }
        }
    }

    close(server_fd);
    return 0;
}