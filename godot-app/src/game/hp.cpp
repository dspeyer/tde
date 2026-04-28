#include "hp.h"
#include "game_sprite.h"

void HP::hurt(float x) {
    current -= x;
    damaged = true;
    if (current <= 0) {
        owner->destroy();
    }
}
