#include "ns.h"


FILE* log_file = NULL;

// Function to get current timestamp
char* get_timestamp() {
    time_t now;
    time(&now);
    char* timestamp = malloc(sizeof(char) * 26);
    ctime_r(&now, timestamp);
    timestamp[24] = '\0'; // Remove newline
    return timestamp;
}

// Function to write to log file and display message
void log_message(int level, const char* format, ...) {
    if (log_file == NULL) {
        log_file = fopen(LOG_FILE, "a");
        if (log_file == NULL) {
            perror("Failed to open log file");
            return;
        }
    }

    char* timestamp = get_timestamp();
    const char* level_str;
    switch (level) {
        case LOG_INFO:    level_str = "INFO"; break;
        case LOG_WARNING: level_str = "WARNING"; break;
        case LOG_ERROR:   level_str = "ERROR"; break;
        default:          level_str = "UNKNOWN";
    }

    // Format message with variable arguments
    va_list args;
    va_start(args, format);
    char message[BUFFER_SIZE];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Write to log file
    fprintf(log_file, "[%s] %s: %s\n", timestamp, level_str, message);
    fflush(log_file);

    // Display on console
    printf("[%s] %s: %s\n", timestamp, level_str, message);
    
    free(timestamp);
}

void cleanup() {
    if (log_file != NULL) {
        fclose(log_file);
    }
}

