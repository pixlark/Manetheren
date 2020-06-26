#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <tuple>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "raycast.h"

[[ noreturn ]] static void fatal(const char * fmt, ...)
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

static SDL_Surface* loadSurface(const char* path, uint32_t format) {
    int w, h, c;
    uint8_t* pixels = stbi_load(path, &w, &h, &c, 4);
    if (pixels == nullptr) {
        fatal("Could not load resource at '%s'", path);
    }
    SDL_Surface* original = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, w, h, 4 * 8, w * 4, SDL_PIXELFORMAT_RGBA32
    );
    SDL_PixelFormat* correct_format = SDL_AllocFormat(format);

    SDL_Surface* converted = SDL_ConvertSurface(original, correct_format, 0);
    
    SDL_FreeFormat(correct_format);
    SDL_FreeSurface(original);
    return converted;
}

static float clamp_angle(float theta) {
    theta = fmod(theta, 2 * M_PI);
    if (theta < 0) {
        theta += 2 * M_PI;
    }
    return theta;
}

static bool circle_aabb_intersect(v2 c, float r, v2 b1, v2 b2) {
    float top    = b1.y;
    float bottom = b2.y;
    float left   = b1.x;
    float right  = b2.x;

    bool within_x = c.x >= left && c.x <= right;
    bool within_y = c.y >= top && c.y <= bottom;

    // Inside
    if (within_x && within_y) {
        return true;
    }
    // Above/Below
    if (within_x && !within_y) {
        return fmin(abs(c.y - top), abs(c.y - bottom)) < r;
    }
    // Left/Right
    if (!within_x && within_y) {
        return fmin(abs(c.x - left), abs(c.x - right)) < r;
    }
    // Top-left?
    if ((c - b1).size() < r) {
        return true;
    }
    // Top-right?
    if ((c - v2(right, top)).size() < r) {
        return true;
    }
    // Bottom-left?
    if ((c - v2(left, bottom)).size() < r) {
        return true;
    }
    // Bottom-right?
    if ((c - b2).size() < r) {
        return true;
    }
    // No corners are inside circle
    return false;
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
// Box
//

Box::Box() : pos(v2(0, 0)), bounds(v2(0, 0)) {}

float Box::top() {
    return pos.y;
}

float Box::left() {
    return pos.x;
}

float Box::bottom() {
    return pos.y + bounds.y;
}

float Box::right() {
    return pos.x + bounds.x;
}

v2 Box::center() {
    return pos + bounds / 2;
}

Box Box::from_center(v2 center, v2 half_bounds) {
    Box box;
    box.pos = center - half_bounds;
    box.bounds = half_bounds * 2;
    return box;
}

bool Box::intersect(Box a, Box b) {
    return
		(a.left() < b.right()) &&
		(a.right() > b.left()) &&
		(a.top() < b.bottom()) &&
		(a.bottom() > b.top());
}

//
// Input
//

Input::Input() {
    keyboard_state = SDL_GetKeyboardState(&num_keys);
    last_frame_snapshot = new uint8_t[num_keys];

    mouse_state = SDL_GetMouseState(nullptr, nullptr);
    
    resetCache();
}

Input::~Input() {
    delete[] last_frame_snapshot;
}

void Input::resetCache() {
    memcpy(last_frame_snapshot, keyboard_state, sizeof(uint8_t) * num_keys);
    
    last_mouse_state = mouse_state;
    mouse_state = SDL_GetMouseState(nullptr, nullptr);

    motion = v2(0, 0);
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

bool Input::btnPressed(uint8_t btn) {
    return (mouse_state & SDL_BUTTON(btn)) && !(last_mouse_state & SDL_BUTTON(btn));
}

bool Input::btnDown(uint8_t btn) {
    return (mouse_state & SDL_BUTTON(btn));
}

bool Input::btnUp(uint8_t btn) {
    return !(mouse_state & SDL_BUTTON(btn)) && (last_mouse_state & SDL_BUTTON(btn));
}

v2 Input::mousePos() {
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    return v2((float) mx, (float) my);
}

//
// World
//

World::World(int width, int height)
    : width(width), height(height)
{
    walls = new Block[width * height]();
}

World::~World() {
    delete[] walls;
}

void World::set(int x, int y, Block type) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        walls[x + width * y] = type;
    }
}

Block World::get(int x, int y) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        return walls[x + width * y];
    }
    return OUTER_WALL; // Anything outside of the map is untraversable, so consider it a wall.
}

