// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
#include "ns.h"

// Function to create a new Trie node
TrieNode *create_node()
{
    TrieNode *node = (TrieNode *)malloc(sizeof(TrieNode));
    if (!node)
    {
        perror("Failed to allocate memory for TrieNode");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        node->children[i] = NULL;
    }
    node->is_end_of_path = 0;
    return node;
}

// Insert a directory path into the Trie
void insert_path(TrieNode *root, const char *path, char *server_ip, int server_port)
{
    TrieNode *current = root;
    for (int i = 0; path[i] != '\0'; i++)
    {
        int index = (unsigned char)path[i]; // ASCII value of the character
        if (!current->children[index])
        {
            current->children[index] = create_node();
        }
        current = current->children[index];
    }
    strcpy(current->SS.ip, server_ip);
    current->SS.port = server_port;
    current->is_end_of_path = 1; // Mark the end of the directory path
}

// Helper function to print all paths in the Trie recursively
void print_paths_recursive(TrieNode *node, char *buffer, int depth)
{
    if (!node)
        return;

    // If this node marks the end of a path, print the accumulated path
    if (node->is_end_of_path)
    {
        buffer[depth] = '\0';
        printf("%s\n", buffer);
    }

    // Recur for all children
    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (node->children[i])
        {
            buffer[depth] = (char)i;
            print_paths_recursive(node->children[i], buffer, depth + 1);
        }
    }
}

// Print all stored paths in the Trie
void print_all_paths(TrieNode *root)
{
    char buffer[1024]; // Temporary buffer to accumulate path characters
    print_paths_recursive(root, buffer, 0);
}

// Free the memory allocated for the Trie
void free_trie(TrieNode *node)
{
    if (!node)
        return;
    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (node->children[i])
        {
            free_trie(node->children[i]);
        }
    }
    free(node);
}

bool has_children(TrieNode *node)
{
    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (node->children[i])
        {
            return true;
        }
    }
    return false;
}

// Recursive function to delete a specific path and its subtree only if the server matches
bool delete_specific_path(TrieNode *node, const char *path, int depth, const char *server_ip, int server_port)
{
    if (!node)
        return false;

    // Base case: Reached the end of the path
    if (path[depth] == '\0')
    {
        // Check if this node belongs to the specified server
        if (node->is_end_of_path && strcmp(node->SS.ip, server_ip) == 0 && node->SS.port == server_port)
        {
            // Unmark the path as an endpoint
            node->is_end_of_path = 0;

            // Free the subtree if no children exist
            if (!has_children(node))
            {
                return true; // Indicate that this node can be deleted
            }
        }
        return false; // Server doesn't match or path can't be deleted
    }

    // Recur for the child node corresponding to the current character
    int index = (unsigned char)path[depth];
    if (!node->children[index])
    {
        return false; // Path does not exist
    }

    // If the child can be deleted, free it and remove the reference
    if (delete_specific_path(node->children[index], path, depth + 1, server_ip, server_port))
    {
        free(node->children[index]);
        node->children[index] = NULL;

        // Check if the current node can also be deleted
        return !has_children(node) && !node->is_end_of_path;
    }

    return false;
}

// Wrapper function to delete a path
void delete_path(TrieNode *root, const char *path, const char *server_ip, int server_port)
{
    if (!root || !path || !server_ip)
    {
        fprintf(stderr, "Invalid input to delete_path\n");
        return;
    }

    if (delete_specific_path(root, path, 0, server_ip, server_port))
    {
        printf("Path '%s' successfully deleted for server %s:%d.\n", path, server_ip, server_port);
    }
    else
    {
        printf("Path '%s' does not exist or server details do not match.\n", path);
    }
}

void reroute_prefix(TrieNode *root, char *src_path, char *dest_path, char *dest_ip, int dest_port)
{
    TrieNode *src_node = root;
    TrieNode *dest_node = root;

    // Traverse to the source path node
    for (int i = 0; src_path[i] != '\0'; i++)
    {
        int index = (unsigned char)src_path[i];
        if (!src_node->children[index])
        {
            printf("Source path '%s' does not exist.\n", src_path);
            return;
        }
        src_node = src_node->children[index];
    }

    // Traverse to the destination path node, creating nodes if necessary
    for (int i = 0; dest_path[i] != '\0'; i++)
    {
        int index = (unsigned char)dest_path[i];
        if (!dest_node->children[index])
        {
            dest_node->children[index] = create_node();
        }
        dest_node = dest_node->children[index];
    }

    // Copy the subtree from src_node to dest_node
    copy_subtree(src_node, dest_node, dest_ip, dest_port);
}

// Helper function to copy the subtree
void copy_subtree(TrieNode *src, TrieNode *dest, char* dest_ip, int dest_port)
{
    if (!src)
        return;
    dest->is_end_of_path = src->is_end_of_path;
    if (src->is_end_of_path)
    {
        strcpy(dest->SS.ip, dest_ip);
        dest->SS.port = dest_port;
    }
    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (src->children[i])
        {
            dest->children[i] = create_node();
            copy_subtree(src->children[i], dest->children[i], dest_ip, dest_port);
        }
    }
}

StorageServer *find_storage_server(TrieNode *root, const char *path)
{
    if (!root || !path)
        return NULL;

    StorageServer *cached = get_from_cache(cache, path);
    if (cached)
    {
        printf("Found cached path in LRU\n");
        return cached;
    }

    TrieNode *current = root;

    for (int i = 0; path[i] != '\0'; i++)
    {
        int index = (unsigned char)path[i];
        if (!current->children[index])
        {
            // printf("null\n");
            return NULL; // Path not found in Trie
        }
        current = current->children[index];
    }

    // Check if we have reached the end of a valid path
    if (current->is_end_of_path)
    {
        // Insert the found storage server into the cache
        put_in_cache(cache, path, &(current->SS));
        return &(current->SS); // Return the Storage Server info
    }

    // printf("null\n");
    return NULL; // Path exists but is not marked as a complete path
}

// // Example usage
// int main() {
//     TrieNode *root = create_node(); // Root of the Trie

//     // List of directory paths to insert
//     const char *paths[] = {
//         "/home/user/docs",
//         "/home/user/music",
//         "/home/user/music/rock",
//         "/home/user/videos",
//         "/var/logs",
//         "/var/www/html",
//         "/tmp"
//     };

//     // Insert each path into the Trie
//     for (int i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
//         insert_path(root, paths[i]);
//     }

//     // Print all paths stored in the Trie
//     printf("Stored directory paths:\n");
//     print_all_paths(root);

//     // Free memory
//     free_trie(root);
//     return 0;
// }
