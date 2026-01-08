#include "jpeg_parser.h"
#include "utils.h"
#include <string.h>

/* Initialize decoder and load file */
jpeg_decoder_t* jpeg_parser_init(const char *filename) {
    jpeg_decoder_t *decoder = (jpeg_decoder_t*)jpeg_malloc(sizeof(jpeg_decoder_t));
    memset(decoder, 0, sizeof(jpeg_decoder_t));

    /* Load file into memory */
    decoder->data = load_file(filename, &decoder->data_size);
    if (!decoder->data) {
        jpeg_free(decoder);
        return NULL;
    }

    decoder->current_pos = 0;

    /* Initialize tables as not set */
    for (int i = 0; i < MAX_QUANT_TABLES; i++) {
        decoder->quant_tables[i].is_set = false;
    }
    for (int i = 0; i < MAX_HUFFMAN_TABLES; i++) {
        decoder->dc_tables[i].is_set = false;
        decoder->ac_tables[i].is_set = false;
    }

    /* Parse all markers and segments */
    if (parse_jpeg_markers(decoder) != 0) {
        jpeg_parser_destroy(decoder);
        return NULL;
    }

    return decoder;
}

/* Find next marker in the stream */
uint16_t find_next_marker(jpeg_decoder_t *decoder) {
    while (decoder->current_pos < decoder->data_size - 1) {
        if (decoder->data[decoder->current_pos] == 0xFF) {
            uint8_t marker_low = decoder->data[decoder->current_pos + 1];

            /* Skip padding bytes (0xFF 0xFF) */
            if (marker_low == 0xFF) {
                decoder->current_pos++;
                continue;
            }

            /* Skip stuffed bytes (0xFF 0x00) */
            if (marker_low == 0x00) {
                decoder->current_pos += 2;
                continue;
            }

            /* Valid marker found */
            uint16_t marker = (0xFF << 8) | marker_low;
            decoder->current_pos += 2;
            return marker;
        }
        decoder->current_pos++;
    }

    return 0;  /* No marker found */
}

/* Parse all JPEG markers */
int parse_jpeg_markers(jpeg_decoder_t *decoder) {
    /* First marker must be SOI */
    uint16_t marker = find_next_marker(decoder);
    if (marker != MARKER_SOI) {
        fprintf(stderr, "Invalid JPEG: missing SOI marker\n");
        return -1;
    }

    printf("Found SOI marker\n");

    /* Parse markers until EOI */
    while (decoder->current_pos < decoder->data_size) {
        marker = find_next_marker(decoder);

        if (marker == 0) {
            fprintf(stderr, "Unexpected end of file\n");
            return -1;
        }

        switch (marker) {
            case MARKER_SOI:
                fprintf(stderr, "Unexpected SOI marker\n");
                return -1;

            case MARKER_EOI:
                printf("Found EOI marker\n");
                return 0;  /* End of image */

            case MARKER_APP0:
                printf("Parsing APP0 (JFIF) marker\n");
                if (parse_app0(decoder) != 0) return -1;
                break;

            case MARKER_DQT:
                printf("Parsing DQT (quantization table) marker\n");
                if (parse_dqt(decoder) != 0) return -1;
                break;

            case MARKER_DHT:
                printf("Parsing DHT (Huffman table) marker\n");
                if (parse_dht(decoder) != 0) return -1;
                break;

            case MARKER_SOF0:
                printf("Parsing SOF0 (baseline DCT) marker\n");
                if (parse_sof0(decoder) != 0) return -1;
                break;

            case MARKER_SOF1:
                printf("Parsing SOF1 (extended sequential DCT) marker\n");
                if (parse_sof0(decoder) != 0) return -1;
                break;

            case MARKER_SOF2:
                printf("Parsing SOF2 (progressive DCT) marker\n");
                if (parse_sof0(decoder) != 0) return -1;
                break;

            case MARKER_SOF3:
                printf("Parsing SOF3 (lossless) marker\n");
                if (parse_sof0(decoder) != 0) return -1;
                break;

            case MARKER_SOS:
                printf("Parsing SOS (start of scan) marker\n");
                if (parse_sos(decoder) != 0) return -1;
                /* SOS is followed by scan data, stop parsing markers */
                return 0;

            case MARKER_DRI:
                printf("Parsing DRI (restart interval) marker\n");
                if (parse_dri(decoder) != 0) return -1;
                break;

            case MARKER_COM:
                printf("Skipping COM (comment) marker\n");
                if (skip_marker_segment(decoder) != 0) return -1;
                break;

            default:
                /* Skip unknown markers */
                if (marker >= 0xFFE0 && marker <= 0xFFEF) {
                    printf("Skipping APP%d marker\n", marker & 0x0F);
                    if (skip_marker_segment(decoder) != 0) return -1;
                } else {
                    printf("Skipping unknown marker: 0x%04X\n", marker);
                    if (skip_marker_segment(decoder) != 0) return -1;
                }
                break;
        }
    }

    fprintf(stderr, "Unexpected end of file (missing EOI)\n");
    return -1;
}

