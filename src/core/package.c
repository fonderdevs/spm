#include "../include/package.h"
#include "../include/common.h"
#include "../include/network.h"
#include "../include/progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <ctype.h>

static char server_url[PATH_SIZE] = DEFAULT_SERVER_URL;

static void network_progress_callback(size_t total, size_t current, void* userdata) {
    progress_callback(total, current, userdata);
}

void update_repos() {
    printf("Updating package repositories from %s...\n", server_url);
    
    char cmd[BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", REPO_PATH);
    if (system(cmd) != 0) {
        printf("Error: Failed to create repository directory\n");
        return;
    }

    char db_url[URL_SIZE];
    snprintf(db_url, sizeof(db_url), "%s/packages.db", server_url);
    
    char db_path[PATH_SIZE];
    snprintf(db_path, sizeof(db_path), "%s/packages.db", REPO_PATH);

    if (!network_init()) {
        printf("Error: Failed to initialize network\n");
        return;
    }

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    struct ProgressData progress = {
        .lastUpdateTime = clock() / (double)CLOCKS_PER_SEC,
        .downloadSpeed = 0,
        .estimatedTime = 0,
        .totalSize = 0,
        .downloaded = 0,
        .lastPercent = -1,
        .termWidth = w.ws_col
    };

    download_result_t result = download_file(db_url, db_path, network_progress_callback, &progress);
    
    network_cleanup();

    if (!result.success) {
        printf("\nError: %s\n", result.error_message);
        printf("URL attempted: %s\n", db_url);
        remove(db_path);
        return;
    }

    FILE *test = fopen(db_path, "r");
    if (!test) {
        printf("Error: packages.db was downloaded but cannot be opened\n");
        return;
    }
    
    char line[1024];
    if (!fgets(line, sizeof(line), test)) {
        printf("Error: packages.db appears to be empty\n");
        fclose(test);
        return;
    }
    fclose(test);

    printf("\nSuccessfully updated package database\n");
}

void install_package(const char* package_name) {
    if (strlen(package_name) > PACKAGE_NAME_MAX) {
        printf("Error: Package name too long\n");
        return;
    }

    char version[64] = {0};
    char category[128] = {0};
    char description[512] = {0};
    char* deps = NULL;
    int dep_count = 0;
    
    if (!get_package_info(package_name, version, category, &deps, &dep_count, description)) {
        printf("Failed to get package info for '%s'\n", package_name);
        return;
    }
    
    printf("\nPackage: %s\n", package_name);
    printf("Version: %s\n", version);
    printf("Category: %s\n", category);
    printf("Description: %s\n\n", description);
    
    bool is_precompiled = (strstr(category, PRECOMPILED_FLAG) != NULL);
    if (is_precompiled) {
        printf("This is a precompiled package.\n");
    }
    
    printf("Do you want to install this package? [Y/n] ");
    char response = getchar();
    if (response == '\n') response = 'Y';
    else while (getchar() != '\n');
    
    if (response != 'Y' && response != 'y') {
        printf("Installation cancelled.\n");
        if (deps) free(deps);
        return;
    }

    if (is_precompiled) {
        if (install_precompiled_package(package_name)) {
            printf("Precompiled package '%s' installed successfully.\n", package_name);
        } else {
            printf("Failed to install precompiled package '%s'.\n", package_name);
        }
        if (deps) free(deps);
        return;
    }

    char package_path[PATH_SIZE];
    char download_url[URL_SIZE];
    char install_script_path[PATH_SIZE];
    char local_file[PATH_SIZE];
    char cmd[BUFFER_SIZE];
    
    snprintf(package_path, sizeof(package_path), "%s/%s", REPO_PATH, package_name);
    mkdir(package_path, 0755);
    
    snprintf(download_url, sizeof(download_url), "%s/%s.tar.xz", server_url, package_name);
    
    snprintf(local_file, sizeof(local_file), "%s/%s.tar.xz", package_path, package_name);
    
    if (!network_init()) {
        printf("Error: Failed to initialize network\n");
        return;
    }

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    struct ProgressData progress = {
        .lastUpdateTime = clock() / (double)CLOCKS_PER_SEC,
        .downloadSpeed = 0,
        .estimatedTime = 0,
        .totalSize = 0,
        .downloaded = 0,
        .lastPercent = -1,
        .termWidth = w.ws_col
    };

    printf("Downloading package '%s'...\n", package_name);
    download_result_t result = download_file(download_url, local_file, network_progress_callback, &progress);
    
    network_cleanup();

    if (!result.success) {
        printf("\nError: %s\n", result.error_message);
        return;
    }
    
    printf("\nExtracting package...\n");
    
    snprintf(cmd, sizeof(cmd), "cd %s && tar xf %s.tar.xz", package_path, package_name);
    if (system(cmd) != 0) {
        printf("Failed to extract package\n");
        return;
    }

    snprintf(install_script_path, sizeof(install_script_path), 
             "%s/%s/.install", package_path, package_name);

    if (access(install_script_path, F_OK) != 0) {
        snprintf(install_script_path, sizeof(install_script_path), 
                 "%s/.install", package_path);
        if (access(install_script_path, F_OK) != 0) {
            printf("Install script not found at: %s\n", install_script_path);
            return;
        }
    }

    snprintf(cmd, sizeof(cmd), "chmod +x %s", install_script_path);
    if (system(cmd) != 0) {
        printf("Warning: Failed to make install script executable\n");
    }

    printf("\n");
    
    struct SpinnerData spinner_data = {
        .running = true,
    };
    strncpy(spinner_data.text, "Installing package...", sizeof(spinner_data.text) - 1);
    spinner_data.text[sizeof(spinner_data.text) - 1] = '\0';
    
    pthread_t spinner_thread;
    pthread_create(&spinner_thread, NULL, spinner_animation, &spinner_data);

    if (strstr(install_script_path, "/.install")) {
        char *dir = strdup(install_script_path);
        *strrchr(dir, '/') = '\0';
        snprintf(cmd, sizeof(cmd), "cd %s && ./.install >/dev/null 2>&1", dir);
        free(dir);
    } else {
        snprintf(cmd, sizeof(cmd), "cd %s && ./.install >/dev/null 2>&1", package_path);
    }

    int install_result = system(cmd);
    
    spinner_data.running = false;
    pthread_join(spinner_thread, NULL);
    
    if (install_result != 0) {
        printf("\033[1;31m✗ Installation failed\033[0m\n");
        return;
    }
    
    char marker_path[PATH_SIZE];
    char version_path[PATH_SIZE];
    
    snprintf(marker_path, sizeof(marker_path),
            "%s/share/steal/installed/%s", INSTALL_PATH, package_name);
    if (access(marker_path, F_OK) != 0) {
        FILE* marker = fopen(marker_path, "w");
        if (marker) fclose(marker);
    }
    
    snprintf(version_path, sizeof(version_path),
            "%s/share/steal/installed/%s.version", INSTALL_PATH, package_name);
    if (access(version_path, F_OK) != 0) {
        FILE* ver_file = fopen(version_path, "w");
        if (ver_file) {
            fprintf(ver_file, "%s\n", version);
            fclose(ver_file);
        }
    }
    
    snprintf(cmd, sizeof(cmd), "rm -rf %s/*", package_path);
    if (system(cmd) != 0) {
        printf("Warning: Failed to clean up package files\n");
    }
    
    snprintf(cmd, sizeof(cmd), "rm -rf %s", package_path);
    if (system(cmd) != 0) {
        printf("Warning: Failed to remove package directory\n");
    }
    
    printf("Installation completed successfully\n");
}

bool install_precompiled_package(const char* package_name) {
    char package_path[PATH_SIZE];
    char download_url[URL_SIZE];
    char local_file[PATH_SIZE];
    char cmd[BUFFER_SIZE];
    
    if (strlen(package_name) > PACKAGE_NAME_MAX) {
        printf("Error: Package name too long\n");
        return false;
    }
    
    snprintf(package_path, sizeof(package_path), "%s/%s", REPO_PATH, package_name);
    mkdir(package_path, 0755);
    
    snprintf(download_url, sizeof(download_url), "%s/%s.bin", server_url, package_name);
    
    if (!network_init()) {
        printf("Error: Failed to initialize network\n");
        return false;
    }
    
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    
    struct ProgressData progress = {
        .lastUpdateTime = clock() / (double)CLOCKS_PER_SEC,
        .downloadSpeed = 0,
        .estimatedTime = 0,
        .totalSize = 0,
        .downloaded = 0,
        .lastPercent = -1,
        .termWidth = w.ws_col
    };

    printf("Downloading precompiled package '%s'...\n", package_name);
    
    snprintf(local_file, sizeof(local_file), "%s/%s.bin", package_path, package_name);
    
    download_result_t result = download_file(download_url, local_file, network_progress_callback, &progress);
    
    network_cleanup();
    
    if (!result.success) {
        printf("\nError: %s\n", result.error_message);
        return false;
    }
    
    printf("\nExtracting precompiled package...\n");
    
    mkdir_p("%s/bin", INSTALL_PATH);
    mkdir_p("%s/lib", INSTALL_PATH);
    mkdir_p("%s/share", INSTALL_PATH);
    mkdir_p("%s/share/steal/installed", INSTALL_PATH);
    
    char metadata_dir[PATH_SIZE];
    snprintf(metadata_dir, sizeof(metadata_dir), "%s/share/steal/metadata/%s", INSTALL_PATH, package_name);
    mkdir_p("%s", metadata_dir);

    snprintf(cmd, sizeof(cmd), "cd / && tar xf '%s'", local_file);
    if (system(cmd) != 0) {
        printf("Failed to extract package\n");
        return false;
    }
    
    char metadata_path[PATH_SIZE];
    snprintf(metadata_path, sizeof(metadata_path), 
             "/usr/share/steal/metadata/%s/info", package_name);
    
    char version[64] = {0};
    char category[128] = {0};
    char description[512] = {0};
    char deps[512] = {0};
    
    FILE* metadata = fopen(metadata_path, "r");
    if (metadata) {
        char line[512];
        while (fgets(line, sizeof(line), metadata)) {
            if (strncmp(line, "version=", 8) == 0) {
                strncpy(version, line + 8, sizeof(version) - 1);
                version[strcspn(version, "\n")] = 0;
            } else if (strncmp(line, "category=", 9) == 0) {
                strncpy(category, line + 9, sizeof(category) - 1);
                category[strcspn(category, "\n")] = 0;
            } else if (strncmp(line, "description=", 12) == 0) {
                strncpy(description, line + 12, sizeof(description) - 1);
                description[strcspn(description, "\n")] = 0;
            } else if (strncmp(line, "dependencies=", 13) == 0) {
                strncpy(deps, line + 13, sizeof(deps) - 1);
                deps[strcspn(deps, "\n")] = 0;
            }
        }
        fclose(metadata);
    }
    
    char marker_path[PATH_SIZE];
    char version_path[PATH_SIZE];
    
    snprintf(marker_path, sizeof(marker_path),
            "%s/share/steal/installed/%s", INSTALL_PATH, package_name);
    FILE* marker = fopen(marker_path, "w");
    if (marker) {
        fprintf(marker, "precompiled\n");
        fclose(marker);
    }
    
    snprintf(version_path, sizeof(version_path),
            "%s/share/steal/installed/%s.version", INSTALL_PATH, package_name);
    FILE* ver_file = fopen(version_path, "w");
    if (ver_file) {
        fprintf(ver_file, "%s\n", version);
        fclose(ver_file);
    }
    
    snprintf(cmd, sizeof(cmd), "rm -f '%s'", local_file);
    system(cmd);
    
    printf("Installation completed successfully\n");
    printf("Package: %s\n", package_name);
    printf("Version: %s\n", version);
    printf("Category: %s\n", category);
    printf("Description: %s\n", description);
    if (deps[0] != '\0') {
        printf("Dependencies: %s\n", deps);
    }
    
    return true;
}

void load_config() {
    FILE *config = fopen(CONFIG_FILE, "r");
    if (config) {
        char line[512];
        while (fgets(line, sizeof(line), config)) {
            if (strncmp(line, "SERVER_URL=", 11) == 0) {
                strcpy(server_url, line + 11);
                server_url[strcspn(server_url, "\n")] = 0;
            }
        }
        fclose(config);
    }
}

void install_with_dependencies(const char* package_name) {
    if (is_package_installed(package_name)) {
        printf("Package '%s' is already installed\n", package_name);
        return;
    }

    char version[64] = {0};
    char category[128] = {0};
    char description[512] = {0};
    char* deps = NULL;
    int dep_count = 0;

    if (!get_package_info(package_name, version, category, &deps, &dep_count, description)) {
        printf("Package '%s' not found in database\n", package_name);
        return;
    }

    char* needed_deps[64] = {0};
    int needed_count = 0;

    if (deps && strcmp(deps, "-") != 0) {
        char* dep_list = strdup(deps);
        char* dep = strtok(dep_list, ",");
        
        while (dep) {
            while (*dep == ' ') dep++;
            char* end = dep + strlen(dep) - 1;
            while (end > dep && *end == ' ') end--;
            *(end + 1) = '\0';

            if (!is_package_installed(dep)) {
                needed_deps[needed_count++] = strdup(dep);
            }
            dep = strtok(NULL, ",");
        }
        free(dep_list);
    }

    if (needed_count > 0) {
        printf("\nThe following dependencies need to be installed:\n");
        for (int i = 0; i < needed_count; i++) {
            printf("  - %s\n", needed_deps[i]);
        }
        printf("\nWould you like to install %s", package_name);
        for (int i = 0; i < needed_count; i++) {
            printf(",%s", needed_deps[i]);
        }
        printf(" [Y/n]: ");

        char response = getchar();
        if (response == '\n') response = 'Y';
        else while (getchar() != '\n');

        if (response != 'Y' && response != 'y') {
            printf("Installation cancelled.\n");
            for (int i = 0; i < needed_count; i++) {
                free(needed_deps[i]);
            }
            if (deps) free(deps);
            return;
        }

        for (int i = 0; i < needed_count; i++) {
            printf("\nInstalling dependency: %s\n", needed_deps[i]);
            install_package(needed_deps[i]);
            free(needed_deps[i]);
        }
    }

    if (deps) free(deps);

    install_package(package_name);
}

bool get_package_info(const char* package_name, char* version, char* category, char** deps, int* dep_count, char* description) {
    char path[512];
    snprintf(path, sizeof(path), "%s/packages.db", REPO_PATH);
    
    FILE* db = fopen(path, "r");
    if (!db) return false;
    
    char line[1024];
    while (fgets(line, sizeof(line), db)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        char* name = strtok(line, "|");
        if (!name) continue;
        
        if (strcmp(name, package_name) == 0) {
            char* ver = strtok(NULL, "|");
            if (ver && version) strncpy(version, ver, 63);
            
            char* cat = strtok(NULL, "|");
            if (cat && category) strncpy(category, cat, 127);
            
            char* dependencies = strtok(NULL, "|");
            if (dependencies && deps && dep_count && strcmp(dependencies, "-") != 0) {
                *deps = strdup(dependencies);
                *dep_count = 1;
                for (char* p = dependencies; *p; p++) {
                    if (*p == ',') (*dep_count)++;
                }
            } else if (deps && dep_count) {
                *deps = NULL;
                *dep_count = 0;
            }
            
            char* desc = strtok(NULL, "|");
            if (desc && description) {
                desc[strcspn(desc, "\n")] = 0;
                strncpy(description, desc, 511);
            }
            
            fclose(db);
            return true;
        }
    }
    
    fclose(db);
    return false;
}

int is_package_installed(const char* package_name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/share/steal/installed/%s", INSTALL_PATH, package_name);
    return access(path, F_OK) == 0;
}

char* get_installed_version(const char* package_name) {
    char path[512];
    snprintf(path, sizeof(path), 
             "%s/share/steal/installed/%s.version", INSTALL_PATH, package_name);
    
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    
    char* version = malloc(64);
    if (fgets(version, 64, f)) {
        version[strcspn(version, "\n")] = 0;
    } else {
        free(version);
        version = NULL;
    }
    
    fclose(f);
    return version;
}

void list_installed_packages(char*** packages, int* count) {
    char path[512];
    snprintf(path, sizeof(path), "%s/share/steal/installed", INSTALL_PATH);
    
    DIR* dir = opendir(path);
    if (!dir) {
        *packages = NULL;
        *count = 0;
        return;
    }
    
    *count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char file_path[PATH_SIZE];
        struct stat st;
        snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
        
        if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
            if (strstr(entry->d_name, ".version") == NULL) {
                (*count)++;
            }
        }
    }
    
    *packages = malloc(sizeof(char*) * (*count));
    
    rewinddir(dir);
    
    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char file_path[PATH_SIZE];
        struct stat st;
        snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
        
        if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
            if (strstr(entry->d_name, ".version") == NULL) {
                (*packages)[i] = strdup(entry->d_name);
                i++;
            }
        }
    }
    
    closedir(dir);
}

