#ifndef STEAL_COMMON_H
#define STEAL_COMMON_H

#include <dirent.h>
#include <sys/types.h>

#define REPO_PATH "/var/lib/steal/repos"
#define INSTALL_PATH "/usr/local"
#define PKG_DB_PATH "/var/lib/steal/pkgdb"
#define DEFAULT_SERVER_URL "https://ftp.fonders.org/packages/"
#define CONFIG_FILE "/etc/steal/config"
#define VERSION "2.0.4"
#define AUTHOR "parkourer10"
#define OS_NAME "FonderOS"

#define BUFFER_SIZE 2048     
#define PATH_SIZE 1024      
#define URL_SIZE 2048       
#define PACKAGE_NAME_MAX 64 

#define KB (1024)
#define MB (1024 * KB)
#define GB (1024 * MB)

#define PRECOMPILED_FLAG "precompiled"

#endif 