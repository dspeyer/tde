#include "board.h"
#include "tile.h"
#include "city.h"
#include "enemy.h"
#include "tower.h"
#include "ammo.h"
#include "constants.h"
#include <cmath>
#include <queue>
#include <algorithm>
#include <random>
#include <iostream>
#include <string>
#include <map>

static std::mt19937& brng() { static std::mt19937 g(std::random_device{}()); return g; }
static float bfrand() { return std::uniform_real_distribution<float>(0,1)(brng()); }
static float sq(float x) { return x*x; }

Board::Board(int w, int h, float cellSize, EmojiStampFn emojiStamp)
    : width(w), height(h), cellSize(cellSize), emojiStampFn(std::move(emojiStamp))
{
    spritesByPlace.assign(w, std::vector<std::unordered_map<uint32_t,GameSprite*>>(h));
    targetting.assign(w, std::vector<TargetCell>(h));
}

Board::~Board() {
    // Sprites own themselves but we need to clean up
    std::vector<GameSprite*> all;
    for (auto& [uid, s] : sprites) all.push_back(s);
    sprites.clear();
    enemies.clear();
    for (auto* s : all) {
        s->_destroyed = true;
        delete s;
    }
    delete menuState;
}

void Board::setMoney(float v) {
    money_ = v;
    if (onRender) onRender();
}

void Board::start() { _generateMap(); }

bool Board::_trueish(int& counter) {
    if (++counter > 200) return false;
    return true;
}

