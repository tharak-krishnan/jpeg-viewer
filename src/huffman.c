#include "huffman.h"
#include "utils.h"
#include <string.h>

/* Generate Huffman codes from BITS and HUFFVAL arrays per JPEG Annex C */
void generate_huffman_codes(huffman_table_t *table) {
    /* Initialize all code lengths to 0 */
    memset(table->code_lengths, 0, sizeof(table->code_lengths));
    memset(table->codes, 0, sizeof(table->codes));

    int code = 0;
    int k = 0;

    /* Generate codes for each bit length (1-16) */
    for (int len = 1; len <= 16; len++) {
        for (int i = 0; i < table->bits[len]; i++) {
            if (k >= 256) {
                fprintf(stderr, "Huffman table overflow\n");
                return;
            }

            uint8_t symbol = table->huffval[k];
            table->codes[symbol] = code;
            table->code_lengths[symbol] = len;

            code++;
            k++;
        }

        code <<= 1;  /* Shift for next code length */
    }
}

/* Decode a Huffman symbol from bit stream */
int decode_huffman_symbol(bit_reader_t *reader, huffman_table_t *table) {
    uint16_t code = 0;

    /* Try code lengths from 1 to 16 bits */
    for (int len = 1; len <= 16; len++) {
        /* Read one more bit */
        int bit = read_bit(reader);
        if (bit < 0) {
            return -1;  /* Error: end of stream */
        }

        code = (code << 1) | bit;

        /* Search through symbols with this code length */
        for (int i = 0; i < 256; i++) {
            if (table->code_lengths[i] == len && table->codes[i] == code) {
                return i;  /* Found the symbol */
            }
        }
    }

    /* No matching code found */
    fprintf(stderr, "Invalid Huffman code: 0x%04X\n", code);
    return -1;
}
