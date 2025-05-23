# Sisop-4-2025-IT06

Repository ini berisi hasil pengerjaan Praktikum Sistem Operasi 2025 Modul 4

| Nama                     | Nrp        |
| ------------------------ | ---------- |
| Paundra Pujo Darmawan    | 5027241008 |
| Putri Joselina Silitonga | 5027241116 |



## SOAL 2 (Putri Joselina Silitonga)

1. Download dan Setup Lingkungan
Kode Lama:

a. Tidak ada pengecekan atau inisialisasi eksplisit untuk direktori relics/ dan file activity.log.

b. Tidak ada pengaturan izin (permissions) atau kepemilikan (ownership) untuk direktori dan file log, yang dapat menyebabkan masalah izin akses saat file system dijalankan.

c. Argumen program tidak divalidasi secara eksplisit, hanya menampilkan pesan error jika argc < 2 tanpa pemeriksaan tambahan.
Kode Baru:

```
if (mkdir(RELICS_DIR, 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "Failed to create relics directory: %s\n", strerror(errno));
    return 1;
}
if (chown(RELICS_DIR, getuid(), getgid()) != 0) {
    fprintf(stderr, "Failed to set relics ownership: %s\n", strerror(errno));
    return 1;
}
if (chmod(RELICS_DIR, 0755) != 0) {
    fprintf(stderr, "Failed to set relics permissions: %s\n", strerror(errno));
    return 1;
}
if (access(LOG_FILE, F_OK) != 0) {
    FILE *log_fp = fopen(LOG_FILE, "w");
    if (log_fp) {
        fclose(log_fp);
        chmod(LOG_FILE, 0644);
    } else {
        fprintf(stderr, "Failed to create activity.log: %s\n", strerror(errno));
        return 1;
    }
}
if (chown(LOG_FILE, getuid(), getgid()) != 0) {
    fprintf(stderr, "Failed to set activity.log ownership: %s\n", strerror(errno));
    return 1;
}
if (chmod(LOG_FILE, 0644) != 0) {
    fprintf(stderr, "Failed to set activity.log permissions: %s\n", strerror(errno));
    return 1;
}
```

Perbaikan:

Menambahkan pembuatan direktori relics dengan izin 0755 dan pengecekan apakah direktori sudah ada (EEXIST).
Mengatur kepemilikan (chown) dan izin (chmod) untuk relics dan activity.log agar sesuai dengan user dan group yang menjalankan program, mencegah masalah izin akses.
Memastikan activity.log dibuat jika belum ada, dengan izin 0644, dan menangani error secara eksplisit.
Menambahkan debug argumen untuk memudahkan pelacakan:

```
for (int i = 0; i < argc; i++) {
    printf("Argument %d: %s\n", i, argv[i]);
}
```

Dampak:
-Kode baru lebih robust karena memastikan lingkungan siap sebelum FUSE dijalankan, mengurangi risiko kegagalan akibat izin atau file yang hilang.

-Logging argumen membantu debugging saat konfigurasi mount point salah.


2. Logging Aktivitas
Kode Lama:
```
void logAct(const char *format, ...) {
    FILE *logFile = fopen(LOG_FILE, "a");
    if (!logFile) return;
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
```

-Logging sederhana menggunakan va_list untuk format variabel.

-Tidak menangani kegagalan pembukaan file log dengan baik (hanya return jika gagal).

-Tidak ada pengecekan izin atau pembuatan file log jika belum ada.

Kode Baru:
```
void log_activity(const char *action) {
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (!log_fp) {
        fprintf(stderr, "Failed to open log file %s: %s\n", LOG_FILE, strerror(errno));
        mode_t mode = 0644;
        log_fp = fopen(LOG_FILE, "w");
        if (log_fp) {
            fclose(log_fp);
            chmod(LOG_FILE, mode);
            log_fp = fopen(LOG_FILE, "a");
        }
    }
    if (!log_fp) {
        fprintf(stderr, "Logging failed, continuing without log\n");
        return;
    }
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(log_fp, "[%s] %s\n", timestamp, action);
    fclose(log_fp);
}

void get_timestamp(char *buffer, size_t size) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm = localtime(&tv.tv_sec);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm);
}
```
Perbaikan:

1. Memisahkan pembuatan timestamp ke fungsi get_timestamp untuk modularitas.

2. Menangani kegagalan pembukaan file log dengan mencoba membuat file baru dan mengatur izinnya (0644).

