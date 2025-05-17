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
const int CHUNK_SIZE = 1024; // Ukuran chunk dalam byte


void logAct(const char *format, ...) {
    FILE *logFile = fopen(LOG_FILE, "a");
    if (!logFile) return; // Jika gagal membuka log, keluar

    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[64];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S] ", timeinfo);

    fprintf(logFile, "%s", timestamp);

    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);

    fprintf(logFile, "\n");
    fclose(logFile);
}


int numChunks(size_t fileSize) {
    return (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE; // Pembulatan ke atas
}


void chunkname(const char *baseName, int chunkNum, char *outName) {
    snprintf(outName, 256, "%s.%03d", baseName, chunkNum); // Pakai snprintf
}


void rpath(const char *name, char *outPath) {
    snprintf(outPath, 256, "%s%s", RELICS_DIR, name); // Pakai snprintf
}


int isChunk(const char *name) {
    return strstr(name, ".00") != NULL; // Cek keberadaan ".00"
}


static int baymax_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    memset(stbuf, 0, sizeof(struct stat)); // Inisialisasi struct stat

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2; // . dan ..
        return 0;
    }

    if (strcmp(path + 1, BAYMAX_FILE) == 0) {
        long totalSize = 0;
        int i = 0;
        char chunkFile[256], relPath[256];

        while (1) {
            chunkname(BAYMAX_FILE, i++, chunkFile);
            rpath(chunkFile, relPath);

            struct stat chunkStat;
            if (stat(relPath, &chunkStat) == -1) break; // Keluar jika chunk tidak ada
            totalSize += chunkStat.st_size;
        }

        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = totalSize;
        return 0;
    }

    return -ENOENT; 
}


static int baymax_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void)offset; (void)fi; (void)flags; // Supress unused parameter warnings

    if (strcmp(path, "/") != 0) return -ENOENT; // Hanya root directory

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, BAYMAX_FILE, NULL, 0, 0);

    return 0;
}


static int baymax_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path + 1, BAYMAX_FILE) != 0) return -ENOENT;

    logAct("OPEN: %s", BAYMAX_FILE); // Log aktivitas buka
    return 0;
}


static int baymax_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)fi;
    if (strcmp(path + 1, BAYMAX_FILE) != 0) return -ENOENT;

    int totalRead = 0;
    int chunkIdx = offset / CHUNK_SIZE;
    int chunkOffset = offset % CHUNK_SIZE;
    size_t toRead = size;
    char chunkFile[256], relPath[256];

    while (toRead > 0) {
        chunkname(BAYMAX_FILE, chunkIdx++, chunkFile);
        rpath(chunkFile, relPath);

        FILE *f = fopen(relPath, "rb");
        if (!f) break;

        fseek(f, chunkOffset, SEEK_SET);
        size_t readSize = (toRead > (CHUNK_SIZE - chunkOffset)) ? (CHUNK_SIZE - chunkOffset) : toRead;
        size_t bytesRead = fread(buf + totalRead, 1, readSize, f);
        fclose(f);

        if (bytesRead == 0) break;

        totalRead += bytesRead;
        toRead -= bytesRead;
        chunkOffset = 0; // Reset offset setelah chunk pertama
    }

    return totalRead;
}

// Fungsi create (membuat file)
static int baymax_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void)mode; (void)fi;
    char filename[256];
    snprintf(filename, sizeof(filename), "%s", path + 1); // Pakai snprintf, lebih aman

    char relPath[256];
    rpath(filename, relPath);

    FILE *f = fopen(relPath, "wb");
    if (!f) return -EIO;
    fclose(f);

    logAct("CREATE: %s", filename);
    return 0;
}

// Fungsi write (menulis data ke file)
static int baymax_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)offset; (void)fi;

    char filename[256];
    snprintf(filename, sizeof(filename), "%s", path + 1);

    int numChunksToWrite = numChunks(size);
    logAct("WRITE: %s -> ", filename);

    int totalWritten = 0;
    char chunkFile[256], relPath[256];

    for (int i = 0; i < numChunksToWrite; ++i) {
        chunkname(filename, i, chunkFile);
        rpath(chunkFile, relPath);

        FILE *f = fopen(relPath, "wb");
        if (!f) return -EIO;

        size_t writeSize = (size > CHUNK_SIZE) ? CHUNK_SIZE : size;
        fwrite(buf + totalWritten, 1, writeSize, f);
        fclose(f);

        totalWritten += writeSize;
        size -= writeSize;

        fprintf(stderr, "%s, ", chunkFile);
    }

    fprintf(stderr, "\n");
    return totalWritten;
}


static int baymax_unlink(const char *path) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s", path + 1);

    logAct("UNLINK: %s", filename);

    int i = 0;
    char chunkFile[256], relPath[256];
    while (1) {
        chunkname(filename, i++, chunkFile);
        rpath(chunkFile, relPath);

        if (unlink(relPath) == -1) break;
        logAct("UNLINK: %s", chunkFile);
    }
    return 0;
}


static int baymax_rename(const char *from, const char *to, unsigned int flags) {
    if (flags) return -EINVAL;

    char oldName[256], newName[256];
    snprintf(oldName, sizeof(oldName), "%s", from + 1);
    snprintf(newName, sizeof(newName), "%s", to + 1);

    logAct("RENAME: %s to %s", oldName, newName);

    int i = 0;
    char oldChunk[256], newChunk[256], oldPath[256], newPath[256];
    while (1) {
        chunkname(oldName, i, oldChunk);
        rpath(oldChunk, oldPath);

        chunkname(newName, i++, newChunk);
        rpath(newChunk, newPath);

        if (rename(oldPath, newPath) == -1) {
            if (errno == ENOENT) break;
            return -errno;
        }
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