/* Parse APP0 (JFIF) segment */
int parse_app0(jpeg_decoder_t *decoder) {
    if (decoder->current_pos + 2 > decoder->data_size) {
        JPEG_ERROR("Truncated APP0 segment");
    }

    uint16_t length = read_uint16_be(&decoder->data[decoder->current_pos]);
    decoder->current_pos += 2;

    if (decoder->current_pos + length - 2 > decoder->data_size) {
        JPEG_ERROR("Truncated APP0 data");
    }

    /* Just skip the APP0 data for now */
    decoder->current_pos += length - 2;

    return 0;
}

/* Parse DQT (Define Quantization Table) */
int parse_dqt(jpeg_decoder_t *decoder) {
    if (decoder->current_pos + 2 > decoder->data_size) {
        JPEG_ERROR("Truncated DQT segment");
    }

    uint16_t length = read_uint16_be(&decoder->data[decoder->current_pos]);
    decoder->current_pos += 2;

    size_t end_pos = decoder->current_pos + length - 2;

    while (decoder->current_pos < end_pos) {
        if (decoder->current_pos >= decoder->data_size) {
            JPEG_ERROR("Truncated DQT data");
        }

        uint8_t pq_tq = decoder->data[decoder->current_pos++];
        uint8_t precision = (pq_tq >> 4) & 0x0F;  /* 0=8-bit, 1=16-bit */
        uint8_t table_id = pq_tq & 0x0F;

        if (table_id >= MAX_QUANT_TABLES) {
            JPEG_ERROR("Invalid quantization table ID");
        }

        if (precision != 0) {
            JPEG_ERROR("Only 8-bit quantization tables supported");
        }

        /* Read 64 coefficients */
        for (int i = 0; i < 64; i++) {
            if (decoder->current_pos >= decoder->data_size) {
                JPEG_ERROR("Truncated quantization table");
            }
            decoder->quant_tables[table_id].table[i] = decoder->data[decoder->current_pos++];
        }

        decoder->quant_tables[table_id].is_set = true;
        printf("  Loaded quantization table %d\n", table_id);
    }

    return 0;
}

