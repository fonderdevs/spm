#define _POSIX_C_SOURCE 200809L //copilot
#include "../include/common.h"
#include "../include/network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libgen.h>

void ensure_mirror_file() {
    FILE* f = fopen(MIRRORS_FILE, "r");
    if (f) {
        fclose(f);
        return;
    }
    char cmd[BUFFER_SIZE];
    char* mirrors_path = strdup(MIRRORS_FILE);
    char* dir = dirname(mirrors_path);
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", dir);
    free(mirrors_path);

    if (system(cmd) != 0) {
        printf("Warning: Failed to create mirrors directory\n");
        return;
    }

    f = fopen(MIRRORS_FILE, "w");
    if (!f) {
        printf("Warning: Could not create mirrors file. Using default mirror. (SLOW) \n");
        return;
    }

    fprintf(f, "#SPM Mirror List\n");
    fprintf(f, "# Format: url|location|description\n");
    fprintf(f, "%s|AS|Primary Mirror\n", DEFAULT_MIRROR_URL);
    fprintf(f, "%s|EU|European Mirror\n", SECONDARY_SERVER_URL);
    fclose(f);
    printf("Created default mirrors file at %s\n", MIRRORS_FILE);
}

//test a mirrors speed because first i thought i can just harcode shit but thats not how it works
double test_mirror_speed(const char* url) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    char test_url[MIRROR_URL_SIZE];
    snprintf(test_url, sizeof(test_url), "%s/packages.db", url);
    
    bool reachable = is_url_reachable(test_url);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    if (!reachable) return -1.0;
    
    return (end.tv_sec - start.tv_sec) + 
           (end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

const char* get_fastest_mirror() {
    // Check if we already have a saved mirror
    FILE* f = fopen(MIRRORS_FILE, "r");
    if (f) {
        char line[1024];
        // Skip comments
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            char* url = strtok(line, "|");
            if (url) {
                url[strcspn(url, "\n")] = 0;
                fclose(f);
                return strdup(url);
            }
        }
        fclose(f);
    }

    // No saved mirror - test them for the first time
    ensure_mirror_file();
    
    f = fopen(MIRRORS_FILE, "r");
    if (!f) {
        return strdup(DEFAULT_MIRROR_URL);
    }

    mirror_t mirrors[MAX_MIRRORS];
    int mirror_count = 0;
    char line[1024];
    
    printf("Testing mirrors for first time setup...\n");
    
    while (fgets(line, sizeof(line), f) && mirror_count < MAX_MIRRORS) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        char* url = strtok(line, "|");
        char* location = strtok(NULL, "|");
        char* desc = strtok(NULL, "|");
        
        if (!url || !location || !desc) continue;
        
        url[strcspn(url, "\n")] = 0;
        location[strcspn(location, "\n")] = 0;
        desc[strcspn(desc, "\n")] = 0;
        
        strncpy(mirrors[mirror_count].url, url, MIRROR_URL_SIZE-1);
        strncpy(mirrors[mirror_count].location, location, 63);
        strncpy(mirrors[mirror_count].description, desc, 255);
        
        printf("Testing %s mirror: ", location);
        fflush(stdout);
        
        mirrors[mirror_count].speed = test_mirror_speed(url);
        
        if (mirrors[mirror_count].speed >= 0) {
            printf("%.2f ms\n", mirrors[mirror_count].speed * 1000);
            mirror_count++;
        } else {
            printf("failed\n");
        }
    }
    
    fclose(f);
    
    int fastest = -1;
    double best_speed = 999999.0;
    
    for (int i = 0; i < mirror_count; i++) {
        if (mirrors[i].speed >= 0 && mirrors[i].speed < best_speed) {
            best_speed = mirrors[i].speed;
            fastest = i;
        }
    }
    
    if (fastest >= 0) {
        // Save the fastest mirror permanently
        FILE* temp = fopen(MIRRORS_FILE, "w");
        if (temp) {
            fprintf(temp, "# SPM mirrors\n");
            fprintf(temp, "%s|%s|%s\n", 
                    mirrors[fastest].url,
                    mirrors[fastest].location,
                    mirrors[fastest].description);
            // Write the rest as backups
            for (int i = 0; i < mirror_count; i++) {
                if (i != fastest) {
                    fprintf(temp, "%s|%s|%s\n",
                            mirrors[i].url,
                            mirrors[i].location,
                            mirrors[i].description);
                }
            }
            fclose(temp);
        }
        printf("Using %s mirror\n", mirrors[fastest].location);
        return strdup(mirrors[fastest].url);
    }

    return strdup(DEFAULT_MIRROR_URL);
}