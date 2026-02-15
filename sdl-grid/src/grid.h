#ifndef GRID_H
#define GRID_H

#include <SDL2/SDL.h>

struct GridRect { int x, y, w, h; int cellSize; };

class Grid {
public:
    Grid(int rows, int cols, int initialCellSize);
    void draw(SDL_Renderer *renderer, int win_w, int win_h, float scale = 1.0f);
    GridRect computeGridRect(int win_w, int win_h) const;
    float centerX(int win_w, int win_h) const;
    float centerY(int win_w, int win_h) const;

private:
    int rows, cols, initialCellSize;
};

#endif // GRID_H
