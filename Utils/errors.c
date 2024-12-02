#include "headers.h"

void get_error_message(int error_code, char *error_message, size_t buffer_size) {
    switch (error_code) {
        case 1:
            strncpy(error_message, "Error 1: File not found.", buffer_size);
            break;
        case 2:
            strncpy(error_message, "Error 2: Access denied.", buffer_size);
            break;
        case 3:
            strncpy(error_message, "Error 3: Disk full.", buffer_size);
            break;
        case 4:
            strncpy(error_message, "Error 4: Illegal operation.", buffer_size);
            break;
        case 5:
            strncpy(error_message, "Error 5: Unknown error.", buffer_size);
            break;
        case 6:
            strncpy(error_message, "Error 6: Attempting to read a directory.", buffer_size);
            break;
        case 7:
            strncpy(error_message, "Error 7: Attempting to write to a directory.", buffer_size);
            break;
        case 8:
            strncpy(error_message, "Error 8: Invalid flag.", buffer_size);
            break;
        case 9:
            strncpy(error_message, "Error 9: Network timeout.", buffer_size);
            break;
        case 10:
            strncpy(error_message, "Error 10: Connection lost.", buffer_size);
            break;
        case 11:
            strncpy(error_message, "Error 11: File already exists.", buffer_size);
            break;
        case 12:
            strncpy(error_message, "Error 12: Path too long.", buffer_size);
            break;
        case 13:
            strncpy(error_message, "Error 13: File system read-only.", buffer_size);
            break;
        case 14:
            strncpy(error_message, "Error 14: Insufficient permissions.", buffer_size);
            break;
        case 15:
            strncpy(error_message, "Error 15: Quota exceeded.", buffer_size);
            break;
        case 16:
            strncpy(error_message, "Error 16: File locked by another process.", buffer_size);
            break;
        case 17:
            strncpy(error_message, "Error 17: Metadata corruption detected.", buffer_size);
            break;
        case 18:
            strncpy(error_message, "Error 18: Resource temporarily unavailable.", buffer_size);
            break;
        case 19:
            strncpy(error_message, "Error 19: Invalid file handle.", buffer_size);
            break;
        case 20:
            strncpy(error_message, "Error 20: Synchronization error.", buffer_size);
            break;
        case 21:
            strncpy(error_message, "Error 21: File system not mounted.", buffer_size);
            break;
        default:
            strncpy(error_message, "Error: Invalid error code.", buffer_size);
            break;
    }

    // Ensure null termination
    error_message[buffer_size - 1] = '\0';
}
