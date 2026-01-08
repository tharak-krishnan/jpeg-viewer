#include "display.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

/* Display image using SDL2 */
int display_image(const uint8_t *image_data, int width, int height, int channels) {
    printf("\nInitializing SDL2...\n");

    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return -1;
    }

    /* Use best quality filtering for smooth scaling */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");  /* 2 = best quality (anisotropic) */

    /* Get display bounds to size window appropriately */
    SDL_DisplayMode display_mode;
    SDL_GetCurrentDisplayMode(0, &display_mode);

    int window_width = width;
    int window_height = height;
    float display_scale = 1.0f;

    /* Scale down large images to fit screen (with padding) */
    int max_width = display_mode.w - 100;
    int max_height = display_mode.h - 100;

    if (window_width > max_width || window_height > max_height) {
        float scale_w = (float)max_width / window_width;
        float scale_h = (float)max_height / window_height;
        display_scale = (scale_w < scale_h) ? scale_w : scale_h;

        window_width = (int)(window_width * display_scale);
        window_height = (int)(window_height * display_scale);

        printf("Window scaled to fit screen: %dx%d -> %dx%d (%.1f%%)\n",
               width, height, window_width, window_height, display_scale * 100);
    }

    /* Create window title with image info */
    char window_title[256];
    snprintf(window_title, sizeof(window_title),
             "JPEG Viewer - %dx%d", width, height);

    /* Create window */
    SDL_Window *window = SDL_CreateWindow(window_title,
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          window_width, window_height,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    /* Create renderer */
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                                                SDL_RENDERER_ACCELERATED |
                                                SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    /* Set logical size to original image dimensions for proper scaling */
    SDL_RenderSetLogicalSize(renderer, width, height);

    /* Create texture */
    SDL_Texture *texture = NULL;

    if (channels == 3) {
        /* RGB image */
        texture = SDL_CreateTexture(renderer,
                                   SDL_PIXELFORMAT_RGB24,
                                   SDL_TEXTUREACCESS_STATIC,
                                   width, height);
    } else if (channels == 1) {
        /* Grayscale - need to convert to RGB */
        uint8_t *rgb_data = (uint8_t*)malloc(width * height * 3);
        if (!rgb_data) {
            fprintf(stderr, "Memory allocation failed\n");
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return -1;
        }

        /* Convert grayscale to RGB */
        for (int i = 0; i < width * height; i++) {
            rgb_data[i * 3 + 0] = image_data[i];
            rgb_data[i * 3 + 1] = image_data[i];
            rgb_data[i * 3 + 2] = image_data[i];
        }

        texture = SDL_CreateTexture(renderer,
                                   SDL_PIXELFORMAT_RGB24,
                                   SDL_TEXTUREACCESS_STATIC,
                                   width, height);

        if (texture) {
            SDL_UpdateTexture(texture, NULL, rgb_data, width * 3);
        }

        free(rgb_data);
    } else {
        fprintf(stderr, "Unsupported number of channels: %d\n", channels);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    /* Update texture with image data (for RGB) */
    if (channels == 3) {
        SDL_UpdateTexture(texture, NULL, image_data, width * 3);
    }

    printf("Display initialized successfully\n");
    printf("Image resolution: %dx%d pixels\n", width, height);
    if (display_scale < 1.0f) {
        printf("Window size: %dx%d (scaled to %.0f%% to fit screen)\n",
               window_width, window_height, display_scale * 100);
        printf("Note: Window is resizable - resize to see more detail!\n");
    } else {
        printf("Window size: %dx%d (native resolution)\n", window_width, window_height);
    }
    printf("Press ESC or close window to exit\n\n");

    /* Event loop */
    int running = 1;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                }
            }
        }

        /* Clear screen */
        SDL_RenderClear(renderer);

        /* Render texture */
        SDL_RenderCopy(renderer, texture, NULL, NULL);

        /* Present */
        SDL_RenderPresent(renderer);

        /* Small delay to reduce CPU usage */
        SDL_Delay(16);  /* ~60 FPS */
    }

    /* Cleanup */
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Display closed\n");
    return 0;
}
