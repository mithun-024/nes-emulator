#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <cstdio>
#include <string>
#include "System.hpp"
#include "Cartridge.hpp"

static const int kWinScale = 3;
static const int kWidth = 256;
static const int kHeight = 240;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::printf("NES Emulator - Milestone 6: PPU rendering + SDL2\n");
        std::printf("Usage: %s path/to/game.nes\n", argv[0]);
        return 1;
    }

    std::string err;
    auto cart = Cartridge::load(argv[1], &err);
    if (!cart) {
        std::fprintf(stderr, "Load failed: %s\n", err.c_str());
        return 1;
    }
    std::printf("Mapper %d, PRG %zuKB, CHR %zuKB\n", cart->mapperId(),
                cart->prgRom().size() / 1024, cart->chrRom().size() / 1024);

    System sys;
    if (!sys.bus.insertCartridge(std::move(cart), &err)) {
        std::fprintf(stderr, "Insert failed: %s\n", err.c_str());
        return 1;
    }
    sys.reset();
    std::printf("Reset -> PC = 0x%04X\n", sys.cpu.PC);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "NES Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kWidth * kWinScale, kHeight * kWinScale, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, 0);
    }
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, kWidth, kHeight);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             kWidth, kHeight);
    if (!texture) {
        std::fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    uint32_t lastTicks = SDL_GetTicks();
    int frames = 0;
    uint32_t fpsTicks = lastTicks;
    double fps = 0.0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN &&
                       event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        sys.runFrame();

        SDL_UpdateTexture(texture, nullptr, sys.bus.getPPU().framebuffer(),
                          kWidth * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        // Cap at ~60 fps when the host is faster than the emulator
        const uint32_t now = SDL_GetTicks();
        const uint32_t elapsed = now - lastTicks;
        if (elapsed < 16) {
            SDL_Delay(16 - elapsed);
        }
        lastTicks = SDL_GetTicks();

        ++frames;
        const uint32_t fpsElapsed = lastTicks - fpsTicks;
        if (fpsElapsed >= 1000) {
            fps = frames * 1000.0 / fpsElapsed;
            char title[128];
            std::snprintf(title, sizeof(title), "NES Emulator - %.1f fps", fps);
            SDL_SetWindowTitle(window, title);
            frames = 0;
            fpsTicks = lastTicks;
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
