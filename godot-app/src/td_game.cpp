#include "td_game.h"
#include "game/tower.h"
#include "game/enemy.h"
#include "game/ammo.h"
#include "game/tile.h"
#include "game/city.h"
#include "game/constants.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <cmath>
#include <algorithm>
#include <iostream>

using namespace godot;

static const int MIN_CELL = 32;
static const float PI = 3.14159265f;

TDGame::TDGame() {}
TDGame::~TDGame() { _destroy_board(); }

Ref<Font> _get_font_for(int cp) {
    Ref<Font> font;
    Ref<FontFile> font_file;
    font = ThemeDB::get_singleton()->get_fallback_font();
    if (font.is_valid() && font->has_char(cp)) return font;
    font_file.instantiate();                                 
    font_file->load_dynamic_font("res://assets/fonts/NotoEmoji-Light.ttf");    
    if (font_file.is_valid() && font_file->has_char(cp)) return font_file;
    font_file->load_dynamic_font("res://assets/fonts/NotoSansMath-Regular.ttf");    
    if (font_file.is_valid() && font_file->has_char(cp)) return font_file;
    font_file->load_dynamic_font("res://assets/fonts/NotoMusic-Regular.ttf");    
    if (font_file.is_valid() && font_file->has_char(cp)) return font_file;
    font_file->load_dynamic_font("res://assets/fonts/ShipporiAntiqueB1-Regular.ttf");
    if (font_file.is_valid() && font_file->has_char(cp)) return font_file;
    return Ref<Font>();
}


std::vector<std::vector<float>> godot::_render_emoji_mask(int codepoint, int grid_w, int max_grid_h) {
    std::vector<std::vector<float>> mask;
    const int font_size = 64;
    
    Ref<Font> font = _get_font_for(codepoint);
    if (font.is_null()) return mask;

    int descent = font->get_descent(font_size);
    Vector2 sz = font->get_char_size(codepoint, font_size) + Vector2(2,descent);
    int grid_h = grid_w * sz.y / sz.x;

    if (grid_h > max_grid_h) {
        grid_w = grid_w * max_grid_h / grid_h;
        grid_h = max_grid_h;
    }
    
    mask.resize(grid_h);
    for (auto& row : mask) row.resize(grid_w);
    
    auto* rs = RenderingServer::get_singleton();

    // Build an offscreen viewport with its own canvas
    RID vp     = rs->viewport_create();
    RID canvas = rs->canvas_create();
    RID ci     = rs->canvas_item_create();

    rs->viewport_set_size(vp, sz.x, sz.y);
    rs->viewport_set_active(vp, true);
    rs->viewport_set_transparent_background(vp, true);
    rs->viewport_set_update_mode(vp, RenderingServer::VIEWPORT_UPDATE_ALWAYS);
    rs->viewport_attach_canvas(vp, canvas);
    rs->canvas_item_set_parent(ci, canvas);

    font->draw_string(ci, Vector2(0, sz.y-descent), String::chr(codepoint),
                      HORIZONTAL_ALIGNMENT_LEFT, sz.x, font_size);

    // Render and read back
    rs->force_draw(false, 0.0);
    RID tex = rs->viewport_get_texture(vp);
    Ref<Image> img = rs->texture_2d_get(tex);

    rs->free_rid(ci);
    rs->free_rid(canvas);
    rs->viewport_set_active(vp, false);
    rs->free_rid(vp);

    if (!img.is_valid() || img->is_empty()) return mask;
    img->convert(Image::FORMAT_RGBA8);
    
    // Downsample to the requested grid; use max(r,g,b)*a so coloured emoji work
    for (int gy = 0; gy < grid_h; gy++) {
        for (int gx = 0; gx < grid_w; gx++) {
            int px0 = gx * sz.x / grid_w, px1 = (gx + 1) * sz.x / grid_w;
            int py0 = gy * sz.y / grid_h, py1 = (gy + 1) * sz.y / grid_h;
            int count = std::max(1, (px1 - px0) * (py1 - py0));
            float sum = 0;
            for (int py = py0; py < py1; py++)
                for (int px = px0; px < px1; px++) {
                    Color c = img->get_pixel(px, py);
                    sum += std::max({c.r, c.g, c.b}) * c.a;
                }
            mask[gy][gx] = sum / count;
        }
    }
    return mask;
}