void remove_package(const char* package_name) {
    if (!is_package_installed(package_name)) {
        printf("Package '%s' is not installed\n", package_name);
        return;
    }

    printf("Removing package '%s'...\n", package_name);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "rm -rf /usr/local/bin/%s /usr/bin/%s "
             "/usr/local/share/%s /usr/share/%s "
             "/usr/local/share/icons/hicolor/scalable/apps/%s.svg",
             package_name, package_name,
             package_name, package_name,
             package_name);

    int result = system(cmd);

    if (result == 0) {
        char marker_path[512];
        char version_path[512];

        snprintf(marker_path, sizeof(marker_path),
                "%s/share/steal/installed/%s", INSTALL_PATH, package_name);
        snprintf(version_path, sizeof(version_path),
                "%s/share/steal/installed/%s.version", INSTALL_PATH, package_name);

        remove(marker_path);
        remove(version_path);

        printf("Package '%s' removed successfully\n", package_name);
    } else {
        printf("Failed to remove package '%s'\n", package_name);
    }
}

void show_version() {
    printf("\n");
    printf("   \033[1;36m╭─────────────────────────────╮\033[0m\n");
    printf("   \033[1;36m│\033[0m     \033[1;35mSteal Package Manager\033[0m    \033[1;36m│\033[0m\n");
    printf("   \033[1;36m│\033[0m        \033[1;33mversion %s\033[0m        \033[1;36m│\033[0m\n", VERSION);
    printf("   \033[1;36m│\033[0m    Made for \033[1;35m%s\033[0m    \033[1;36m│\033[0m\n", OS_NAME);
    printf("   \033[1;36m│\033[0m       by \033[1;32m%s\033[0m       \033[1;36m│\033[0m\n", AUTHOR);
    printf("   \033[1;36m╰─────────────────────────────╯\033[0m\n");
    printf("\n");
}

