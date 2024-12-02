#include "ns.h"

// Hash function
unsigned long hash_function(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

// Create a new hash table
HashTable *create_hash_table(int size) {
    HashTable *table = (HashTable *)malloc(sizeof(HashTable));
    if (!table) {
        perror("Failed to allocate memory for hash table");
        exit(EXIT_FAILURE);
    }
    
    table->size = size;
    table->entries = (HashEntry **)calloc(size, sizeof(HashEntry *));
    if (!table->entries) {
        free(table);
        perror("Failed to allocate memory for hash entries");
        exit(EXIT_FAILURE);
    }
    
    return table;
}

// Create a new LRU cache
LRUCache *create_lru_cache(int capacity) {
    LRUCache *cache = (LRUCache *)malloc(sizeof(LRUCache));
    if (!cache) {
        perror("Failed to allocate memory for LRU cache");
        exit(EXIT_FAILURE);
    }
    
    cache->capacity = capacity;
    cache->size = 0;
    cache->head = NULL;
    cache->tail = NULL;
    cache->table = create_hash_table(capacity * 2); // Double size to reduce collisions
    
    return cache;
}

// Create a new LRU node
LRUNode *create_lru_node(const char *path, StorageServer *server) {
    LRUNode *node = (LRUNode *)malloc(sizeof(LRUNode));
    if (!node) {
        perror("Failed to allocate memory for LRU node");
        exit(EXIT_FAILURE);
    }
    
    node->path = strdup(path);
    if (!node->path) {
        free(node);
        perror("Failed to allocate memory for path");
        exit(EXIT_FAILURE);
    }
    
    memcpy(&node->server, server, sizeof(StorageServer));
    node->prev = NULL;
    node->next = NULL;
    
    return node;
}

// Move node to front (most recently used)
void move_to_front(LRUCache *cache, LRUNode *node) {
    if (node == cache->head) {
        return;
    }
    
    // Remove from current position
    if (node->prev) {
        node->prev->next = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    }
    if (node == cache->tail) {
        cache->tail = node->prev;
    }
    
    // Move to front
    node->next = cache->head;
    node->prev = NULL;
    if (cache->head) {
        cache->head->prev = node;
    }
    cache->head = node;
    
    if (!cache->tail) {
        cache->tail = node;
    }
}

// Add or update entry in hash table
void hash_put(HashTable *table, const char *key, LRUNode *node) {
    unsigned long index = hash_function(key) % table->size;
    
    HashEntry *entry = table->entries[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->node = node;
            return;
        }
        entry = entry->next;
    }
    
    // Create new entry
    HashEntry *new_entry = (HashEntry *)malloc(sizeof(HashEntry));
    if (!new_entry) {
        perror("Failed to allocate memory for hash entry");
        exit(EXIT_FAILURE);
    }
    
    new_entry->key = strdup(key);
    new_entry->node = node;
    new_entry->next = table->entries[index];
    table->entries[index] = new_entry;
}

// Get entry from hash table
LRUNode *hash_get(HashTable *table, const char *key) {
    unsigned long index = hash_function(key) % table->size;
    
    HashEntry *entry = table->entries[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->node;
        }
        entry = entry->next;
    }
    
    return NULL;
}

// Remove entry from hash table
void hash_remove(HashTable *table, const char *key) {
    unsigned long index = hash_function(key) % table->size;
    
    HashEntry *entry = table->entries[index];
    HashEntry *prev = NULL;
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                table->entries[index] = entry->next;
            }
            free(entry->key);
            free(entry);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

// Get storage server info from cache
StorageServer *get_from_cache(LRUCache *cache, const char *path) {
    LRUNode *node = hash_get(cache->table, path);
    if (node) {
        move_to_front(cache, node);
        return &node->server;
    }
    return NULL;
}

// Put storage server info into cache
void put_in_cache(LRUCache *cache, const char *path, StorageServer *server) {
    // Check if path already exists
    LRUNode *existing = hash_get(cache->table, path);
    if (existing) {
        memcpy(&existing->server, server, sizeof(StorageServer));
        move_to_front(cache, existing);
        return;
    }
    
    // Create new node
    LRUNode *node = create_lru_node(path, server);
    
    // Add to front of LRU list
    node->next = cache->head;
    if (cache->head) {
        cache->head->prev = node;
    }
    cache->head = node;
    
    if (!cache->tail) {
        cache->tail = node;
    }
    
    // Add to hash table
    hash_put(cache->table, path, node);
    cache->size++;
    
    // Remove least recently used if cache is full
    if (cache->size > cache->capacity) {
        LRUNode *lru = cache->tail;
        cache->tail = lru->prev;
        if (cache->tail) {
            cache->tail->next = NULL;
        }
        
        hash_remove(cache->table, lru->path);
        free(lru->path);
        free(lru);
        cache->size--;
    }
}

// Free the LRU cache
void free_lru_cache(LRUCache *cache) {
    if (!cache) return;
    
    // Free all nodes
    LRUNode *current = cache->head;
    while (current) {
        LRUNode *next = current->next;
        free(current->path);
        free(current);
        current = next;
    }
    
    // Free hash table entries
    for (int i = 0; i < cache->table->size; i++) {
        HashEntry *entry = cache->table->entries[i];
        while (entry) {
            HashEntry *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    
    free(cache->table->entries);
    free(cache->table);
    free(cache);
}