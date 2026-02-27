#ifndef BOUNDARY_H
#define BOUNDARY_H

#include <SDL2/SDL.h>
#include <vector>
#include "grid.h"

// Manages a set of grid cells that define a containment boundary for particles.
// Users can "lasso" select cells; particles are kept inside the selected region.
class Boundary {
public:
    Boundary(int rows = 0, int cols = 0);

    // resize the internal map (clears previous selections)
    void resize(int rows, int cols);
    void clear() noexcept;

    // cell operations
    void toggleCell(int row, int col) noexcept;
    void setCell(int row, int col, bool value) noexcept;
    [[nodiscard]] bool isCellSelected(int row, int col) const noexcept;

    // world-coordinate queries
    [[nodiscard]] bool contains(float worldX, float worldY, const GridRect& gridRect) const noexcept;
    [[nodiscard]] bool cellAt(float worldX, float worldY, const GridRect& gridRect,
                               int& outRow, int& outCol) const noexcept;
    [[nodiscard]] int selectedCount() const noexcept;

    // rendering helper for visualizing the boundary
    void render(SDL_Renderer* renderer, const GridRect& gridRect,
                float scale = 1.0f, float centerX = 0.0f, float centerY = 0.0f) const;

private:
    int rows;
    int cols;
    std::vector<bool> cells; // row-major
};

#endif // BOUNDARY_H
