#include "decoder.h"
#include "huffman.h"
#include "dct.h"
#include "utils.h"
#include <string.h>
#include <sys/time.h>

/* Profiling timers (in microseconds) */
static double huffman_time_us = 0.0;
static double idct_time_us = 0.0;

static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

/* Main JPEG decoding function */
int jpeg_decode(jpeg_decoder_t *decoder) {
    printf("\nStarting JPEG decode...\n");

    /* Reset profiling timers */
    huffman_time_us = 0.0;
    idct_time_us = 0.0;

    /* Generate Huffman codes from tables */
    for (int i = 0; i < MAX_HUFFMAN_TABLES; i++) {
        if (decoder->dc_tables[i].is_set) {
            generate_huffman_codes(&decoder->dc_tables[i]);
            printf("Generated DC Huffman codes for table %d\n", i);
        }
        if (decoder->ac_tables[i].is_set) {
            generate_huffman_codes(&decoder->ac_tables[i]);
            printf("Generated AC Huffman codes for table %d\n", i);
        }
    }

    /* Allocate component buffers */
    for (int i = 0; i < decoder->frame.num_components; i++) {
        component_info_t *comp = &decoder->frame.components[i];

        /* Calculate component dimensions based on sampling factors */
        decoder->component_width[i] = (decoder->frame.width * comp->h_sampling +
                                      decoder->max_h_sampling - 1) / decoder->max_h_sampling;
        decoder->component_height[i] = (decoder->frame.height * comp->v_sampling +
                                       decoder->max_v_sampling - 1) / decoder->max_v_sampling;

        /* Round up to multiple of 8 */
        decoder->component_width[i] = ((decoder->component_width[i] + 7) / 8) * 8;
        decoder->component_height[i] = ((decoder->component_height[i] + 7) / 8) * 8;

        size_t buffer_size = decoder->component_width[i] * decoder->component_height[i];
        decoder->component_buffers[i] = (uint8_t*)jpeg_malloc(buffer_size);
        memset(decoder->component_buffers[i], 0, buffer_size);

        printf("Component %d buffer: %dx%d\n",
               i, decoder->component_width[i], decoder->component_height[i]);
    }

    /* Initialize bit reader for scan data */
    bit_reader_t reader;
    bit_reader_init(&reader, decoder->scan_data, decoder->scan_data_size);

    /* Initialize DC predictors */
    for (int i = 0; i < MAX_COMPONENTS; i++) {
        decoder->dc_predictors[i] = 0;
    }

    /* Decode all MCUs */
    printf("Decoding %d x %d MCUs...\n", decoder->mcu_width, decoder->mcu_height);

    int mcu_count = 0;
    for (int mcu_row = 0; mcu_row < decoder->mcu_height; mcu_row++) {
        for (int mcu_col = 0; mcu_col < decoder->mcu_width; mcu_col++) {
            /* Check for restart marker if restart interval is set */
            if (decoder->restart_interval > 0 && mcu_count > 0 &&
                mcu_count % decoder->restart_interval == 0) {
                /* Reset DC predictors */
                for (int i = 0; i < MAX_COMPONENTS; i++) {
                    decoder->dc_predictors[i] = 0;
                }

                /* Align to byte boundary */
                reader.bits_in_buffer = 0;
                reader.bit_buffer = 0;

                /* Skip restart marker (RST0-RST7) */
                if (reader.byte_pos < reader.data_size - 1) {
                    if (reader.data[reader.byte_pos] == 0xFF) {
                        uint8_t marker = reader.data[reader.byte_pos + 1];
                        if (marker >= 0xD0 && marker <= 0xD7) {
                            reader.byte_pos += 2;  /* Skip RST marker */
                        }
                    }
                }
            }

            if (decode_mcu(decoder, &reader, mcu_row, mcu_col) != 0) {
                fprintf(stderr, "Failed to decode MCU at (%d, %d)\n", mcu_col, mcu_row);
                return -1;
            }
            mcu_count++;
        }

        if ((mcu_row + 1) % 10 == 0) {
            printf("  Decoded %d / %d rows\n", mcu_row + 1, decoder->mcu_height);
        }
    }

    printf("Decoding complete!\n");
    printf("  Huffman decoding: %.2f ms\n", huffman_time_us / 1000.0);
    printf("  IDCT:             %.2f ms\n", idct_time_us / 1000.0);
    return 0;
}

