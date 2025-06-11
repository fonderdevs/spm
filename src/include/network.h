#ifndef spm_NETWORK_H
#define spm_NETWORK_H
#include <stddef.h>
#include <stdbool.h>
typedef void (*progress_callback_t)(size_t total, size_t current, void* userdata);

typedef struct {
    bool success;
    char error_message[256];
} download_result_t;

typedef struct {
    char url[256];
    double response_time;
    bool reachable;
} mirror_info_t;
bool network_init(void);
void network_cleanup(void);
download_result_t download_file(const char* url, const char* local_path, progress_callback_t progress_cb, void* userdata);
bool is_url_reachable(const char* url);
const char* get_fastest_mirror(void); //gas gas gas, im gonna step on the gas
double test_mirror_speed(const char* url);

#endif