void Board::_generateMap() {
    game_type = (GameType)(int)(bfrand() * COUNT_GAME_TYPES);
    new Announcement(game_type_names[game_type], this);
    static const std::vector<std::string> tiles = {/*"plains",*/"swamp","jungle","hills","mountains"};
    static std::map<std::string, std::string> halftype = {{"swamp","jungle"},{"jungle","swamp"},{"hills","jungle"},{"mountains","hills"}};
    static const std::vector<std::vector<int>> blocks = {{33, 127}, {0x2600, 0x2BFF}, {0x1F004, 0x1FAF8}, {0x1D100, 0x1D1FF}};
    std::vector<std::vector<std::string>> terrain(width, std::vector<std::string>(height, "plains"));
    int nfeatures = (int)(bfrand() * 5) + 4;
    for (int i = 0; i < nfeatures; i++) {
        auto& type = tiles[(int)(bfrand() * tiles.size())];

        if (bfrand() < 1.0f/3.0f && emojiStampFn) {
            // Emoji-shaped feature
            auto block = blocks[ (int)(bfrand()*blocks.size()) ];
            int codepoint = bfrand() * (block[1]-block[0]) + block[0];
            int fw = width * (bfrand() * 0.25 + 0.75);
            auto stamp = emojiStampFn(codepoint, fw, height);
            if (stamp.size() == 0) {
                i--;
                continue;
            }
            int fh = stamp.size();
            fw = stamp[0].size(); // usually unchanged, unless the char was too narrow
            int ox = (int)(bfrand() * std::max(1, width  - fw));
            int oy = (int)(bfrand() * std::max(1, height - fh));

            // std::cerr << std::hex << codepoint << " of " << type << " sized " << std::dec << fw << "x" << fh << " @ " << ox << "," << oy << " within " << width << "X" << height << std::endl;
            for (int sy = 0; sy < fh; sy++) {
                for (int sx = 0; sx < fw; sx++) {
                    if (stamp[sy][sx] > 0.5f) {
                        terrain[ox+sx][oy+sy] = type;
                    } else if (stamp[sy][sx] > 0.05f) {
                        terrain[ox+sx][oy+sy] = halftype[type];
                    }
                }
            }
            // terrain[ox][oy] = "mountains";
            // terrain[ox][oy+fh-1] = "mountains";
            // terrain[ox+fw-1][oy] = "mountains";
            // terrain[ox+fw-1][oy+fh-1] = "mountains";
        } else if (bfrand() < 0.5f) {
            // Line-segment feature (ridge, river, corridor)
            float cx = bfrand() * width, cy = bfrand() * height;
            float angle = bfrand() * 2 * (float)M_PI;
            float len  = std::min(width, height) * (0.3f + bfrand() * 0.4f);
            float x1 = cx - std::cos(angle) * len / 2;
            float y1 = cy - std::sin(angle) * len / 2;
            float x2 = cx + std::cos(angle) * len / 2;
            float y2 = cy + std::sin(angle) * len / 2;
            float maxW = std::min(width, height) * (0.08f + bfrand() * 0.07f);
            float segdx = x2-x1, segdy = y2-y1;
            float len2  = sq(segdx) + sq(segdy);
            for (int x = 0; x < width; x++) {
                for (int y = 0; y < height; y++) {
                    float t = (len2 > 0) ? std::max(0.f, std::min(1.f,
                        ((x-x1)*segdx + (y-y1)*segdy) / len2)) : 0.f;
                    float d = std::sqrt(sq(x - (x1+t*segdx)) + sq(y - (y1+t*segdy)));
                    if (d < maxW * (0.7f + bfrand() * 0.3f))
                        terrain[x][y] = type;
                }
            }
        } else {
            // Circular blob feature
            float cx = bfrand() * width, cy = bfrand() * height;
            float maxR = std::min(width, height) * (0.15f + bfrand() * 0.25f);
            for (int x = 0; x < width; x++) {
                for (int y = 0; y < height; y++) {
                    float d = std::sqrt(sq(x-cx)+sq(y-cy));
                    if (d < maxR * (0.7f + bfrand() * 0.3f))
                        terrain[x][y] = type;
                }
            }
        }
    }

    if (game_type == ASSAULT) {
        for (int y = 0; y < height/3; y++) {
            float p = 1 - 9 * (float)(y*y)/(float)(height*height);
            for (int x = 0; x < width; x++)
                if (bfrand() < p) terrain[x][y] = "jungle";
        }
    }
            
    // Evil cities (top third)
    int nevil = (int)(bfrand() * 3) + 1;
    for (int i = 0; i < nevil; i++) {
        int x = std::floor(bfrand() * (width - 1));
        int y = std::floor(bfrand() * (height / 3));
        for (int xi=0; xi<2; xi++)
            for (int yi=0; yi<2; yi++)
                terrain[x+xi][y+yi] = "plains";
        new EvilCity(x+0.5, y+0.5, this);
        money_ += 10;
    }

    // Good cities (bottom third)
    int ngood = (int)(bfrand() * 3) + 1;
    for (int i = 0; i < ngood; i++) {
        int x = std::floor(bfrand() * (width - 1));
        int y = std::floor(bfrand() * height / 3 + 2 * height / 3 - 1);
        new City(x+.5, y+.5, this);
        for (int xi=0; xi<2; xi++)
            for (int yi=0; yi<2; yi++)
                terrain[x+xi][y+yi] = "plains";
    }

    for (int x = 0; x < width; x++)
        for (int y = 0; y < height; y++)
            new Tile(x, y, terrain[x][y], this);

    // Ensure valid path
    suppressRecalc = true;
    int outer = 0;
    while (_trueish(outer)) {
        recalcTargetting();
        auto [px,py] = targettingProblem();
        if (px==-1) break;
        int inner = 0;
        while (_trueish(inner)) {
            auto blockers = spritesOverlapping(px, py, 1);
            for (auto* b : blockers) {
                if (b->blocksEnemy && !dynamic_cast<City*>(b) && !dynamic_cast<EvilCity*>(b)) {
                    b->destroy();
                    break;
                }
            }
            if (targetting[px][py].dist < CHEATCOST) {
                break;
            }
            if (bfrand() < 0.75) {
                py += py<height/2 ? 1 : -1;
            } else if (bfrand() < 0.5 && px>0) {
                px--;
            } else if (bfrand() < 0.5 && px<width-1) {
                px++;
            } else if (py>0 && py<height-1) {
                py += py<height/2 ? -1 : 1;                
            }
        }
        if (outer>150) {
            GameSprite* city;
            for (auto& [uid,sprite] : sprites) if (dynamic_cast<City*>(sprite)) { city=sprite; break; }
            float px = city->x_ + (bfrand()<0.5 ? -.5 : .5);
            float py = city->y_ + (bfrand()<0.5 ? -.5 : .5);
            float dx = (bfrand()<0.5) ? -1 : 1;
            float dy = bfrand() - 0.5;
            while (px>0 && px<width) {
                auto blockers = spritesOverlapping(px, py, 1);
                for (auto* b : blockers) {
                    if (b->blocksEnemy && !dynamic_cast<City*>(b) && !dynamic_cast<EvilCity*>(b)) {
                        b->destroy();
                    }
                }
                px += dx;
                py += dy;
            }
        }
    }
    suppressRecalc = false;

    int plainCount = 0;
    for (auto& [uid, s] : sprites)
        if (s->img == "plains") plainCount++;
    if (game_type == CR_VICTORY)
        finalcr = std::round(std::pow(plainCount, 1.3f) / 5);
    else
        finalcr = std::numeric_limits<float>::infinity();
    if (game_type == GATHER)
        finalmoney = std::round(std::pow(plainCount, 1.3f) / 5);
    else
        finalmoney = std::numeric_limits<float>::infinity();        
    
    for (auto& [uid, s] : sprites)
        if (dynamic_cast<City*>(s)) initLives += (int)s->hp->current;

    // Always show evil-city arrows on start
    _rebuildEvilArrows();
    if (onRender) onRender();
}