3. Menambahkan pesan error ke stderr untuk debugging jika logging gagal, tetapi tetap melanjutkan eksekusi.
Logging lebih deskriptif dengan prefix seperti DEBUG, ERROR, atau WRITE untuk membedakan jenis aktivitas.


Dampak:
-Logging lebih andal dan informatif, memudahkan pelacakan kesalahan.

-Penanganan error yang lebih baik mencegah program crash jika file log tidak dapat diakses.

3. Fungsi getattr
Kode Lama:
```
static int baymax_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
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
            if (stat(relPath, &chunkStat) == -1) break;
            totalSize += chunkStat.st_size;
        }
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = totalSize;
        return 0;
    }
    return -ENOENT;
}
```
1. Hanya mendukung file Baymax.jpeg, tidak fleksibel untuk file lain.

2. Tidak mengatur st_uid dan st_gid, yang dapat menyebabkan masalah kepemilikan.

3. Tidak ada logging untuk debugging.
Kode Baru:
```
static int baymax_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    char log[256];
    snprintf(log, sizeof(log), "DEBUG: getattr called for %s", path);
    log_activity(log);
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        snprintf(log, sizeof(log), "DEBUG: getattr succeeded for %s (directory)", path);
        log_activity(log);
        return 0;
    }
    char chunk_path[MAX_PATH];
    snprintf(chunk_path, MAX_PATH, "%s/%s.000", RELICS_DIR, path + 1);
    if (access(chunk_path, F_OK) == 0) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        size_t total_size = 0;
        int max_chunks = strcmp(path + 1, BAYMAX_FILE) == 0 ? 14 : 999;
        for (int i = 0; i < max_chunks; i++) {
            snprintf(chunk_path, MAX_PATH, "%s/%s.%03d", RELICS_DIR, path + 1, i);
            struct stat chunk_st;
            if (stat(chunk_path, &chunk_st) != 0) break;
            total_size += chunk_st.st_size;
        }
        stbuf->st_size = total_size;
        snprintf(log, sizeof(log), "DEBUG: getattr succeeded for %s (file, size %zu)", path, total_size);
        log_activity(log);
        return 0;
    }
    snprintf(log, sizeof(log), "DEBUG: getattr failed for %s: %s", path, strerror(ENOENT));
    log_activity(log);
    return -ENOENT;
}
```
Perbaikan:
- Mendukung file apa pun di direktori relics (tidak hanya Baymax.jpeg), dengan memeriksa keberadaan chunk pertama (.000).

- Menambahkan batas chunk maksimum (14 untuk Baymax.jpeg, 999 untuk file lain) untuk mencegah loop tak terbatas.

- Mengatur st_uid dan st_gid untuk kepemilikan yang benar.

- Menambahkan logging untuk setiap pemanggilan, keberhasilan, dan kegagalan.

Dampak:
1. Kode baru lebih fleksibel, mendukung banyak file, dan lebih aman dengan batas chunk.
2. Logging membantu melacak masalah atribut file.


4. Fungsi readdir
Kode Lama:
```
static int baymax_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    if (strcmp(path, "/") != 0) return -ENOENT;
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, BAYMAX_FILE, NULL, 0, 0);
    return 0;
}
```
- Hanya menampilkan Baymax.jpeg secara statis, tidak membaca isi direktori relics/.

- Tidak menyediakan atribut file (struct stat) dalam filler, yang dapat menyebabkan informasi tidak lengkap di aplikasi pengguna.

-Tidak ada logging.