void TDGame::_bind_methods() {
    ClassDB::bind_method(D_METHOD("new_game"),       &TDGame::new_game);
    ClassDB::bind_method(D_METHOD("toggle_arrows"),  &TDGame::toggle_arrows);
    ClassDB::bind_method(D_METHOD("set_tick_speed","s"), &TDGame::set_tick_speed);
    ClassDB::bind_method(D_METHOD("get_money"),      &TDGame::get_money);
    ClassDB::bind_method(D_METHOD("get_lives"),      &TDGame::get_lives);
}

void TDGame::_ready() {
    _load_textures();
    _init_board();
    set_process(true);
    set_process_input(true);
}

void TDGame::_load_textures() {
    static const std::vector<std::string> names = {
        "arrow","artillery","biggest","cannonball","cannon","chariot","city","clump",
        "evilcity","explosion","fail","first","flame","flamethrower","hills",
        "horsemen","howitzer","jungle","laserbolt","laser","last","marines",
        "mountains","placeholder","plains","pusher","rocket","sell","shells",
        "swamp","tank","upgrade","warriors","wind","engineers","favicon","target"
    };
    auto* rl = ResourceLoader::get_singleton();
    for (auto& n : names) {
        String path = String("res://assets/images/") + n.c_str() + ".png";
        if (rl->exists(path)) {
            textures[n] = rl->load(path);
        }
    }
}

void TDGame::_init_board() {
    auto* vp = get_viewport();
    Vector2 vpSize = vp ? vp->get_visible_rect().size : Vector2(480, 854);
    float boardW = vpSize.x;
    float boardH = vpSize.y - TOOLBAR_H;
    boardOffsetX = 0;
    boardOffsetY = TOOLBAR_H;

    rows = 30;
    cols = (int)( (float)rows * boardW / boardH );
    cellSize = (int)std::min(boardW/(cols+1), boardH/(rows+1));
    
    boardZoom = 1.0f;
    boardPanX = 0.0f;
    boardPanY = 0.0f;
    _clamp_pan();

    _destroy_board();
    board = new Board(cols, rows, cellSize, _render_emoji_mask);
    board->onRender = [this]() { queue_redraw(); };
    board->onGameEvent = [this](bool isVic, bool fl) {
        showResult   = true;
        resultVictory  = isVic;
        resultFlawless = fl;
        queue_redraw();
    };
    board->start();
    queue_redraw();
}

void TDGame::_destroy_board() {
    delete board;
    board = nullptr;
    tickAcc = 0;
}

void TDGame::new_game() {
    showHelp = showResult = false;
    _init_board();
}

void TDGame::set_tick_speed(int s) {
    if (board) board->tickSpeed = s;
}

void TDGame::toggle_arrows() {
    if (board) board->toggleArrows();
}

// ── Pan / zoom helpers ───────────────────────────────────────────────────────
void TDGame::_clamp_pan() {
    auto* vp = get_viewport();
    float vpW = vp ? vp->get_visible_rect().size.x : 480;
    float vpH = (vp ? vp->get_visible_rect().size.y : 854) - TOOLBAR_H;
    float cs      = cellSize * boardZoom;
    float half    = 0.5f * cs;
    float one     = cs;
    float scaledW = cols * cs;
    float scaledH = rows * cs;
    if (scaledW+half <= vpW)
        boardPanX = (vpW - scaledW) / 2 + half;
    else
        boardPanX = std::max(vpW - scaledW, std::min(one, boardPanX));
    if (scaledH+half <= vpH)
        boardPanY = (vpH - scaledH) / 2 + half;
    else
        boardPanY = std::max(vpH - scaledH, std::min(one, boardPanY));
}

// ── Game loop ────────────────────────────────────────────────────────────────
void TDGame::_process(double delta) {
    if (!board || board->gameOver || board->tickSpeed == 0) return;
    tickAcc += delta;
    double tickInterval = 0.032 / board->tickSpeed;
    while (tickAcc >= tickInterval) {
        tickAcc -= tickInterval;
        board->tick();
    }
}

