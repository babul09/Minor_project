#include "boundary.h"
#include <algorithm>

Boundary::Boundary(int rows_, int cols_)
    : rows(0), cols(0) {
    resize(rows_, cols_);
}

void Boundary::resize(int rows_, int cols_) {
    rows = rows_;
    cols = cols_;
    cells.assign(static_cast<size_t>(rows) * cols, false);
}

void Boundary::clear() noexcept {
    std::fill(cells.begin(), cells.end(), false);
}

void Boundary::toggleCell(int row, int col) noexcept {
    if (row < 0 || row >= rows || col < 0 || col >= cols) return;
    cells[row * cols + col] = !cells[row * cols + col];
}

void Boundary::setCell(int row, int col, bool value) noexcept {
    if (row < 0 || row >= rows || col < 0 || col >= cols) return;
    cells[row * cols + col] = value;
}

bool Boundary::isCellSelected(int row, int col) const noexcept {
    if (row < 0 || row >= rows || col < 0 || col >= cols) return false;
    return cells[row * cols + col];
}

bool Boundary::cellAt(float worldX, float worldY, const GridRect& gridRect,
                       int& outRow, int& outCol) const noexcept {
    if (worldX < gridRect.x || worldY < gridRect.y ||
        worldX >= gridRect.x + gridRect.w || worldY >= gridRect.y + gridRect.h) {
        return false;
    }
    const int cellSize = gridRect.cellSize;
    const float lx = worldX - gridRect.x;
    const float ly = worldY - gridRect.y;
    outCol = static_cast<int>(lx / cellSize);
    outRow = static_cast<int>(ly / cellSize);
    if (outRow < 0 || outRow >= rows || outCol < 0 || outCol >= cols) return false;
    return true;
}

bool Boundary::contains(float worldX, float worldY, const GridRect& gridRect) const noexcept {
    int r, c;
    if (!cellAt(worldX, worldY, gridRect, r, c)) return false;
    return isCellSelected(r, c);
}

int Boundary::selectedCount() const noexcept {
    return static_cast<int>(std::count(cells.begin(), cells.end(), true));
}

void Boundary::render(SDL_Renderer* renderer, const GridRect& gridRect,
                      float scale, float centerX, float centerY) const {
    if (rows <= 0 || cols <= 0) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 200, 50, 50, 120); // semi-transparent red

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (!isCellSelected(r, c)) continue;
            float world_x = static_cast<float>(gridRect.x + c * gridRect.cellSize);
            float world_y = static_cast<float>(gridRect.y + r * gridRect.cellSize);
            float sw = static_cast<float>(gridRect.cellSize) * scale;
            float sh = static_cast<float>(gridRect.cellSize) * scale;
            int sx = static_cast<int>(std::floor((world_x - centerX) * scale + centerX + 0.5f));
            int sy = static_cast<int>(std::floor((world_y - centerY) * scale + centerY + 0.5f));
            int sw_int = std::max(1, static_cast<int>(std::floor(sw + 0.5f)));
            int sh_int = std::max(1, static_cast<int>(std::floor(sh + 0.5f)));
            SDL_Rect rect = { sx, sy, sw_int, sh_int };
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}
