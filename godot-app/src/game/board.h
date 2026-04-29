#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <cstdint>
#include <string>
#include "game_sprite.h"

// Callback type: renders a char and returns a [?][w] float mask (0=empty, 1=solid)
using EmojiStampFn = std::function<std::vector<std::vector<float>>(int codepoint, int w, int maxh)>;

class Tower;
class City;
class EvilCity;

struct TargetCell {
    float dist = 1e18f;
    int dx = 0, dy = 0;
};

struct MenuOpt {
    std::string img;
    int         cost;   // negative = sell price
    std::string label;  // displayed below icon
};

struct MenuState {
    float x, y;
    std::vector<MenuOpt> opts;
    std::function<void(std::string)> resolve;
};


enum GameType {
    CR_VICTORY,
    ASSAULT,
    GATHER,
    COUNT_GAME_TYPES
};

class Board {
public:
    int width, height;
    float cellSize;

    std::unordered_map<uint32_t, GameSprite*> sprites;
    std::unordered_map<uint32_t, GameSprite*> enemies;

    // spritesByPlace[x][y] = map of uid→sprite
    std::vector<std::vector<std::unordered_map<uint32_t, GameSprite*>>> spritesByPlace;
    std::vector<std::vector<TargetCell>> targetting;

    GameType game_type;
    
    int    tickCount  = 0;
    int    tickSpeed  = 0;
    float  money_     = 0;
    float  finalmoney = 0;
    float  killedcr   = 0; 
    float  totcr      = 0;
    float  finalcr    = 0;
    int    initLives  = 0;
    bool   showArrows = false;
    bool   gameOver   = false;
    bool   victory    = false;
    bool   flawless   = false;
    bool   suppressRecalc = false;

    GameSprite* target    = nullptr;
    MenuState*  menuState = nullptr;

    // callbacks (set by TDGame)
    std::function<void()>               onRender;
    std::function<void(bool,bool)>      onGameEvent; // (isVictory, flawless)

    EmojiStampFn emojiStampFn;

    Board(int w, int h, float cellSize, EmojiStampFn emojiStamp = nullptr);
    ~Board();

    float getMoney() const { return money_; }
    void  setMoney(float v);
    void  addMoney(float v) { setMoney(money_ + v); }

    void start();
    void tick();
    void uiTick();

    void recalcTargetting();
    std::pair<int,int> targettingProblem();
    bool targettingOK();

    // returns list of overlapping sprites (optionally filtered by predicate)
    std::vector<GameSprite*> spritesOverlapping(float x, float y, float s) const;
    int spritesOverlappingCount(float x, float y, float s,
                                std::function<bool(GameSprite*)> pred) const;

    void handleTap(float cellX, float cellY);
    bool trySetTarget(float cellX, float cellY); // hit-test at press time for moving targets
    void resolveMenu(const std::string& choice);

    void onDefeat();

    int  getLives() const;
    std::string getProgress() const;

    void toggleArrows();

private:
    void _generateMap();
    bool _trueish(int& counter);

    void _showTowerMenu(Tower* tower, GameSprite* rangeSprite = nullptr);
    void _showMenu(float x, float y, const std::vector<MenuOpt>& opts,
                   std::function<void(std::string)> resolve);

    void _rebuildArrows();
    void _rebuildEvilArrows();
    void _clearArrows();
    void _showPath(float x, float y, bool follow);
};