v2 World::nextBoundary(v2 pos, v2 dir, Direction* hit_dir) {
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
    if (hit_dir != nullptr) {
        *hit_dir = xb_dist < yb_dist ? VERTICAL : HORIZONTAL;
    }
    return xb_dist < yb_dist ? x_boundary : y_boundary;
}

WallInfo::WallInfo(Direction dir, Block type) : dir(dir), type(type) {}

v2 World::wallBoundary(v2 pos, v2 dir, WallInfo* wall_info) {
    Direction hit_dir;
    while (pos.x >= 0 && pos.x < width && pos.y >= 0 && pos.y < height) {
        v2 boundary = nextBoundary(pos, dir, &hit_dir);
        // Go slightly into the block we're facing so we can floor()
        v2 test_pos = boundary + dir * 0.0001;
        int x = floor(test_pos.x), y = floor(test_pos.y);
        if (get(x, y)) {
            if (wall_info != nullptr) {
                *wall_info = WallInfo(hit_dir, get(x, y));
            }
            return boundary;
        }
        pos = test_pos;
    }
    if (wall_info != nullptr) {
        *wall_info = WallInfo(hit_dir, OUTER_WALL);
    }
    return pos;
}

//
// Game
//

Game::Game(Engine* engine)
    : engine(engine), world(15, 15), player(v2(4.778035, 0.495602)), view_angle(-0.667112)
{
    world.set(6, 6, INNER_WALL);
    world.set(6, 7, INNER_WALL);
    world.set(7, 6, INNER_WALL);
    world.set(8, 6, INNER_WALL);
    world.set(8, 7, INNER_WALL);

    dark_wall  = loadSurface("res/dark-wall.bmp", engine->canvas->format->format);
    light_wall = loadSurface("res/light-wall.bmp", engine->canvas->format->format);
    sky        = loadSurface("res/cloud.bmp", engine->canvas->format->format);
}

Game::~Game() {
    SDL_FreeSurface(dark_wall);
    SDL_FreeSurface(light_wall);
    SDL_FreeSurface(sky);
}

void Game::update() {
    { // Move player
        v2 inp = v2(0, 0);
        inp.x += engine->input->keyDown(SDL_SCANCODE_A) ? -1.0 : 0.0;
        inp.x += engine->input->keyDown(SDL_SCANCODE_D) ? +1.0 : 0.0;
        inp.y += engine->input->keyDown(SDL_SCANCODE_W) ? -1.0 : 0.0;
        inp.y += engine->input->keyDown(SDL_SCANCODE_S) ? +1.0 : 0.0;

        // Rotate vector
        // view angle: 0.0 is -->
        float rotation = view_angle + M_PI / 2.0; // rotation: 0.0 is ^
        v2 dir = v2(
            inp.x * cos(rotation) - inp.y * sin(rotation),
            inp.x * sin(rotation) + inp.y * cos(rotation)
        );
        
        const float speed = 5.0;
        v2 next = player + dir * speed * engine->delta;

        const float player_radius = 0.25;

        { // Collide
            int px = floor(player.x), py = floor(player.y);
            int horiz_x = px + (dir.x > 0 ? 1 : -1),
                horiz_y = py;
            int vert_x  = px,
                vert_y  = py + (dir.y > 0 ? 1 : -1);

            // We resolve collisions by looking at the three blocks in
            // the direction we are moving. Depending on which we
            // intersect, we gradually scoot away until we're in a
            // good spot.
            const float fudge_factor = 0.001;
            size_t limit = 500;
            bool ok;
            do {
                ok = true;
                // Check the block next to us on the X axis. Slide horizontally.
                if (world.get(horiz_x, horiz_y) &&
                    circle_aabb_intersect(
                        next, player_radius,
                        v2(horiz_x, horiz_y), v2(horiz_x + 1, horiz_y + 1))) {
                    ok = false;
                    next.x -= dir.x * fudge_factor;
                }
                // Check the block next to us on the Y axis. Slide vertically.
                if (world.get(vert_x, vert_y) &&
                    circle_aabb_intersect(
                        next, player_radius,
                        v2(vert_x, vert_y), v2(vert_x + 1, vert_y + 1))) {
                    ok = false;
                    next.y -= dir.y * fudge_factor;
                }
                // If we're NOT intersecting with the blocks
                // immediately next to us, make sure we're not walking
                // into the corner of the block diagonal from
                // us. Slide vertically if we are.
                if (ok &&
                    world.get(horiz_x, vert_y) &&
                    circle_aabb_intersect(
                        next, player_radius,
                        v2(horiz_x, vert_y), v2(horiz_x + 1, vert_y + 1))) {
                    ok = false;
                    next.y -= dir.y * fudge_factor;
                }
                limit--;
            } while (!ok && limit > 0);
            player = next;
        }
    }
    { // Rotate player
        float dir = 0.0;
        dir += engine->input->keyDown(SDL_SCANCODE_RIGHT) ? +1.0 : 0.0;
        dir += engine->input->keyDown(SDL_SCANCODE_LEFT)  ? -1.0 : 0.0;

        if (mouse_control) {
            float amt = engine->input->motion.x;
            const float sensitivity = 0.2;
            dir += amt * sensitivity;
        }
        
        const float speed = 2.0;
        view_angle += dir * speed * engine->delta;
        view_angle = clamp_angle(view_angle);
    }
    { // Hotkeys
        if (engine->input->keyPressed(SDL_SCANCODE_DOWN)) {
            fov_degrees -= 5.0;
        }
        if (engine->input->keyPressed(SDL_SCANCODE_UP)) {
            fov_degrees += 5.0;
        }
        fov = fov_degrees * (M_PI / 180.0);
        half_fov = fov / 2.0;

        if (engine->input->keyPressed(SDL_SCANCODE_SPACE)) {
            if (mouse_control) {
                SDL_SetRelativeMouseMode(SDL_FALSE);
                mouse_control = false;
            } else {
                SDL_SetRelativeMouseMode(SDL_TRUE);
                mouse_control = true;
            }
        }
    }
    { // Add/remove tiles
        if (!mouse_control && engine->input->btnPressed(SDL_BUTTON_LEFT)) {
            v2 mpos = engine->input->mousePos();
            int x_start = engine->width / 2;
            if (mpos.x > x_start) {
                int x = floor((mpos.x - x_start) / ((engine->width / 2) / world.width)),
                    y = floor(mpos.y / (engine->height / world.height));
                world.set(x, y, world.get(x, y) ? NO_WALL : INNER_WALL);
            }
        }
    }
}

