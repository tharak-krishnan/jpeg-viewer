# JPEG Viewer - Custom JPEG Decoder

A JPEG image viewer written in C99 with a custom JPEG decoder built from scratch based on the JPEG standard (ITU-T T.81 / ISO/IEC 10918-1).

## Features

- **Custom JPEG decoder** implementing baseline sequential DCT-based JPEG
- Supports 8-bit precision
- Handles grayscale and YCbCr color spaces
- Supports various chroma subsampling (4:4:4, 4:2:2, 4:2:0)
- SDL2-based GUI for image display
- Command-line interface

## Requirements

- C99-compliant compiler (GCC or Clang)
- SDL2 library
- Make

## Installing SDL2

### macOS (using Homebrew)
```bash
brew install sdl2
```

### Ubuntu/Debian
```bash
sudo apt-get install libsdl2-dev
```

### Fedora/RHEL
```bash
sudo dnf install SDL2-devel
```

### Arch Linux
```bash
sudo pacman -S sdl2
```

## Building

```bash
make          # Build release version
make debug    # Build with debug symbols
make clean    # Clean build artifacts
```

## Usage

```bash
./bin/jpeg_viewer <jpeg_file>
```

Example:
```bash
./bin/jpeg_viewer test_images/sample.jpg
```

### Controls

- **ESC** - Close window and exit
- **Close button** - Exit application

## Project Structure

```
.
├── src/
│   ├── main.c              # Entry point
│   ├── jpeg_parser.c/h     # JPEG marker and segment parsing
│   ├── huffman.c/h         # Huffman code generation and decoding
│   ├── dct.c/h             # Inverse DCT implementation
│   ├── decoder.c/h         # Core JPEG decoding
│   ├── color.c/h           # YCbCr to RGB conversion
│   ├── display.c/h         # SDL2 display
│   └── utils.c/h           # Utilities (bit reading, etc.)
├── include/
│   └── jpeg_types.h        # Common data structures
├── test_images/            # Sample JPEG files
├── Makefile
└── README.md
```

## Implementation Details

### Supported JPEG Features

- **Baseline Sequential DCT** (SOF0)
- **8-bit sample precision**
- **Grayscale** (1 component)
- **YCbCr color** (3 components)
- **Huffman encoding** (DC and AC tables)
- **Quantization tables**
- **Chroma subsampling** (4:4:4, 4:2:2, 4:2:0)

### Not Supported

- Progressive JPEG
- Arithmetic coding
- 12-bit or 16-bit precision
- CMYK color space
- Lossless JPEG

## Algorithm Overview

1. **Parse JPEG file** - Extract markers (SOI, DQT, DHT, SOF0, SOS, EOI)
2. **Build Huffman tables** - Generate codes from BITS/HUFFVAL arrays
3. **Decode MCUs** - Process Minimum Coded Units (8x8 blocks)
4. **Huffman decode** - Decompress DC and AC coefficients
5. **Dequantize** - Multiply by quantization table values
6. **IDCT** - Inverse Discrete Cosine Transform (frequency → spatial)
7. **Color conversion** - YCbCr to RGB
8. **Display** - Render using SDL2

## Testing

To test with your own JPEG images:

1. Place JPEG files in `test_images/` directory
2. Run the viewer:
   ```bash
   ./bin/jpeg_viewer test_images/your_image.jpg
   ```

Best results with:
- Baseline JPEG files
- 8-bit precision
- Standard chroma subsampling

## Troubleshooting

### "SDL2/SDL.h file not found"
- Install SDL2 using the instructions above
- If using macOS with Homebrew, SDL2 headers should be in `/opt/homebrew/include/SDL2/`

### "Invalid Huffman code" errors
- The image may be progressive JPEG (not supported)
- Try with a baseline JPEG image

### Image appears corrupted
- Check that quantization and Huffman tables are correctly parsed
- Verify the image is baseline DCT-based JPEG

## References

- JPEG Standard: ITU-T T.81 / ISO/IEC 10918-1
- https://www.w3.org/Graphics/JPEG/itu-t81.pdf

## License

This is an educational project demonstrating JPEG decoding from scratch.
