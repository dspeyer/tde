#include "city.h"
#include "board.h"
#include "hp.h"
#include "enemy.h"
#include "constants.h"
#include <cmath>
#include <random>

static std::mt19937& rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}
static float frand() {
    return std::uniform_real_distribution<float>(0,1)(rng());
}

// City -----------------------------------------------------------------------
City::City(float x, float y, Board* board)
    : GameSprite(x, y, ZTOWER, 2, "city", board)
{
    blocksTower = true;
    hp = new HP(this, 10);
}

void City::destroy() {
    GameSprite::destroy();
    int cityCount = 0;
    for (auto& [uid, s] : board->sprites)
        if (dynamic_cast<City*>(s)) cityCount++;
    if (cityCount == 0)
        board->onDefeat();
    else
        board->recalcTargetting();
    if (board->onRender) board->onRender();
}

// EvilCity -------------------------------------------------------------------
EvilCity::EvilCity(float x, float y, Board* board)
    : GameSprite(x, y, ZTOWER, 2, "evilcity", board)
{
    blocksTower = true;
    targettable = (board->game_type==ASSAULT);
    if (targettable) hp = new HP(this, 10000);
    _pickEnemy();
}

float EvilCity::_wrandom() {
    float pow = (1.0f - vcr / 0.3f) / 2.0f;
    return std::pow(frand(), pow);
}

void EvilCity::_pickEnemy() {
    int speed  = (int)(_wrandom() * 3);
    bool big   = (vcr > 0.1f) && (_wrandom() < 0.5f);
    bool guns  = false;
    int  cr    = speed + (guns ? 1 : 0) + (big ? 3 : 0) + 1;
    if (cr_acc > 5)
        nextEnemy = {speed, true, guns, cr + 3};
    else
        nextEnemy = {speed, big, guns, cr};
}

void EvilCity::onTick() {
    if (board->totcr >= board->finalcr) return;
    vcr   += vvcr;
    cr_acc += vcr;
    int enemyCount = (int)board->enemies.size();
    if (cr_acc >= nextEnemy.cr && enemyCount < 30) {
        _spawnEnemy(nextEnemy);
        if (frand() < 0.3f) _pickEnemy();
    }
}

void EvilCity::_spawnEnemy(const EnemyOpts& opts) {
    new Enemy(x_, y_, opts, board);
    cr_acc -= opts.cr;
    board->totcr += opts.cr;
}
