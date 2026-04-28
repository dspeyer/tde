#pragma once
#include <string>
#include <cstdint>
#include "hp.h"

class Board;

class GameSprite {
public:
    static uint32_t next_uid;

    uint32_t uid;
    float x_, y_;
    float s;        // size in cells
    float theta_ = 0;
    float opacity;
    int z;
    std::string img;
    Board* board;

    // flags
    bool blocksTower = false;
    bool blocksEnemy = false;
    bool isArrow     = false;
    bool isEnemy     = false;
    bool isRange     = false;
    bool isLaser     = false;
    bool targettable = false;
    bool isJungle    = false;

    // laser
    float laserTheta = 0;

    HP* hp = nullptr;

    GameSprite(float x, float y, int z, float s, const std::string& img, Board* board);
    virtual ~GameSprite();

    // x/y setters update spatial hash
    void setX(float v);
    void setY(float v);

    virtual void onTick() {}
    virtual void destroy();

    friend class Board;
protected:
    bool _destroyed = false;
    void _lsh();
    void _unlsh();
    void _xlsh(bool remove);
};
