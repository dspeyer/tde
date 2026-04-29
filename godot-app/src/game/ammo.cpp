#include "ammo.h"
#include "board.h"
#include "hp.h"
#include "enemy.h"
#include "tile.h"
#include "constants.h"
#include <cmath>

static float sq(float x) { return x*x; }

// Ammo -----------------------------------------------------------------------
Ammo::Ammo(const AmmoParams& p, Board* board)
    : GameSprite(p.x, p.y, ZAMMO, p.ammosize, p.ammo, board)
    , startX(p.x), startY(p.y), range(p.range), damage(p.damage)
    , ammoType(p.ammo)
{
    simple = (p.ammo == "cannonball" || p.ammo == "shells");
    theta_ = p.theta;
}

void Ammo::onTick() {
    float vx = 0.2f * std::cos(theta_ * (float)M_PI / 180);
    float vy = 0.2f * std::sin(theta_ * (float)M_PI / 180);
    setX(x_ + vx); setY(y_ + vy);

    if (sq(startX - x_) + sq(startY - y_) > sq(range)) { destroy(); return; }

    auto over = board->spritesOverlapping(x_, y_, s);
    for (auto* t : over) {
        if (ammoType == "rocket" && (dynamic_cast<Enemy*>(t) || t == board->target)) {
            setX(x_ + 2*vx); setY(y_ + 2*vy);
            new Explosion(x_, y_, damage, board);
            destroy(); return;
        }
        if (!dynamic_cast<Enemy*>(t) && t != board->target) continue;
        if (dynamic_cast<Tile*>(t) && !simple) continue;
        if (t->hp) t->hp->hurt(damage);
        if (ammoType == "wind") { /* pusher: handled below */ }
        if (simple) { destroy(); return; }
    }
    // wind push
    if (ammoType == "wind") {
        for (auto* t : over) {
            if (auto* e = dynamic_cast<Enemy*>(t)) {
                e->vx = vx / 2; e->vy = vy / 2;
            }
        }
    }
}

// LaserBolt ------------------------------------------------------------------
LaserBolt::LaserBolt(float x, float y, float theta, float damage, float range, Board* board)
    : GameSprite(x, y, ZAMMO, 0.2f, "laserbolt", board)
{
    opacity    = 0.9f;
    isLaser    = true;
    laserTheta = theta;
    theta_     = theta;

    float step = 0.2f;
    for (float i = 0; i <= board->width + board->height; i += step) {
        float xi = x + i * std::cos(theta * (float)M_PI / 180);
        float yi = y + i * std::sin(theta * (float)M_PI / 180);
        for (auto* e : board->spritesOverlapping(xi, yi, 0.2f)) {
            if (dynamic_cast<Enemy*>(e) && e->hp)
                e->hp->hurt(damage);
        }
    }
}

void LaserBolt::onTick() {
    opacity -= 0.2f;
    if (opacity < 0) destroy();
}

// Explosion ------------------------------------------------------------------
Explosion::Explosion(float x, float y, float damage, Board* board)
    : GameSprite(x, y, ZAMMO, 3, "explosion", board)
{
    opacity = 1;
    for (auto* e : board->spritesOverlapping(x, y, 3))
        if (dynamic_cast<Enemy*>(e) && e->hp) e->hp->hurt(damage);
}

void Explosion::onTick() {
    opacity -= 0.2f;
    if (opacity <= 0) destroy();
}

// FailMarker -----------------------------------------------------------------
FailMarker::FailMarker(float x, float y, float s, Board* board)
    : GameSprite(x, y, ZAMMO+5, s, "fail", board)
{
    opacity = 1;
}

void FailMarker::onUITick() {
    opacity -= 0.1f;
    if (opacity <= 0) destroy();
}