// ── Input ────────────────────────────────────────────────────────────────────
void TDGame::_handle_tap(float px, float py) {
    std::string btn = _btn_at(px, py);
    if (!btn.empty()) {
        if (btn == "help")   { showHelp = !showHelp; queue_redraw(); }
        if (btn == "new")    { new_game(); }
        if (btn == "arrows") { toggle_arrows(); }
        if (btn == "again")  { new_game(); }
        if (btn == "close")  { showHelp = false; queue_redraw(); }
        if (btn.size() >= 5 && btn.substr(0,5) == "speed") {
            set_tick_speed(std::stoi(btn.substr(5)));
            queue_redraw();
        }
        if (btn.size() >= 5 && btn.substr(0,5) == "menu_") {
            board->resolveMenu(btn.substr(5));
        }
        return;
    }
    if (!showHelp && !showResult && py > TOOLBAR_H) {
        float cx = (px - boardOffsetX - boardPanX) / (cellSize * boardZoom);
        float cy = (py - boardOffsetY - boardPanY) / (cellSize * boardZoom);
        board->handleTap(cx, cy);
    }
    get_viewport()->set_input_as_handled();
}

void TDGame::_input(const Ref<InputEvent>& event) {
    if (!board) return;

    // ── Touch (mobile) ───────────────────────────────────────────────────────
    if (auto* touch = Object::cast_to<InputEventScreenTouch>(*event)) {
        usingTouch_ = true;
        int idx = touch->get_index();
        if (touch->is_pressed()) {
            touches_[idx] = touch->get_position();
            if (touches_.size() == 1) {
                touchPanIdx_  = idx;
                touchDragLast_ = touch->get_position();
                touchDidDrag_  = false;
                // Hit-test targettable sprites at press time while enemies are
                // still under the finger; on release the enemy may have moved.
                touchTargetedAtPress_ = false;
                Vector2 pos = touch->get_position();
                if (!showHelp && !showResult && pos.y > TOOLBAR_H) {
                    float cx = (pos.x - boardOffsetX - boardPanX) / (cellSize * boardZoom);
                    float cy = (pos.y - boardOffsetY - boardPanY) / (cellSize * boardZoom);
                    touchTargetedAtPress_ = board->trySetTarget(cx, cy);
                }
            } else if (touches_.size() == 2) {
                auto it = touches_.begin();
                Vector2 a = it->second; ++it;
                Vector2 b = it->second;
                pinchStartMid_  = (a + b) * 0.5f;
                pinchStartDist_ = (b - a).length();
                pinchStartZoom_ = boardZoom;
                pinchStartPan_  = Vector2(boardPanX, boardPanY);
            }
        } else {
            if ((int)touches_.size() == 1 && idx == touchPanIdx_ && !touchDidDrag_) {
                if (!touchTargetedAtPress_)
                    _handle_tap(touches_[idx].x, touches_[idx].y);
                touchTargetedAtPress_ = false;
            }
            touches_.erase(idx);
            if (touches_.size() == 1) {
                touchPanIdx_   = touches_.begin()->first;
                touchDragLast_ = touches_.begin()->second;
                touchDidDrag_  = false;
            }
        }
        get_viewport()->set_input_as_handled();
        return;
    }

    // ── Touch drag (mobile) ──────────────────────────────────────────────────
    if (auto* drag = Object::cast_to<InputEventScreenDrag>(*event)) {
        int idx = drag->get_index();
        if (touches_.find(idx) == touches_.end()) return;
        Vector2 newPos = drag->get_position();

        if (touches_.size() == 1) {
            Vector2 delta = newPos - touchDragLast_;
            if (!touchDidDrag_ && delta.length() > DRAG_THRESHOLD)
                touchDidDrag_ = true;
            if (touchDidDrag_) {
                boardPanX += delta.x;
                boardPanY += delta.y;
                _clamp_pan();
                queue_redraw();
            }
            touchDragLast_ = newPos;
        } else if (touches_.size() >= 2) {
            touches_[idx] = newPos;
            auto it = touches_.begin();
            Vector2 a = it->second; ++it;
            Vector2 b = it->second;
            float newDist = (b - a).length();
            Vector2 newMid = (a + b) * 0.5f;

            float scale = (pinchStartDist_ > 0) ? newDist / pinchStartDist_ : 1.0f;
            float newZoom = std::max(1.0f, std::min(MAX_ZOOM, pinchStartZoom_ * scale));

            // Keep the board point under the initial pinch midpoint fixed
            float bx = (pinchStartMid_.x - boardOffsetX - pinchStartPan_.x) / (pinchStartZoom_ * cellSize);
            float by = (pinchStartMid_.y - boardOffsetY - pinchStartPan_.y) / (pinchStartZoom_ * cellSize);
            boardPanX = newMid.x - boardOffsetX - bx * newZoom * cellSize;
            boardPanY = newMid.y - boardOffsetY - by * newZoom * cellSize;
            boardZoom = newZoom;
            _clamp_pan();
            queue_redraw();
            return;
        }
        touches_[idx] = newPos;
        get_viewport()->set_input_as_handled();
        return;
    }

    // ── Mouse ────────────────────────────────────────────────────────────────
    if (auto* mb = Object::cast_to<InputEventMouseButton>(*event)) {
        // Scroll wheel to zoom around cursor
        if (mb->is_pressed()) {
            float zoomFactor = 0;
            if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_UP)   zoomFactor =  1.15f;
            if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN)  zoomFactor =  1.0f / 1.15f;
            if (zoomFactor != 0) {
                float newZoom = std::max(1.0f, std::min(MAX_ZOOM, boardZoom * zoomFactor));
                Vector2 pos = mb->get_position();
                float bx = (pos.x - boardOffsetX - boardPanX) / (boardZoom * cellSize);
                float by = (pos.y - boardOffsetY - boardPanY) / (boardZoom * cellSize);
                boardPanX = pos.x - boardOffsetX - bx * newZoom * cellSize;
                boardPanY = pos.y - boardOffsetY - by * newZoom * cellSize;
                boardZoom = newZoom;
                _clamp_pan();
                queue_redraw();
                get_viewport()->set_input_as_handled();
                return;
            }
        }
        if (!usingTouch_ && mb->get_button_index() == MOUSE_BUTTON_LEFT) {
            if (mb->is_pressed()) {
                mouseDown_     = true;
                mouseDragging_ = false;
                mouseDownPos_  = mb->get_position();
                panAtMouseDown_ = Vector2(boardPanX, boardPanY);
            } else if (mouseDown_) {
                if (!mouseDragging_)
                    _handle_tap(mouseDownPos_.x, mouseDownPos_.y);
                mouseDown_ = false;
            }
            get_viewport()->set_input_as_handled();
        }
        return;
    }

    if (auto* mm = Object::cast_to<InputEventMouseMotion>(*event)) {
        if (!usingTouch_ && mouseDown_) {
            Vector2 delta = mm->get_position() - mouseDownPos_;
            if (!mouseDragging_ && delta.length() > DRAG_THRESHOLD)
                mouseDragging_ = true;
            if (mouseDragging_) {
                boardPanX = panAtMouseDown_.x + delta.x;
                boardPanY = panAtMouseDown_.y + delta.y;
                _clamp_pan();
                queue_redraw();
            }
        }
        return;
    }
}

