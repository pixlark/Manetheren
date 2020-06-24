#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <SDL2/SDL.h>

#include "raycast.h"

static void fatal(const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	fprintf(stderr, "\e[31m\e[1mencountered error\e[0m:\n");
	vfprintf(stderr, fmt, args);
	printf("\n");

	va_end(args);
	abort();
}

static void draw_line(SDL_Surface* surface, uint32_t color, int x1, int y1, int x2, int y2) {
    // Copied, cleaned up, and slightly modified from
    // https://stackoverflow.com/questions/10060046/drawing-lines-with-bresenhams-line-algorithm
    int x, y, dx, dy, dx1, dy1, px, py, xe, ye, i;
    dx = x2 - x1;
    dy = y2 - y1;
    dx1 = fabs(dx);
    dy1 = fabs(dy);
    px = 2 * dy1 - dx1;
    py = 2 * dx1 - dy1;

    if (SDL_LockSurface(surface) != 0) {
        fatal(SDL_GetError());
    }
    uint32_t* pixels = (uint32_t*) surface->pixels;

    auto put_pixel =
        [&](int x, int y) {
            if (x >= 0 && x < surface->w && y >= 0 && y < surface->h) {
                pixels[x + y * surface->w] = color;
            }
        };
    
    if (dy1 <= dx1) {
        if (dx >= 0) {
            x = x1;
            y = y1;
            xe = x2;
        } else {
            x = x2;
            y = y2;
            xe = x1;
        }
        put_pixel(x, y);
        for (i = 0; x < xe; i++) {
            x = x + 1;
            if (px<0) {
                px = px + 2 * dy1;
            } else {
                if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0)){
                    y = y + 1;
                } else {
                    y = y - 1;
                }
                px = px + 2 * (dy1 - dx1);
            }
            put_pixel(x, y);
        }
    } else {
        if (dy >= 0) {
            x = x1;
            y = y1;
            ye = y2;
        } else {
            x = x2;
            y = y2;
            ye = y1;
        }
        put_pixel(x, y);
        for (i = 0; y < ye; i++) {
            y = y + 1;
            if(py <= 0) {
                py = py + 2 * dx1;
            } else {
                if((dx < 0 && dy < 0) || (dx > 0 && dy > 0)) {
                    x = x + 1;
                } else {
                    x = x - 1;
                }
                py = py + 2 * (dx1 - dy1);
            }
            put_pixel(x, y);
        }
    }
    
    SDL_UnlockSurface(surface);
}

//
// v2
//

v2::v2(float x, float y) : x(x), y(y) {}

float v2::size() {
    return sqrt(x * x + y * y);
}

v2 v2::operator +(v2 v) {
    return v2(x + v.x, y + v.y);
}

void v2::operator +=(v2 v) {
    *this = *this + v;
}

v2 v2::operator -(v2 v) {
    return v2(x - v.x, y - v.y);
}

void v2::operator -=(v2 v) {
    *this = *this - v;
}

v2 v2::operator *(float f) {
    return v2(x * f, y * f);
}

void v2::operator *=(float f) {
    *this = *this * f;
}

v2 v2::operator /(float f) {
    return v2(x / f, y / f);
}

void v2::operator /=(float f) {
    *this = *this / f;
}

//
// Input
//

Input::Input() {
    keyboard_state = SDL_GetKeyboardState(&num_keys);
    last_frame_snapshot = new uint8_t[num_keys];
    resetCache();
}

Input::~Input() {
    delete[] last_frame_snapshot;
}

void Input::resetCache() {
    memcpy(last_frame_snapshot, keyboard_state, sizeof(uint8_t) * num_keys);
}

bool Input::keyPressed(SDL_Scancode code) {
    return keyboard_state[code] && !last_frame_snapshot[code];
}

bool Input::keyDown(SDL_Scancode code) {
    return keyboard_state[code];
}

bool Input::keyUp(SDL_Scancode code) {
    return !keyboard_state[code] && last_frame_snapshot[code];
}

//
// World
//

World::World(int width, int height)
    : width(width), height(height)
{
    walls = new bool[width * height]();
    
}

World::~World() {
    delete[] walls;
}

void World::set(int x, int y, bool val) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        walls[x + width * y] = val;
    }
}

bool World::get(int x, int y) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        return walls[x + width * y];
    }
    return false;
}