void show_help() {
    printf("\n");
    printf("   \033[1;36m╭─────────────────────────────╮\033[0m\n");
    printf("   \033[1;36m│\033[0m     \033[1;35mSteal Package Manager\033[0m    \033[1;36m│\033[0m\n");
    printf("   \033[1;36m│\033[0m        \033[1;33mversion %s\033[0m        \033[1;36m│\033[0m\n", VERSION);
    printf("   \033[1;36m╰─────────────────────────────╯\033[0m\n\n");
    printf("  \033[1;37mUsage:\033[0m steal <command> [package]\n\n");
    printf("  \033[1;37mCommands:\033[0m\n");
    printf("    \033[1;32mupdate\033[0m                    Update package repositories\n");
    printf("    \033[1;32mupgrade\033[0m                   Upgrade installed packages\n");
    printf("    \033[1;32minstall\033[0m <pkg>             Install a package\n");
    printf("    \033[1;32mremove\033[0m  <pkg>             Remove an installed package\n");
    printf("    \033[1;32msearch\033[0m  <term>            Search for packages\n");
    printf("    \033[1;32mswitch-version\033[0m <pkg> <ver> Switch between installed versions of a package\n");
    printf("                               Examples: steal switch-version <package> <version>\n");
    printf("    \033[1;32mversion\033[0m                   Show version information\n");
    printf("    \033[1;32mhelp\033[0m                      Show this help message\n");
    printf("\n");
    printf("  \033[1;37mNote:\033[0m Most commands require root privileges. Use \033[1;33msudo steal <command>\033[0m\n");
    printf("\n");
}

