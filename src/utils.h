#ifndef UTILS_H
#define UTILS_H

#include "../include/jpeg_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error handling macros */
#define JPEG_ERROR(msg) do { fprintf(stderr, "JPEG Error: %s\n", msg); return -1; } while(0)
#define JPEG_ERROR_NULL(msg) do { fprintf(stderr, "JPEG Error: %s\n", msg); return NULL; } while(0)

/* Memory allocation helpers */
void* jpeg_malloc(size_t size);
void jpeg_free(void *ptr);

/* Bit reader functions */
void bit_reader_init(bit_reader_t *reader, const uint8_t *data, size_t data_size);
int read_bit(bit_reader_t *reader);
int read_bits(bit_reader_t *reader, int n);
int peek_bits(bit_reader_t *reader, int n);
void skip_bits(bit_reader_t *reader, int n);

/* JPEG-specific bit reading with byte stuffing */
uint8_t read_byte_from_scan(bit_reader_t *reader);
void fill_bit_buffer(bit_reader_t *reader, int min_bits);

/* Receive and extend (sign extension for DC/AC coefficients) */
int receive_and_extend(bit_reader_t *reader, int size);

/* File I/O helpers */
uint8_t* load_file(const char *filename, size_t *size);

/* Utility functions */
uint16_t read_uint16_be(const uint8_t *data);
int clamp(int value, int min, int max);

#endif /* UTILS_H */
