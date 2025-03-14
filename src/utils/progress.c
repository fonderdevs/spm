#include "../include/progress.h"
#include "../include/common.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>

void print_size(double size) {
    if (size > GB) {
        printf("%.2f GB", size / GB);
    } else if (size > MB) {
        printf("%.2f MB", size / MB);
    } else if (size > KB) {
        printf("%.2f KB", size / KB);
    } else {
        printf("%.0f B", size);
    }
}

void* spinner_animation(void* arg) {
    struct SpinnerData* data = (struct SpinnerData*)arg;
    const char* frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    int frame = 0;
    
    while (data->running) {
        printf("\r\033[K\033[1;36m%s\033[0m %s", frames[frame], data->text);
        fflush(stdout);
        frame = (frame + 1) % 10;
        usleep(80000);
    }
    printf("\r\033[K");
    return NULL;
}

void progress_callback(size_t total, size_t current, void* userdata) {
    struct ProgressData* progress = (struct ProgressData*)userdata;
    double currentTime = clock() / (double)CLOCKS_PER_SEC;
    double timeDiff = currentTime - progress->lastUpdateTime;
    
    if (timeDiff >= 0.1 || current == total) {
        progress->totalSize = total;
        progress->downloaded = current;
        
        progress->downloadSpeed = (current - progress->downloaded) / timeDiff;
        if (progress->downloadSpeed > 0) {
            progress->estimatedTime = (total - current) / progress->downloadSpeed;
        }
        
        int percent = 0;
        if (total > 0) {
            percent = (int)((current / (double)total) * 100);
        }
        
        if (percent != progress->lastPercent) {
            printf("\r\033[K");
            
            int barWidth = progress->termWidth - 50;
            if (barWidth < 10) barWidth = 10;
            
            int filledWidth = 0;
            if (total > 0) {
                filledWidth = (int)((current / (double)total) * barWidth);
            }
            
            if (filledWidth < 0) filledWidth = 0;
            if (filledWidth > barWidth) filledWidth = barWidth;
            
            printf("[");
            for (int i = 0; i < barWidth; i++) {
                if (i < filledWidth) printf("=");
                else if (i == filledWidth) printf(">");
                else printf(" ");
            }
            printf("] %3d%% ", percent);
            
            print_size(current);
            printf("/");
            if (total > 0) {
                print_size(total);
            } else {
                printf("?");
            }
            
            if (progress->downloadSpeed > 0) {
                printf(" %.1f MB/s ", progress->downloadSpeed / MB);
                if (progress->estimatedTime < 60) {
                    printf("(%.0fs left)", progress->estimatedTime);
                } else {
                    printf("(%.1fm left)", progress->estimatedTime / 60);
                }
            }
            
            fflush(stdout);
            progress->lastPercent = percent;
            progress->lastUpdateTime = currentTime;
        }
    }
} 