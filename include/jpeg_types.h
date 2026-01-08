#ifndef JPEG_TYPES_H
#define JPEG_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* JPEG Marker Definitions */
#define MARKER_SOI  0xFFD8  /* Start of Image */
#define MARKER_EOI  0xFFD9  /* End of Image */
#define MARKER_SOF0 0xFFC0  /* Start of Frame (Baseline DCT) */
#define MARKER_SOF1 0xFFC1  /* Start of Frame (Extended Sequential DCT) */
#define MARKER_SOF2 0xFFC2  /* Start of Frame (Progressive DCT) */
#define MARKER_SOF3 0xFFC3  /* Start of Frame (Lossless) */
#define MARKER_DHT  0xFFC4  /* Define Huffman Table */
#define MARKER_DQT  0xFFDB  /* Define Quantization Table */
#define MARKER_SOS  0xFFDA  /* Start of Scan */
#define MARKER_APP0 0xFFE0  /* Application Segment 0 (JFIF) */
#define MARKER_DRI  0xFFDD  /* Define Restart Interval */
#define MARKER_RST0 0xFFD0  /* Restart Marker 0 */
#define MARKER_RST7 0xFFD7  /* Restart Marker 7 */
#define MARKER_COM  0xFFFE  /* Comment */

/* Maximum values */
#define MAX_COMPONENTS 3
#define MAX_HUFFMAN_TABLES 4
#define MAX_QUANT_TABLES 4
#define BLOCK_SIZE 64

/* Quantization table (8x8 = 64 coefficients) */
typedef struct {
    uint8_t table[BLOCK_SIZE];
    bool is_set;
} quantization_table_t;

/* Huffman table */
typedef struct {
    uint8_t bits[17];           /* Number of codes of each length (1-16), bits[0] unused */
    uint8_t huffval[256];       /* Symbol values */
    uint16_t codes[256];        /* Generated Huffman codes for each symbol */
    uint8_t code_lengths[256];  /* Code length for each symbol */
    bool is_set;
} huffman_table_t;

/* Component information (Y, Cb, Cr) */
typedef struct {
    uint8_t id;                 /* Component identifier */
    uint8_t h_sampling;         /* Horizontal sampling factor */
    uint8_t v_sampling;         /* Vertical sampling factor */
    uint8_t quant_table_id;     /* Quantization table selector */
    uint8_t dc_table_id;        /* DC Huffman table selector */
    uint8_t ac_table_id;        /* AC Huffman table selector */
} component_info_t;

/* Frame header (SOF) */
typedef struct {
    uint8_t precision;          /* Sample precision (typically 8 bits) */
    uint16_t height;            /* Image height in pixels */
    uint16_t width;             /* Image width in pixels */
    uint8_t num_components;     /* Number of components (1=grayscale, 3=YCbCr) */
    component_info_t components[MAX_COMPONENTS];
} frame_header_t;

/* Bit reader for compressed data stream */
typedef struct {
    const uint8_t *data;        /* Pointer to compressed data */
    size_t data_size;           /* Size of data in bytes */
    size_t byte_pos;            /* Current byte position */
    uint8_t bit_pos;            /* Current bit position within byte (0-7) */
    uint32_t bit_buffer;        /* Buffer for bit operations */
    int bits_in_buffer;         /* Number of bits currently in buffer */
} bit_reader_t;

/* JPEG decoder state */
typedef struct {
    /* File data */
    uint8_t *data;              /* Raw JPEG file data */
    size_t data_size;           /* Size of file data */
    size_t current_pos;         /* Current position in file data */

    /* Tables */
    quantization_table_t quant_tables[MAX_QUANT_TABLES];
    huffman_table_t dc_tables[MAX_HUFFMAN_TABLES];
    huffman_table_t ac_tables[MAX_HUFFMAN_TABLES];

    /* Frame info */
    frame_header_t frame;

    /* Scan data */
    const uint8_t *scan_data;   /* Pointer to start of compressed scan data */
    size_t scan_data_size;      /* Size of scan data */

    /* MCU info */
    int mcu_width;              /* Number of MCUs horizontally */
    int mcu_height;             /* Number of MCUs vertically */
    int mcu_size_x;             /* MCU width in pixels */
    int mcu_size_y;             /* MCU height in pixels */
    int max_h_sampling;         /* Maximum horizontal sampling factor */
    int max_v_sampling;         /* Maximum vertical sampling factor */

    /* Component buffers (separate Y, Cb, Cr planes) */
    uint8_t *component_buffers[MAX_COMPONENTS];
    int component_width[MAX_COMPONENTS];
    int component_height[MAX_COMPONENTS];

    /* Decoded image data (final RGB or grayscale) */
    uint8_t *image_data;        /* RGB or grayscale output */
    int width;                  /* Output image width */
    int height;                 /* Output image height */
    int channels;               /* Number of channels (1 or 3) */

    /* DC prediction */
    int16_t dc_predictors[MAX_COMPONENTS];

    /* Restart interval */
    uint16_t restart_interval;  /* Number of MCUs between restart markers */
} jpeg_decoder_t;

/* Zigzag scan order for 8x8 blocks */
static const int zigzag[BLOCK_SIZE] = {
     0,  1,  5,  6, 14, 15, 27, 28,
     2,  4,  7, 13, 16, 26, 29, 42,
     3,  8, 12, 17, 25, 30, 41, 43,
     9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

#endif /* JPEG_TYPES_H */