v2 World::nextBoundary(v2 pos, v2 dir) {
    v2 x_boundary(0, 0);
    float xb_dist;
    { // X
        float x_delta = (dir.x > 0 ? ceil(pos.x) : floor(pos.x)) - pos.x;
        float slope = dir.y / dir.x;
        float y_delta = x_delta * slope;
        x_boundary.x = pos.x + x_delta;
        x_boundary.y = pos.y + y_delta;
        xb_dist = v2(x_delta, y_delta).size();
    }
    v2 y_boundary(0, 0);
    float yb_dist;
    { // Y
        float y_delta = (dir.y > 0 ? ceil(pos.y) : floor(pos.y)) - pos.y;
        float inverse_slope = dir.x / dir.y;
        float x_delta = y_delta * inverse_slope;
        y_boundary.x = pos.x + x_delta;
        y_boundary.y = pos.y + y_delta;
        yb_dist = v2(x_delta, y_delta).size();
    }
    // Return the shorter
    return xb_dist < yb_dist ? x_boundary : y_boundary;
}

v2 World::wallBoundary(v2 pos, v2 dir, bool* world_edge) {
    while (pos.x >= 0 && pos.x < width && pos.y >= 0 && pos.y < height) {
        v2 boundary = nextBoundary(pos, dir);
        // Go slightly into the block we're facing so we can floor()
        v2 test_pos = boundary + dir * 0.01;
        int x = floor(test_pos.x), y = floor(test_pos.y);
        if (get(x, y)) {
            if (world_edge != nullptr) {
                *world_edge = false;
            }
            return boundary;
        }
        pos = test_pos;
    }
    if (world_edge != nullptr) {
        *world_edge = true;
    }
    return pos;
}

//
// Game
//

Game::Game(Engine* engine)
    : engine(engine), world(15, 15), player(v2(3.2, 3.2)), view_angle(0.0)
{
    world.set(6, 6, true);
    world.set(6, 7, true);
    world.set(7, 6, true);
    world.set(8, 6, true);
    world.set(8, 7, true);
}

void Game::update() {
    { // Move player
        auto d = v2(0, 0);
        d.x += engine->input->keyDown(SDL_SCANCODE_A) ? -1.0 : 0.0;
        d.x += engine->input->keyDown(SDL_SCANCODE_D) ? +1.0 : 0.0;
        d.y += engine->input->keyDown(SDL_SCANCODE_W) ? -1.0 : 0.0;
        d.y += engine->input->keyDown(SDL_SCANCODE_S) ? +1.0 : 0.0;
        const float speed = 5.0;
        player += d * speed * engine->delta;
    }
    { // Rotate player
        float dir = 0.0;
        dir += engine->input->keyDown(SDL_SCANCODE_RIGHT) ? +1.0 : 0.0;
        dir += engine->input->keyDown(SDL_SCANCODE_LEFT)  ? -1.0 : 0.0;
        const float speed = 2.0;
        view_angle += dir * speed * engine->delta;
    }
}

void Game::render3D(SDL_Surface* surface, int width, int height) {
    float left_view = view_angle - half_fov;
    float rads_per_pixel = fov / width;
    
    for (int x = 0; x < width; x++) {
        float angle = left_view + x * rads_per_pixel;
        v2 dir(cos(angle), sin(angle));
        bool world_edge;
        v2 boundary = world.wallBoundary(player, dir, &world_edge);
        float dist = (boundary - player).size();
        float r = (height / 2) / (dist + 1.0);

        SDL_Rect dest;
        dest.x = x;
        dest.y = (height / 2) - r;
        dest.w = 1;
        dest.h = r * 2;
        SDL_FillRect(
            surface, &dest,
            world_edge
              ? SDL_MapRGB(surface->format, 0xff, 0xff, 0xff)
              : SDL_MapRGB(surface->format, 0xff, 0, 0)
        );
    }
}

