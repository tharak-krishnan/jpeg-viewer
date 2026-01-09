#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdint.h>

/* Save image as PPM file for comparison */
int save_ppm(const char *filename, const uint8_t *image_data,
             int width, int height, int channels);

#endif /* OUTPUT_H */
