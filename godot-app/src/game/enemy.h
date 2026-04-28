#pragma once
#include "game_sprite.h"
#include "city.h"

class Enemy : public GameSprite {
public:
    int   speed;
    bool  big;
    bool  guns;
    int   cr;
    float vx = 0, vy = 0;
    float maxSpeed;
    float accel;
    float clumpScore = 0;
    float mult;
    
    Enemy(float x, float y, const EnemyOpts& opts, Board* board);
    void onTick() override;
    void destroy() override;
};
