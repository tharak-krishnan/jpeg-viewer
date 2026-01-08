#ifndef DECODER_H
#define DECODER_H

#include "../include/jpeg_types.h"

/* Main decoding function */
int jpeg_decode(jpeg_decoder_t *decoder);

/* Decode a single MCU (Minimum Coded Unit) */
int decode_mcu(jpeg_decoder_t *decoder, bit_reader_t *reader, int mcu_row, int mcu_col);

/* Decode a single 8x8 block */
int decode_block(jpeg_decoder_t *decoder, bit_reader_t *reader,
                 huffman_table_t *dc_table, huffman_table_t *ac_table,
                 int16_t *dc_predictor, int16_t *block);

/* Store decoded 8x8 block into component buffer */
void store_block(jpeg_decoder_t *decoder, int component,
                 int mcu_row, int mcu_col, int block_h, int block_v,
                 const uint8_t *block_data);

#endif /* DECODER_H */
