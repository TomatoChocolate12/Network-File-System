#ifndef NS_H
#define NS_H

#include "../Utils/constants.h"
#include "../Utils/headers.h"
#include "../Utils/typedefs.h"
#include "../Utils/errors.h"

#define PATH_MAX 4096

// DEFINITIONS for LRU and TRIE
#define MAX_CHILDREN 256 // Maximum number of child nodes (for ASCII characters)
#define MAX_PATH_LENGTH 1024
#define CACHE_SIZE 10
#define MAX_SUBDIRS 64

// Definitions for Failure Detection
#define HEALTH_CHECK_INTERVAL 2
#define MAX_RETRIES 3

// Definitions for Log
#define LOG_INFO 0
#define LOG_WARNING 1
#define LOG_ERROR 2
#define LOG_FILE "naming_server.log"

#define MAX_STORAGE_SERVERS 10

typedef struct command_packet
{
    int operation; // 0 for create, 1 for delete
    int type;      // 0 for directory, 1 for file
    char path[PATH_MAX];
} command_packet;

// TYPEDEFS for LRU and TRIE


typedef struct
{
    char ip[INET_ADDRSTRLEN];
    int port;
} StorageServer;

// Trie Node Structure
typedef struct TrieNode
{
    struct TrieNode *children[MAX_CHILDREN];
    int is_end_of_path;
    StorageServer SS;
} TrieNode;

typedef struct StorageServernew
{
    char ip[INET_ADDRSTRLEN];      // IP address of the server
    int port;                      // Port number
    TrieNode *file_structure;      // Trie for the server's file paths
    struct StorageServernew *next; // Linked list to hold multiple servers
} StorageServernew;

typedef struct file_header
{
    char filename[256];
    size_t filesize;
    int type; // 0 for directory, 1 for file
} file_header;

typedef struct
{
    char ip[INET_ADDRSTRLEN];
    int port;
    bool is_active;
    time_t last_seen;
    int failed_checks;
    pthread_mutex_t health_mutex;
} ServerHealth;

// LRU Cache node structure
typedef struct LRUNode {
    char *path;                 // The cached path
    StorageServer server;       // The storage server info
    struct LRUNode *prev;      // Previous node in DLL
    struct LRUNode *next;      // Next node in DLL
} LRUNode;

// LRU Cache structure
typedef struct LRUCache{
    int capacity;              // Maximum number of entries
    int size;                  // Current number of entries
    LRUNode *head;            // Most recently used
    LRUNode *tail;            // Least recently used
    struct HashTable *table;   // Hash table for O(1) lookup
}LRUCache;

// Hash table entry structure
typedef struct HashEntry {
    char *key;                 // Path
    LRUNode *node;            // Pointer to corresponding LRU node
    struct HashEntry *next;    // For handling collisions
} HashEntry;

// Hash table structure
typedef struct HashTable {
    int size;                  // Size of the hash table
    HashEntry **entries;       // Array of hash entries
} HashTable;

extern int async_writing[MAX_STORAGE_SERVERS];

extern int server_count;
extern pthread_mutex_t mutex;
extern StorageServer storage_servers[MAX_STORAGE_SERVERS];
extern TrieNode *file_trie;
extern LRUCache* cache;

char *get_timestamp();
void log_message(int level, const char *format, ...);
void *handle_ss_registration(void *client_socket_ptr);
void cleanup();

// TRIE FUNCTIONS

TrieNode *create_node();
void insert_path(TrieNode *root, const char *path, char *server_ip, int server_port);
void print_paths_recursive(TrieNode *node, char *buffer, int depth);
void print_all_paths(TrieNode *root);
void free_trie(TrieNode *node);
bool has_children(TrieNode *node);
bool delete_specific_path(TrieNode *node, const char *path, int depth, const char *server_ip, int server_port);
void delete_path(TrieNode *root, const char *path, const char *server_ip, int server_port);
StorageServer *find_storage_server(TrieNode *root, const char *path);
print_all_paths_to_client(TrieNode* root,int Client_socket);

// NS_COMMANDS

void handle_ns_commands(char *command_buffer);
int connect_to_storage(const char *ip_address, int port);
void send_create(const char *ip_address, int port, const char *path, int type);
void send_delete(const char *ip_address, int port, const char *path, int type);
int send_file(const char *filepath, int sock);
int receive_file(int src_sock, const char *dest_path);
void copy_file_network(const char *src, const char *dest, const char *src_server_ip, const char *dest_server_ip, int src_port, int dest_port);
void copy_directory_network(const char *src, const char *dest, const char *src_server_ip, const char *dest_server_ip, int src_port, int dest_port,int flag);
void list_directory(int Client_socket);


//LRU FUNCTIONS
unsigned long hash_function(const char *str);
HashTable *create_hash_table(int size);
LRUCache *create_lru_cache(int capacity);
LRUNode *create_lru_node(const char *path, StorageServer *server);
void move_to_front(LRUCache *cache, LRUNode *node);
void hash_put(HashTable *table, const char *key, LRUNode *node);
LRUNode *hash_get(HashTable *table, const char *key);
void hash_remove(HashTable *table, const char *key);
StorageServer *get_from_cache(LRUCache *cache, const char *path);
void put_in_cache(LRUCache *cache, const char *path, StorageServer *server);
void free_lru_cache(LRUCache *cache);

#endif