/* Decode a single MCU */
int decode_mcu(jpeg_decoder_t *decoder, bit_reader_t *reader, int mcu_row, int mcu_col) {
    double t_start, t_end;

    /* Decode each component in the MCU */
    for (int comp = 0; comp < decoder->frame.num_components; comp++) {
        component_info_t *component = &decoder->frame.components[comp];

        /* Decode all blocks for this component in the MCU */
        for (int v = 0; v < component->v_sampling; v++) {
            for (int h = 0; h < component->h_sampling; h++) {
                int16_t block[64];
                memset(block, 0, sizeof(block));

                /* Decode block (Huffman decoding) */
                t_start = get_time_us();
                if (decode_block(decoder, reader,
                               &decoder->dc_tables[component->dc_table_id],
                               &decoder->ac_tables[component->ac_table_id],
                               &decoder->dc_predictors[comp],
                               block) != 0) {
                    return -1;
                }
                t_end = get_time_us();
                huffman_time_us += (t_end - t_start);

                /* Apply IDCT with integrated dequantization */
                t_start = get_time_us();
                uint8_t spatial_block[64];
                idct_2d(block, decoder->quant_tables[component->quant_table_id].table, spatial_block);
                t_end = get_time_us();
                idct_time_us += (t_end - t_start);

                /* Store in component buffer */
                store_block(decoder, comp, mcu_row, mcu_col, h, v, spatial_block);
            }
        }
    }

    return 0;
}

/* Decode a single 8x8 block */
int decode_block(jpeg_decoder_t *decoder, bit_reader_t *reader,
                 huffman_table_t *dc_table, huffman_table_t *ac_table,
                 int16_t *dc_predictor, int16_t *block) {
    (void)decoder;  /* Unused parameter */

    /* Decode DC coefficient */
    int dc_symbol = decode_huffman_symbol(reader, dc_table);
    if (dc_symbol < 0) {
        fprintf(stderr, "Failed to decode DC symbol\n");
        return -1;
    }

    int dc_diff = receive_and_extend(reader, dc_symbol);
    *dc_predictor += dc_diff;
    block[0] = *dc_predictor;

    /* Decode 63 AC coefficients */
    int k = 1;
    while (k < 64) {
        int ac_symbol = decode_huffman_symbol(reader, ac_table);
        if (ac_symbol < 0) {
            fprintf(stderr, "Failed to decode AC symbol\n");
            return -1;
        }

        /* EOB (End of Block) */
        if (ac_symbol == 0x00) {
            break;
        }

        /* ZRL (Zero Run Length - 16 zeros) */
        if (ac_symbol == 0xF0) {
            k += 16;
            continue;
        }

        /* Extract run length and size */
        int run = (ac_symbol >> 4) & 0x0F;   /* High nibble: zero run */
        int size = ac_symbol & 0x0F;         /* Low nibble: magnitude bits */

        /* Skip zeros */
        k += run;

        if (k >= 64) {
            break;
        }

        /* Decode coefficient value and store in natural order */
        int value = receive_and_extend(reader, size);
        extern const int jpeg_natural_order[64];
        block[jpeg_natural_order[k]] = value;

        k++;
    }

    return 0;
}

/* Store 8x8 block into component buffer */
void store_block(jpeg_decoder_t *decoder, int component,
                 int mcu_row, int mcu_col, int block_h, int block_v,
                 const uint8_t *block_data) {
    component_info_t *comp = &decoder->frame.components[component];
    uint8_t *buffer = decoder->component_buffers[component];
    int width = decoder->component_width[component];

    /* Calculate block position in component buffer */
    int block_x = mcu_col * comp->h_sampling * 8 + block_h * 8;
    int block_y = mcu_row * comp->v_sampling * 8 + block_v * 8;

    /* Copy 8x8 block into buffer */
    for (int y = 0; y < 8; y++) {
        int dest_y = block_y + y;
        if (dest_y >= decoder->component_height[component]) {
            break;
        }

        for (int x = 0; x < 8; x++) {
            int dest_x = block_x + x;
            if (dest_x >= decoder->component_width[component]) {
                break;
            }

            buffer[dest_y * width + dest_x] = block_data[y * 8 + x];
        }
    }
}