/* Parse DHT (Define Huffman Table) */
int parse_dht(jpeg_decoder_t *decoder) {
    if (decoder->current_pos + 2 > decoder->data_size) {
        JPEG_ERROR("Truncated DHT segment");
    }

    uint16_t length = read_uint16_be(&decoder->data[decoder->current_pos]);
    decoder->current_pos += 2;

    size_t end_pos = decoder->current_pos + length - 2;

    while (decoder->current_pos < end_pos) {
        if (decoder->current_pos >= decoder->data_size) {
            JPEG_ERROR("Truncated DHT data");
        }

        uint8_t tc_th = decoder->data[decoder->current_pos++];
        uint8_t table_class = (tc_th >> 4) & 0x0F;  /* 0=DC, 1=AC */
        uint8_t table_id = tc_th & 0x0F;

        if (table_id >= MAX_HUFFMAN_TABLES) {
            JPEG_ERROR("Invalid Huffman table ID");
        }

        huffman_table_t *table;
        if (table_class == 0) {
            table = &decoder->dc_tables[table_id];
        } else {
            table = &decoder->ac_tables[table_id];
        }

        /* Read BITS array (16 values) */
        table->bits[0] = 0;  /* Not used */
        int total_symbols = 0;
        for (int i = 1; i <= 16; i++) {
            if (decoder->current_pos >= decoder->data_size) {
                JPEG_ERROR("Truncated Huffman BITS");
            }
            table->bits[i] = decoder->data[decoder->current_pos++];
            total_symbols += table->bits[i];
        }

        if (total_symbols > 256) {
            JPEG_ERROR("Invalid Huffman table (too many symbols)");
        }

        /* Read HUFFVAL array */
        for (int i = 0; i < total_symbols; i++) {
            if (decoder->current_pos >= decoder->data_size) {
                JPEG_ERROR("Truncated Huffman HUFFVAL");
            }
            table->huffval[i] = decoder->data[decoder->current_pos++];
        }

        table->is_set = true;
        printf("  Loaded Huffman table (class=%d, id=%d)\n", table_class, table_id);
    }

    return 0;
}

/* Parse SOF0 (Start of Frame - Baseline DCT) */
int parse_sof0(jpeg_decoder_t *decoder) {
    if (decoder->current_pos + 2 > decoder->data_size) {
        JPEG_ERROR("Truncated SOF0 segment");
    }

    uint16_t length = read_uint16_be(&decoder->data[decoder->current_pos]);
    decoder->current_pos += 2;
    (void)length;  /* Length validated by subsequent checks */

    if (decoder->current_pos + 6 > decoder->data_size) {
        JPEG_ERROR("Truncated SOF0 data");
    }

    decoder->frame.precision = decoder->data[decoder->current_pos++];
    decoder->frame.height = read_uint16_be(&decoder->data[decoder->current_pos]);
    decoder->current_pos += 2;
    decoder->frame.width = read_uint16_be(&decoder->data[decoder->current_pos]);
    decoder->current_pos += 2;
    decoder->frame.num_components = decoder->data[decoder->current_pos++];

    printf("  Image: %dx%d, %d components, %d-bit precision\n",
           decoder->frame.width, decoder->frame.height,
           decoder->frame.num_components, decoder->frame.precision);

    if (decoder->frame.num_components > MAX_COMPONENTS) {
        JPEG_ERROR("Too many components");
    }

    if (decoder->frame.precision != 8) {
        JPEG_ERROR("Only 8-bit precision supported");
    }

    /* Read component specifications */
    for (int i = 0; i < decoder->frame.num_components; i++) {
        if (decoder->current_pos + 3 > decoder->data_size) {
            JPEG_ERROR("Truncated component info");
        }

        decoder->frame.components[i].id = decoder->data[decoder->current_pos++];
        uint8_t sampling = decoder->data[decoder->current_pos++];
        decoder->frame.components[i].h_sampling = (sampling >> 4) & 0x0F;
        decoder->frame.components[i].v_sampling = sampling & 0x0F;
        decoder->frame.components[i].quant_table_id = decoder->data[decoder->current_pos++];

        printf("  Component %d: H=%d, V=%d, Q=%d\n",
               i, decoder->frame.components[i].h_sampling,
               decoder->frame.components[i].v_sampling,
               decoder->frame.components[i].quant_table_id);
    }

    /* Calculate MCU dimensions */
    decoder->max_h_sampling = 0;
    decoder->max_v_sampling = 0;
    for (int i = 0; i < decoder->frame.num_components; i++) {
        if (decoder->frame.components[i].h_sampling > decoder->max_h_sampling) {
            decoder->max_h_sampling = decoder->frame.components[i].h_sampling;
        }
        if (decoder->frame.components[i].v_sampling > decoder->max_v_sampling) {
            decoder->max_v_sampling = decoder->frame.components[i].v_sampling;
        }
    }

    decoder->mcu_size_x = decoder->max_h_sampling * 8;
    decoder->mcu_size_y = decoder->max_v_sampling * 8;
    decoder->mcu_width = (decoder->frame.width + decoder->mcu_size_x - 1) / decoder->mcu_size_x;
    decoder->mcu_height = (decoder->frame.height + decoder->mcu_size_y - 1) / decoder->mcu_size_y;

    printf("  MCU: %dx%d pixels, %dx%d MCUs in image\n",
           decoder->mcu_size_x, decoder->mcu_size_y,
           decoder->mcu_width, decoder->mcu_height);

    return 0;
}