Kode Baru:
```
static int baymax_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/") != 0) {
        snprintf(log, sizeof(log), "ERROR: readdir called on non-root path %s", path);
        log_activity(log);
        return -ENOENT;
    }
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_mode = S_IFDIR | 0755;
    st.st_uid = getuid();
    st.st_gid = getgid();
    filler(buf, ".", &st, 0);
    filler(buf, "..", &st, 0);
    DIR *dir = opendir(RELICS_DIR);
    if (!dir) {
        snprintf(log, sizeof(log), "ERROR: Failed to open relics directory: %s", strerror(errno));
        log_activity(log);
        return -EIO;
    }
    struct dirent *entry;
    char listed[1024][MAX_PATH];
    int listed_count = 0;
    while ((entry = readdir(dir))) {
        if (strstr(entry->d_name, ".000")) {
            char filename[MAX_PATH];
            strncpy(filename, entry->d_name, MAX_PATH - 1);
            char *dot = strrchr(filename, '.');
            if (dot) *dot = '\0';
            int skip = 0;
            for (int i = 0; i < listed_count; i++) {
                if (strcmp(listed[i], filename) == 0) {
                    skip = 1;
                    break;
                }
            }
            if (!skip) {
                st.st_mode = S_IFREG | 0644;
                st.st_uid = getuid();
                st.st_gid = getgid();
                size_t total_size = 0;
                int max_chunks = strcmp(filename, BAYMAX_FILE) == 0 ? 14 : 999;
                for (int i = 0; i < max_chunks; i++) {
                    char chunk_path[MAX_PATH];
                    snprintf(chunk_path, MAX_PATH, "%s/%s.%03d", RELICS_DIR, filename, i);
                    struct stat chunk_st;
                    if (stat(chunk_path, &chunk_st) != 0) break;
                    total_size += chunk_st.st_size;
                }
                st.st_size = total_size;
                if (filler(buf, filename, &st, 0)) {
                    closedir(dir);
                    snprintf(log, sizeof(log), "ERROR: readdir filler failed for %s", filename);
                    log_activity(log);
                    return -ENOMEM;
                }
                strncpy(listed[listed_count++], filename, MAX_PATH - 1);
            }
        }
    }
    closedir(dir);
    snprintf(log, sizeof(log), "DEBUG: readdir completed for %s", path);
    log_activity(log);
    return 0;
}
```
Perbaikan:
- Membaca isi direktori relics/ secara dinamis menggunakan opendir dan readdir.

- Hanya menampilkan file dengan chunk .000 dan menghapus suffix chunk untuk nama file asli.

- Menyimpan daftar file yang sudah ditampilkan untuk menghindari duplikasi.

- Menyediakan atribut struct stat (termasuk ukuran total file dan kepemilikan) untuk setiap file.

- Menambahkan logging untuk setiap langkah dan error handling (misalnya, kegagalan membuka direktori).

Dampak:
1. Kode baru mendukung banyak file, bukan hanya Baymax.jpeg.

2. Informasi file lebih lengkap dan akurat, meningkatkan kompatibilitas dengan aplikasi pengguna.
Logging dan error handling membuat debugging lebih mudah.

5. Fungsi read
Kode Lama:
```
static int baymax_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
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
        chunkOffset = 0;
    }
    return totalRead;
}
```

- Hanya mendukung Baymax.jpeg.

- Tidak memeriksa ukuran total file, berpotensi membaca chunk yang tidak ada.

- Tidak ada logging untuk debugging.

Kode Baru:
```
static int baymax_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char *filename = strdup(path + 1);
    if (!filename) return -ENOMEM;
    size_t total_size = 0;
    int max_chunks = strcmp(filename, BAYMAX_FILE) == 0 ? 14 : 999;
    for (int i = 0; i < max_chunks; i++) {
        char chunk_path[MAX_PATH];
        snprintf(chunk_path, MAX_PATH, "%s/%s.%03d", RELICS_DIR, filename, i);
        struct stat chunk_st;
        if (stat(chunk_path, &chunk_st) != 0) break;
        total_size += chunk_st.st_size;
    }
    if (offset >= total_size) {
        free(filename);
        return 0;
    }
    if (offset + size > total_size) size = total_size - offset;
    int chunk_id = offset / CHUNK_SIZE;
    size_t chunk_offset = offset % CHUNK_SIZE;
    size_t bytes_read = 0;
    while (bytes_read < size && chunk_id < max_chunks) {
        char chunk_path[MAX_PATH];
        snprintf(chunk_path, MAX_PATH, "%s/%s.%03d", RELICS_DIR, filename, chunk_id);
        FILE *fp = fopen(chunk_path, "rb");
        if (!fp) {
            char log[256];
            snprintf(log, sizeof(log), "ERROR: read failed for %s chunk %d: %s", filename, chunk_id, strerror(errno));
            log_activity(log);
            free(filename);
            return -EIO;
        }
        fseek(fp, chunk_offset, SEEK_SET);
        size_t chunk_size = CHUNK_SIZE - chunk_offset;
        if (chunk_size > size - bytes_read) chunk_size = size - bytes_read;
        size_t read_bytes = fread(buf + bytes_read, 1, chunk_size, fp);
        fclose(fp);
        bytes_read += read_bytes;
        chunk_id++;
        chunk_offset = 0;
    }
    free(filename);
    return bytes_read;
}
```

Perbaikan:
- Mendukung banyak file dengan memeriksa chunk pertama (.000).
Menghitung ukuran total file untuk memastikan offset dan size valid.

-Menambahkan batas chunk maksimum (14 untuk Baymax.jpeg, 999 untuk lainnya).

