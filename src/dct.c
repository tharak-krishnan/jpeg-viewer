#include "dct.h"
#include "utils.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 1D IDCT helper function */
static void idct_1d(float *data, int stride) {
    float tmp[8];

    for (int i = 0; i < 8; i++) {
        float sum = 0.0f;

        for (int u = 0; u < 8; u++) {
            float cu = (u == 0) ? (1.0f / sqrtf(2.0f)) : 1.0f;
            sum += cu * data[u * stride] * cosf((2.0f * i + 1.0f) * u * M_PI / 16.0f);
        }

        tmp[i] = sum / 2.0f;
    }

    /* Copy back to data */
    for (int i = 0; i < 8; i++) {
        data[i * stride] = tmp[i];
    }
}

/* 2D IDCT using separable property */
void idct_2d(int16_t *input_block, uint8_t *output_block) {
    float temp[64];

    /* Convert int16 to float */
    for (int i = 0; i < 64; i++) {
        temp[i] = (float)input_block[i];
    }

    /* Apply 1D IDCT to each row */
    for (int row = 0; row < 8; row++) {
        idct_1d(&temp[row * 8], 1);
    }

    /* Apply 1D IDCT to each column */
    for (int col = 0; col < 8; col++) {
        idct_1d(&temp[col], 8);
    }

    /* Convert to uint8 with level shift and clamping */
    for (int i = 0; i < 64; i++) {
        int val = (int)(temp[i] + 128.5f);
        output_block[i] = clamp(val, 0, 255);
    }
}

/* Dequantize block */
void dequantize_block(int16_t *block, const uint8_t *quant_table) {
    for (int i = 0; i < 64; i++) {
        block[i] = block[i] * quant_table[i];
    }
}
