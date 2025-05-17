#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h> 
#include <time.h>   

const char *BAYMAX_FILE = "Baymax.jpeg";
const char *RELICS_DIR = "relics/";
const char *LOG_FILE = "activity.log";
const int CHUNK_SIZE = 1024; 


void log_activity(const char *format, ...) {
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S] ", tm);

        fprintf(log_file, "%s", timestamp);

        va_list args;
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);

        fprintf(log_file, "\n");
        fclose(log_file);
    }
}


int get_num_chunks(size_t file_size) {
    return (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
}


void get_chunk_filename(const char *filename, int chunk_num, char *chunk_filename) {
    sprintf(chunk_filename, "%s.%03d", filename, chunk_num);
}


void get_relics_path(const char *filename, char *relics_path) {
    sprintf(relics_path, "%s%s", RELICS_DIR, filename);
}


int is_chunk_file(const char *filename) {
    return strstr(filename, ".00") != NULL;
}


static int baymax_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(path + 1, BAYMAX_FILE) == 0) {
        // Calculate total size from chunks
        long total_size = 0;
        for (int i = 0; ; i++) {
            char chunk_filename[256];
            get_chunk_filename(BAYMAX_FILE, i, chunk_filename);
            char relics_path[256];
            get_relics_path(chunk_filename, relics_path);

            struct stat chunk_st;
            if (stat(relics_path, &chunk_st) == -1) {
                break; // Stop if chunk doesn't exist
            }
            total_size += chunk_st.st_size;
        }
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = total_size;
    } else {
        return -ENOENT;
    }
    return 0;
}

static int baymax_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    if (strcmp(path, "/") == 0) {
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        filler(buf, BAYMAX_FILE, NULL, 0, 0);
    } else {
        return -ENOENT;
    }
    return 0;
}

static int baymax_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path + 1, BAYMAX_FILE) != 0) {
        return -ENOENT;
    }
    log_activity("READ: %s", BAYMAX_FILE);
    return 0;
}

static int baymax_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)fi;

    if (strcmp(path + 1, BAYMAX_FILE) != 0) {
        return -ENOENT;
    }

    int total_read = 0;
    int chunk_num = offset / CHUNK_SIZE;
    int chunk_offset = offset % CHUNK_SIZE;
    size_t bytes_to_read = size;

    while (bytes_to_read > 0) {
        char chunk_filename[256];
        get_chunk_filename(BAYMAX_FILE, chunk_num, chunk_filename);
        char relics_path[256];
        get_relics_path(chunk_filename, relics_path);

        FILE *chunk_file = fopen(relics_path, "rb");
        if (!chunk_file) {
            break; 
        }

        fseek(chunk_file, chunk_offset, SEEK_SET);
        size_t read_size = (bytes_to_read > (CHUNK_SIZE - chunk_offset)) ? (CHUNK_SIZE - chunk_offset) : bytes_to_read;
        size_t bytes_read = fread(buf + total_read, 1, read_size, chunk_file);
        fclose(chunk_file);

        if (bytes_read == 0) {
            break; // End of file or error
        }

        total_read += bytes_read;
        bytes_to_read -= bytes_read;
        chunk_num++;
        chunk_offset = 0; // Reset chunk offset for subsequent chunks
    }

    return total_read;
}

static int baymax_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void)mode;
    (void)fi;

    char filename[256];
    strcpy(filename, path + 1); // Remove leading '/'

    char relics_path[256];
    get_relics_path(filename, relics_path);

    FILE *new_file = fopen(relics_path, "wb"); // Create the first chunk
    if (!new_file) {
        return -EIO;
    }
    fclose(new_file);

    log_activity("CREATE: %s", filename);
    return 0;
}

static int baymax_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)offset;
    (void)fi;

    char filename[256];
    strcpy(filename, path + 1); // Remove leading '/'

    int num_chunks = get_num_chunks(size);
    log_activity("WRITE: %s -> ", filename);

    int total_written = 0;
    for (int i = 0; i < num_chunks; i++) {
        char chunk_filename[256];
        get_chunk_filename(filename, i, chunk_filename);
        char relics_path[256];
        get_relics_path(chunk_filename, relics_path);

        FILE *chunk_file = fopen(relics_path, "wb");
        if (!chunk_file) {
            return -EIO;
        }

        size_t write_size = (size > CHUNK_SIZE) ? CHUNK_SIZE : size;
        fwrite(buf + total_written, 1, write_size, chunk_file);
        fclose(chunk_file);

        total_written += write_size;
        size -= write_size;

        fprintf(stderr, "%s, ", chunk_filename); // Log chunk names
    }
    fprintf(stderr, "\n");
    return total_written;
}

static int baymax_unlink(const char *path) {
    char filename[256];
    strcpy(filename, path + 1); // Remove leading '/'

    log_activity("DELETE: %s", filename);

    for (int i = 0; ; i++) {
        char chunk_filename[256];
        get_chunk_filename(filename, i, chunk_filename);
        char relics_path[256];
        get_relics_path(chunk_filename, relics_path);

        if (unlink(relics_path) == -1) {
            break; // Stop if unlink fails (file doesn't exist)
        }
        log_activity("DELETE: %s", chunk_filename);
    }
    return 0;
}

static int baymax_rename(const char *from, const char *to, unsigned int flags) {
    if (flags) return -EINVAL;

    char from_filename[256];
    strcpy(from_filename, from + 1);
    char to_filename[256];
    strcpy(to_filename, to + 1);

    log_activity("RENAME: %s to %s", from_filename, to_filename);

    
    int i = 0;
    while (1) {
        char from_chunk_filename[256];
        get_chunk_filename(from_filename, i, from_chunk_filename);
        char from_relics_path[256];
        get_relics_path(from_chunk_filename, from_relics_path);

        char to_chunk_filename[256];
        get_chunk_filename(to_filename, i, to_chunk_filename);
        char to_relics_path[256];
        get_relics_path(to_chunk_filename, to_relics_path);

        if (rename(from_relics_path, to_relics_path) == -1) {
            if (errno == ENOENT) break; // No more chunks
            return -errno;
        }
        i++;
    }
    return 0;
}


static struct fuse_operations baymax_oper = {
    .getattr = baymax_getattr,
    .readdir = baymax_readdir,
    .open = baymax_open,
    .read = baymax_read,
    .create = baymax_create,
    .write = baymax_write,
    .unlink = baymax_unlink,
    .rename = baymax_rename,
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <mount_point>\n", argv[0]);
        return 1;
    }

    return fuse_main(argc, argv, &baymax_oper, NULL);
}
