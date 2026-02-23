#include "grid.h"
#include <algorithm>
#include <cmath>

Grid::Grid(int rows_, int cols_, int initialCellSize_)
    : rows(rows_), cols(cols_), initialCellSize(initialCellSize_) {}

GridRect Grid::computeGridRect(int win_w, int win_h) const noexcept {
    const int cs_w = win_w / cols;
    const int cs_h = win_h / rows;
    const int cs = std::max(1, std::min(cs_w, cs_h));
    
    const int grid_w = cs * cols;
    const int grid_h = cs * rows;
    const int offset_x = (win_w - grid_w) / 2;
    const int offset_y = (win_h - grid_h) / 2;
    
    return GridRect{ offset_x, offset_y, grid_w, grid_h, cs };
}

void Grid::draw(SDL_Renderer* renderer, int win_w, int win_h, float scale) const {
    const GridRect r = computeGridRect(win_w, win_h);
    
    // Keep the grid centered while scaling
    const float cx = r.x + r.w * 0.5f;
    const float cy = r.y + r.h * 0.5f;
    const int sw = std::max(1, static_cast<int>(std::floor(r.w * scale + 0.5f)));
    const int sh = std::max(1, static_cast<int>(std::floor(r.h * scale + 0.5f)));
    const int sx = static_cast<int>(std::floor(cx - sw * 0.5f + 0.5f));
    const int sy = static_cast<int>(std::floor(cy - sh * 0.5f + 0.5f));
    const int scs = std::max(1, static_cast<int>(std::floor(r.cellSize * scale + 0.5f)));
    
    const GridRect sr = { sx, sy, sw, sh, scs };

    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    
    // Draw horizontal lines
    for (int i = 0; i <= rows; ++i) {
        const int y = sr.y + i * sr.cellSize;
        SDL_RenderDrawLine(renderer, sr.x, y, sr.x + sr.w, y);
    }
    
    // Draw vertical lines
    for (int j = 0; j <= cols; ++j) {
        const int x = sr.x + j * sr.cellSize;
        SDL_RenderDrawLine(renderer, x, sr.y, x, sr.y + sr.h);
    }
} 

float Grid::centerX(int win_w, int win_h) const noexcept {
    const GridRect r = computeGridRect(win_w, win_h);
    return r.x + r.w * 0.5f;
}

float Grid::centerY(int win_w, int win_h) const noexcept {
    const GridRect r = computeGridRect(win_w, win_h);
    return r.y + r.h * 0.5f;
}
