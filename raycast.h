struct v2 {
    float x, y;
    v2(float x, float y);
    float size();
    
    v2 operator +(v2);
    void operator +=(v2);
    v2 operator -(v2);
    void operator -=(v2);
    v2 operator *(float);
    void operator *=(float);
    v2 operator /(float);
    void operator /=(float);
};

enum Direction {
    HORIZONTAL,
    VERTICAL,
};

enum WallType {
    WALL_OUTER,
    WALL_BLOCK,
};

struct WallInfo {
    Direction dir;
    WallType type;
    WallInfo(Direction dir, WallType type);
};

struct World {
    int width, height;
    
private:
    bool* walls;
    
public:
    World(int width, int height);
    World(const World&) = default;
    ~World();
    void set(int x, int y, bool val);
    bool get(int x, int y);
    v2 nextBoundary(v2 pos, v2 dir, Direction* hit_dir = nullptr);
    v2 wallBoundary(v2 pos, v2 dir, WallInfo* wall_info = nullptr);
};

struct Engine;

static float fov_degrees = 60.0;
static float fov = fov_degrees * M_PI / 180.0;
static float half_fov = fov / 2.0;
static float plane_distance = 1.0;

struct Game {
private:    
    Engine* engine;
    World world;
    v2 player;
    float view_angle; // Radians
    bool mouse_control = false;

    SDL_Surface* dark_wall;
    SDL_Surface* light_wall;
    SDL_Surface* sky;

public:
    Game(Engine* engine);
    Game(const Game&) = default;
    ~Game();
    void update();
    void render3D(SDL_Surface* surface, int width, int height);
    void renderTopDown(SDL_Surface* surface, int size);
    void render();
};

struct Input {
    v2 motion = {0, 0};
    
private:
    int num_keys;
    const uint8_t* keyboard_state;
    uint8_t* last_frame_snapshot;
    uint32_t mouse_state;
    uint32_t last_mouse_state;

public:
    Input();
    Input(const Input&) = default;
    ~Input();
    void resetCache();
    bool keyPressed(SDL_Scancode code);
    bool keyDown(SDL_Scancode code);
    bool keyUp(SDL_Scancode code);
    bool btnPressed(uint8_t btn);
    bool btnDown(uint8_t btn);
    bool btnUp(uint8_t btn);
    v2 mousePos();
};

struct Engine {
    SDL_Surface* canvas;
    float delta;
    int width, height;
    Input* input;
    TTF_Font* font;

private:
    SDL_Window* window;
    uint64_t last_tick;
    Game* game;

public:
    Engine(const char* title, int width, int height);
    ~Engine();
    void renderText(const char* title, int x, int y);
    bool frame();
};
