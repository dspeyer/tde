#pragma once

class GameSprite;

class HP {
public:
    GameSprite* owner;
    float max_hp;
    float current;
    bool damaged = false;

    HP(GameSprite* owner, float max_hp)
        : owner(owner), max_hp(max_hp), current(max_hp) {}

    void hurt(float x);

    float fraction() const {
        float f = current / max_hp;
        return f < 0 ? 0 : f;
    }
};