-Menambahkan logging untuk setiap kegagalan baca chunk.
Mengelola memori dengan baik menggunakan strdup dan free.

Dampak:
1. Lebih fleksibel dan aman untuk berbagai file.

2. Logging membantu melacak kesalahan baca.

6. Fungsi create dan write
```
static int baymax_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s", path + 1);
    char relPath[256];
    rpath(filename, relPath);
    FILE *f = fopen(relPath, "wb");
    if (!f) return -EIO;
    fclose(f);
    logAct("CREATE: %s", filename);
    return 0;
}

static int baymax_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
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
```

- Tidak mendukung penulisan ke chunk dengan offset, selalu menulis dari awal.

- Logging write tidak mencatat semua chunk yang ditulis dalam satu entri log.

-Tidak ada penanganan memori dinamis yang baik.

Kode Baru:
```
static int baymax_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char chunk_path[MAX_PATH];
    snprintf(chunk_path, MAX_PATH, "%s/%s.000", RELICS_DIR, path + 1);
    FILE *fp = fopen(chunk_path, "wb");
    if (!fp) {
        char log[256];
        snprintf(log, sizeof(log), "ERROR: create failed for %s: %s", path, strerror(errno));
        log_activity(log);
        return -EIO;
    }
    fclose(fp);
    char log[256];
    snprintf(log, sizeof(log), "WRITE: %s -> %s.000", path + 1, path + 1);
    log_activity(log);
    return 0;
}

static int baymax_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char *filename = strdup(path + 1);
    if (!filename) return -ENOMEM;
    char log[1024] = {0};
    snprintf(log, sizeof(log), "WRITE: %s", filename);
    int chunk_id = offset / CHUNK_SIZE;
    size_t chunk_offset = offset % CHUNK_SIZE;
    size_t bytes_written = 0;
    while (bytes_written < size) {
        char chunk_path[MAX_PATH];
        snprintf(chunk_path, MAX_PATH, "%s/%s.%03d", RELICS_DIR, filename, chunk_id);
        FILE *fp = fopen(chunk_path, chunk_offset == 0 ? "wb" : "ab");
        if (!fp) {
            char log_tmp[256];
            snprintf(log_tmp, sizeof(log_tmp), "ERROR: write failed for %s chunk %d: %s", filename, chunk_id, strerror(errno));
            log_activity(log_tmp);
            free(filename);
            return -EIO;
        }
        size_t to_write = CHUNK_SIZE - chunk_offset;
        if (to_write > size - bytes_written) to_write = size - bytes_written;
        size_t written = fwrite(buf + bytes_written, 1, to_write, fp);
        fclose(fp);
        bytes_written += written;
        char tmp[64];
        snprintf(tmp, sizeof(tmp), " -> %s.%03d", filename, chunk_id);
        strcat(log, tmp);
        chunk_id++;
        chunk_offset = 0;
    }
    log_activity(log);
    free(filename);
    return bytes_written;
}
```
Perbaikan:

- Mendukung penulisan dengan offset, menggunakan mode "ab" untuk append jika bukan chunk baru.

- Logging mencatat semua chunk yang ditulis dalam satu entri log.

- Menggunakan strdup untuk manajemen memori yang lebih aman.

- Menambahkan error handling dan logging untuk kegagalan penulisan.

Dampak:
1. Penulisan lebih fleksibel dan mendukung operasi append.

2. Logging yang lebih terperinci memudahkan pelacakan.

