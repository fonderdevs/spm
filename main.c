#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/types.h>
#include <dirent.h>
#define REPO_PATH "/var/lib/steal/repos"
#define INSTALL_PATH "/usr/local"
#define PKG_DB_PATH "/var/lib/steal/pkgdb"
#define DEFAULT_SERVER_URL "http://192.168.29.150:8080/packages/"
#define CONFIG_FILE "/etc/steal/config"
#define VERSION "2.0.0"
#define AUTHOR "parkourer10"
#define OS_NAME "FonderOS"
#define BUFFER_SIZE 2048  // For general commands
#define PATH_SIZE 512     // For paths


void update_repos();
void upgrade_packages();
void install_package(const char* package_name);
void load_config();
void install_with_dependencies(const char* package_name);
bool get_package_info(const char* package_name, char* version, char* category, char** deps, int* dep_count, char* description);
int is_package_installed(const char* package_name);
char* get_installed_version(const char* package_name);
void list_installed_packages(char*** packages, int* count);
void remove_package(const char* package_name);
void show_version();
void show_help();

char server_url[PATH_SIZE] = DEFAULT_SERVER_URL;

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
        update_repos();
    } else if (strcmp(argv[1], "upgrade") == 0) {
        upgrade_packages();
    } else if (strcmp(argv[1], "install") == 0) {
        if (argc < 3) {
            printf("Error: Package name required\n");
            return 1;
        }
        install_with_dependencies(argv[2]);
    } else if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) {
            printf("Error: Package name required\n");
            return 1;
        }
        remove_package(argv[2]);
    } else {
        printf("Unknown command: %s\n", argv[1]);
        show_help();
        return 1;
    }

    return 0;
}