// ── Drawing helpers ──────────────────────────────────────────────────────────
Ref<Texture2D> TDGame::_tex(const std::string& name) const {
    auto it = textures.find(name);
    if (it != textures.end()) return it->second;
    return Ref<Texture2D>();
}

// Returns screen-space position for a board-coordinate point
Vector2 TDGame::_cell_to_px(float cx, float cy) const {
    float zoom = boardZoom;
    return Vector2(boardOffsetX + boardPanX + cx * cellSize * zoom,
                   boardOffsetY + boardPanY + cy * cellSize * zoom);
}

void TDGame::_register_btn(float x, float y, float w, float h, const std::string& id) {
    _buttons.push_back({x, y, w, h, id});
}

std::string TDGame::_btn_at(float x, float y) const {
    for (int i = (int)_buttons.size()-1; i >= 0; i--) {
        auto& b = _buttons[i];
        if (x >= b.x && x < b.x+b.w && y >= b.y && y < b.y+b.h)
            return b.id;
    }
    return "";
}

Color TDGame::_parse_rgba(const std::string& s) const {
    int r=200,g=200,b=200; float a=0.8f;
    sscanf(s.c_str(), "rgba(%d,%d,%d,%f)", &r, &g, &b, &a);
    return Color(r/255.f, g/255.f, b/255.f, a);
}

// ── Main draw ────────────────────────────────────────────────────────────────
void TDGame::_draw() {
    _buttons.clear();
    if (board) _draw_board();
    if (board && board->menuState) _draw_menu();
    _draw_toolbar();          // drawn last so it always appears on top
    if (showHelp)   _draw_help();
    if (showResult) _draw_result();
}

