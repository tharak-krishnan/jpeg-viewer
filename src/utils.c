#include "utils.h"

/* Memory allocation helpers */
void* jpeg_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    return ptr;
}

void jpeg_free(void *ptr) {
    if (ptr) {
        free(ptr);
    }
}

/* Initialize bit reader */
void bit_reader_init(bit_reader_t *reader, const uint8_t *data, size_t data_size) {
    reader->data = data;
    reader->data_size = data_size;
    reader->byte_pos = 0;
    reader->bit_pos = 0;
    reader->bit_buffer = 0;
    reader->bits_in_buffer = 0;
}

/* Read a single bit from the bit stream */
int read_bit(bit_reader_t *reader) {
    if (reader->bits_in_buffer == 0) {
        fill_bit_buffer(reader, 1);
    }

    if (reader->bits_in_buffer == 0) {
        return -1;  /* Error: no more data */
    }

    reader->bits_in_buffer--;
    return (reader->bit_buffer >> reader->bits_in_buffer) & 1;
}

/* Read n bits from the bit stream */
int read_bits(bit_reader_t *reader, int n) {
    if (n == 0) return 0;
    if (n > 16) return -1;  /* Maximum 16 bits at a time */

    while (reader->bits_in_buffer < n) {
        fill_bit_buffer(reader, n);
    }

    if (reader->bits_in_buffer < n) {
        return -1;  /* Error: not enough data */
    }

    reader->bits_in_buffer -= n;
    int result = (reader->bit_buffer >> reader->bits_in_buffer) & ((1 << n) - 1);

    return result;
}

/* Peek at n bits without consuming them */
int peek_bits(bit_reader_t *reader, int n) {
    if (n == 0) return 0;
    if (n > 16) return -1;

    while (reader->bits_in_buffer < n) {
        fill_bit_buffer(reader, n);
    }

    if (reader->bits_in_buffer < n) {
        return -1;
    }

    return (reader->bit_buffer >> (reader->bits_in_buffer - n)) & ((1 << n) - 1);
}

/* Skip n bits */
void skip_bits(bit_reader_t *reader, int n) {
    while (n > 0 && reader->bits_in_buffer > 0) {
        int to_skip = (n < reader->bits_in_buffer) ? n : reader->bits_in_buffer;
        reader->bits_in_buffer -= to_skip;
        n -= to_skip;
    }

    if (n > 0) {
        reader->byte_pos += n / 8;
        reader->bit_pos = n % 8;
    }
}

/* Read byte from scan data with byte stuffing handling */
uint8_t read_byte_from_scan(bit_reader_t *reader) {
    if (reader->byte_pos >= reader->data_size) {
        return 0;
    }

    uint8_t byte = reader->data[reader->byte_pos++];

    /* Handle byte stuffing: 0xFF 0x00 -> 0xFF */
    if (byte == 0xFF && reader->byte_pos < reader->data_size) {
        uint8_t next_byte = reader->data[reader->byte_pos];
        if (next_byte == 0x00) {
            reader->byte_pos++;  /* Skip the stuffed 0x00 */
        }
    }

    return byte;
}

/* Fill bit buffer with at least min_bits */
void fill_bit_buffer(bit_reader_t *reader, int min_bits) {
    while (reader->bits_in_buffer < min_bits && reader->byte_pos < reader->data_size) {
        uint8_t byte = read_byte_from_scan(reader);
        reader->bit_buffer = (reader->bit_buffer << 8) | byte;
        reader->bits_in_buffer += 8;

        /* Limit buffer size to prevent overflow */
        if (reader->bits_in_buffer > 24) {
            break;
        }
    }
}

/* Receive and extend - decode signed magnitude values */
int receive_and_extend(bit_reader_t *reader, int size) {
    if (size == 0) {
        return 0;
    }

    int value = read_bits(reader, size);

    if (value < 0) {
        return 0;  /* Error in reading bits */
    }

    /* Check if value is negative (high bit is 0) */
    int vt = 1 << (size - 1);
    if (value < vt) {
        /* Negative number: extend the sign */
        value = value - (1 << size) + 1;
    }

    return value;
}

/* Load entire file into memory */
uint8_t* load_file(const char *filename, size_t *size) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return NULL;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Allocate buffer */
    uint8_t *buffer = (uint8_t*)jpeg_malloc(*size);

    /* Read file */
    size_t read_size = fread(buffer, 1, *size, file);
    fclose(file);

    if (read_size != *size) {
        fprintf(stderr, "Failed to read complete file\n");
        jpeg_free(buffer);
        return NULL;
    }

    return buffer;
}

/* Read 16-bit big-endian value */
uint16_t read_uint16_be(const uint8_t *data) {
    return (data[0] << 8) | data[1];
}

/* Clamp value to range [min, max] */
int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