/* Parse SOS (Start of Scan) */
int parse_sos(jpeg_decoder_t *decoder) {
    if (decoder->current_pos + 2 > decoder->data_size) {
        JPEG_ERROR("Truncated SOS segment");
    }

    uint16_t length = read_uint16_be(&decoder->data[decoder->current_pos]);
    decoder->current_pos += 2;
    (void)length;  /* Length validated by subsequent checks */

    if (decoder->current_pos >= decoder->data_size) {
        JPEG_ERROR("Truncated SOS data");
    }

    uint8_t num_components = decoder->data[decoder->current_pos++];
    printf("  Scan has %d components\n", num_components);

    /* Read component selectors and table selectors */
    for (int i = 0; i < num_components; i++) {
        if (decoder->current_pos + 2 > decoder->data_size) {
            JPEG_ERROR("Truncated SOS component");
        }

        uint8_t component_id = decoder->data[decoder->current_pos++];
        uint8_t table_selector = decoder->data[decoder->current_pos++];

        /* Find matching component in frame */
        for (int j = 0; j < decoder->frame.num_components; j++) {
            if (decoder->frame.components[j].id == component_id) {
                decoder->frame.components[j].dc_table_id = (table_selector >> 4) & 0x0F;
                decoder->frame.components[j].ac_table_id = table_selector & 0x0F;
                printf("  Component %d: DC table %d, AC table %d\n",
                       j, decoder->frame.components[j].dc_table_id,
                       decoder->frame.components[j].ac_table_id);
                break;
            }
        }
    }

    /* Skip spectral selection and successive approximation (not used in baseline) */
    decoder->current_pos += 3;

    /* Remaining data until EOI or next marker is scan data */
    decoder->scan_data = &decoder->data[decoder->current_pos];
    decoder->scan_data_size = decoder->data_size - decoder->current_pos;

    printf("  Scan data starts at offset %zu\n", decoder->current_pos);

    return 0;
}

/* Parse DRI (Define Restart Interval) */
int parse_dri(jpeg_decoder_t *decoder) {
    if (decoder->current_pos + 2 > decoder->data_size) {
        JPEG_ERROR("Truncated DRI segment");
    }

    uint16_t length = read_uint16_be(&decoder->data[decoder->current_pos]);
    decoder->current_pos += 2;
    (void)length;  /* Length validated by subsequent checks */

    if (decoder->current_pos + 2 > decoder->data_size) {
        JPEG_ERROR("Truncated DRI data");
    }

    decoder->restart_interval = read_uint16_be(&decoder->data[decoder->current_pos]);
    decoder->current_pos += 2;

    printf("  Restart interval: %d MCUs\n", decoder->restart_interval);

    return 0;
}

/* Skip unknown marker segment */
int skip_marker_segment(jpeg_decoder_t *decoder) {
    if (decoder->current_pos + 2 > decoder->data_size) {
        JPEG_ERROR("Truncated marker segment");
    }

    uint16_t length = read_uint16_be(&decoder->data[decoder->current_pos]);
    decoder->current_pos += 2;

    if (decoder->current_pos + length - 2 > decoder->data_size) {
        JPEG_ERROR("Truncated marker data");
    }

    decoder->current_pos += length - 2;
    return 0;
}

/* Destroy decoder and free memory */
void jpeg_parser_destroy(jpeg_decoder_t *decoder) {
    if (decoder) {
        if (decoder->data) {
            jpeg_free(decoder->data);
        }
        if (decoder->image_data) {
            jpeg_free(decoder->image_data);
        }
        for (int i = 0; i < MAX_COMPONENTS; i++) {
            if (decoder->component_buffers[i]) {
                jpeg_free(decoder->component_buffers[i]);
            }
        }
        jpeg_free(decoder);
    }
}
