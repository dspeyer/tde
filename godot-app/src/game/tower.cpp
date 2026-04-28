#include "tower.h"
#include "board.h"
#include "hp.h"
#include "enemy.h"
#include "ammo.h"
#include "constants.h"
#include <cmath>
#include <algorithm>
#include <limits>

static float sq(float x) { return x*x; }
static int lastClumpTick = -1;

Tower::Tower(float x, float y, const std::string& img, Board* board)
    : GameSprite(x, y, ZTOWER, 2, img, board)
{
    blocksTower = true;
    blocksEnemy = true;

    auto& stats = towerStatsMap().at(img);
    range      = stats.range;
    damage     = stats.damage;
    reloadTime = stats.reloadTime;
    ammo       = stats.ammo;
    ammosize   = stats.ammosize;
    cost       = stats.cost;

    int nhills = board->spritesOverlappingCount(x, y, 2,
        [](GameSprite* g){ return g->img == "hills"; });
    range += nhills / 4.0f;

    board->setMoney(board->getMoney() - cost);
    hp       = new HP(this, 100);
    opacity  = 0.5f;
}

GameSprite* Tower::pickTarget() {
    if (board->target &&
        sq(board->target->x_ - x_) + sq(board->target->y_ - y_) < sq(range))
        return board->target;

    std::vector<GameSprite*> targets;
    for (auto& [uid, e] : board->enemies)
        if (sq(e->x_ - x_) + sq(e->y_ - y_) < sq(range))
            targets.push_back(e);
    if (targets.empty()) return nullptr;

    if (policy == "clump" && lastClumpTick < board->tickCount) {
        for (auto& [uid, i] : board->enemies) {
            float clump = 0;
            for (auto& [uid2, j] : board->enemies) {
                float d = std::max(std::sqrt(sq(i->x_-j->x_)+sq(i->y_-j->y_)), 0.1f);
                clump += 1.0f / d;
            }
            // store clumpiness — attach as custom field via dynamic_cast
            if (auto* en = dynamic_cast<Enemy*>(i))
                en->clumpScore = clump;
        }
        lastClumpTick = board->tickCount;
    }

    GameSprite* best = targets[0];
    float bestV = -1e18f;
    for (auto* t : targets) {
        float score = _scoreFor(policy, t);
        if (score > bestV) { bestV = score; best = t; }
    }
    return best;
}

float Tower::_scoreFor(const std::string& pol, GameSprite* t) const {
    int rx = std::max(0, std::min(board->width-1,  (int)std::round(t->x_)));
    int ry = std::max(0, std::min(board->height-1, (int)std::round(t->y_)));
    float dist = board->targetting[rx][ry].dist;

    if (pol == "first")   return -dist;
    if (pol == "biggest") return t->hp ? t->hp->current : 0;
    if (pol == "clump")   { if (auto* e=dynamic_cast<Enemy*>(t)) return e->clumpScore; return 0; }
    if (pol == "last")    return dist;
    return 0;
}

void Tower::onTick() {
    if (opacity < 1) {
        if (dying) {
            opacity -= 0.02f;
            if (opacity < 0.25f) { destroy(); return; }
        } else {
            opacity = std::min(1.0f, opacity + 0.02f);
        }
    }
    if (dying) return;

    reload++;
    GameSprite* t = pickTarget();
    if (!t) { theta_ = std::fmod(theta_ + 1, 360); return; }
    theta_ = std::atan2(t->y_ - y_, t->x_ - x_) * 180.0f / (float)M_PI;

    if (reload >= reloadTime) {
        if (img == "laser") {
            new LaserBolt(x_, y_, theta_, damage, range, board);
        } else {
            AmmoParams p{x_, y_, ammo, ammosize, theta_, range, damage};
            new Ammo(p, board);
        }
        reload = 0;
    }
}

bool Tower::upgrade() {
    if (board->getMoney() < cost) return false;
    board->setMoney(board->getMoney() - cost);
    cost  = (int)(cost * 2);
    damage    *= 1.5f;
    range     *= 1.1f;
    reloadTime /= 1.1f;
    upgraded++;
    return true;
}

void Tower::sell() {
    board->addMoney(cost / 2);
    dying = true;
    opacity = 0.75f;
}

void Tower::nextPolicy() {
    auto it = std::find(POLICY_NAMES.begin(), POLICY_NAMES.end(), policy);
    int idx = (int)(it - POLICY_NAMES.begin());
    policy = POLICY_NAMES[(idx + 1) % POLICY_NAMES.size()];
}

void Tower::destroy() {
    GameSprite::destroy();
    board->recalcTargetting();
    if (board->onRender) board->onRender();
}