7. Fungsi unlink
Kode Lama:
```
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
Tidak memeriksa apakah ada chunk yang berhasil dihapus sebelum mengembalikan 0, berpotensi menyesatkan jika tidak ada file.
Logging terpisah untuk setiap chunk, kurang efisien.

Kode Baru:

```
static int baymax_unlink(const char *path) {
    char *filename = strdup(path + 1);
    if (!filename) return -ENOMEM;
    char log[1024] = {0};
    snprintf(log, sizeof(log), "DELETE: %s", filename);
    int max_chunks = strcmp(filename, BAYMAX_FILE) == 0 ? 14 : 999;
    int chunk_count = 0;
    for (int i = 0; i < max_chunks; i++) {
        char chunk_path[MAX_PATH];
        snprintf(chunk_path, MAX_PATH, "%s/%s.%03d", RELICS_DIR, filename, i);
        if (access(chunk_path, F_OK) != 0) break;
        if (unlink(chunk_path) == 0) {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), " -> %s.%03d", filename, i);
            strcat(log, tmp);
            chunk_count++;
        } else {
            char log_tmp[256];
            snprintf(log_tmp, sizeof(log_tmp), "ERROR: unlink failed for %s.%03d: %s", filename, i, strerror(errno));
            log_activity(log_tmp);
        }
    }
    if (chunk_count > 0) {
        log_activity(log);
    }
    free(filename);
    return chunk_count > 0 ? 0 : -ENOENT;
}
```
Perbaikan:

- Mengembalikan -ENOENT jika tidak ada chunk yang dihapus, lebih akurat.

-Logging semua chunk yang dihapus dalam satu entri.
Menambahkan batas chunk maksimum untuk keamanan.

-Menangani error unlink secara eksplisit.

Dampak:
Operasi penghapusan lebih akurat dan logging lebih efisien.

8. Fungsi Tambahan di Kode Lama: rename
Kode Lama:
```
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
```
- Mendukung operasi rename untuk chunk file.

- Tidak ada di kode baru, kemungkinan dihapus karena tidak diperlukan untuk kasus penggunaan saat ini.

Dampak:
Kode baru lebih sederhana tanpa rename, tetapi kehilangan fungsionalitas ini jika dibutuhkan.

## Kode Baymax.c walaupun sudah diperbarui tetap tidak solve.
## Kendalanya adalah: 
Dari log dan kode, berikut masalah yang saya temukan:
1. Error getattr failed untuk file seperti /.Trash, /.Trash-1000, /.xdg-volume-info, /autorun.inf:

Filesystem saya mencoba mengakses file sistem seperti /.Trash, tapi kode saya mengembalikan -ENOENT (No such file or directory) karena file ini tidak ada di relics. Ini normal untuk beberapa aplikasi (seperti file explorer), tapi menunjukkan filesystem saya tidak menangani file di luar Baymax.jpeg dengan baik.

2. Fungsi readdir berulang kali dipanggil:
Log menunjukkan readdir completed for / berkali-kali. Ini bisa jadi karena file explorer atau aplikasi lain terus memeriksa direktori, tapi bisa juga menandakan kode saya tidak menangani offset dengan benar.


3. Fungsi readdir berulang kali dipanggil:
Log menunjukkan readdir completed for / berkali-kali. Ini bisa jadi karena file explorer atau aplikasi lain terus memeriksa direktori, tapi bisa juga menandakan kode saya tidak menangani offset dengan benar.

4. Penanganan File Sistem yang Tidak Relevan
Kendala: Log menunjukkan banyak permintaan LOOKUP dan getattr untuk file seperti /.Trash, /.Trash-1000, /.xdg-volume-info, dan /autorun.inf, yang semuanya mengembalikan error ENOENT (No such file or directory). Meskipun ini bukan error kritis, ini menunjukkan bahwa sistem mencoba mengakses file yang tidak ada di filesystemmu.
Penyebab: File-file ini biasanya dicari oleh file manager (seperti Nautilus) atau sistem operasi. Kode baymax sudah benar mengembalikan ENOENT, tapi ini bisa mengalihkan perhatian dari fokus utama, yaitu menampilkan Baymax.jpeg.
# Soal 3 (Paundra Pujo Darmawan)

Pada soal ini, objective kita adalah menggunakan linux secara isolasi menggunakan docker. Jadi kita menggunakan image linux yang sudah ada dan menjalankan program didalam kontainer yang terisolasi. Pertama-tama kita dapat setting untuk membuat kontainer dengan [docker-compose.yml](soal_3/docker-compose.yml). Dan menjalankan program yang sudah kita buat kedalam kontainer tersebut dengan [Dockerfile](soal_3/Dockerfile).

Sedangkan pada program sendiri, kita membuat program yang menggunakan konsep FUSE secara sederhana, dengan objektif untuk mendeteksi program yang terdapat kata `nafis` ataupun `kimcun` di dalamnya. Dimana kita harus mereverse nama file tersebut. Dan untuk file yang tidak terdapat kata kata tersebut, kita harus melakukan encrypt menggunakan rot13 untuk melindungi isi file dari nafis dan kimcun.

Melakukan scanning untuk file yang memiliki kata kata `nafis` ataupun `kimcun`.

```c
int is_malicious(const char *filename) {
    return strstr(filename, "nafis") || strstr(filename, "kimcun");
}
```

Melakukan reverse string pada file yang mengandung kata kata `nafis` ataupun `kimcun` didalamnya.

```c
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
```

Melakukan enkripsi pada file normal.

```c
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
```
