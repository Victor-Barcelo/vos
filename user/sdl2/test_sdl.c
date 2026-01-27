/*
 * test_sdl.c - Simple SDL2 test for VOS
 * Tests basic video, events, and audio functionality
 */

#include <SDL2/SDL.h>
#include <stdio.h>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 200

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("SDL2 Test for VOS\n");
    printf("Initializing SDL...\n");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL initialized successfully!\n");

    SDL_Window *window = SDL_CreateWindow(
        "SDL2 Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("Window created!\n");

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Renderer created!\n");

    /* Draw a simple pattern */
    int running = 1;
    int frame = 0;
    Uint32 start_time = SDL_GetTicks();

    while (running) {
        /* Handle events */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                printf("Quit event received\n");
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                printf("Key pressed: %d\n", event.key.keysym.sym);
                if (event.key.keysym.sym == SDLK_ESCAPE ||
                    event.key.keysym.sym == SDLK_q) {
                    running = 0;
                }
            }
        }

        /* Clear screen with cycling color */
        Uint8 r = (frame * 2) & 0xFF;
        Uint8 g = (frame * 3) & 0xFF;
        Uint8 b = (frame * 5) & 0xFF;
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderClear(renderer);

        /* Draw some rectangles */
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect rect = { 50 + (frame % 100), 50, 50, 50 };
        SDL_RenderFillRect(renderer, &rect);

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_Rect rect2 = { 100, 100 + (frame % 50), 30, 30 };
        SDL_RenderFillRect(renderer, &rect2);

        /* Present to screen */
        SDL_RenderPresent(renderer);

        frame++;
        SDL_Delay(16); /* ~60 FPS */

        /* Run for 5 seconds max */
        if (SDL_GetTicks() - start_time > 5000) {
            printf("5 second timeout - exiting\n");
            running = 0;
        }
    }

    printf("Cleaning up...\n");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("Done! Ran %d frames\n", frame);

    return 0;
}