void Board::recalcTargetting() {
    for (auto& col : targetting)
        for (auto& cell : col) { cell.dist = 1e18f; cell.dx = 0; cell.dy = 0; }

    std::queue<std::pair<int,int>> q;
    for (auto& [uid, s] : sprites) {
        if (!dynamic_cast<City*>(s)) continue;
        for (int dx : {-1,1}) for (int dy : {-1,1}) {
            int cx = (int)std::round(s->x_ + dx*0.5f);
            int cy = (int)std::round(s->y_ + dy*0.5f);
            if (cx<0||cy<0||cx>=width||cy>=height) continue;
            targetting[cx][cy] = {0, 0, 0};
            q.push({cx, cy});
        }
    }

    static const int ddx[] = {-1,1,0,0};
    static const int ddy[] = {0,0,-1,1};
    while (!q.empty()) {
        auto [x, y] = q.front(); q.pop();
        float cost = spritesOverlappingCount(x, y, 1,
            [](GameSprite* g){ return g->blocksEnemy; }) ? CHEATCOST : 1;
        float nd = targetting[x][y].dist + cost;
        for (int d = 0; d < 4; d++) {
            int xn = x+ddx[d], yn = y+ddy[d];
            if (xn<0||yn<0||xn>=width||yn>=height) continue;
            if (targetting[xn][yn].dist <= nd) continue;
            targetting[xn][yn] = {nd, -ddx[d], -ddy[d]};
            q.push({xn, yn});
        }
    }

    if (showArrows) _rebuildArrows();
    else            _rebuildEvilArrows();
}

std::pair<int,int> Board::targettingProblem() {
    for (auto& [uid, s] : sprites) {
        if (s->img != "evilcity" && !s->isEnemy) continue;
        int rx = std::max(0, std::min(width-1,  (int)std::round(s->x_)));
        int ry = std::max(0, std::min(height-1, (int)std::round(s->y_)));
        bool intangible = spritesOverlappingCount(s->x_, s->y_, s->s,
            [](GameSprite* g){ return g->blocksEnemy; }) > 0;
        if (targetting[rx][ry].dist >= CHEATCOST && !intangible) return {rx,ry};
    }
    return {-1,-1};
}

bool Board::targettingOK() {
    return targettingProblem().first == -1;
}

std::vector<GameSprite*> Board::spritesOverlapping(float x, float y, float s) const {
    std::unordered_map<uint32_t, GameSprite*> out;
    int x0=(int)std::round(x-s/2+0.001f), x1=(int)std::round(x+s/2-0.001f);
    int y0=(int)std::round(y-s/2+0.001f), y1=(int)std::round(y+s/2-0.001f);
    for (int xi=x0; xi<=x1; xi++) {
        for (int yi=y0; yi<=y1; yi++) {
            if (xi<0||yi<0||xi>=width||yi>=height) continue;
            for (auto& [uid, sp] : spritesByPlace[xi][yi]) {
                if (std::max(std::abs(sp->x_-x), std::abs(sp->y_-y)) < (sp->s+s)/2)
                    out[uid] = sp;
            }
        }
    }
    std::vector<GameSprite*> result;
    result.reserve(out.size());
    for (auto& [uid, sp] : out) result.push_back(sp);
    return result;
}

