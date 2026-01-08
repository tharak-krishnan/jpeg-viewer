#ifndef JPEG_PARSER_H
#define JPEG_PARSER_H

#include "../include/jpeg_types.h"

/* Initialize decoder and parse JPEG file */
jpeg_decoder_t* jpeg_parser_init(const char *filename);

/* Parse JPEG markers and segments */
int parse_jpeg_markers(jpeg_decoder_t *decoder);

/* Individual marker parsers */
int parse_soi(jpeg_decoder_t *decoder);
int parse_app0(jpeg_decoder_t *decoder);
int parse_dqt(jpeg_decoder_t *decoder);
int parse_dht(jpeg_decoder_t *decoder);
int parse_sof0(jpeg_decoder_t *decoder);
int parse_sos(jpeg_decoder_t *decoder);
int parse_dri(jpeg_decoder_t *decoder);

/* Helper functions */
uint16_t find_next_marker(jpeg_decoder_t *decoder);
int skip_marker_segment(jpeg_decoder_t *decoder);

/* Cleanup */
void jpeg_parser_destroy(jpeg_decoder_t *decoder);

#endif /* JPEG_PARSER_H */
