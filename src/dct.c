#include "dct.h"
#include "utils.h"

/*
 * Accurate integer IDCT implementation based on the Loeffler-Ligtenberg-Moschytz
 * algorithm as described in ICASSP '89. This implementation is designed to match
 * the quality of libjpeg's slow integer IDCT.
 */

#define DCTSIZE 8
#define DCTSIZE2 64
#define CONST_BITS 13
#define PASS1_BITS 2
#define CENTERJSAMPLE 128

/* Fixed-point constants scaled by 2^13 */
#define FIX_0_298631336  ((int32_t)  2446)
#define FIX_0_390180644  ((int32_t)  3196)
#define FIX_0_541196100  ((int32_t)  4433)
#define FIX_0_765366865  ((int32_t)  6270)
#define FIX_0_899976223  ((int32_t)  7373)
#define FIX_1_175875602  ((int32_t)  9633)
#define FIX_1_501321110  ((int32_t)  12299)
#define FIX_1_847759065  ((int32_t)  15137)
#define FIX_1_961570560  ((int32_t)  16069)
#define FIX_2_053119869  ((int32_t)  16819)
#define FIX_2_562915447  ((int32_t)  20995)
#define FIX_3_072711026  ((int32_t)  25172)

/* Multiply macro */
#define MULTIPLY(var, const) ((var) * (const))

/* Descale with proper rounding */
#define RIGHT_SHIFT(x, n) (((x) + (1L << ((n)-1))) >> (n))

/* Range limiting - create lookup table for clamping to 0-255 */
static uint8_t range_limit_table[1024];
static int range_limit_initialized = 0;

static void init_range_limit_table(void) {
    if (range_limit_initialized) return;

    int i;
    /* Negative values map to 0 */
    for (i = 0; i < 384; i++) {
        range_limit_table[i] = 0;
    }
    /* Valid range 0-255 */
    for (i = 0; i < 256; i++) {
        range_limit_table[i + 384] = i;
    }
    /* Values > 255 map to 255 */
    for (i = 0; i < 384; i++) {
        range_limit_table[i + 640] = 255;
    }

    range_limit_initialized = 1;
}

/* Dequantize macro */
#define DEQUANTIZE(coef, quantval) ((coef) * (quantval))