int Board::spritesOverlappingCount(float x, float y, float s,
    std::function<bool(GameSprite*)> pred) const
{
    int cnt = 0;
    for (auto* sp : spritesOverlapping(x, y, s))
        if (pred(sp)) cnt++;
    return cnt;
}

void Board::uiTick() {
    std::vector<uint32_t> uids;
    uids.reserve(sprites.size());
    for (auto& [uid,_] : sprites) uids.push_back(uid);
    for (auto uid : uids) {
        auto it = sprites.find(uid);
        if (it != sprites.end()) it->second->onUITick();
    }
    if (onRender) onRender();
}

void Board::tick() {
    tickCount++;
    // Copy uid list to avoid invalidation during iteration
    std::vector<uint32_t> uids;
    uids.reserve(sprites.size());
    for (auto& [uid,_] : sprites) uids.push_back(uid);
    for (auto uid : uids) {
        auto it = sprites.find(uid);
        if (it != sprites.end()) it->second->onTick();
    }

    if ((totcr>=finalcr && enemies.empty()) || money_>finalmoney) {
        if (!gameOver) {
            gameOver = true; victory = true;
        }
    }
    bool has_enemy = false;
    for (auto& [uid, s] : sprites) {
        if (dynamic_cast<EvilCity*>(s) || dynamic_cast<Enemy*>(s)) {
            has_enemy = true;
            break;
        }
    }
    if ( ! has_enemy) {
        gameOver = true;
        victory = true;
    }
            
    if (victory) {
        int lives = getLives();
        flawless = (lives == initLives);
        if (onGameEvent) onGameEvent(true, flawless);
    }
    if (onRender) onRender();
}

bool Board::trySetTarget(float cx, float cy) {
    if (menuState) return false;
    for (auto& [uid, s] : sprites) {
        if (!s->targettable) continue;
        if (std::abs(s->x_ - cx) < s->s/2+0.2f && std::abs(s->y_ - cy) < s->s/2+0.2f) {
            target = (target == s) ? nullptr : s;
            if (onRender) onRender();
            return true;
        }
    }
    return false;
}

void Board::handleTap(float cx, float cy) {
    if (menuState) return;

    // Tapped existing tower?
    for (auto& [uid, s] : sprites) {
        auto* t = dynamic_cast<Tower*>(s);
        if (!t) continue;
        if (std::abs(t->x_ - cx) < t->s/2+0.1f && std::abs(t->y_ - cy) < t->s/2+0.1f) {
            _showTowerMenu(t);
            return;
        }
    }

    // Tapped targettable?
    for (auto& [uid, s] : sprites) {
        if (!s->targettable) continue;
        if (std::abs(s->x_ - cx) < s->s/2+0.2f && std::abs(s->y_ - cy) < s->s/2+0.2f) {
            target = (target == s) ? nullptr : s;
            if (onRender) onRender();
            return;
        }
    }

    // Place tower
    float x = std::round(cx+0.5f) - 0.5f;
    float y = std::round(cy+0.5f) - 0.5f;
    x = std::max(0.5f, std::min((float)width  - 1.5f, x));
    y = std::max(0.5f, std::min((float)height - 1.5f, y));

    if (spritesOverlappingCount(x, y, 2, [](GameSprite* g){ return g->blocksTower; })) return;

    auto* ph = new GameSprite(x, y, ZTOWER, 2, "placeholder", this);
    ph->blocksEnemy = true;
    recalcTargetting();

    if (!targettingOK()) {
        new FailMarker(x, y, 2, this);
        ph->destroy(); delete ph;
        recalcTargetting();
        if (onRender) onRender();
        return;
    }

    std::vector<MenuOpt> opts;
    for (auto& name : TOWER_NAMES) {
        int c = towerStatsMap().at(name).cost;
        opts.push_back({name, c, "$"+std::to_string(c)});
    }
    if (onRender) onRender();

    _showMenu(x, y, opts, [this, x, y, ph](std::string choice) {
        ph->destroy(); delete ph;
        if (!choice.empty()) {
            auto* tower = new Tower(x, y, choice, this);
            recalcTargetting();
            if (!targettingOK() || money_ < 0) {
                setMoney(money_ + tower->cost);
                tower->destroy(); delete tower;
                recalcTargetting();
            }
        } else {
            recalcTargetting();
        }
        if (onRender) onRender();
    });
}

