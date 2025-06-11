#ifndef spm_COMMON_H
#define spm_COMMON_H

#include <dirent.h>
#include <sys/types.h>

#define REPO_PATH "/var/lib/spm/repos"
#define INSTALL_PATH "/usr/local"
#define PKG_DB_PATH "/var/lib/spm/pkgdb"
#define DEFAULT_SERVER_URL "https://ftp.fonders.org/packages/"
#define SECONDARY_SERVER_URL "https://ftp2.fonders.org/packages/"
#define CONFIG_FILE "/etc/spm/config"
#define VERSION "2.0.5" //sperm
#define AUTHOR "parkourer10"
#define OS_NAME "FonderOS"

#define BUFFER_SIZE 2048     
#define PATH_SIZE 1024      
#define URL_SIZE 2048       
#define PACKAGE_NAME_MAX 64 

#define KB (1024)
#define MB (1024 * KB)
#define GB (1024 * MB)
#define TB (1024 * GB) //lmaooooo, but?

#define PRECOMPILED_FLAG "precompiled"
#define MIRRORS_FILE "/etc/spm/mirrors.txt"
#define DEFAULT_MIRROR_URL "https://ftp.fonders.org/packages"
#define MAX_MIRRORS 32
#define MIRROR_URL_SIZE 512
#define MIRROR_TIMESTAMP_FILE "/etc/spm/mirror.last"

typedef struct {
    char url[MIRROR_URL_SIZE];
    char location[64];
    char description[256];
    double speed;
} mirror_t;

#endif