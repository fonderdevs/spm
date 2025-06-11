#ifndef spm_PACKAGE_H
#define spm_PACKAGE_H

#include <stdbool.h>

//functions lol
void update_repos(void);
void upgrade_packages(void);
void install_package(const char* package_name);
void load_config(void);
void install_with_dependencies(const char* package_name);
bool get_package_info(const char* package_name, char* version, char* category, char** deps, int* dep_count, char* description);
int is_package_installed(const char* package_name);
char* get_installed_version(const char* package_name);
void list_installed_packages(char*** packages, int* count);
void remove_package(const char* package_name);
void show_version(void);
void show_help(void);
void search_packages(const char* search_term);
char* str_case_str(const char* haystack, const char* needle);
void mkdir_p(const char* format, const char* prefix);
bool install_precompiled_package(const char* package_name);
void switch_version(const char* package_name, const char* version);

#endif  