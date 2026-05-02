#pragma once
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include "game/board.h"

namespace godot {

class TDGame : public Node2D {
    GDCLASS(TDGame, Node2D)

public:
    TDGame();
    ~TDGame();

    void _ready() override;
    void _process(double delta) override;
    void _draw() override;
    void _input(const Ref<InputEvent>& event) override;

    // Godot bindings
    void new_game();
    int  get_tick_speed() const { return board ? board->tickSpeed : 2; }
    void set_tick_speed(int s);
    bool get_show_arrows() const { return board ? board->showArrows : false; }
    void toggle_arrows();
    int  get_money()  const { return board ? (int)board->getMoney() : 0; }
    int  get_lives()  const { return board ? board->getLives() : 0; }

protected:
    static void _bind_methods();

private:
    Board* board      = nullptr;
    double tickAcc    = 0;
    double uiTickAcc  = 0;
    uint32_t savedSeed_ = 0;
    bool   showHelp     = false;
    bool   showResult   = false;
    bool   resultVictory  = false;
    bool   resultFlawless = false;

    // Toolbar geometry
    static const int TOOLBAR_H = 52;
    float cellSize = 48;
    int   cols = 8, rows = 6;

    std::unordered_map<std::string, Ref<Texture2D>> textures;

    // UI state
    float  boardOffsetX = 0, boardOffsetY = 0;

    // Pan / zoom
    float boardZoom = 1.0f;
    float boardPanX = 0.0f;
    float boardPanY = 0.0f;
    static constexpr float MAX_ZOOM      = 4.0f;
    static constexpr float DRAG_THRESHOLD = 10.0f;
    void _clamp_pan();

    // Touch gesture state
    std::unordered_map<int, Vector2> touches_;
    int     touchPanIdx_    = -1;
    bool    touchDidDrag_          = false;
    bool    touchTargetedAtPress_  = false;
    Vector2 touchDragLast_;
    float   pinchStartDist_ = 0.0f;
    float   pinchStartZoom_ = 1.0f;
    Vector2 pinchStartMid_;
    Vector2 pinchStartPan_;

    // Mouse drag state (suppressed once a real touch event is seen)
    bool    usingTouch_    = false;
    bool    mouseDown_     = false;
    bool    mouseDragging_ = false;
    Vector2 mouseDownPos_;
    Vector2 panAtMouseDown_;

    void _handle_tap(float px, float py);

    void _load_textures();
    void _init_board(uint32_t seed = 0);
    void _destroy_board();

    void _draw_toolbar();
    void _draw_board();
    void _draw_sprite(const GameSprite* s);
    void _draw_menu();
    void _draw_help();
    void _draw_result();

    // button hit-testing
    struct Btn { float x,y,w,h; std::string id; };
    std::vector<Btn> _buttons;
    void _register_btn(float x, float y, float w, float h, const std::string& id);
    std::string _btn_at(float x, float y) const;

    Color _parse_rgba(const std::string& s) const;
    Ref<Texture2D> _tex(const std::string& name) const;
    Vector2 _cell_to_px(float cx, float cy) const;
};

// Renders a character into a [<=maxh][w] float mask using an offscreen viewport.
std::vector<std::vector<float>> _render_emoji_mask(int codepoint, int w, int maxh);

    
} // namespace godot