/* 2D IDCT with integrated dequantization */
void idct_2d(int16_t *input_block, const uint8_t *quant_table, uint8_t *output_block) {
    int32_t workspace[DCTSIZE2];
    int32_t *wsptr;
    int16_t *inptr;
    const uint8_t *quantptr;
    uint8_t *outptr;
    int32_t tmp0, tmp1, tmp2, tmp3;
    int32_t tmp10, tmp11, tmp12, tmp13;
    int32_t z1, z2, z3;
    int ctr;

    init_range_limit_table();

    /* Pass 1: process columns, dequantize and store into workspace */
    inptr = input_block;
    quantptr = quant_table;
    wsptr = workspace;

    for (ctr = DCTSIZE; ctr > 0; ctr--) {
        /* Check for all-zero AC coefficients */
        if (inptr[DCTSIZE*1] == 0 && inptr[DCTSIZE*2] == 0 &&
            inptr[DCTSIZE*3] == 0 && inptr[DCTSIZE*4] == 0 &&
            inptr[DCTSIZE*5] == 0 && inptr[DCTSIZE*6] == 0 &&
            inptr[DCTSIZE*7] == 0) {
            /* AC terms all zero - DC only */
            int32_t dcval = DEQUANTIZE(inptr[DCTSIZE*0], quantptr[DCTSIZE*0]) << PASS1_BITS;

            wsptr[DCTSIZE*0] = dcval;
            wsptr[DCTSIZE*1] = dcval;
            wsptr[DCTSIZE*2] = dcval;
            wsptr[DCTSIZE*3] = dcval;
            wsptr[DCTSIZE*4] = dcval;
            wsptr[DCTSIZE*5] = dcval;
            wsptr[DCTSIZE*6] = dcval;
            wsptr[DCTSIZE*7] = dcval;

            inptr++;
            quantptr++;
            wsptr++;
            continue;
        }

        /* Even part */
        z2 = DEQUANTIZE(inptr[DCTSIZE*0], quantptr[DCTSIZE*0]);
        z3 = DEQUANTIZE(inptr[DCTSIZE*4], quantptr[DCTSIZE*4]);
        z2 <<= CONST_BITS;
        z3 <<= CONST_BITS;
        /* Add rounding bias for final descale */
        z2 += 1L << (CONST_BITS - PASS1_BITS - 1);

        tmp0 = z2 + z3;
        tmp1 = z2 - z3;

        z2 = DEQUANTIZE(inptr[DCTSIZE*2], quantptr[DCTSIZE*2]);
        z3 = DEQUANTIZE(inptr[DCTSIZE*6], quantptr[DCTSIZE*6]);

        z1 = MULTIPLY(z2 + z3, FIX_0_541196100);
        tmp2 = z1 + MULTIPLY(z2, FIX_0_765366865);
        tmp3 = z1 - MULTIPLY(z3, FIX_1_847759065);

        tmp10 = tmp0 + tmp2;
        tmp13 = tmp0 - tmp2;
        tmp11 = tmp1 + tmp3;
        tmp12 = tmp1 - tmp3;

        /* Odd part */
        tmp0 = DEQUANTIZE(inptr[DCTSIZE*7], quantptr[DCTSIZE*7]);
        tmp1 = DEQUANTIZE(inptr[DCTSIZE*5], quantptr[DCTSIZE*5]);
        tmp2 = DEQUANTIZE(inptr[DCTSIZE*3], quantptr[DCTSIZE*3]);
        tmp3 = DEQUANTIZE(inptr[DCTSIZE*1], quantptr[DCTSIZE*1]);

        z2 = tmp0 + tmp2;
        z3 = tmp1 + tmp3;

        z1 = MULTIPLY(z2 + z3, FIX_1_175875602);
        z2 = MULTIPLY(z2, -FIX_1_961570560);
        z3 = MULTIPLY(z3, -FIX_0_390180644);
        z2 += z1;
        z3 += z1;

        z1 = MULTIPLY(tmp0 + tmp3, -FIX_0_899976223);
        tmp0 = MULTIPLY(tmp0, FIX_0_298631336);
        tmp3 = MULTIPLY(tmp3, FIX_1_501321110);
        tmp0 += z1 + z2;
        tmp3 += z1 + z3;

        z1 = MULTIPLY(tmp1 + tmp2, -FIX_2_562915447);
        tmp1 = MULTIPLY(tmp1, FIX_2_053119869);
        tmp2 = MULTIPLY(tmp2, FIX_3_072711026);
        tmp1 += z1 + z3;
        tmp2 += z1 + z2;

        /* Final output stage for pass 1 */
        wsptr[DCTSIZE*0] = (int32_t) RIGHT_SHIFT(tmp10 + tmp3, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE*7] = (int32_t) RIGHT_SHIFT(tmp10 - tmp3, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE*1] = (int32_t) RIGHT_SHIFT(tmp11 + tmp2, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE*6] = (int32_t) RIGHT_SHIFT(tmp11 - tmp2, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE*2] = (int32_t) RIGHT_SHIFT(tmp12 + tmp1, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE*5] = (int32_t) RIGHT_SHIFT(tmp12 - tmp1, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE*3] = (int32_t) RIGHT_SHIFT(tmp13 + tmp0, CONST_BITS - PASS1_BITS);
        wsptr[DCTSIZE*4] = (int32_t) RIGHT_SHIFT(tmp13 - tmp0, CONST_BITS - PASS1_BITS);

        inptr++;
        quantptr++;
        wsptr++;
    }

    /* Pass 2: process rows from workspace, store into output array */
    wsptr = workspace;
    for (ctr = 0; ctr < DCTSIZE; ctr++) {
        outptr = output_block + ctr * DCTSIZE;

        /* Add range center and rounding bias */
        z2 = (int32_t) wsptr[0] +
             ((((int32_t) CENTERJSAMPLE) << (PASS1_BITS + 3)) +
              (1L << (PASS1_BITS + 2)));
        z3 = (int32_t) wsptr[4];

        tmp0 = (z2 + z3) << CONST_BITS;
        tmp1 = (z2 - z3) << CONST_BITS;

        z2 = (int32_t) wsptr[2];
        z3 = (int32_t) wsptr[6];

        z1 = MULTIPLY(z2 + z3, FIX_0_541196100);
        tmp2 = z1 + MULTIPLY(z2, FIX_0_765366865);
        tmp3 = z1 - MULTIPLY(z3, FIX_1_847759065);

        tmp10 = tmp0 + tmp2;
        tmp13 = tmp0 - tmp2;
        tmp11 = tmp1 + tmp3;
        tmp12 = tmp1 - tmp3;

        /* Odd part */
        tmp0 = (int32_t) wsptr[7];
        tmp1 = (int32_t) wsptr[5];
        tmp2 = (int32_t) wsptr[3];
        tmp3 = (int32_t) wsptr[1];

        z2 = tmp0 + tmp2;
        z3 = tmp1 + tmp3;

        z1 = MULTIPLY(z2 + z3, FIX_1_175875602);
        z2 = MULTIPLY(z2, -FIX_1_961570560);
        z3 = MULTIPLY(z3, -FIX_0_390180644);
        z2 += z1;
        z3 += z1;

        z1 = MULTIPLY(tmp0 + tmp3, -FIX_0_899976223);
        tmp0 = MULTIPLY(tmp0, FIX_0_298631336);
        tmp3 = MULTIPLY(tmp3, FIX_1_501321110);
        tmp0 += z1 + z2;
        tmp3 += z1 + z3;

        z1 = MULTIPLY(tmp1 + tmp2, -FIX_2_562915447);
        tmp1 = MULTIPLY(tmp1, FIX_2_053119869);
        tmp2 = MULTIPLY(tmp2, FIX_3_072711026);
        tmp1 += z1 + z3;
        tmp2 += z1 + z2;

        /* Final output stage with range limiting */
        int val;

        val = RIGHT_SHIFT(tmp10 + tmp3, CONST_BITS + PASS1_BITS + 3) + 384;
        outptr[0] = range_limit_table[val];

        val = RIGHT_SHIFT(tmp10 - tmp3, CONST_BITS + PASS1_BITS + 3) + 384;
        outptr[7] = range_limit_table[val];

        val = RIGHT_SHIFT(tmp11 + tmp2, CONST_BITS + PASS1_BITS + 3) + 384;
        outptr[1] = range_limit_table[val];

        val = RIGHT_SHIFT(tmp11 - tmp2, CONST_BITS + PASS1_BITS + 3) + 384;
        outptr[6] = range_limit_table[val];

        val = RIGHT_SHIFT(tmp12 + tmp1, CONST_BITS + PASS1_BITS + 3) + 384;
        outptr[2] = range_limit_table[val];

        val = RIGHT_SHIFT(tmp12 - tmp1, CONST_BITS + PASS1_BITS + 3) + 384;
        outptr[5] = range_limit_table[val];

        val = RIGHT_SHIFT(tmp13 + tmp0, CONST_BITS + PASS1_BITS + 3) + 384;
        outptr[3] = range_limit_table[val];

        val = RIGHT_SHIFT(tmp13 - tmp0, CONST_BITS + PASS1_BITS + 3) + 384;
        outptr[4] = range_limit_table[val];

        wsptr += DCTSIZE;
    }
}
