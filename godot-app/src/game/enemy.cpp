#include "enemy.h"
#include "board.h"
#include "hp.h"
#include "city.h"
#include "constants.h"
#include <cmath>
#include <random>

static std::mt19937& erng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}
static float efrand() { return std::uniform_real_distribution<float>(0,1)(erng()); }
static float sq(float x) { return x*x; }

static const float speeds[]  = {0.01f, 0.03f, 0.02f};
static const float accels[]  = {0.001f, 0.001f, 0.002f};

Enemy::Enemy(float x, float y, const EnemyOpts& opts, Board* board)
    : GameSprite(x, y, ZENEMY, opts.big ? 0.95f : 0.7f,
                 opts.speed==0&&!opts.guns ? "warriors" :
                 opts.speed==1&&!opts.guns ? "chariot"  :
                 opts.speed==2&&!opts.guns ? "horsemen" :
                 opts.speed==0&&opts.guns  ? "marines"  : "tank",
                 board)
    , speed(opts.speed), big(opts.big), guns(opts.guns), cr(opts.cr)
    , maxSpeed(speeds[opts.speed]), accel(accels[opts.speed])
{
    mult = 1 + std::pow(board->totcr / 100.0f, 1.5f);
    float hpVal = (opts.big ? 1200 : 400) * mult;
    hp  = new HP(this, std::ceil(hpVal));
    isEnemy    = true;
    targettable = true;
    board->enemies[uid] = this;
}

void Enemy::onTick() {
    float dx = 0, dy = 0;
    float fx = x_ - std::floor(x_);
    float fy = y_ - std::floor(y_);

    for (int sx = 0; sx <= 1; sx++) {
        for (int sy = 0; sy <= 1; sy++) {
            float r = (sx ? fx : 1-fx) * (sy ? fy : 1-fy);
            int xi = (int)std::floor(x_) + sx;
            int yi = (int)std::floor(y_) + sy;
            if (xi < 0 || xi >= board->width || yi < 0 || yi >= board->height) continue;
            auto& cell = board->targetting[xi][yi];
            dx += cell.dx * r;
            dy += cell.dy * r;
        }
    }

    float ms = maxSpeed;
    if (board->spritesOverlappingCount(x_, y_, s, [](GameSprite* g){ return g->img=="swamp"; }))
        ms /= 2;

    if (dx == 0) dx = ms * (efrand() - 0.5f) / 5;
    if (dy == 0) dy = ms * (efrand() - 0.5f) / 5;

    vx += dx * accel;
    vy += dy * accel;
    float spd = std::sqrt(sq(vx) + sq(vy));
    if (spd > ms) { vx *= ms/spd; vy *= ms/spd; }

    float nx = x_ + vx;
    float ny = y_ + vy;
    bool tangible = board->spritesOverlappingCount(x_, y_, s,
        [](GameSprite* g){ return g->blocksEnemy; }) == 0;

    std::vector<GameSprite*> xb, yb;

    if (nx < 0 || nx > board->width-1) { vx = 0; }
    else if (tangible) {
        xb = board->spritesOverlapping(nx, y_, s);
        bool blocked = false;
        for (auto* g : xb) if (g->blocksEnemy) { blocked=true; break; }
        if (blocked) vx = 0;
        else xb.clear();
    }

    if (ny < 0 || ny > board->height-1) { vy = 0; }
    else if (tangible) {
        yb = board->spritesOverlapping(x_, ny, s);
        bool blocked = false;
        for (auto* g : yb) if (g->blocksEnemy) { blocked=true; break; }
        if (blocked) vy = 0;
        else yb.clear();
    }

    int rx = (int)std::round(x_), ry = (int)std::round(y_);
    rx = std::max(0, std::min(board->width-1, rx));
    ry = std::max(0, std::min(board->height-1, ry));
    float dist = board->targetting[rx][ry].dist;
    if (dist >= CHEATCOST) {
        for (auto* b : xb) if (b->hp) b->hp->hurt(10);
        for (auto* b : yb) if (b->hp) b->hp->hurt(10);
    }

    theta_ = std::atan2(vy, vx) * 180.0f / (float)M_PI;
    setX(x_ + vx);
    setY(y_ + vy);

    for (auto* s2 : board->spritesOverlapping(x_, y_, s)) {
        if (auto* city = dynamic_cast<City*>(s2)) {
            city->hp->hurt(big ? 2 : 1);
            destroy();
            return;
        }
    }
}

void Enemy::destroy() {
    if (_destroyed) return;
    board->addMoney(cr + (int)std::floor(mult));
    board->killedcr += cr;
    board->enemies.erase(uid);
    GameSprite::destroy();
}
