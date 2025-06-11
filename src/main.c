#include "include/common.h"
#include "include/network.h"
#include "include/package.h"
#include "include/progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <ctype.h>

//checking if running with root or not
bool is_root() {
    return (geteuid() == 0);
}

void show_sudo_warning(const char* command) {
    printf("\033[1;31mWarning: '%s' command requires root privileges\033[0m\n", command);
    printf("\033[1;33mPlease run with sudo: sudo spm %s\033[0m\n\n", command);
}

int main(int argc, char *argv[]) {
    load_config();
    if (argc < 2) {
        show_help();
        return 1;
    }

    if (strcmp(argv[1], "version") == 0) {
        show_version();
    } else if (strcmp(argv[1], "help") == 0) {
        show_help();
    } else if (strcmp(argv[1], "update") == 0) {
        if (!is_root()) {
            show_sudo_warning(argv[1]);
            return 1;
        }
        update_repos();
    } else if (strcmp(argv[1], "upgrade") == 0) {
        if (!is_root()) {
            show_sudo_warning(argv[1]);
            return 1;
        }
        upgrade_packages();
    } else if (strcmp(argv[1], "install") == 0) {
        if (argc < 3) {
            printf("Error: Package name required\n");
            return 1;
        }
        if (!is_root()) {
            show_sudo_warning(argv[1]);
            return 1;
        }
        install_with_dependencies(argv[2]);
    } else if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) {
            printf("Error: Package name required\n");
            return 1;
        }
        if (!is_root()) {
            show_sudo_warning(argv[1]);
            return 1;
        }
        remove_package(argv[2]);
    } else if (strcmp(argv[1], "search") == 0) {
        if (argc < 3) {
            printf("Error: Search term required\n");
            return 1;
        }
        search_packages(argv[2]);
    } else if (strcmp(argv[1], "switch-version") == 0) {
        if (argc < 3) {
            printf("Error: Package name required\n");
            return 1;
        }
        if (argc < 4) {
            printf("Error: Version required\n");
            return 1;
        }
        if (!is_root()) {
            show_sudo_warning(argv[1]);
            return 1;
        }
        switch_version(argv[2], argv[3]);
    } else {
        printf("Unknown command: %s\n", argv[1]);
        show_help();
        return 1;
    }

    return 0;
}
