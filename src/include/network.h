#ifndef STEAL_NETWORK_H
#define STEAL_NETWORK_H

#include <stddef.h>
#include <stdbool.h>

// Progress callback function type
typedef void (*progress_callback_t)(size_t total, size_t current, void* userdata);

// Download result structure
typedef struct {
    bool success;
    char error_message[256];
} download_result_t;

// Initialize network subsystem
bool network_init(void);

// Cleanup network subsystem
void network_cleanup(void);

// Download a file from URL to local path with progress callback
download_result_t download_file(const char* url, const char* local_path, progress_callback_t progress_cb, void* userdata);

// Check if a URL is reachable
bool is_url_reachable(const char* url);

#endif // STEAL_NETWORK_H 