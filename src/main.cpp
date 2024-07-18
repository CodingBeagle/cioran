#include <iostream>

// SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char **argv) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO) != 0) {
        const char* sdl_error = SDL_GetError();

        std::cout << "Failed to initialize SDL: " << sdl_error << std::endl;
        terminate();
    }

    // Create a window
    SDL_Window* window = SDL_CreateWindow(
        "Cioran",
        800, 600,
        SDL_WINDOW_VULKAN);

    if (window == nullptr) {
        std::cout << "Failed to create window: " << SDL_GetError() << std::endl;
        terminate();
    }

    bool running = true;
    while (running) {
        // SDL_PollEvent is the favored way of receving system events since it can be done from the main loop and does not suspend the main loop
        // while waiting for an event to be posted.
        // Common practice is to use a while loop to process all events in the event queue.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
    }

    // Clean up SDL resources
    SDL_Quit();    

    return 0;
}