void Board::_showTowerMenu(Tower* tower, GameSprite* rangeSprite) {
    if (!rangeSprite) {
        rangeSprite = new GameSprite(tower->x_, tower->y_, ZTOWER-1,
                                     2*tower->range, "", this);
        rangeSprite->isRange = true;
        rangeSprite->opacity = 0.7f;
    }

    int sell = tower->cost / 2;
    std::vector<MenuOpt> opts = {
        {"upgrade", tower->cost, "$"+std::to_string(tower->cost)},
        {"sell",    -sell,       "+$"+std::to_string(sell)},
        {tower->policy, 0,      tower->policy},
    };

    _showMenu(tower->x_, tower->y_, opts,
        [this, tower, rangeSprite](std::string choice) {
            if (choice == "upgrade") {
                tower->upgrade();
                rangeSprite->s = 2 * tower->range;
                _showTowerMenu(tower, rangeSprite);
            } else if (choice == "sell") {
                rangeSprite->destroy(); delete rangeSprite;
                tower->sell();
                if (onRender) onRender();
            } else if (!choice.empty() && choice == tower->policy) {
                tower->nextPolicy();
                _showTowerMenu(tower, rangeSprite);
            } else {
                rangeSprite->destroy(); delete rangeSprite;
                if (onRender) onRender();
            }
        });
}

void Board::_showMenu(float x, float y, const std::vector<MenuOpt>& opts,
                      std::function<void(std::string)> resolve) {
    menuState = new MenuState{x, y, opts, resolve};
    if (onRender) onRender();
}

void Board::resolveMenu(const std::string& choice) {
    if (!menuState) return;
    auto ms = menuState;
    menuState = nullptr;
    if (onRender) onRender();
    ms->resolve(choice);
    delete ms;
}

void Board::onDefeat() {
    if (gameOver) return;
    gameOver = true;
    if (onGameEvent) onGameEvent(false, false);
}

int Board::getLives() const {
    int total = 0;
    for (auto& [uid, s] : sprites)
        if (dynamic_cast<const City*>(s) && s->hp)
            total += (int)s->hp->current;
    return total;
}

std::string Board::getProgress() const {
    std::string out = std::to_string((int)killedcr) + "/" + std::to_string((int)std::min(totcr, finalcr)) + "/";
    if (game_type == CR_VICTORY) out += std::to_string((int)finalcr);
    else out += u8"∞";
    return out;
}

void Board::toggleArrows() {
    showArrows = !showArrows;
    if (showArrows) _rebuildArrows();
    else            _rebuildEvilArrows();
    if (onRender) onRender();
}

void Board::_clearArrows() {
    std::vector<GameSprite*> arrows;
    for (auto& [uid, s] : sprites)
        if (s->isArrow) arrows.push_back(s);
    for (auto* a : arrows) { a->destroy(); delete a; }
}

void Board::_rebuildArrows() {
    _clearArrows();
    for (int x = 0; x < width; x++)
        for (int y = 0; y < height; y++)
            _showPath(x, y, false);
}

void Board::_rebuildEvilArrows() {
    _clearArrows();
    for (auto& [uid, s] : sprites) {
        if (!dynamic_cast<EvilCity*>(s)) continue;
        _showPath(s->x_ - 0.5f, s->y_ - 0.5f, true);
        _showPath(s->x_ + 0.5f, s->y_ - 0.5f, true);
        _showPath(s->x_ - 0.5f, s->y_ + 0.5f, true);
        _showPath(s->x_ + 0.5f, s->y_ + 0.5f, true);
    }
}

void Board::_showPath(float x, float y, bool follow) {
    if (spritesOverlappingCount(x, y, 1, [](GameSprite* g){ return g->isArrow; })) return;
    int xi = (int)std::round(x), yi = (int)std::round(y);
    if (xi<0||yi<0||xi>=width||yi>=height) return;
    auto& cell = targetting[xi][yi];
    if (cell.dist >= 1e17f) return;
    float theta = std::atan2(cell.dy, cell.dx) * 180.0f / (float)M_PI;
    auto* arrow = new GameSprite(x, y, ZARROW, 1, "arrow", this);
    arrow->isArrow  = true;
    arrow->theta_   = theta;
    arrow->opacity  = follow ? 0.4f : 0.8f;
    if (follow && (cell.dx || cell.dy))
        _showPath(x + cell.dx, y + cell.dy, true);
}
