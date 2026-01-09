#ifndef DCT_H
#define DCT_H

#include <stdint.h>

/* Apply 2D inverse DCT to an 8x8 block with integrated dequantization */
void idct_2d(int16_t *input_block, const uint8_t *quant_table, uint8_t *output_block);

#endif /* DCT_H */
