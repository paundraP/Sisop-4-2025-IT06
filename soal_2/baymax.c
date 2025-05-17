#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>   
#include <gd.h>     


unsigned char hex_to_int(char a, char b) {
    int high = 0, low = 0;

    if (a >= '0' && a <= '9')
        high = a - '0';
    else if (a >= 'a' && a <= 'f')
        high = a - 'a' + 10;
    else if (a >= 'A' && a <= 'F')
        high = a - 'A' + 10;

    if (b >= '0' && b <= '9')
        low = b - '0';
    else if (b >= 'a' && b <= 'f')
        low = b - 'a' + 10;
    else if (b >= 'A' && b <= 'F')
        low = b - 'A' + 10;

    return (high << 4) | low;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    char *input_filename = argv[1];
    FILE *input_file = fopen(input_filename, "r");
    if (!input_file) {
        perror("Error opening input file");
        return 1;
    }

    // Baca semua hex dari file
    char hex_string[100000] = "";  // Asumsi ukuran maksimum string hex
    char buffer[3];
    while (fgets(buffer, sizeof(buffer), input_file) != NULL) {
        strcat(hex_string, buffer);
    }
    fclose(input_file);

    // Hitung ukuran data gambar (asumsi gambar persegi)
    int data_len = strlen(hex_string) / 2;
    int img_width = (int)sqrt(data_len);
    int img_height = img_width;

    // Buat gambar GD
    gdImagePtr im = gdImageCreateTrueColor(img_width, img_height);
    if (!im) {
        fprintf(stderr, "Error creating image\n");
        return 1;
    }

    // Konversi hex string ke data gambar
    unsigned char *img_data = (unsigned char *)malloc(data_len);
    if (!img_data) {
        fprintf(stderr, "Error allocating memory for image data\n");
        gdImageDestroy(im);
        return 1;
    }

    for (int i = 0; i < data_len; i++) {
        img_data[i] = hex_to_int(hex_string[i * 2], hex_string[i * 2 + 1]);
    }

    // Isi pixel gambar (grayscale)
    int k = 0;
    for (int y = 0; y < img_height; y++) {
        for (int x = 0; x < img_width; x++) {
            if (k < data_len) {
                int color = gdTrueColor(img_data[k], img_data[k], img_data[k]);
                gdImageSetPixel(im, x, y, color);
                k++;
            } else {
                gdImageSetPixel(im, x, y, gdTrueColor(0, 0, 0)); // Black padding
            }
        }
    }

    free(img_data);

    // Membuat nama file output
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char timestamp[20];
    sprintf(timestamp, "%04d-%02d-%02d_%02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    char *output_filename = malloc(strlen(input_filename) + strlen("_image_") + strlen(timestamp) + 4 + 1); // +4 for ".png", +1 for null terminator
    if (!output_filename) {
        fprintf(stderr, "Error allocating memory for output filename\n");
        gdImageDestroy(im);
        return 1;
    }
    strcpy(output_filename, input_filename);
    char *dot = strrchr(output_filename, '.');
    if (dot)
        *dot = '\0'; // Hapus ekstensi asli
    strcat(output_filename, "_image_");
    strcat(output_filename, timestamp);
    strcat(output_filename, ".png");

    // Simpan gambar
    FILE *output_file = fopen(output_filename, "wb");
    if (!output_file) {
        perror("Error opening output file");
        gdImageDestroy(im);
        free(output_filename);
        return 1;
    }
    gdImagePng(im, output_file);
    fclose(output_file);
    gdImageDestroy(im);

    // Membuat log
    FILE *log_file = fopen("conversion.log", "a");
    if (!log_file) {
        perror("Error opening log file");
        free(output_filename);
        return 1;
    }
    fprintf(log_file, "[%04d-%02d-%02d][%02d:%02d:%02d]: Successfully converted hexadecimal text %s to %s\n",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
            input_filename, output_filename);
    fclose(log_file);

    free(output_filename);
    return 0;
}