void Game::render3D(SDL_Surface* surface, int width, int height) {
    { // Floor
        SDL_Rect floor_rect;
        floor_rect.x = 0;
        floor_rect.y = height / 2;
        floor_rect.w = width;
        floor_rect.h = height / 2;
        SDL_FillRect(
            surface, &floor_rect,
            SDL_MapRGB(surface->format, 0x20, 0x20, 0x20)
        );
    }
    { // Sky
        float percent_sky_visible = fov / (2 * M_PI);

        if (view_angle - half_fov >= 0 && view_angle + half_fov < 2 * M_PI) {
            // No tiling necessary
            SDL_Rect src;
            src.x = (view_angle - half_fov) / (2 * M_PI) * sky->w;
            src.y = 0;
            src.w = sky->w * percent_sky_visible;
            src.h = sky->h;
            
            SDL_Rect dest;
            dest.x = 0;
            dest.y = 0;
            dest.w = width;
            dest.h = height / 2;
            
            SDL_BlitScaled(
                sky, &src,
                surface, &dest
            );    
        } else {
            // The crease in the sky is visible, so we tile
            float left_angle = (2 * M_PI) - clamp_angle(view_angle - half_fov);
            float right_angle = clamp_angle(view_angle + half_fov);
            {
                // Left sky
                SDL_Rect src;
                src.x = sky->w - sky->w * percent_sky_visible * (left_angle / fov);
                src.y = 0;
                src.w = sky->w * percent_sky_visible * (left_angle / fov);
                src.h = sky->h;

                SDL_Rect dest;
                dest.x = 0;
                dest.y = 0;
                dest.w = width * (left_angle / fov);
                dest.h = height / 2;

                SDL_BlitScaled(
                    sky, &src,
                    surface, &dest
                );
            }
            {
                // Right sky
                SDL_Rect src;
                src.x = 0;
                src.y = 0;
                src.w = sky->w * percent_sky_visible * (right_angle / fov);
                src.h = sky->h;

                SDL_Rect dest;
                dest.x = width - (width * (right_angle / fov));
                dest.y = 0;
                dest.w = width * (right_angle / fov);
                dest.h = height / 2;

                SDL_BlitScaled(
                    sky, &src,
                    surface, &dest
                );
            }

        }
    }
    
    float left_view = view_angle - half_fov;
    float rads_per_pixel = fov / width;
    
    for (int x = 0; x < width; x++) {
        float angle = left_view + x * rads_per_pixel;
        v2 dir(cos(angle), sin(angle));
        WallInfo wall_info(HORIZONTAL, OUTER_WALL);
        v2 boundary = world.wallBoundary(player, dir, &wall_info);
        float dist = (boundary - player).size() * cos(abs(view_angle - angle));
        float r = (height / (dist * 2)) * plane_distance;

        SDL_Rect dest;
        dest.x = x;
        dest.y = (height / 2) - r;
        dest.w = 1;
        dest.h = r * 2;

        // Texture mapping
        float texture_x =
            wall_info.dir == HORIZONTAL
            ? boundary.x - floor(boundary.x)
            : boundary.y - floor(boundary.y);
        SDL_Surface* texture;
        switch (wall_info.type) {
        case NO_WALL:
            fatal("Unreachable");
        case OUTER_WALL:
            texture = light_wall;
            break;
        case INNER_WALL:
            texture = dark_wall;
            break;
        }

        SDL_Rect src;
        src.x = (int) (texture_x * (float) texture->w);
        src.y = 0;
        src.w = 1;
        src.h = texture->h;
        
        SDL_BlitScaled(
            texture, &src, surface, &dest
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
            
            if (!world.get(x, y)) {
                SDL_FillRect(
                    surface, &rect,
                    SDL_MapRGB(surface->format, 0, 0, 0)
                );
            } else {
                SDL_Surface* wall;
                switch (world.get(x, y)) {
                case NO_WALL:
                    fatal("Unreachable");
                case OUTER_WALL:
                    wall = light_wall;
                    break;
                case INNER_WALL:
                    wall = dark_wall;
                    break;
                }
                SDL_BlitScaled(
                    wall, nullptr,
                    surface, &rect
                );
            }
        }
    }
    
    // Grid
    auto line_color = SDL_MapRGB(surface->format, 0x90, 0x90, 0x90);
    for (int i = 0; i < world.height; i++) {
        { // Horizontal
            SDL_Rect rect;
            rect.x = 0;
            rect.y = i * box_size;
            rect.w = size;
            rect.h = 1;
            SDL_FillRect(
                surface, &rect, line_color
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
                surface, &rect, line_color
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
        const int radius = box_size * 0.25;
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
    // Info text
    {
        char buf[512];
        sprintf(buf, "FOV: %.2f", fov_degrees);
        engine->renderText(buf, 0, 0);

        sprintf(buf, "ANGLE: %.2f", view_angle * (180.0 / M_PI));
        engine->renderText(buf, 0, 30);

        sprintf(buf, "MOUSE CONTROL: %s", mouse_control ? "ON" : "OFF");
        engine->renderText(buf, 0, 60);

        sprintf(buf, "MOTION: %3.0f", engine->input->motion.x);
        engine->renderText(buf, 0, 90);
    }
}

//
// Engine
//

Engine::Engine(const char* title, int width, int height) {
    this->width = width;
    this->height = height;

    int err = 0;
    err = SDL_Init(SDL_INIT_EVERYTHING);
    if (err != 0) {
        fatal(SDL_GetError());
    }

    err = TTF_Init();
    if (err != 0) {
        fatal(TTF_GetError());
    }

    font = TTF_OpenFont("res/VGA8.ttf", 26);
    if (font == nullptr) {
        fatal(TTF_GetError());
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

    // There is ZERO reason these should be heap-allocated, but C++
    // refuses to let me perform the above initialization BEFORE their
    // constructors are called, no matter what I do. This has me
    // royally pissed.
    game = new Game(this);
    input = new Input();
}

Engine::~Engine() {
    delete game;
    delete input;
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Engine::renderText(const char* msg, int x, int y) {
    auto blit_text =
        [&](SDL_Surface* surf, int x_offset, int y_offset) {
            SDL_Rect rect;
            rect.x = x + x_offset;
            rect.y = y + y_offset;
            rect.w = surf->w;
            rect.h = surf->h;
            SDL_BlitSurface(surf, nullptr, canvas, &rect);
        };
    
    SDL_Surface* shadow = TTF_RenderText_Solid(font, msg, {0, 0, 0});
    SDL_Surface* text   = TTF_RenderText_Solid(font, msg, {0xff, 0xff, 0xff});

    blit_text(shadow, -2, 2);
    blit_text(text, 0, 0);
    
    SDL_FreeSurface(shadow);
    SDL_FreeSurface(text);
}

bool Engine::frame() {
    input->resetCache();
    
    // Consume events
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            return false;
        } else if (event.type == SDL_MOUSEMOTION) {
            input->motion = v2(event.motion.xrel, event.motion.yrel);
        }
    }
    
    if (input->keyPressed(SDL_SCANCODE_ESCAPE)) {
        return false;
    }
    
    // Update
    game->update();

    // Render
    SDL_FillRect(canvas, nullptr, SDL_MapRGB(canvas->format, 0xff, 0xff, 0xff));
    game->render();
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
