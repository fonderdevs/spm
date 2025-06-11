#ifndef spm_PROGRESS_H
#define spm_PROGRESS_H

#include <stdbool.h>
#include <pthread.h>

struct ProgressData {
    double lastUpdateTime;
    double downloadSpeed;
    double estimatedTime;
    double totalSize;
    double downloaded;
    int lastPercent;
    int termWidth;
};

struct SpinnerData {
    bool running;
    char text[256];
};

void print_size(double size);
void* spinner_animation(void* arg);
void progress_callback(size_t total, size_t current, void* userdata);

#endif   