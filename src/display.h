#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

/* Display image in SDL2 window */
int display_image(const uint8_t *image_data, int width, int height, int channels);

#endif /* DISPLAY_H */
