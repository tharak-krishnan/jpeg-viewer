#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "jpeg_parser.h"
#include "decoder.h"
#include "color.h"
#include "display.h"
#include "output.h"
#include "utils.h"

/* Get current time in microseconds */
static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

void print_usage(const char *program_name) {
    printf("JPEG Viewer - Custom JPEG Decoder\n");
    printf("Usage: %s <jpeg_file> [--save-ppm output.ppm]\n", program_name);
    printf("\n");
    printf("Example:\n");
    printf("  %s image.jpg\n", program_name);
    printf("  %s image.jpg --save-ppm output.ppm\n", program_name);
    printf("\n");
    printf("Controls:\n");
    printf("  ESC - Close window and exit\n");
}

int main(int argc, char *argv[]) {
    /* Check command line arguments */
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    const char *output_ppm = NULL;

    /* Parse optional --save-ppm argument */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--save-ppm") == 0 && i + 1 < argc) {
            output_ppm = argv[i + 1];
            i++;
        }
    }

    printf("========================================\n");
    printf("JPEG Viewer - Custom JPEG Decoder\n");
    printf("========================================\n");
    printf("File: %s\n\n", filename);

    double t_start, t_end;
    double parse_time, decode_time, color_time, total_time;

    /* Parse JPEG file */
    printf("Parsing JPEG file...\n");
    t_start = get_time_us();
    jpeg_decoder_t *decoder = jpeg_parser_init(filename);
    t_end = get_time_us();
    parse_time = (t_end - t_start) / 1000.0;  /* Convert to ms */

    if (!decoder) {
        fprintf(stderr, "Failed to parse JPEG file\n");
        return 1;
    }

    /* Decode JPEG data */
    t_start = get_time_us();
    if (jpeg_decode(decoder) != 0) {
        fprintf(stderr, "Failed to decode JPEG data\n");
        jpeg_parser_destroy(decoder);
        return 1;
    }
    t_end = get_time_us();
    decode_time = (t_end - t_start) / 1000.0;  /* Convert to ms */

    /* Convert to RGB */
    t_start = get_time_us();
    if (ycbcr_to_rgb(decoder) != 0) {
        fprintf(stderr, "Failed to convert color space\n");
        jpeg_parser_destroy(decoder);
        return 1;
    }
    t_end = get_time_us();
    color_time = (t_end - t_start) / 1000.0;  /* Convert to ms */

    total_time = parse_time + decode_time + color_time;

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
    printf("\n");
    printf("Performance Profile:\n");
    printf("  Parsing:         %8.2f ms (%5.1f%%)\n", parse_time, 100.0 * parse_time / total_time);
    printf("  Decoding:        %8.2f ms (%5.1f%%)\n", decode_time, 100.0 * decode_time / total_time);
    printf("  Color Convert:   %8.2f ms (%5.1f%%)\n", color_time, 100.0 * color_time / total_time);
    printf("  --------------------------------\n");
    printf("  Total:           %8.2f ms\n", total_time);
    printf("\n");

    /* Save PPM if requested */
    if (output_ppm) {
        if (save_ppm(output_ppm, decoder->image_data, decoder->width,
                    decoder->height, decoder->channels) != 0) {
            fprintf(stderr, "Failed to save PPM file\n");
        }
    }

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
