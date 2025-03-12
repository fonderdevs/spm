#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <curl/curl.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <ctype.h>
#define REPO_PATH "/var/lib/steal/repos"
#define INSTALL_PATH "/usr/local"
#define PKG_DB_PATH "/var/lib/steal/pkgdb"
#define DEFAULT_SERVER_URL "http://192.168.29.150:8080/packages/"
#define CONFIG_FILE "/etc/steal/config"
#define VERSION "2.0.3"
#define AUTHOR "parkourer10"
#define OS_NAME "FonderOS"
#define BUFFER_SIZE 2048     // For general commands
#define PATH_SIZE 1024       // Increased for paths
#define URL_SIZE 2048        // For URLs
#define PACKAGE_NAME_MAX 64  // Maximum package name length
#define KB (1024)
#define MB (1024 * KB)
#define GB (1024 * MB)
#define PRECOMPILED_FLAG "precompiled" // Flag to indicate precompiled packages

// Define DT_REG if not available
#ifndef DT_REG
#define DT_REG 8
#endif

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
    char text[256];  // Changed from const char* to fixed buffer
};

void* spinner_animation(void* arg) {
    struct SpinnerData* data = (struct SpinnerData*)arg;
    const char* frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    int frame = 0;
    
    while (data->running) {
        printf("\r\033[K\033[1;36m%s\033[0m %s", frames[frame], data->text);
        fflush(stdout);
        frame = (frame + 1) % 10;
        usleep(80000); // 80ms delay
    }
    printf("\r\033[K"); // Clear line when done
    return NULL;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

static void print_size(double size) {
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

static int progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow) {
    struct ProgressData *progress = (struct ProgressData *)clientp;
    (void)ultotal; (void)ulnow;  // Unused parameters
    
    double currentTime = clock() / (double)CLOCKS_PER_SEC;
    double timeDiff = currentTime - progress->lastUpdateTime;
    
    if (timeDiff >= 0.1 || dlnow == dltotal) {  // Update every 0.1 seconds or at completion
        progress->totalSize = dltotal;
        progress->downloaded = dlnow;
        
        // Calculate speed and ETA
        progress->downloadSpeed = (dlnow - progress->downloaded) / timeDiff;
        if (progress->downloadSpeed > 0) {
            progress->estimatedTime = (dltotal - dlnow) / progress->downloadSpeed;
        }
        
        // Calculate percentage - protect against division by zero
        int percent = 0;
        if (dltotal > 0) {
            percent = (int)((dlnow / dltotal) * 100);
        }
        
        // Only redraw if percentage changed
        if (percent != progress->lastPercent) {
            // Clear line and move cursor to start
            printf("\r\033[K");
            
            // Print progress bar
            int barWidth = progress->termWidth - 50;  // Reserve space for text
            if (barWidth < 10) barWidth = 10;  // Ensure minimum width
            
            int filledWidth = 0;
            if (dltotal > 0) {
                filledWidth = (int)((dlnow / dltotal) * barWidth);
            }
            
            // Ensure filledWidth is within valid range
            if (filledWidth < 0) filledWidth = 0;
            if (filledWidth > barWidth) filledWidth = barWidth;
            
            printf("[");
            for (int i = 0; i < barWidth; i++) {
                if (i < filledWidth) printf("=");
                else if (i == filledWidth) printf(">");
                else printf(" ");
            }
            printf("] %3d%% ", percent);
            
            // Print size info
            print_size(dlnow);
            printf("/");
            if (dltotal > 0) {
                print_size(dltotal);
            } else {
                printf("?");
            }
            
            // Print speed and ETA
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
    return 0;
}

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
void search_packages(const char* search_term);
char* str_case_str(const char* haystack, const char* needle);
void mkdir_p(const char* format, const char* prefix);
bool install_precompiled_package(const char* package_name);

char server_url[PATH_SIZE] = DEFAULT_SERVER_URL;

bool check_executable_exists(const char* name) {
    char cmd[BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", name);
    return system(cmd) == 0;
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
    } else if (strcmp(argv[1], "search") == 0) {
        if (argc < 3) {
            printf("Error: Search term required\n");
            return 1;
        }
        search_packages(argv[2]);
    } else {
        printf("Unknown command: %s\n", argv[1]);
        show_help();
        return 1;
    }

    return 0;
}

void update_repos() {
    printf("Updating package repositories from %s...\n", server_url);
    
    // First ensure the repo directory exists
    char cmd[BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", REPO_PATH);
    if (system(cmd) != 0) {
        printf("Error: Failed to create repository directory\n");
        return;
    }

    // Initialize CURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        printf("Error: Failed to initialize CURL\n");
        return;
    }

    // Set up the URL for packages.db
    char db_url[URL_SIZE];
    snprintf(db_url, sizeof(db_url), "%s/packages.db", server_url);
    
    // Open file to write
    char db_path[PATH_SIZE];
    snprintf(db_path, sizeof(db_path), "%s/packages.db", REPO_PATH);
    FILE *fp = fopen(db_path, "wb");
    if (!fp) {
        printf("Error: Failed to open %s for writing\n", db_path);
        printf("Maybe run as sudo or root?\n");
        curl_easy_cleanup(curl);
        return;
    }

    // Set up CURL options
    curl_easy_setopt(curl, CURLOPT_URL, db_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    
    // Perform the download
    CURLcode res = curl_easy_perform(curl);
    fclose(fp);

    if (res != CURLE_OK) {
        printf("Error: Failed to download packages.db: %s\n", curl_easy_strerror(res));
        printf("URL attempted: %s\n", db_url);
        // Remove potentially incomplete file
        remove(db_path);
        curl_easy_cleanup(curl);
        return;
    }

    curl_easy_cleanup(curl);
    
    // Verify the file was downloaded and is readable
    FILE *test = fopen(db_path, "r");
    if (!test) {
        printf("Error: packages.db was downloaded but cannot be opened\n");
        return;
    }
    
    // Read first line to verify it's a valid database file
    char line[1024];
    if (!fgets(line, sizeof(line), test)) {
        printf("Error: packages.db appears to be empty\n");
        fclose(test);
        return;
    }
    fclose(test);

    printf("Successfully updated package database\n");
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

bool install_precompiled_package(const char* package_name) {
    char package_path[PATH_SIZE];
    char download_url[URL_SIZE];
    char local_file[PATH_SIZE];
    char cmd[BUFFER_SIZE];
    
    // Add length check at the start
    if (strlen(package_name) > PACKAGE_NAME_MAX) {
        printf("Error: Package name too long\n");
        return false;
    }
    
    // Create package directory
    int ret = snprintf(package_path, sizeof(package_path), REPO_PATH "/%s", package_name);
    if (ret < 0 || (size_t)ret >= sizeof(package_path)) {
        printf("Error: Package path too long\n");
        return false;
    }
    mkdir(package_path, 0755);
    
    // Set up download URL for precompiled package
    ret = snprintf(download_url, sizeof(download_url), "%s/%s.bin", server_url, package_name);
    if (ret < 0 || (size_t)ret >= sizeof(download_url)) {
        printf("Error: Download URL too long\n");
        return false;
    }
    
    // Initialize CURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        printf("Failed to initialize download\n");
        return false;
    }
    
    // Get terminal width for progress bar
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    
    // Set up progress data
    struct ProgressData progress = {
        .lastUpdateTime = clock() / (double)CLOCKS_PER_SEC,
        .downloadSpeed = 0,
        .estimatedTime = 0,
        .totalSize = 0,
        .downloaded = 0,
        .lastPercent = -1,
        .termWidth = w.ws_col
    };

    // Download the precompiled package
    printf("Downloading precompiled package '%s'...\n", package_name);
    
    // Set up local file path
    ret = snprintf(local_file, sizeof(local_file), "%s/%s.bin", package_path, package_name);
    if (ret < 0 || (size_t)ret >= sizeof(local_file)) {
        printf("Error: Local file path too long\n");
        curl_easy_cleanup(curl);
        return false;
    }
    FILE *fp = fopen(local_file, "wb");
    if (!fp) {
        printf("Error: Could not create local file\n");
        curl_easy_cleanup(curl);
        return false;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, download_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &progress);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        printf("\nFailed to download precompiled package: %s\n", curl_easy_strerror(res));
        return false;
    }
    
    printf("\nExtracting precompiled package...\n");
    
    // Create temporary extraction directory
    char temp_dir[PATH_SIZE];
    snprintf(temp_dir, sizeof(temp_dir), "%s/temp_%s", package_path, package_name);
    
    // Remove any existing temp directory first
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", temp_dir);
    system(cmd);
    
    // Create fresh temp directory
    mkdir(temp_dir, 0755);
    
    // Extract package
    snprintf(cmd, sizeof(cmd), "cd '%s' && tar xf '%s.bin' -C '%s' 2>/dev/null", 
             package_path, package_name, temp_dir);
    
    if (system(cmd) != 0) {
        printf("Failed to extract package\n");
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", temp_dir);
        system(cmd);
        return false;
    }
    
    // Create necessary directories
    mkdir_p("%s/bin", INSTALL_PATH);
    mkdir_p("%s/lib", INSTALL_PATH);
    mkdir_p("%s/share", INSTALL_PATH);
    mkdir_p("%s/share/steal/installed", INSTALL_PATH);
    
    // Move files to their destinations
    printf("Installing files...\n");
    
    // First try to copy everything from usr/ if it exists
    snprintf(cmd, sizeof(cmd), "if [ -d '%s/usr' ]; then cp -r '%s/usr/'* '%s/'; else cp -r '%s/'* '%s/'; fi",
             temp_dir, temp_dir, INSTALL_PATH, temp_dir, INSTALL_PATH);
    
    if (system(cmd) != 0) {
        printf("Failed to install package files\n");
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", temp_dir);
        system(cmd);
        return false;
    }
    
    // Read metadata from the package
    char metadata_path[PATH_SIZE];
    snprintf(metadata_path, sizeof(metadata_path), 
             "%s/usr/share/steal/metadata/%s/info", temp_dir, package_name);
    
    char version[64] = {0};
    char category[128] = {0};
    char description[512] = {0};
    
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
            }
        }
        fclose(metadata);
    }
    
    // Create installation marker and version file
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
    
    // Cleanup
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' '%s/%s.bin'", temp_dir, package_path, package_name);
    system(cmd);
    
    printf("Installation completed successfully\n");
    printf("Package: %s\n", package_name);
    printf("Version: %s\n", version);
    printf("Category: %s\n", category);
    printf("Description: %s\n", description);
    
    return true;
}