void TDGame::_draw_toolbar() {
    if (!board) return;
    const String HEART   = String::chr(0x2764); // ❤
    const String ARROW_NE = String::chr(0x2197); // ↗
    auto* vp = get_viewport();
    float W = vp ? vp->get_visible_rect().size.x : 480;

    draw_rect(Rect2(0, 0, W, TOOLBAR_H), Color(0.5f,0.5f,0.5f,0.9f));

    float cy = TOOLBAR_H / 2;
    int SPEEDH = 9;
    float bh2 = cy - SPEEDH;
    float bh = bh2 * 2;
    int spacing = (W - 48 - 7*18 - 36 - 72 - 60 - 40) / 6;
    float x = spacing / 2;
    auto label = [&](const String& txt, float w) {
        draw_string(ThemeDB::get_singleton()->get_fallback_font(), Vector2(x+2, cy+5), txt,
                    HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color(1,1,1));
        x += w + spacing;
    };
    auto button = [&](const String& txt, float w, const std::string& id) {
        draw_rect(Rect2(x, cy-bh2, w, bh), Color(1,1,1,0.2f));
        draw_string(ThemeDB::get_singleton()->get_fallback_font(), Vector2(x+4, cy+5), txt,
                    HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color(1,1,1));
        _register_btn(x, cy-bh2, w, bh, id);
        x += w + spacing;
    };

    auto money_text = "$"+String::num_int64(board->getMoney());
    if (board->game_type==GATHER) money_text += "/" + String::num_int64(board->finalmoney);
    label(money_text, 48);

    draw_string(ThemeDB::get_singleton()->get_fallback_font(), Vector2(x, SPEEDH), "Speed",
                HORIZONTAL_ALIGNMENT_LEFT, -1, SPEEDH, Color(0.9f,0.9f,0.9f));
    float sx = x;
    for (int i = 0; i <= 6; i++) {
        float bx = sx + i * 18;
        bool active = i==0 ? (board->tickSpeed == i) : (board->tickSpeed >= i);
        auto color = active ? Color(0.3f,0.6f,1.f) : Color(1,1,1,0.2f);
        if (i == 0 ) {
            draw_rect(Rect2(bx+1, cy-bh2*0.7, 4, bh*0.7), color);
            draw_rect(Rect2(bx+7, cy-bh2*0.7, 4, bh*0.7), color);
        } else {
            PackedVector2Array points;
            points.push_back(Vector2(bx, cy-bh2));
            points.push_back(Vector2(bx, cy+bh2));
            points.push_back(Vector2(bx+16, cy));
            PackedColorArray pca;
            pca.push_back(color);
            draw_polygon(points, pca);
        }
        draw_string(ThemeDB::get_singleton()->get_fallback_font(), Vector2(bx+3, cy+5),
                    String::num_int64(i), HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color(1,1,1));
        _register_btn(bx, cy-bh2, 16, bh, "speed"+std::to_string(i));
    }
    x = sx + 7*18 + spacing;

    label(String::num_int64(board->getLives()) + " " + HEART, 36);
    label(String::utf8(board->getProgress().c_str()), 72);

    button((board->showArrows ? String("Hide") : String("Show")) + ARROW_NE, 60, "arrows");
    button("New",  40, "new");
}

bool compare_sprite(const GameSprite* a, const GameSprite* b) {
    if (a->z != b->z) return a->z < b->z;
    if (a->x_ != b->x_) return a->x_ < b->x_;
    if (a->y_ != b->y_) return a->y_ < b->y_;
    if (a->s != b->s) return a->s > b->s;
    return a < b;
}

void TDGame::_draw_board() {
    auto* vp = get_viewport();
    float vpW = vp ? vp->get_visible_rect().size.x : 480;
    float vpH = (vp ? vp->get_visible_rect().size.y : 854) - TOOLBAR_H;

    draw_rect(Rect2(boardOffsetX, boardOffsetY, vpW, vpH), Color(0.54f,0.45f,0.29f));

    std::vector<const GameSprite*> sorted;
    for (auto& [uid, s] : board->sprites) sorted.push_back(s);
    std::sort(sorted.begin(), sorted.end(), compare_sprite);

    for (auto* s : sorted) _draw_sprite(s);
}

