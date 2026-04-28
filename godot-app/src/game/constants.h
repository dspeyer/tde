#pragma once
#include <string>
#include <unordered_map>

static const int ZTILE   = 0;
static const int ZARROW  = 5;
static const int ZTOWER  = 10;
static const int ZENEMY  = 20;
static const int ZAMMO   = 30;
static const int ZCOVER  = 40;

static const int CHEATCOST = 1000000;

struct TowerStats {
    float range;
    float damage;
    float reloadTime;
    std::string ammo;
    float ammosize;
    int   cost;
    std::string desc;
};

inline const std::unordered_map<std::string, TowerStats>& towerStatsMap() {
    static std::unordered_map<std::string, TowerStats> m = {
        {"artillery",    {4,  5,     4, "shells",     0.2f,  5, "Basic damage-dealer"}},
        {"cannon",       {6,  240, 160, "cannonball", 0.2f,  5, "Maximum DPS, but slow to fire"}},
        {"howitzer",     {4,  15,   25, "rocket",     0.5f, 10, "Fires rockets that explode on target"}},
        {"laser",        {3,  15,   50, "laserbolt",  0.2f, 15, "Overpenetrates to the edge of the world"}},
        {"flamethrower", {3,  0.3f,  3, "flame",      0.5f, 15, "Short range, damages all enemies it covers"}},
        {"pusher",       {4,  0.75f,50, "wind",       1.0f, 10, "Shoves targets back a short distance"}},
    };
    return m;
}

static const std::vector<std::string> TOWER_NAMES = {
    "artillery","cannon","howitzer","laser","flamethrower","pusher"
};

static const std::vector<std::string> POLICY_NAMES = {
    "first","biggest","clump","last"
};

