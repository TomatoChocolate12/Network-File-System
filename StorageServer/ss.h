#include "../Utils/constants.h"
#include "../Utils/headers.h"
#include "../Utils/typedefs.h"
#include "../Utils/errors.h"

// #define PORT 7070
// #define STORAGE_PORT 7070

extern int PORT;

void register_with_naming_server();
void handle_client(int client_socket);
void send_file_content(int client_socket, const char *filename);
void write_to_file(int client_socket, const char *filename, const char *content);
void send_file_info(int client_socket, const char *filename);
void stream_audio(int client_socket, const char *filename);
void send_directory_contents(int sock, const char *path, char* parent_dir, int send_content);
void *async_write(int client_socket, const char *filename, char *content, const char * client_ip);