void TDGame::_draw_sprite(const GameSprite* s) {
    if (s->opacity <= 0) return;
    float cs  = cellSize * boardZoom;

    // Range circle
    if (s->isRange) {
        float r = s->s * cs / 2;
        Vector2 center = _cell_to_px(s->x_, s->y_);
        for (float t = 0.79f; t <= 1.0f; t += 0.03f) {
            draw_arc(center, r*t, 0, 2*PI, 64,
                     Color(1,1,1, 0.75*t*t*t*s->opacity), 1, 1);
        }
        return;
    }

    // Laser bolt
    if (s->isLaser) {
        float len = (board->width + board->height) * cs;
        Vector2 origin = _cell_to_px(s->x_, s->y_);
        float rad = s->laserTheta * PI / 180.f;
        Vector2 end = origin + Vector2(std::cos(rad)*len, std::sin(rad)*len);
        draw_line(origin, end, Color(0.67f,0.67f,1.f, s->opacity), std::max(2.f, 0.2f*cs));
        return;
    }

    float size = s->s * cs;
    float left = boardOffsetX + boardPanX + (s->x_ - s->s/2) * cs;
    float top  = boardOffsetY + boardPanY + (s->y_ - s->s/2) * cs;
    Rect2 rect(left, top, size, size);

    const Tower* tower = dynamic_cast<const Tower*>(s);
    if (tower) {
        for ( int upgraded=tower->upgraded; upgraded>0; upgraded-=4 ) {
            float rg = 1.0f - 0.2f / std::sqrt((float)upgraded);
            float b  = 0.8f / std::sqrt((float)upgraded);
            float upgradeRad  = 0.6 - 0.2 / upgraded;
            float r = size * upgradeRad;
            Color c(rg, rg, b, 0.8f*s->opacity); 
            draw_arc(rect.get_center(), r, 0, 2*PI, upgraded < 3 ? 48 : upgraded+1, c, std::max(2.f, size*0.05f));
        }
    }
    
    auto tex = _tex(s->img);
    if (tex.is_valid()) {
        Vector2 ctr = rect.get_center();
        draw_set_transform(ctr, s->theta_ * PI / 180.f, Vector2(1,1));
        draw_texture_rect(tex, Rect2(-size/2, -size/2, size, size), false, Color(1,1,1,s->opacity));
        draw_set_transform(Vector2(0,0), 0, Vector2(1,1));
    } else if (!s->img.empty() && s->img != "placeholder") {
        draw_rect(rect, Color(0.5f,0.5f,0.5f,s->opacity));
    }


    if (s->hp && s->hp->damaged) {
        float bw = size * 0.94f, bh = size * 0.07f;
        float bx = left + size * 0.03f, by = top + size * 0.03f;
        draw_rect(Rect2(bx, by, bw, bh), Color(0.4f,0,0));
        draw_rect(Rect2(bx, by, bw * s->hp->fraction(), bh), Color(1,1,1));
    }

    if (board->target == s) {
        auto tgt = _tex("target");
        if (tgt.is_valid())
            draw_texture_rect(tgt, Rect2(left, top, size, size), false, Color(1,1,1,0.85f));
    }

    if (s->z==ZTILE) {
        Color lc(0,0,0,0.1);
        Vector2 lt(left, top);
        Vector2 lb(left, top+size);
        Vector2 rt(left+size, top);
        if ((int)(s->x_+.001) % 2 == 0) {
            draw_line(lt, lb, lc, 2.0);
        }
        if ((int)(s->y_+.001) % 2 == 0) {
            draw_line(lt, rt, lc, 2.0);
        }
    }
}

void TDGame::_draw_menu() {
    if (!board->menuState) return;
    auto* ms = board->menuState;

    // Backdrop covers the entire board viewport area
    auto* vp = get_viewport();
    float W  = vp ? vp->get_visible_rect().size.x : 480;
    float bh = (vp ? vp->get_visible_rect().size.y : 854) - TOOLBAR_H;
    draw_rect(Rect2(boardOffsetX, boardOffsetY, W, bh), Color(0,0,0,0.15f));
    _register_btn(boardOffsetX, boardOffsetY, W, bh, "menu_");

    float cs      = cellSize * boardZoom;
    float btnSize = std::max(18.f, cs * 0.75f);
    float radius  = cs * 1.2f;
    int   n = (int)ms->opts.size();

    for (int i = 0; i < n; i++) {
        auto& opt = ms->opts[i];
        float angle = 2*PI*(float(i)/n + 0.25f);
        float dx = -radius * std::cos(angle);
        float dy = -radius * std::sin(angle);
        float bx = boardOffsetX + boardPanX + ms->x * cs + dx - btnSize/2;
        float by = boardOffsetY + boardPanY + ms->y * cs + dy - btnSize/2;

        bool canAfford = (opt.cost <= 0 || board->getMoney() >= opt.cost);
        Color bg = canAfford ? Color(0.67f,0.67f,0.67f) : Color(0.5f,0.3f,0.3f);
        draw_rect(Rect2(bx, by, btnSize, btnSize), bg);
        draw_rect(Rect2(bx, by, btnSize, btnSize), Color(0.5f,0.5f,0.5f), false, 2);

        auto tex = _tex(opt.img);
        if (tex.is_valid())
            draw_texture_rect(tex, Rect2(bx+2, by+2, btnSize-4, btnSize*0.55f), false);

        Color txtColor = canAfford ? Color(0,0,0) : Color(1,0,0);
        String lbl = opt.label.empty() ? opt.img.c_str() : opt.label.c_str();
        draw_string(ThemeDB::get_singleton()->get_fallback_font(),
                    Vector2(bx+2, by+btnSize-4), lbl,
                    HORIZONTAL_ALIGNMENT_CENTER, btnSize-4, btnSize/4, txtColor);

        _register_btn(bx, by, btnSize, btnSize, "menu_"+opt.img);
    }
}

