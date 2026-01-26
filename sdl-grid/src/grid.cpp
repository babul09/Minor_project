#include "grid.h"
#include <algorithm>

Grid::Grid(int rows_, int cols_, int initialCellSize_)
    : rows(rows_), cols(cols_), initialCellSize(initialCellSize_) {}

GridRect Grid::computeGridRect(int win_w, int win_h) const {
    int cs_w = win_w / cols;
    int cs_h = win_h / rows;
    int cs = cs_w < cs_h ? cs_w : cs_h;
    if (cs < 1) cs = 1;
    int grid_w = cs * cols;
    int grid_h = cs * rows;
    int offset_x = (win_w - grid_w) / 2;
    int offset_y = (win_h - grid_h) / 2;
    GridRect r = { offset_x, offset_y, grid_w, grid_h, cs };
    return r;
}

void Grid::draw(SDL_Renderer *renderer, int win_w, int win_h) {
    GridRect r = computeGridRect(win_w, win_h);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    for (int i = 0; i <= rows; ++i) {
        int y = r.y + i * r.cellSize;
        SDL_RenderDrawLine(renderer, r.x, y, r.x + r.w, y);
    }
    for (int j = 0; j <= cols; ++j) {
        int x = r.x + j * r.cellSize;
        SDL_RenderDrawLine(renderer, x, r.y, x, r.y + r.h);
    }
}

float Grid::centerX(int win_w, int win_h) const {
    GridRect r = computeGridRect(win_w, win_h);
    return r.x + r.w * 0.5f;
}

float Grid::centerY(int win_w, int win_h) const {
    GridRect r = computeGridRect(win_w, win_h);
    return r.y + r.h * 0.5f;
}
