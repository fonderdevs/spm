#include "../include/network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 8192
#define MAX_REDIRECTS 5
#define HTTP_PORT 80
#define HTTPS_PORT 443

static char* extract_host_from_url(const char* url, char* host, size_t host_size) {
    const char* start = strstr(url, "://");
    if (!start) return NULL;
    
    start += 3;
    const char* end = strchr(start, '/');
    if (!end) end = start + strlen(start);
    
    size_t len = end - start;
    if (len >= host_size) return NULL;
    
    strncpy(host, start, len);
    host[len] = '\0';
    return (char*)end;
}

static char* extract_path_from_url(const char* url) {
    const char* start = strstr(url, "://");
    if (!start) return NULL;
    
    start = strchr(start + 3, '/');
    if (!start) return strdup("/");
    
    return strdup(start);
}

static bool parse_http_header(const char* header, int* status_code, size_t* content_length) {
    const char* status_start = strchr(header, ' ');
    if (!status_start) return false;
    *status_code = atoi(status_start + 1);
    
    *content_length = 0;
    const char* cl_start = strstr(header, "Content-Length:");
    if (cl_start) {
        cl_start += 15;
        while (*cl_start == ' ') cl_start++;
        *content_length = strtoull(cl_start, NULL, 10);
    }
    
    return true;
}

bool network_init(void) {
    return true;
}

void network_cleanup(void) {
    //huh?
}

static download_result_t handle_download(const char* host, const char* path, const char* local_path, 
                                      progress_callback_t progress_cb, void* userdata) {
    download_result_t result = {false, ""};
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    char request[4096];
    char buffer[BUFFER_SIZE];
    FILE* fp = NULL;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rv = getaddrinfo(host, "80", &hints, &servinfo);
    if (rv != 0) {
        snprintf(result.error_message, sizeof(result.error_message), 
                "Failed to resolve host: %s", gai_strerror(rv));
        return result;
    }
    
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;
        
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        
        break;
    }
    
    if (p == NULL) {
        snprintf(result.error_message, sizeof(result.error_message), 
                "Failed to connect to host");
        freeaddrinfo(servinfo);
        return result;
    }
    
    freeaddrinfo(servinfo);
    
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: steal/%s\r\n"
             "Connection: close\r\n\r\n",
             path, host, "2.0.4");
    

    if (send(sockfd, request, strlen(request), 0) == -1) {
        snprintf(result.error_message, sizeof(result.error_message), 
                "Failed to send request");
        close(sockfd);
        return result;
    }
    
    fp = fopen(local_path, "wb");
    if (!fp) {
        snprintf(result.error_message, sizeof(result.error_message), 
                "Failed to create local file");
        close(sockfd);
        return result;
    }
    
    bool header_complete = false;
    int status_code = 0;
    size_t content_length = 0;
    size_t total_read = 0;
    char header[8192] = {0};
    size_t header_len = 0;
    
    while (1) {
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;
        
        if (!header_complete) {
            char* body_start = strstr(buffer, "\r\n\r\n");
            if (body_start) {
                header_complete = true;
                size_t header_part = body_start - buffer + 4;
                    
                
                memcpy(header + header_len, buffer, header_part);
                header_len += header_part;
                header[header_len] = '\0';
                
               
                if (!parse_http_header(header, &status_code, &content_length)) {
                    snprintf(result.error_message, sizeof(result.error_message), 
                            "Failed to parse HTTP header");
                    break;
                }
                
                if (status_code != 200) {
                    snprintf(result.error_message, sizeof(result.error_message), 
                            "HTTP error: %d", status_code);
                    break;
                }
                
              
                size_t body_len = bytes - header_part;
                if (body_len > 0) {
                    fwrite(body_start + 4, 1, body_len, fp);
                    total_read += body_len;
                }
            } else {
                memcpy(header + header_len, buffer, bytes);
                header_len += bytes;
            }
        } else {
            fwrite(buffer, 1, bytes, fp);
            total_read += bytes;
        }
        
        if (progress_cb && content_length > 0) {
            progress_cb(content_length, total_read, userdata);
        }
    }
    
    fclose(fp);
    close(sockfd);
    
    if (status_code == 200) {
        result.success = true;
    }
    
    return result;
}

download_result_t download_file(const char* url, const char* local_path, 
                              progress_callback_t progress_cb, void* userdata) {
    download_result_t result = {false, ""};
    char host[256];
    
    char* path = extract_path_from_url(url);
    if (!path || !extract_host_from_url(url, host, sizeof(host))) {
        snprintf(result.error_message, sizeof(result.error_message), 
                "Invalid URL format");
        free(path);
        return result;
    }
    
    result = handle_download(host, path, local_path, progress_cb, userdata);
    free(path);
    return result;
}

bool is_url_reachable(const char* url) {
    char host[256];
    struct addrinfo hints, *servinfo;
    
    if (!extract_host_from_url(url, host, sizeof(host))) {
        return false;
    }
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(host, "80", &hints, &servinfo) != 0) {
        return false;
    }
    
    freeaddrinfo(servinfo);
    return true;
} 