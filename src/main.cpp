#include <SDL2/SDL.h>
#include <stdlib.h>
#include <time.h>

#define WINDOW_WIDTH 2400
#define WINDOW_HEIGHT 1800
#define NUM_POINTS 1000
#define MIN_PIXELS_PER_SECOND 30
#define MAX_PIXELS_PER_SECOND 60
#define POINT_SIZE 3 // Size of each point in pixels

static SDL_Window*   window   = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_FPoint    points[NUM_POINTS];
static float         point_speeds[NUM_POINTS];

float randf() {
    return (float)rand() / (float)RAND_MAX;
}

int main(int argc, char* argv[]) {
    srand(time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("SDL2 Points Example",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH,
                              WINDOW_HEIGHT,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("Couldn't create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Initialize points
    for (int i = 0; i < NUM_POINTS; i++) {
        points[i].x = randf() * WINDOW_WIDTH;
        points[i].y = randf() * WINDOW_HEIGHT;
        point_speeds[i] =
            MIN_PIXELS_PER_SECOND +
            (randf() * (MAX_PIXELS_PER_SECOND - MIN_PIXELS_PER_SECOND));
    }

    Uint64    last_time = SDL_GetTicks64();
    int       running   = 1;
    SDL_Event event;

    // Main game loop
    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        // Calculate delta time
        Uint64 now     = SDL_GetTicks64();
        float  elapsed = (now - last_time) / 1000.0f;
        last_time      = now;

        // Update points
        for (int i = 0; i < NUM_POINTS; i++) {
            float distance = elapsed * point_speeds[i];
            points[i].x += distance;
            points[i].y += distance;

            if (points[i].x >= WINDOW_WIDTH || points[i].y >= WINDOW_HEIGHT) {
                if (rand() % 2) {
                    points[i].x = randf() * WINDOW_WIDTH;
                    points[i].y = 0.0f;
                } else {
                    points[i].x = 0.0f;
                    points[i].y = randf() * WINDOW_HEIGHT;
                }
                point_speeds[i] =
                    MIN_PIXELS_PER_SECOND +
                    (randf() * (MAX_PIXELS_PER_SECOND - MIN_PIXELS_PER_SECOND));
            }
        }

        // Render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        // Draw points as rectangles
        for (int i = 0; i < NUM_POINTS; i++) {
            SDL_FRect rect = {points[i].x - POINT_SIZE / 2,
                              points[i].y - POINT_SIZE / 2,
                              POINT_SIZE,
                              POINT_SIZE};
            SDL_RenderFillRectF(renderer, &rect);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