// Helper function to create directory tree
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

void install_package(const char* package_name) {
    // Add length check at the start
    if (strlen(package_name) > PACKAGE_NAME_MAX) {
        printf("Error: Package name too long\n");
        return;
    }

    // First get package info to show size
    char version[64] = {0};
    char category[128] = {0};
    char description[512] = {0};
    char* deps = NULL;
    int dep_count = 0;
    
    if (!get_package_info(package_name, version, category, &deps, &dep_count, description)) {
        printf("Failed to get package info for '%s'\n", package_name);
        return;
    }
    
    // Show package info and ask for confirmation
    printf("\nPackage: %s\n", package_name);
    printf("Version: %s\n", version);
    printf("Category: %s\n", category);
    printf("Description: %s\n\n", description);
    
    // Check if this is a precompiled package
    bool is_precompiled = (strstr(category, PRECOMPILED_FLAG) != NULL);
    if (is_precompiled) {
        printf("This is a precompiled package.\n");
    }
    
    // Ask for confirmation
    printf("Do you want to install this package? [Y/n] ");
    char response = getchar();
    if (response == '\n') response = 'Y';  // Default to yes
    else while (getchar() != '\n');  // Clear input buffer
    
    if (response != 'Y' && response != 'y') {
        printf("Installation cancelled.\n");
        if (deps) free(deps);
        return;
    }

    // If precompiled, use the specialized installation function
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
    char download_url[URL_SIZE];  // Use larger buffer for URLs
    char install_script_path[PATH_SIZE];
    char local_file[PATH_SIZE];
    char cmd[BUFFER_SIZE];
    
    // Create package directory with length check
    int ret = snprintf(package_path, sizeof(package_path), REPO_PATH "/%s", package_name);
    if (ret < 0 || (size_t)ret >= sizeof(package_path)) {
        printf("Error: Package path too long\n");
        return;
    }
    mkdir(package_path, 0755);
    
    // Set up download URL with length check
    ret = snprintf(download_url, sizeof(download_url), "%s/%s.tar.xz", server_url, package_name);
    if (ret < 0 || (size_t)ret >= sizeof(download_url)) {
        printf("Error: Download URL too long\n");
        return;
    }
    
    // Initialize CURL
    CURL *curl = curl_easy_init();
    if (!curl) {
        printf("Failed to initialize download\n");
        return;
    }
    
    // Get terminal width for progress bar
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    
    // Set up progress data
    struct ProgressData progress = {
        .lastUpdateTime = clock() / (double)CLOCKS_PER_SEC,
        .downloadSpeed = 0,
        .estimatedTime = 0,
        .totalSize = 0,
        .downloaded = 0,
        .lastPercent = -1,
        .termWidth = w.ws_col
    };

    // Download the package
    printf("Downloading package '%s'...\n", package_name);
    
    // Set up local file path with length check
    ret = snprintf(local_file, sizeof(local_file), "%s/%s.tar.xz", package_path, package_name);
    if (ret < 0 || (size_t)ret >= sizeof(local_file)) {
        printf("Error: Local file path too long\n");
        return;
    }
    FILE *fp = fopen(local_file, "wb");
    
    curl_easy_setopt(curl, CURLOPT_URL, download_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &progress);
    
    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        printf("\nFailed to download package: %s\n", curl_easy_strerror(res));
        return;
    }
    
    printf("\nExtracting package...\n");
    
    // Just extract the package
    snprintf(cmd, sizeof(cmd), "cd %s && tar xf %s.tar.xz", package_path, package_name);
    if (system(cmd) != 0) {
        printf("Failed to extract package\n");
        return;
    }

    // Check for install script in the extracted directory
    ret = snprintf(install_script_path, sizeof(install_script_path), 
             "%s/%s/.install", package_path, package_name);
    if (ret < 0 || (size_t)ret >= sizeof(install_script_path)) {
        printf("Error: Install script path too long\n");
        return;
    }

    // Try the first path (package_name/.install)
    if (access(install_script_path, F_OK) != 0) {
        // If not found, try alternate path (.install)
        ret = snprintf(install_script_path, sizeof(install_script_path), 
                 "%s/.install", package_path);
        if (ret < 0 || (size_t)ret >= sizeof(install_script_path)) {
            printf("Error: Install script path too long\n");
            return;
        }
        if (access(install_script_path, F_OK) != 0) {
            printf("Install script not found at: %s\n", install_script_path);
            return;
        }
    }

    // Make install script executable
    snprintf(cmd, sizeof(cmd), "chmod +x %s", install_script_path);
    if (system(cmd) != 0) {
        printf("Warning: Failed to make install script executable\n");
    }

    printf("\n");  // Add a newline before starting animation
    
    // Set up the spinner
    struct SpinnerData spinner_data = {
        .running = true,
    };
    strncpy(spinner_data.text, "Installing package...", sizeof(spinner_data.text) - 1);
    spinner_data.text[sizeof(spinner_data.text) - 1] = '\0';
    
    // Start spinner animation in a separate thread
    pthread_t spinner_thread;
    pthread_create(&spinner_thread, NULL, spinner_animation, &spinner_data);

    // Execute install script from the directory where it was found, redirecting output
    if (strstr(install_script_path, "/.install")) {
        char *dir = strdup(install_script_path);
        *strrchr(dir, '/') = '\0';  // Remove /.install to get directory
        snprintf(cmd, sizeof(cmd), "cd %s && ./.install >/dev/null 2>&1", dir);
        free(dir);
    } else {
        snprintf(cmd, sizeof(cmd), "cd %s && ./.install >/dev/null 2>&1", package_path);
    }

    int install_result = system(cmd);
    
    // Stop the spinner
    spinner_data.running = false;
    pthread_join(spinner_thread, NULL);
    
    if (install_result != 0) {
        printf("\033[1;31m✗ Installation failed\033[0m\n");
        return;
    }
    
    // Create installation marker and version file
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
    
    // Cleanup ALL downloaded files including source and .install
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

    // Create arrays to track needed dependencies
    char* needed_deps[64] = {0};  // Store packages that need to be installed
    int needed_count = 0;

    // Only process dependencies if they exist and aren't marked as "-"
    if (deps && strcmp(deps, "-") != 0) {
        char* dep_list = strdup(deps);  // Make a copy since strtok modifies the string
        char* dep = strtok(dep_list, ",");
        
        while (dep) {
            // Trim whitespace
            while (*dep == ' ') dep++;
            char* end = dep + strlen(dep) - 1;
            while (end > dep && *end == ' ') end--;
            *(end + 1) = '\0';

            // Check if dependency is already satisfied (either installed by package manager or exists in system)
            if (!is_package_installed(dep) && !check_executable_exists(dep)) {
                needed_deps[needed_count++] = strdup(dep);
            }
            dep = strtok(NULL, ",");
        }
        free(dep_list);
    }

    // If we have dependencies to install, prompt the user
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
        if (response == '\n') response = 'Y';  // Default to yes
        else while (getchar() != '\n');  // Clear input buffer

        if (response != 'Y' && response != 'y') {
            printf("Installation cancelled.\n");
            // Clean up
            for (int i = 0; i < needed_count; i++) {
                free(needed_deps[i]);
            }
            if (deps) free(deps);
            return;
        }

        // Install needed dependencies
        for (int i = 0; i < needed_count; i++) {
            printf("\nInstalling dependency: %s\n", needed_deps[i]);
            install_package(needed_deps[i]);
            free(needed_deps[i]);
        }
    }

    if (deps) free(deps);

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
            if (ver && version) strncpy(version, ver, 63);
            
            // Get category
            char* cat = strtok(NULL, "|");
            if (cat && category) strncpy(category, cat, 127);
            
            // Get dependencies
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
            
            // Get description
            char* desc = strtok(NULL, "|");
            if (desc && description) {
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
        // Skip . and .. directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check if it's a regular file using stat instead of d_type
        char file_path[PATH_SIZE];
        struct stat st;
        snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
        
        if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
            // Skip .version files
            if (strstr(entry->d_name, ".version") == NULL) {
                (*count)++;
            }
        }
    }
    
    // Allocate array
    *packages = malloc(sizeof(char*) * (*count));
    
    // Reset directory stream
    rewinddir(dir);
    
    // Fill array
    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check if it's a regular file using stat instead of d_type
        char file_path[PATH_SIZE];
        struct stat st;
        snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
        
        if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
            // Skip .version files
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
    printf("    \033[1;32msearch\033[0m  <term>    Search for packages\n");
    printf("    \033[1;32mversion\033[0m           Show version information\n");
    printf("    \033[1;32mhelp\033[0m              Show this help message\n");
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
    
    // Table header
    printf(" \033[1;37m%-20s %-15s %-20s %s\033[0m\n", "Package", "Version", "Category", "Description");
    printf(" \033[1;37m%-20s %-15s %-20s %s\033[0m\n", "-------", "-------", "--------", "-----------");
    
    while (fgets(line, sizeof(line), db)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        // Make a copy of the line for processing
        char line_copy[1024];
        strncpy(line_copy, line, sizeof(line_copy));
        
        char* name = strtok(line_copy, "|");
        if (!name) continue;
        
        // Check if search term is in name
        if (str_case_str(name, search_term) != NULL) {
            char* version = strtok(NULL, "|");
            char* category = strtok(NULL, "|");
            char* deps = strtok(NULL, "|");
            char* description = strtok(NULL, "|");
            
            // Remove newline if present in description
            if (description) {
                description[strcspn(description, "\n")] = 0;
            } else {
                description = "No description";
            }
            
            // Check if package is installed
            int installed = is_package_installed(name);
            
            // Color for installed packages
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

// Case-insensitive substring search
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
