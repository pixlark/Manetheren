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
    v2 nextBoundary(v2 pos, v2 dir);
    v2 wallBoundary(v2 pos, v2 dir);
    void renderTopDown(SDL_Surface* surface, int render_size);
};

struct Engine;

struct Game {
private:
    Engine* engine;
    World world;
    v2 player;
    float angle; // Radians

public:
    Game(Engine* engine);
    Game(const Game&) = default;
    ~Game() = default;
    void update();
    void render();
};

struct Input {
private:
    int num_keys;
    const uint8_t* keyboard_state;
    uint8_t* last_frame_snapshot;

public:
    Input();
    Input(const Input&) = default;
    ~Input();
    void resetCache();
    bool keyPressed(SDL_Scancode code);
    bool keyDown(SDL_Scancode code);
    bool keyUp(SDL_Scancode code);
};

struct Engine {
    SDL_Surface* canvas;
    float delta;
    int width, height;
    Input* input;

private:
    SDL_Window* window;
    uint64_t last_tick;
    Game game;

public:
    Engine(const char* title, int width, int height);
    Engine(const Engine&) = default;
    ~Engine();
    bool frame();
};
