#pragma once
#include "game_sprite.h"
#include <string>

class Tower : public GameSprite {
public:
    float range, damage, reloadTime;
    std::string ammo;
    float ammosize;
    int   cost;
    float reload = 0;
    std::string policy = "first";
    bool  dying = false;
    int   upgraded = 0;

    Tower(float x, float y, const std::string& img, Board* board);

    void onTick() override;
    bool upgrade();
    void sell();
    void nextPolicy();
    void destroy() override;

    GameSprite* pickTarget();

private:
    float _scoreFor(const std::string& pol, GameSprite* target) const;
};
