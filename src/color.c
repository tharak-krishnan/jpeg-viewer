#include "color.h"
#include "utils.h"
#include <string.h>
#include <sys/time.h>

static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

/* Convert YCbCr to RGB */
int ycbcr_to_rgb(jpeg_decoder_t *decoder) {
    printf("\nConverting YCbCr to RGB...\n");

    double t_start, t_end;
    double upsample_time = 0.0, convert_time = 0.0;

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

        t_start = get_time_us();

        cb_upsampled = (uint8_t*)jpeg_malloc(decoder->width * decoder->height);
        cr_upsampled = (uint8_t*)jpeg_malloc(decoder->width * decoder->height);

        upsample_component(decoder->component_buffers[1], cb_upsampled,
                          decoder->component_width[1], decoder->component_height[1],
                          decoder->width, decoder->height);

        upsample_component(decoder->component_buffers[2], cr_upsampled,
                          decoder->component_width[2], decoder->component_height[2],
                          decoder->width, decoder->height);

        t_end = get_time_us();
        upsample_time = (t_end - t_start) / 1000.0;  /* Convert to ms */
    } else {
        /* No upsampling needed (4:4:4) */
        cb_upsampled = decoder->component_buffers[1];
        cr_upsampled = decoder->component_buffers[2];
    }

    /* Convert YCbCr to RGB using fixed-point integer arithmetic (like libjpeg) */
    printf("Converting color space...\n");
    t_start = get_time_us();

    /* Fixed-point coefficients (scaled by 2^16) from libjpeg */
    #define SCALEBITS 16
    #define ONE_HALF  (1 << (SCALEBITS-1))
    #define FIX(x)  ((int)((x) * (1L << SCALEBITS) + 0.5))

    const int cr_r = FIX(1.40200);    /* 91881 */
    const int cb_g = FIX(0.34414);    /* 22554 */
    const int cr_g = FIX(0.71414);    /* 46802 */
    const int cb_b = FIX(1.77200);    /* 116130 */

    for (int y = 0; y < decoder->height; y++) {
        for (int x = 0; x < decoder->width; x++) {
            int y_val = decoder->component_buffers[0][y * decoder->component_width[0] + x];
            int cb_val = cb_upsampled[y * decoder->width + x] - 128;
            int cr_val = cr_upsampled[y * decoder->width + x] - 128;

            /* YCbCr to RGB conversion using fixed-point arithmetic */
            int r = y_val + ((cr_r * cr_val + ONE_HALF) >> SCALEBITS);
            int g = y_val - ((cb_g * cb_val + cr_g * cr_val + ONE_HALF) >> SCALEBITS);
            int b = y_val + ((cb_b * cb_val + ONE_HALF) >> SCALEBITS);

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

    t_end = get_time_us();
    convert_time = (t_end - t_start) / 1000.0;  /* Convert to ms */

    #undef SCALEBITS
    #undef ONE_HALF
    #undef FIX

    /* Free upsampled buffers if they were allocated */
    if (cb_upsampled != decoder->component_buffers[1]) {
        jpeg_free(cb_upsampled);
    }
    if (cr_upsampled != decoder->component_buffers[2]) {
        jpeg_free(cr_upsampled);
    }

    printf("Color conversion complete\n");
    if (upsample_time > 0.0) {
        printf("  Chroma upsample:  %.2f ms\n", upsample_time);
    }
    printf("  YCbCr->RGB:       %.2f ms\n", convert_time);
    return 0;
}

/* Helper to safely get chroma sample with boundary handling */
static inline uint8_t get_chroma_sample(const uint8_t *src, int x, int y,
                                        int width, int height) {
    if (x < 0) x = 0;
    if (x >= width) x = width - 1;
    if (y < 0) y = 0;
    if (y >= height) y = height - 1;
    return src[y * width + x];
}

/* Upsample component using libjpeg h2v2 fancy upsampling (9:3:3:1 weighting) */
void upsample_component(const uint8_t *src, uint8_t *dst,
                        int src_width, int src_height,
                        int dst_width, int dst_height) {

    /* Check if this is h2v2 subsampling (2x in both directions) */
    if (dst_width == src_width * 2 && dst_height == src_height * 2) {
        /* libjpeg h2v2 fancy upsampling using 9:3:3:1 weights */
        for (int src_y = 0; src_y < src_height; src_y++) {
            for (int src_x = 0; src_x < src_width; src_x++) {
                /* Each chroma sample maps to a 2x2 block of output pixels */
                int dst_x = src_x * 2;
                int dst_y = src_y * 2;

                /* Get the four neighboring chroma samples for 9:3:3:1 weighting */
                uint8_t c00 = get_chroma_sample(src, src_x,     src_y,     src_width, src_height);
                uint8_t c10 = get_chroma_sample(src, src_x + 1, src_y,     src_width, src_height);
                uint8_t c01 = get_chroma_sample(src, src_x,     src_y + 1, src_width, src_height);
                uint8_t c11 = get_chroma_sample(src, src_x + 1, src_y + 1, src_width, src_height);

                /* Compute 2x2 output block using 9:3:3:1 weights (total = 16)
                 * Top-left: weight 9 to c00
                 * Top-right: weight 9 to c10
                 * Bottom-left: weight 9 to c01
                 * Bottom-right: weight 9 to c11
                 */

                /* Top-left output pixel (closest to c00) */
                if (dst_y < dst_height && dst_x < dst_width) {
                    int val = (9 * c00 + 3 * c10 + 3 * c01 + 1 * c11 + 8) >> 4;
                    dst[dst_y * dst_width + dst_x] = val;
                }

                /* Top-right output pixel (closest to c10) */
                if (dst_y < dst_height && dst_x + 1 < dst_width) {
                    int val = (3 * c00 + 9 * c10 + 1 * c01 + 3 * c11 + 8) >> 4;
                    dst[dst_y * dst_width + dst_x + 1] = val;
                }

                /* Bottom-left output pixel (closest to c01) */
                if (dst_y + 1 < dst_height && dst_x < dst_width) {
                    int val = (3 * c00 + 1 * c10 + 9 * c01 + 3 * c11 + 8) >> 4;
                    dst[(dst_y + 1) * dst_width + dst_x] = val;
                }

                /* Bottom-right output pixel (closest to c11) */
                if (dst_y + 1 < dst_height && dst_x + 1 < dst_width) {
                    int val = (1 * c00 + 3 * c10 + 3 * c01 + 9 * c11 + 8) >> 4;
                    dst[(dst_y + 1) * dst_width + dst_x + 1] = val;
                }
            }
        }
    } else {
        /* Fallback to simple bilinear for non-h2v2 cases */
        float x_ratio = (float)(src_width) / (float)dst_width;
        float y_ratio = (float)(src_height) / (float)dst_height;

        for (int y = 0; y < dst_height; y++) {
            for (int x = 0; x < dst_width; x++) {
                float src_x_f = (x + 0.5f) * x_ratio - 0.5f;
                float src_y_f = (y + 0.5f) * y_ratio - 0.5f;

                if (src_x_f < 0.0f) src_x_f = 0.0f;
                if (src_y_f < 0.0f) src_y_f = 0.0f;

                int x0 = (int)src_x_f;
                int y0 = (int)src_y_f;
                float dx = src_x_f - x0;
                float dy = src_y_f - y0;

                int x1 = (x0 + 1 < src_width) ? x0 + 1 : x0;
                int y1 = (y0 + 1 < src_height) ? y0 + 1 : y0;

                uint8_t p00 = src[y0 * src_width + x0];
                uint8_t p10 = src[y0 * src_width + x1];
                uint8_t p01 = src[y1 * src_width + x0];
                uint8_t p11 = src[y1 * src_width + x1];

                float val = p00 * (1.0f - dx) * (1.0f - dy) +
                           p10 * dx * (1.0f - dy) +
                           p01 * (1.0f - dx) * dy +
                           p11 * dx * dy;

                dst[y * dst_width + x] = (uint8_t)(val + 0.5f);
            }
        }
    }
}
