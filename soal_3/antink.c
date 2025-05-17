#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
#include <time.h>

#define BASE_DIR "/it24_host"
#define LOG_FILE "/var/log/it24.log"

int is_malicious(const char *filename) {
    return strstr(filename, "nafis") || strstr(filename, "kimcun");
}

void get_current_time(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "[%Y-%m-%d %H:%M:%S]", tm_info);
}

void write_log(const char *type, const char *message) {
    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        char timestamp[64];
        get_current_time(timestamp, sizeof(timestamp));
        fprintf(log, "%s [%s] %s\n", timestamp, type, message);
        fclose(log);
    }
}

// ref: https://www.geeksforgeeks.org/reverse-string-in-c/
void reverse_string(char *content) {
    int l = 0;
    int r = strlen(content) - 1;
    char t;

    while (l < r) {
        t = content[l];
        content[l] = content[r];
        content[r] = t;

        l++;
        r--;
    }
}

// ref: https://gist.github.com/Ztuu/e9106e9095422a7d7266653f1e156366
char *rot13(const char *src)
{
    if(src == NULL){
      return NULL;
    }
  
    char* result = malloc(strlen(src)+1);
    
    if(result != NULL){
      strcpy(result, src);
      char* current_char = result;
      
      while(*current_char != '\0'){
        if((*current_char >= 97 && *current_char <= 122) || (*current_char >= 65 && *current_char <= 90)){
          if(*current_char > 109 || (*current_char > 77 && *current_char < 91)){
            *current_char -= 13;
          }else{
            *current_char += 13;
          }
        }
        current_char++;
      }
    }
    return result;
}

static int antink_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%s", BASE_DIR, path);
    
    int res = lstat(full_path, stbuf);
    if (res == -1) return -errno;
    
    return 0;
}

static int antink_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%s", BASE_DIR, path);
    
    DIR *dp = opendir(full_path);
    if (!dp) return -errno;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        char display_name[256];
        strcpy(display_name, de->d_name);
        
        if (is_malicious(de->d_name)) {
            reverse_string(display_name);
            char log_msg[512];
            snprintf(log_msg, sizeof(log_msg), "Reversed malicious filename: %s -> %s", 
                    de->d_name, display_name);
            write_log("ALERT", log_msg);
        }

        filler(buf, display_name, &st, 0, 0);
    }

    closedir(dp);
    return 0;
}

static int antink_open(const char *path, struct fuse_file_info *fi) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%s", BASE_DIR, path);
    
    int fd = open(full_path, fi->flags);
    if (fd == -1) return -errno;
    
    close(fd);
    return 0;
}

static int antink_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%s", BASE_DIR, path);
    
    int fd = open(full_path, O_RDONLY);
    if (fd == -1) return -errno;

    char *content = malloc(size + 1);
    if (!content) {
        close(fd);
        return -ENOMEM;
    }
    
    int res = pread(fd, content, size, offset);
    if (res == -1) {
        free(content);
        close(fd);
        return -errno;
    }
    content[res] = '\0';

    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    
    if (is_malicious(filename)) {
        char log_msg[512];
        snprintf(log_msg, sizeof(log_msg), "Accessed malicious file: %s (content not encrypted)", filename);
        write_log("ALERT", log_msg);
        
        memcpy(buf, content, res);
    } else {
        char *rot13_content = rot13(content);
        if (rot13_content) {
            char log_msg[512];
            snprintf(log_msg, sizeof(log_msg), "Encrypted normal file: %s", filename);
            write_log("ENCRYPT", log_msg);
            
            memcpy(buf, rot13_content, strlen(rot13_content));
            free(rot13_content);
            res = strlen(rot13_content);
        } else {
            memcpy(buf, content, res);
        }
    }
    
    free(content);
    close(fd);
    return res;
}

static struct fuse_operations antink_oper = {
    .getattr = antink_getattr,
    .readdir = antink_readdir,
    .open = antink_open,
    .read = antink_read,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point>\n", argv[0]);
        return 1;
    }

    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        char timestamp[64];
        get_current_time(timestamp, sizeof(timestamp));
        fprintf(log, "%s [SYSTEM] AntiNK FUSE filesystem starting\n", timestamp);
        fclose(log);
    }

    char *fuse_argv[] = { argv[0], "-f", argv[2] };
    int fuse_argc = 3;

    return fuse_main(fuse_argc, fuse_argv, &antink_oper, NULL);
}