void Game::renderTopDown(SDL_Surface* surface, int size) {
    int box_size = size / world.height;
    
    // Boxes
    for (int y = 0; y < world.height; y++) {
        for (int x = 0; x < world.width; x++) {
            SDL_Rect rect;
            rect.x = x * box_size;
            rect.y = y * box_size;
            rect.w = box_size;
            rect.h = box_size;
            SDL_FillRect(
                surface, &rect,
                world.get(x, y)
                  ? SDL_MapRGB(surface->format, 0xff, 0, 0)
                  : SDL_MapRGB(surface->format, 0, 0, 0)
            );
        }
    }
    
    // Grid
    for (int i = 0; i < world.height; i++) {
        { // Horizontal
            SDL_Rect rect;
            rect.x = 0;
            rect.y = i * box_size;
            rect.w = size;
            rect.h = 1;
            SDL_FillRect(
                surface, &rect, SDL_MapRGB(surface->format, 0xff, 0xff, 0xff)
            );
        }
        {
            // Vertical
            SDL_Rect rect;
            rect.x = i * box_size;
            rect.y = 0;
            rect.w = 1;
            rect.h = size;
            SDL_FillRect(
                surface, &rect, SDL_MapRGB(surface->format, 0xff, 0xff, 0xff)
            );
        }
    }

    // Sight
    auto line_at_angle =
        [&] (uint32_t color, float theta) {
            auto dir = v2(cos(theta), sin(theta));
            v2 start = player;
            v2 end = world.wallBoundary(start, dir);
            
            int x1 = (start.x / world.width)  * size,
                y1 = (start.y / world.height) * size,
                x2 = (end.x   / world.width)  * size,
                y2 = (end.y   / world.height) * size;
            draw_line(
                surface, color,
                x1, y1, x2, y2
            );
        };
    line_at_angle(SDL_MapRGB(surface->format, 0, 0xff, 0), view_angle - half_fov);
    line_at_angle(SDL_MapRGB(surface->format, 0xff, 0xff, 0xff), view_angle);
    line_at_angle(SDL_MapRGB(surface->format, 0, 0xff, 0), view_angle + half_fov);

    { // Player
        const int radius = 10;
        SDL_Rect rect;
        rect.x = (player.x / world.width)  * size - radius;
        rect.y = (player.y / world.height) * size - radius;
        rect.w = radius * 2 + 1;
        rect.h = radius * 2 + 1;
        SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 0, 0xc3, 0xff));
    }
}

void Game::render() {
    // 3D view
    {
        int size = engine->height;
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
            0, size, size, sizeof(uint32_t) * 8, engine->canvas->format->format
        );

        render3D(surface, size, size);

        SDL_Rect dest;
        dest.x = 0;
        dest.y = 0;
        dest.w = size;
        dest.h = size;
        SDL_BlitSurface(surface, nullptr, engine->canvas, &dest);
        SDL_FreeSurface(surface);
    }
    // Top-down view
    {
        int size = engine->height;
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
            0, size, size, sizeof(uint32_t) * 8, engine->canvas->format->format
        );
        
        renderTopDown(surface, size);
        
        SDL_Rect dest;
        dest.x = engine->width - size;
        dest.y = 0;
        dest.w = size;
        dest.h = size;
        SDL_BlitSurface(surface, nullptr, engine->canvas, &dest);
        SDL_FreeSurface(surface);
    }
}

//
// Engine
//

Engine::Engine(const char* title, int width, int height)
    : game(this)
{
    this->width = width;
    this->height = height;

    int err;
    err = SDL_Init(SDL_INIT_EVERYTHING);
    if (err != 0) {
        fatal(SDL_GetError());
    }

    window = SDL_CreateWindow(
        title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN
    );
    if (window == nullptr) {
        fatal(SDL_GetError());
    }
            
    canvas = SDL_GetWindowSurface(window);
            
    last_tick = SDL_GetPerformanceCounter();
    delta = 1.0 / 60.0;

    input = new Input();
}

Engine::~Engine() {
    delete input;
    SDL_DestroyWindow(window);
    SDL_Quit();
}

bool Engine::frame() {
    // Consume events
    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event) != 0) {
        if (sdl_event.type == SDL_QUIT) {
            return false;
        }
    }

    if (input->keyPressed(SDL_SCANCODE_ESCAPE)) {
        return false;
    }
    
    // Update
    game.update();

    // Render
    SDL_FillRect(canvas, nullptr, SDL_MapRGB(canvas->format, 0xff, 0xff, 0xff));
    game.render();
    SDL_UpdateWindowSurface(window);
            
    { // Update delta
        uint64_t tick = SDL_GetPerformanceCounter();
        delta = (float) (tick - last_tick) / SDL_GetPerformanceFrequency();
        last_tick = tick;
    }

    // Constrain FPS
    const float max_fps = 60.0;
    if (delta < 1.0 / max_fps) {
        float remaining_time = (1.0 / max_fps) - delta;

        const uint64_t one_billion = 1000000000;
        uint64_t nanoseconds = (uint64_t) (delta * ((float) one_billion));
        struct timespec t;
        t.tv_sec  = nanoseconds / one_billion;
        t.tv_nsec = nanoseconds % one_billion;
        nanosleep(&t, nullptr);
    }

    return true;
}

int main() {
    Engine engine("Raycast", 1200, 600);
    while (engine.frame());
    
    return 0;
}