void TDGame::_draw_help() {
    auto* vp = get_viewport();
    float W = vp ? vp->get_visible_rect().size.x : 480;
    float H = vp ? vp->get_visible_rect().size.y : 854;

    float ox = W*0.05f, oy = H*0.10f;
    float w  = W*0.90f, h  = H*0.80f;
    draw_rect(Rect2(ox, oy, w, h), Color(1,1,0.87f));

    float y = oy+20;
    auto heading = [&](const String& t) {
        draw_string(ThemeDB::get_singleton()->get_fallback_font(), Vector2(ox+w/2-60, y), t,
                    HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color(0.2f,0.2f,0.2f));
        y += 24;
    };
    auto body = [&](const String& t) {
        draw_string(ThemeDB::get_singleton()->get_fallback_font(), Vector2(ox+8, y), t,
                    HORIZONTAL_ALIGNMENT_LEFT, w-16, 12, Color(0.2f,0.2f,0.2f));
        y += 18;
    };

    heading("Tower Defense Eternal");
    body("Enemies emerge from evil cities (red) and try to destroy your cities (blue).");
    body("Kill them with towers. Towers block enemies but must leave a path.");
    y += 4;
    heading("Towers");
    for (auto& name : TOWER_NAMES) {
        auto& st = towerStatsMap().at(name);
        body((String(name.c_str()) + " $" + String::num_int64(st.cost) + " - " + st.desc.c_str()).ascii().get_data());
    }
    y += 4;
    heading("Policies: First | Biggest | Clump | Last");
    body("Tap a tower to upgrade/sell/cycle policy.");
    body("Tap an enemy or jungle to focus all towers on it.");

    float bx=ox+w/2-30, by=oy+h-36;
    draw_rect(Rect2(bx, by, 60, 28), Color(0.5f,0.5f,0.5f));
    draw_string(ThemeDB::get_singleton()->get_fallback_font(), Vector2(bx+10, by+19), "Close",
                HORIZONTAL_ALIGNMENT_LEFT,-1,14,Color(1,1,1));
    _register_btn(bx, by, 60, 28, "close");
}

void TDGame::_draw_result() {
    auto* vp = get_viewport();
    float W = vp ? vp->get_visible_rect().size.x : 480;
    float H = vp ? vp->get_visible_rect().size.y : 854;
    draw_rect(Rect2(0,0,W,H), Color(0,0,0,0.5f));

    String title = resultVictory
        ? (resultFlawless ? "Flawless Victory!" : "Victory!")
        : "Defeat";
    Color tc = resultVictory ? Color(0,1,0) : Color(1,0,0);
    draw_string(ThemeDB::get_singleton()->get_fallback_font(), Vector2(0, H/2-30),
                title, HORIZONTAL_ALIGNMENT_CENTER, W, 40, tc);

    float bx=W/2-50, by=H/2+20;
    draw_rect(Rect2(bx, by, 100, 36), Color(1,1,1));
    draw_string(ThemeDB::get_singleton()->get_fallback_font(), Vector2(bx+10, by+24), "Play Again",
                HORIZONTAL_ALIGNMENT_LEFT,-1,16,Color(0.2f,0.2f,0.2f));
    _register_btn(bx, by, 100, 36, "again");
}
