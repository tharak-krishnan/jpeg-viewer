#include "output.h"
#include <stdio.h>

/* Save image as PPM file for comparison */
int save_ppm(const char *filename, const uint8_t *image_data,
             int width, int height, int channels) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open output file: %s\n", filename);
        return -1;
    }

    /* Write PPM header */
    fprintf(f, "P6\n%d %d\n255\n", width, height);

    /* Write pixel data */
    if (channels == 3) {
        /* RGB - write directly */
        fwrite(image_data, 1, width * height * 3, f);
    } else if (channels == 1) {
        /* Grayscale - convert to RGB by duplicating */
        for (int i = 0; i < width * height; i++) {
            uint8_t gray = image_data[i];
            fwrite(&gray, 1, 1, f);
            fwrite(&gray, 1, 1, f);
            fwrite(&gray, 1, 1, f);
        }
    }

    fclose(f);
    printf("Saved output to: %s\n", filename);
    return 0;
}