void search_packages(const char* search_term) {
    char path[512];
    snprintf(path, sizeof(path), "%s/packages.db", REPO_PATH);
    
    FILE* db = fopen(path, "r");
    if (!db) {
        printf("Error: Could not open package database\n");
        printf("Try running 'steal update' first\n");
        return;
    }
    
    printf("\n \033[1;36mSearching for packages matching: \033[1;33m%s\033[0m\n\n", search_term);
    
    char line[1024];
    int found = 0;
    
    printf(" \033[1;37m%-20s %-15s %-20s %s\033[0m\n", "Package", "Version", "Category", "Description");
    printf(" \033[1;37m%-20s %-15s %-20s %s\033[0m\n", "-------", "-------", "--------", "-----------");
    
    while (fgets(line, sizeof(line), db)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        char line_copy[1024];
        strncpy(line_copy, line, sizeof(line_copy));
        
        char* name = strtok(line_copy, "|");
        if (!name) continue;
        
        if (str_case_str(name, search_term) != NULL) {
            char* version = strtok(NULL, "|");
            char* category = strtok(NULL, "|");
            char* deps = strtok(NULL, "|");
            char* description = strtok(NULL, "|");
            
            if (description) {
                description[strcspn(description, "\n")] = 0;
            } else {
                description = "No description";
            }
            
            int installed = is_package_installed(name);
            
            if (installed) {
                printf(" \033[1;32m%-20s\033[0m %-15s %-20s %s\n", name, 
                       version ? version : "unknown", 
                       category ? category : "unknown", 
                       description);
            } else {
                printf(" %-20s %-15s %-20s %s\n", name, 
                       version ? version : "unknown", 
                       category ? category : "unknown", 
                       description);
            }
            
            found++;
        }
    }
    
    fclose(db);
    
    if (found == 0) {
        printf(" \033[1;33mNo packages found matching: %s\033[0m\n", search_term);
    } else {
        printf("\n \033[1;32mFound %d package(s)\033[0m\n", found);
        printf(" \033[1;32mInstalled packages are highlighted in green\033[0m\n");
    }
    printf("\n");
}

