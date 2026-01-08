#ifndef DCT_H
#define DCT_H

#include <stdint.h>

/* Apply 2D inverse DCT to an 8x8 block */
void idct_2d(int16_t *input_block, uint8_t *output_block);

/* Dequantize a block using quantization table */
void dequantize_block(int16_t *block, const uint8_t *quant_table);

#endif /* DCT_H */
