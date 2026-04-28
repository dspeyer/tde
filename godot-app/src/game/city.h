#pragma once
#include "game_sprite.h"
#include <string>

class City : public GameSprite {
public:
    City(float x, float y, Board* board);
    void destroy() override;
};

struct EnemyOpts {
    int speed;
    bool big;
    bool guns;
    int cr;
};

class EvilCity : public GameSprite {
public:
    EvilCity(float x, float y, Board* board);
    void onTick() override;

private:
    float vvcr    = 0.00001f;
    float vcr     = 0.002f;
    float cr_acc  = 0;
    EnemyOpts nextEnemy;

    float  _wrandom();
    void   _pickEnemy();
    void   _spawnEnemy(const EnemyOpts& opts);
};