char* str_case_str(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    
    size_t needle_len = strlen(needle);
    if (needle_len == 0) return (char*)haystack;
    
    while (*haystack) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            const char* h = haystack;
            const char* n = needle;
            size_t i = 0;
            
            while (i < needle_len && h[i] && tolower((unsigned char)h[i]) == tolower((unsigned char)n[i])) {
                i++;
            }
            
            if (i == needle_len) {
                return (char*)haystack;
            }
        }
        haystack++;
    }
    
    return NULL;
}

void mkdir_p(const char* format, const char* prefix) {
    char path[PATH_SIZE];
    snprintf(path, sizeof(path), format, prefix);
    
    char* p = path;
    while ((p = strchr(p + 1, '/')) != NULL) {
        *p = '\0';
        mkdir(path, 0755);
        *p = '/';
    }
    mkdir(path, 0755);
}

void upgrade_packages() {
    printf("Checking for updates...\n");
    
    char** installed_packages;
    int pkg_count = 0;
    list_installed_packages(&installed_packages, &pkg_count);
    
    if (pkg_count == 0) {
        printf("No installed packages found.\n");
        return;
    }
    
    int updates_available = 0;
    
    for (int i = 0; i < pkg_count; i++) {
        char version[64] = {0};
        char category[128] = {0};
        char description[512] = {0};
        char* deps = NULL;
        int dep_count = 0;
        
        if (get_package_info(installed_packages[i], version, category, &deps, &dep_count, description)) {
            char* installed_ver = get_installed_version(installed_packages[i]);
            
            if (installed_ver && strcmp(installed_ver, version) != 0) {
                printf("Update available for %s: %s -> %s\n", 
                       installed_packages[i], installed_ver, version);
                updates_available++;
                
                printf("Upgrading %s...\n", installed_packages[i]);
                install_with_dependencies(installed_packages[i]);
            }
            
            free(installed_ver);
            if (deps) free(deps);
        }
        
        free(installed_packages[i]);
    }
    
    free(installed_packages);
    
    if (updates_available == 0) {
        printf("All packages are up to date.\n");
    } else {
        printf("Upgraded %d package(s).\n", updates_available);
    }
}

