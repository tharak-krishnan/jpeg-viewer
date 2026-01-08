#include "color.h"
#include "utils.h"
#include <string.h>

/* Convert YCbCr to RGB */
int ycbcr_to_rgb(jpeg_decoder_t *decoder) {
    printf("\nConverting YCbCr to RGB...\n");

    decoder->width = decoder->frame.width;
    decoder->height = decoder->frame.height;
    decoder->channels = decoder->frame.num_components;

    /* Handle grayscale (1 component) */
    if (decoder->frame.num_components == 1) {
        printf("Grayscale image detected\n");
        size_t size = decoder->width * decoder->height;
        decoder->image_data = (uint8_t*)jpeg_malloc(size);

        /* Copy Y component directly */
        for (int y = 0; y < decoder->height; y++) {
            for (int x = 0; x < decoder->width; x++) {
                decoder->image_data[y * decoder->width + x] =
                    decoder->component_buffers[0][y * decoder->component_width[0] + x];
            }
        }

        printf("Grayscale conversion complete\n");
        return 0;
    }

    /* Handle RGB (3 components - YCbCr) */
    if (decoder->frame.num_components != 3) {
        fprintf(stderr, "Unsupported number of components: %d\n",
                decoder->frame.num_components);
        return -1;
    }

    printf("Color image detected (YCbCr)\n");

    /* Allocate RGB output buffer */
    size_t rgb_size = decoder->width * decoder->height * 3;
    decoder->image_data = (uint8_t*)jpeg_malloc(rgb_size);

    /* Check if chroma upsampling is needed */
    component_info_t *y_comp = &decoder->frame.components[0];
    component_info_t *cb_comp = &decoder->frame.components[1];

    uint8_t *cb_upsampled = NULL;
    uint8_t *cr_upsampled = NULL;

    /* Upsample Cb and Cr if needed */
    if (cb_comp->h_sampling != y_comp->h_sampling ||
        cb_comp->v_sampling != y_comp->v_sampling) {
        printf("Upsampling chroma components...\n");

        cb_upsampled = (uint8_t*)jpeg_malloc(decoder->width * decoder->height);
        cr_upsampled = (uint8_t*)jpeg_malloc(decoder->width * decoder->height);

        upsample_component(decoder->component_buffers[1], cb_upsampled,
                          decoder->component_width[1], decoder->component_height[1],
                          decoder->width, decoder->height);

        upsample_component(decoder->component_buffers[2], cr_upsampled,
                          decoder->component_width[2], decoder->component_height[2],
                          decoder->width, decoder->height);
    } else {
        /* No upsampling needed (4:4:4) */
        cb_upsampled = decoder->component_buffers[1];
        cr_upsampled = decoder->component_buffers[2];
    }

    /* Convert YCbCr to RGB */
    printf("Converting color space...\n");
    for (int y = 0; y < decoder->height; y++) {
        for (int x = 0; x < decoder->width; x++) {
            int y_val = decoder->component_buffers[0][y * decoder->component_width[0] + x];
            int cb_val = cb_upsampled[y * decoder->width + x];
            int cr_val = cr_upsampled[y * decoder->width + x];

            /* YCbCr to RGB conversion */
            int r = y_val + (int)(1.402f * (cr_val - 128));
            int g = y_val - (int)(0.344136f * (cb_val - 128) + 0.714136f * (cr_val - 128));
            int b = y_val + (int)(1.772f * (cb_val - 128));

            /* Clamp to [0, 255] */
            r = clamp(r, 0, 255);
            g = clamp(g, 0, 255);
            b = clamp(b, 0, 255);

            /* Store RGB */
            int idx = (y * decoder->width + x) * 3;
            decoder->image_data[idx + 0] = r;
            decoder->image_data[idx + 1] = g;
            decoder->image_data[idx + 2] = b;
        }
    }

    /* Free upsampled buffers if they were allocated */
    if (cb_upsampled != decoder->component_buffers[1]) {
        jpeg_free(cb_upsampled);
    }
    if (cr_upsampled != decoder->component_buffers[2]) {
        jpeg_free(cr_upsampled);
    }

    printf("Color conversion complete\n");
    return 0;
}

/* Upsample component using nearest-neighbor interpolation */
void upsample_component(const uint8_t *src, uint8_t *dst,
                        int src_width, int src_height,
                        int dst_width, int dst_height) {
    float x_ratio = (float)src_width / (float)dst_width;
    float y_ratio = (float)src_height / (float)dst_height;

    for (int y = 0; y < dst_height; y++) {
        for (int x = 0; x < dst_width; x++) {
            int src_x = (int)(x * x_ratio);
            int src_y = (int)(y * y_ratio);

            /* Clamp to source bounds */
            if (src_x >= src_width) src_x = src_width - 1;
            if (src_y >= src_height) src_y = src_height - 1;

            dst[y * dst_width + x] = src[src_y * src_width + src_x];
        }
    }
}
