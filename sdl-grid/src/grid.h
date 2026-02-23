#ifndef GRID_H
#define GRID_H

#include <SDL2/SDL.h>

// Structure representing a grid rectangle with cell size information
struct GridRect { 
    int x, y, w, h; 
    int cellSize; 
};

// Grid class for managing and rendering a grid layout
class Grid {
public:
    Grid(int rows, int cols, int initialCellSize);
    
    // Render the grid with optional scaling
    void draw(SDL_Renderer* renderer, int win_w, int win_h, float scale = 1.0f) const;
    
    // Compute the grid rectangle based on window dimensions
    [[nodiscard]] GridRect computeGridRect(int win_w, int win_h) const noexcept;
    
    // Get the center X coordinate of the grid
    [[nodiscard]] float centerX(int win_w, int win_h) const noexcept;
    
    // Get the center Y coordinate of the grid
    [[nodiscard]] float centerY(int win_w, int win_h) const noexcept;

private:
    int rows;
    int cols;
    int initialCellSize;
};

#endif // GRID_H
