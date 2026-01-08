#include <stdio.h>
#include <stdlib.h>
#include "jpeg_parser.h"
#include "decoder.h"
#include "color.h"
#include "display.h"
#include "utils.h"

void print_usage(const char *program_name) {
    printf("JPEG Viewer - Custom JPEG Decoder\n");
    printf("Usage: %s <jpeg_file>\n", program_name);
    printf("\n");
    printf("Example:\n");
    printf("  %s image.jpg\n", program_name);
    printf("\n");
    printf("Controls:\n");
    printf("  ESC - Close window and exit\n");
}

int main(int argc, char *argv[]) {
    /* Check command line arguments */
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *filename = argv[1];

    printf("========================================\n");
    printf("JPEG Viewer - Custom JPEG Decoder\n");
    printf("========================================\n");
    printf("File: %s\n\n", filename);

    /* Parse JPEG file */
    printf("Parsing JPEG file...\n");
    jpeg_decoder_t *decoder = jpeg_parser_init(filename);
    if (!decoder) {
        fprintf(stderr, "Failed to parse JPEG file\n");
        return 1;
    }

    /* Decode JPEG data */
    if (jpeg_decode(decoder) != 0) {
        fprintf(stderr, "Failed to decode JPEG data\n");
        jpeg_parser_destroy(decoder);
        return 1;
    }

    /* Convert to RGB */
    if (ycbcr_to_rgb(decoder) != 0) {
        fprintf(stderr, "Failed to convert color space\n");
        jpeg_parser_destroy(decoder);
        return 1;
    }

    /* Free component buffers to reduce memory usage */
    for (int i = 0; i < MAX_COMPONENTS; i++) {
        if (decoder->component_buffers[i]) {
            jpeg_free(decoder->component_buffers[i]);
            decoder->component_buffers[i] = NULL;
        }
    }

    /* Free original JPEG data - no longer needed */
    if (decoder->data) {
        jpeg_free(decoder->data);
        decoder->data = NULL;
    }

    /* Display image */
    printf("\n========================================\n");
    printf("Decoded successfully!\n");
    printf("Image: %dx%d, %d channel(s)\n",
           decoder->width, decoder->height, decoder->channels);
    printf("Memory optimized for display\n");
    printf("========================================\n");

    if (display_image(decoder->image_data, decoder->width,
                     decoder->height, decoder->channels) != 0) {
        fprintf(stderr, "Failed to display image\n");
        jpeg_parser_destroy(decoder);
        return 1;
    }

    /* Cleanup */
    jpeg_parser_destroy(decoder);

    printf("\nProgram exited successfully\n");
    return 0;
}
