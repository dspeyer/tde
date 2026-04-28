#pragma once
#include "game_sprite.h"
#include <string>

class Tile : public GameSprite {
public:
    Tile(float x, float y, const std::string& img, Board* board);
    void destroy() override;
};
