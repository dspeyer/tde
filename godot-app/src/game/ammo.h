#pragma once
#include "game_sprite.h"

struct AmmoParams {
    float x, y;
    std::string ammo;
    float ammosize;
    float theta;
    float range;
    float damage;
};

class Ammo : public GameSprite {
public:
    float startX, startY;
    float range, damage;
    bool  simple;
    std::string ammoType;

    Ammo(const AmmoParams& p, Board* board);
    void onTick() override;
};

class LaserBolt : public GameSprite {
public:
    LaserBolt(float x, float y, float theta, float damage, float range, Board* board);
    void onTick() override;
};

class Explosion : public GameSprite {
public:
    Explosion(float x, float y, float damage, Board* board);
    void onTick() override;
};

class FailMarker : public GameSprite {
public:
    FailMarker(float x, float y, float s, Board* board);
    void onTick() override;
};
