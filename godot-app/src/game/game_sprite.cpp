#include "game_sprite.h"
#include "board.h"
#include "hp.h"
#include "constants.h"
#include <cmath>

uint32_t GameSprite::next_uid = 0;

GameSprite::GameSprite(float x, float y, int z, float s, const std::string& img, Board* board)
    : uid(next_uid++), x_(x), y_(y), z(z), s(s), img(img), opacity(1.0f), board(board)
{
    board->sprites[uid] = this;
    _lsh();
}

GameSprite::~GameSprite() {
    delete hp;
}

void GameSprite::setX(float v) { _unlsh(); x_ = v; _lsh(); }
void GameSprite::setY(float v) { _unlsh(); y_ = v; _lsh(); }

void GameSprite::destroy() {
    if (_destroyed) return;
    _destroyed = true;
    _unlsh();
    board->sprites.erase(uid);
    if (board->target == this) board->target = nullptr;
}

void GameSprite::_xlsh(bool remove) {
    int x0 = (int)std::round(x_ - s / 2 + 0.001f);
    int x1 = (int)std::round(x_ + s / 2 - 0.001f);
    int y0 = (int)std::round(y_ - s / 2 + 0.001f);
    int y1 = (int)std::round(y_ + s / 2 - 0.001f);
    for (int xi = x0; xi <= x1; xi++) {
        for (int yi = y0; yi <= y1; yi++) {
            if (xi < 0 || yi < 0 || xi >= board->width || yi >= board->height) continue;
            auto& cell = board->spritesByPlace[xi][yi];
            if (remove) cell.erase(uid);
            else        cell[uid] = this;
        }
    }
}

void GameSprite::_lsh()   { _xlsh(false); }
void GameSprite::_unlsh() { _xlsh(true); }


Announcement::Announcement(const std::string& _text, Board* _board)
    : GameSprite(_board->width/2, _board->height/2, ZCOVER, _board->width/2, "", _board)
    , text(_text) {}

void Announcement:: onUITick() {
    s *= 1.05;
    opacity -= 0.03;
    if (opacity <= 0) {
        destroy();
    }
}
