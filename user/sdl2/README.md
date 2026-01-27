# VOS SDL2 Shim (libvos-sdl2)

A minimal SDL2-compatible library for VOS that maps SDL2 API calls to VOS syscalls.

## Features

### Video
- `SDL_CreateWindow` / `SDL_DestroyWindow`
- `SDL_CreateRenderer` / `SDL_DestroyRenderer`
- `SDL_CreateTexture` / `SDL_UpdateTexture` / `SDL_DestroyTexture`
- `SDL_RenderClear` / `SDL_RenderCopy` / `SDL_RenderPresent`
- `SDL_RenderDrawPoint` / `SDL_RenderDrawLine` / `SDL_RenderDrawRect` / `SDL_RenderFillRect`
- `SDL_CreateRGBSurface` / `SDL_FreeSurface` / `SDL_BlitSurface`
- Double buffering support

### Audio
- `SDL_OpenAudio` / `SDL_CloseAudio`
- `SDL_PauseAudio`
- `SDL_PumpAudio` (VOS extension - call in main loop for audio)
- `SDL_MixAudio`

### Events
- `SDL_PollEvent` / `SDL_WaitEvent`
- Keyboard events (SDL_KEYDOWN, SDL_KEYUP)
- Mouse events (SDL_MOUSEMOTION, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP)
- SDL_QUIT (Ctrl+C)

### Timer
- `SDL_GetTicks` / `SDL_GetTicks64`
- `SDL_Delay`
- `SDL_GetPerformanceCounter` / `SDL_GetPerformanceFrequency`

## Building

```bash
cd user/sdl2
make
```

This creates `libvos-sdl2.a`.

## Usage

### Include Path
```c
#include <SDL2/SDL.h>
```

### Compile Flags
```
-I/path/to/user/sdl2/include
```

### Link Flags
```
-L/path/to/user/sdl2 -lvos-sdl2
```

## Important Notes

### No Threading
VOS has no threading support. Audio must be pumped manually:

```c
while (running) {
    SDL_PollEvent(&event);
    // ... game logic ...
    SDL_PumpAudio();  // Feed audio data
    SDL_RenderPresent(renderer);
}
```

### Supported Pixel Formats
- `SDL_PIXELFORMAT_ARGB8888` (recommended)
- `SDL_PIXELFORMAT_RGBA8888`

### VOS Syscalls Used
- Video: `sys_gfx_blit_rgba`, `sys_gfx_flip`, `sys_gfx_clear`, `sys_gfx_double_buffer`
- Audio: `sys_audio_open`, `sys_audio_write`, `sys_audio_close`
- Events: Terminal raw mode, `sys_poll`
- Timer: `sys_uptime_ms`, `sys_nanosleep`

## Example

See `test_sdl.c` for a complete example.

```c
#include <SDL2/SDL.h>

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Window *win = SDL_CreateWindow("Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        320, 200, SDL_WINDOW_SHOWN);

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 0);

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }

        SDL_SetRenderDrawColor(ren, 0, 0, 128, 255);
        SDL_RenderClear(ren);
        SDL_RenderPresent(ren);

        SDL_Delay(16);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
```

## Limitations

- No OpenGL support
- No joystick/gamepad support
- No window resizing (VOS uses fixed framebuffer)
- Audio requires manual pumping (no background threads)
- Limited to 44.1kHz audio (VOS SB16 driver limitation)
