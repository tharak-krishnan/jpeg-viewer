#ifndef COLOR_H
#define COLOR_H

#include "../include/jpeg_types.h"

/* Convert YCbCr component buffers to RGB image */
int ycbcr_to_rgb(jpeg_decoder_t *decoder);

/* Upsample chroma component (for 4:2:0 and 4:2:2 subsampling) */
void upsample_component(const uint8_t *src, uint8_t *dst,
                        int src_width, int src_height,
                        int dst_width, int dst_height);

#endif /* COLOR_H */