void update_repos() {
    printf("Updating package repositories from %s...\n", server_url);
    char cmd[BUFFER_SIZE];
    
    snprintf(cmd, sizeof(cmd), 
             "mkdir -p %s && cd %s && wget -q %s/packages.db",
             REPO_PATH, REPO_PATH, server_url);
    if (system(cmd) != 0) {
        printf("Failed to update repositories\n");
    }
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
    
    // Check each installed package for updates
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
                
                // Install the new version
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

void install_package(const char* package_name) {
    char package_path[PATH_SIZE];
    char download_cmd[BUFFER_SIZE];
    char install_script_path[PATH_SIZE];
    
    // Create package directory
    int ret = snprintf(package_path, sizeof(package_path), 
             REPO_PATH "/%s", package_name);
    if (ret < 0 || (size_t)ret >= sizeof(package_path)) {
        printf("Error: Package path too long\n");
        return;
    }
    
    // Download and extract package
    printf("Downloading package '%s'...\n", package_name);
    ret = snprintf(download_cmd, sizeof(download_cmd),
             "mkdir -p %s && cd %s && wget -q %s/%s.tar.xz && tar xf %s.tar.xz && rm %s.tar.xz",
             package_path, package_path, server_url, package_name, package_name, package_name);
    if (ret < 0 || (size_t)ret >= sizeof(download_cmd)) {
        printf("Error: Command too long\n");
        return;
    }
    
    if (system(download_cmd) != 0) {
        printf("Failed to download package '%s'\n", package_name);
        return;
    }
    
    // Check for install script
    ret = snprintf(install_script_path, sizeof(install_script_path), 
             "%s/.install", package_path);
    if (ret < 0 || (size_t)ret >= sizeof(install_script_path)) {
        printf("Error: Install script path too long\n");
        return;
    }

    if (access(install_script_path, F_OK) != 0) {
        printf("Install script not found for package '%s'\n", package_name);
        return;
    }

    // Execute install script
    char cmd[BUFFER_SIZE];
    ret = snprintf(cmd, sizeof(cmd), "cd %s && sh .install", package_path);
    if (ret < 0 || (size_t)ret >= sizeof(cmd)) {
        printf("Error: Command too long\n");
        return;
    }
    
    printf("Executing install script...\n");
    int result = system(cmd);

    if (result == 0) {
        // Create installation marker and version file
        char marker_path[PATH_SIZE];
        char version_path[PATH_SIZE];
        char version[64] = {0};
        char category[128] = {0};
        char description[512] = {0};
        char* deps = NULL;
        int dep_count = 0;
        
        // Get package version
        if (get_package_info(package_name, version, category, &deps, &dep_count, description)) {
            // Create marker directory
            ret = snprintf(marker_path, sizeof(marker_path), 
                    "%s/share/steal/installed", INSTALL_PATH);
            if (ret >= 0 && (size_t)ret < sizeof(marker_path)) {
                mkdir(marker_path, 0755);
            }
            
            // Create marker file
            ret = snprintf(marker_path, sizeof(marker_path),
                    "%s/share/steal/installed/%s", INSTALL_PATH, package_name);
            if (ret >= 0 && (size_t)ret < sizeof(marker_path)) {
                FILE* marker = fopen(marker_path, "w");
                if (marker) fclose(marker);
            }
            
            // Create version file
            ret = snprintf(version_path, sizeof(version_path),
                    "%s/share/steal/installed/%s.version", INSTALL_PATH, package_name);
            if (ret >= 0 && (size_t)ret < sizeof(version_path)) {
                FILE* ver_file = fopen(version_path, "w");
                if (ver_file) {
                    fprintf(ver_file, "%s\n", version);
                    fclose(ver_file);
                }
            }
            
            if (deps) free(deps);
        }
        
        // Cleanup ALL downloaded files including source and .install
        ret = snprintf(cmd, sizeof(cmd), "rm -rf %s/*", package_path);
        if (ret >= 0 && (size_t)ret < sizeof(cmd)) {
            if (system(cmd) != 0) {
                printf("Warning: Failed to clean up package files\n");
            }
        }
        
        ret = snprintf(cmd, sizeof(cmd), "rm -rf %s", package_path);
        if (ret >= 0 && (size_t)ret < sizeof(cmd)) {
            if (system(cmd) != 0) {
                printf("Warning: Failed to remove package directory\n");
            }
        }
        
        printf("Installation completed successfully\n");
    } else {
        printf("Installation failed with error code: %d\n", result);
        // Cleanup on failure too
        snprintf(cmd, sizeof(cmd), "rm -rf %s", package_path);
        if (system(cmd) != 0) {
            printf("Warning: Failed to clean up after failed installation\n");
        }
    }
}

void load_config() {
    FILE *config = fopen(CONFIG_FILE, "r");
    if (config) {
        char line[512];
        while (fgets(line, sizeof(line), config)) {
            if (strncmp(line, "SERVER_URL=", 11) == 0) {
                strcpy(server_url, line + 11);
                // Remove newline if present
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

    // Only process dependencies if they exist and aren't marked as "-"
    if (deps && strcmp(deps, "-") != 0) {
        char* dep = strtok(deps, ",");
        while (dep) {
            printf("Installing dependency: %s\n", dep);
            install_with_dependencies(dep);
            dep = strtok(NULL, ",");
        }
        free(deps);
    }

    // Now install the actual package
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
            // Get version
            char* ver = strtok(NULL, "|");
            if (ver) strncpy(version, ver, 63);
            
            // Get category
            char* cat = strtok(NULL, "|");
            if (cat) strncpy(category, cat, 127);
            
            // Get dependencies
            char* dependencies = strtok(NULL, "|");
            if (dependencies && strcmp(dependencies, "-") != 0) {
                *deps = strdup(dependencies);
                *dep_count = 1;
                for (char* p = dependencies; *p; p++) {
                    if (*p == ',') (*dep_count)++;
                }
            } else {
                *deps = NULL;
                *dep_count = 0;
            }
            
            // Get description
            char* desc = strtok(NULL, "|");
            if (desc) {
                // Remove newline if present
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
        // Remove newline if present
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
    
    // Count packages first
    struct dirent* entry;
    *count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // Regular file
            (*count)++;
        }
    }
    
    // Allocate array
    *packages = malloc(sizeof(char*) * (*count));
    
    // Reset directory stream
    rewinddir(dir);
    
    // Fill array
    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            (*packages)[i] = strdup(entry->d_name);
            i++;
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

    // Remove all possible locations and symlinks
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
        // Remove installation markers
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
    printf("    \033[1;32mupdate\033[0m            Update package repositories\n");
    printf("    \033[1;32mupgrade\033[0m           Upgrade installed packages\n");
    printf("    \033[1;32minstall\033[0m <pkg>     Install a package\n");
    printf("    \033[1;32mremove\033[0m  <pkg>     Remove an installed package\n");
    printf("    \033[1;32mversion\033[0m           Show version information\n");
    printf("    \033[1;32mhelp\033[0m              Show this help message\n");
    printf("\n");
} 