void switch_version(const char* package_name, const char* version) {
    char base_package[PACKAGE_NAME_MAX] = {0};
    char version_pattern[PACKAGE_NAME_MAX] = {0};
    
    strncpy(base_package, package_name, PACKAGE_NAME_MAX - 1);
    
    if (!is_package_installed(package_name)) {
        snprintf(version_pattern, sizeof(version_pattern), "%s%s", base_package, version);
        
        if (!is_package_installed(version_pattern)) {
            char** installed_packages;
            int pkg_count = 0;
            list_installed_packages(&installed_packages, &pkg_count);
            
            printf("Looking for packages related to '%s'...\n", base_package);
            
            int found = 0;
            char* matching_packages[64] = {0};
            
            for (int i = 0; i < pkg_count; i++) {
                if (strncmp(installed_packages[i], base_package, strlen(base_package)) == 0) {
                    matching_packages[found++] = strdup(installed_packages[i]);
                }
                free(installed_packages[i]);
            }
            free(installed_packages);
            
            if (found == 0) {
                printf("Error: No packages found related to '%s'\n", base_package);
                return;
            }
            
            printf("\nFound %d related packages:\n", found);
            for (int i = 0; i < found; i++) {
                char* pkg_version = get_installed_version(matching_packages[i]);
                printf("  - %s (version: %s)\n", matching_packages[i], 
                       pkg_version ? pkg_version : "unknown");
                if (pkg_version) free(pkg_version);
            }
            
            char* target_package = NULL;
            for (int i = 0; i < found; i++) {
                if (strstr(matching_packages[i], version)) {
                    target_package = matching_packages[i];
                    break;
                }
                
                char* pkg_version = get_installed_version(matching_packages[i]);
                if (pkg_version && strcmp(pkg_version, version) == 0) {
                    target_package = matching_packages[i];
                    free(pkg_version);
                    break;
                }
                if (pkg_version) free(pkg_version);
            }
            
            if (!target_package) {
                printf("Error: No package found with version '%s'\n", version);
                printf("Please install the package first or specify a valid version\n");
                
                for (int i = 0; i < found; i++) {
                    free(matching_packages[i]);
                }
                return;
            }
            
            printf("Setting '%s' as the default version of '%s'...\n", 
                   target_package, base_package);
            
            mkdir_p("%s/bin", INSTALL_PATH);
            mkdir_p("%s/lib", INSTALL_PATH);
            mkdir_p("%s/include", INSTALL_PATH);
            
            snprintf(cmd, sizeof(cmd),
                     "rm -f %s/bin/%s %s/lib/%s* %s/include/%s*",
                     INSTALL_PATH, base_package, INSTALL_PATH, base_package, INSTALL_PATH, base_package);
            system(cmd);
            
            snprintf(cmd, sizeof(cmd),
                     "for file in $(find %s/bin -name '%s*' -type f -o -type l 2>/dev/null); do "
                     "  target_name=$(echo $(basename $file) | sed 's/%s/%s/'); "
                     "  ln -sf $file %s/bin/$target_name; "
                     "done",
                     INSTALL_PATH, target_package, target_package, base_package, INSTALL_PATH);
            system(cmd);
            
            snprintf(cmd, sizeof(cmd),
                     "for file in $(find %s/lib -name '%s*' -type f -o -type l 2>/dev/null); do "
                     "  target_name=$(echo $(basename $file) | sed 's/%s/%s/'); "
                     "  ln -sf $file %s/lib/$target_name; "
                     "done",
                     INSTALL_PATH, target_package, target_package, base_package, INSTALL_PATH);
            system(cmd);
            
            snprintf(cmd, sizeof(cmd),
                     "for file in $(find %s/include -name '%s*' -type f -o -type l 2>/dev/null); do "
                     "  target_name=$(echo $(basename $file) | sed 's/%s/%s/'); "
                     "  ln -sf $file %s/include/$target_name; "
                     "done",
                     INSTALL_PATH, target_package, target_package, base_package, INSTALL_PATH);
            system(cmd);
            
            char marker_path[PATH_SIZE];
            snprintf(marker_path, sizeof(marker_path),
                    "%s/share/steal/defaults/%s.default", INSTALL_PATH, base_package);
            
            mkdir_p("%s/share/steal/defaults", INSTALL_PATH);
            
            FILE* marker = fopen(marker_path, "w");
            if (marker) {
                fprintf(marker, "%s\n", target_package);
                fclose(marker);
                printf("Successfully set '%s' as the default for '%s'\n", 
                       target_package, base_package);
            } else {
                printf("Warning: Failed to create default marker file\n");
            }
            
            for (int i = 0; i < found; i++) {
                if (matching_packages[i] != target_package) {
                    free(matching_packages[i]);
                }
            }
            
            return;
        }
    }
    
    char* current_version = get_installed_version(package_name);
    if (!current_version) {
        printf("Error: Could not determine current version of '%s'\n", package_name);
        return;
    }
    
    if (strcmp(current_version, version) == 0) {
        printf("Package '%s' is already using version %s\n", package_name, version);
        free(current_version);
        return;
    }
    
    char version_path[PATH_SIZE];
    snprintf(version_path, sizeof(version_path), 
             "%s/share/steal/versions/%s/%s", INSTALL_PATH, package_name, version);
    
    if (access(version_path, F_OK) != 0) {
        printf("Error: Version %s of package '%s' is not installed\n", version, package_name);
        printf("Available versions:\n");
        
        char versions_dir[PATH_SIZE];
        snprintf(versions_dir, sizeof(versions_dir), 
                 "%s/share/steal/versions/%s", INSTALL_PATH, package_name);
        
        DIR* dir = opendir(versions_dir);
        if (!dir) {
            printf("  No alternative versions found\n");
            free(current_version);
            return;
        }
        
        struct dirent* entry;
        int found = 0;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            printf("  - %s%s\n", entry->d_name, 
                   strcmp(entry->d_name, current_version) == 0 ? " (current)" : "");
            found = 1;
        }
        
        if (!found) {
            printf("  No alternative versions found\n");
        }
        
        closedir(dir);
        free(current_version);
        return;
    }
    
    printf("Switching '%s' from version %s to %s...\n", package_name, current_version, version);
    
    char cmd[BUFFER_SIZE];
    
    snprintf(cmd, sizeof(cmd),
             "rm -f %s/bin/%s* %s/lib/%s* %s/include/%s*",
             INSTALL_PATH, package_name, INSTALL_PATH, package_name, INSTALL_PATH, package_name);
    system(cmd);
    
    snprintf(cmd, sizeof(cmd),
             "find %s -type f -o -type l | while read file; do "
             "target=$(echo $file | sed 's|%s/share/steal/versions/%s/%s/||'); "
             "target_dir=$(dirname %s/$target); "
             "mkdir -p $target_dir; "
             "ln -sf $file %s/$target; "
             "done",
             version_path, INSTALL_PATH, package_name, version, INSTALL_PATH, INSTALL_PATH);
    
    int result = system(cmd);
    
    if (result == 0) {
        char version_file_path[PATH_SIZE];
        snprintf(version_file_path, sizeof(version_file_path),
                 "%s/share/steal/installed/%s.version", INSTALL_PATH, package_name);
        
        FILE* f = fopen(version_file_path, "w");
        if (f) {
            fprintf(f, "%s\n", version);
            fclose(f);
            printf("Successfully switched '%s' to version %s\n", package_name, version);
        } else {
            printf("Warning: Failed to update version file\n");
        }
    } else {
        printf("Error: Failed to switch versions\n");
    }
    
    free(current_version);
} 