#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <gd.h>

unsigned char hexToInt(char a, char b) {
    int highVal = 0, lowVal = 0;
    if (a >= '0' && a <= '9') highVal = a - '0';
    else if (a >= 'a' && a <= 'f') highVal = a - 'a' + 10;
    else if (a >= 'A' && a <= 'F') highVal = a - 'A' + 10;
    if (b >= '0' && b <= '9') lowVal = b - '0';
    else if (b >= 'a' && b <= 'f') lowVal = b - 'a' + 10;
    else if (b >= 'A' && b <= 'F') lowVal = b - 'A' + 10;
    return (highVal << 4) | lowVal;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Pemakaian: %s <berkas_masukan>\n", argv[0]);
        return 1;
    }
    char *inFilename = argv[1];
    FILE *inFile = fopen(inFilename, "r");
    if (!inFile) {
        perror("Gagal membuka berkas masukan");
        return 1;
    }
    char hexStr[100000] = "";
    char buff[3];
    while (fgets(buff, sizeof(buff), inFile) != NULL) {
        strcat(hexStr, buff);
    }
    fclose(inFile);
    int dataLen = strlen(hexStr) / 2;
    int imgW = (int)sqrt(dataLen);
    int imgH = imgW;
    gdImagePtr img = gdImageCreateTrueColor(imgW, imgH);
    if (!img) {
        fprintf(stderr, "Gagal membuat gambar\n");
        return 1;
    }
    unsigned char *imgData = (unsigned char *)malloc(dataLen);
    if (!imgData) {
        fprintf(stderr, "Gagal alokasi memori data gambar\n");
        gdImageDestroy(img);
        return 1;
    }
    for (int i = 0; i < dataLen; i++) {
        imgData[i] = hexToInt(hexStr[i * 2], hexStr[i * 2 + 1]);
    }
    int idx = 0;
    for (int y = 0; y < imgH; y++) {
        for (int x = 0; x < imgW; x++) {
            if (idx < dataLen) {
                int colorCode = gdTrueColor(imgData[idx], imgData[idx], imgData[idx]);
                gdImageSetPixel(img, x, y, colorCode);
                idx++;
            } else {
                gdImageSetPixel(img, x, y, gdTrueColor(0, 0, 0));
            }
        }
    }
    free(imgData);
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char tstamp[20];
    sprintf(tstamp, "%04d-%02d-%02d_%02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    char *outFile = malloc(strlen(inFilename) + strlen("_gambar_") + strlen(tstamp) + 4 + 1);
    if (!outFile) {
        fprintf(stderr, "Gagal alokasi memori nama berkas keluaran\n");
        gdImageDestroy(img);
        return 1;
    }
    strcpy(outFile, inFilename);
    char *dotPtr = strrchr(outFile, '.');
    if (dotPtr) *dotPtr = '\0';
    strcat(outFile, "_gambar_");
    strcat(outFile, tstamp);
    strcat(outFile, ".png");
    FILE *oFile = fopen(outFile, "wb");
    if (!oFile) {
        perror("Gagal membuka berkas keluaran");
        gdImageDestroy(img);
        free(outFile);
        return 1;
    }
    gdImagePng(img, oFile);
    fclose(oFile);
    gdImageDestroy(img);
    FILE *logFile = fopen("konversi.log", "a");
    if (!logFile) {
        perror("Gagal membuka berkas log");
        free(outFile);
        return 1;
    }
    fprintf(logFile, "[%04d-%02d-%02d][%02d:%02d:%02d]: Berhasil mengkonversi teks heksadesimal %s menjadi %s\n",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
            inFilename, outFile);
    fclose(logFile);
    free(outFile);
    return 0;
}
