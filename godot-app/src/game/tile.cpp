#include "tile.h"
#include "board.h"
#include "hp.h"
#include "constants.h"

Tile::Tile(float x, float y, const std::string& img, Board* board)
    : GameSprite(x, y, ZTILE, 1, img, board)
{
    blocksTower = (img == "jungle" || img == "swamp" || img == "mountains");
    blocksEnemy = (img == "jungle" || img == "hills" || img == "mountains");
    hp          = new HP(this, 200);
    isJungle    = (img == "jungle");
    targettable = (img == "jungle");
}

void Tile::destroy() {
    if (_destroyed) return;
    GameSprite::destroy();
    new Tile(x_, y_, "plains", board);
    if (!board->suppressRecalc) {
        board->recalcTargetting();
        if (board->onRender) board->onRender();
    }
}
