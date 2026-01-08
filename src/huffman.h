#ifndef HUFFMAN_H
#define HUFFMAN_H

#include "../include/jpeg_types.h"

/* Generate Huffman codes from BITS and HUFFVAL arrays */
void generate_huffman_codes(huffman_table_t *table);

/* Decode a symbol from the bit stream */
int decode_huffman_symbol(bit_reader_t *reader, huffman_table_t *table);

#endif /* HUFFMAN_H */
