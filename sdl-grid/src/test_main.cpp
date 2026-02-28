#include "api_client.h"
#include "boundary.h"
#include "grid.h"
#include "particle.h"
#include <cmath>
#include <iostream>

int main() {
  std::cout << "Running tests...\n";

  double outX, outY;
  CoordinateTransformer::latLonToScreen(0.0, 0.0, 800, 600, outX, outY);

  if (std::abs(outX - 400.0) > 1e-4) {
    std::cerr << "Test Failed: outX should be 400, got " << outX << "\n";
    return 1;
  }
  if (std::abs(outY - 300.0) > 1e-4) {
    std::cerr << "Test Failed: outY should be 300, got " << outY << "\n";
    return 1;
  }

  std::cout << "CoordinateTransformer math OK.\n";

  // Test Grid boundaries
  Grid grid(10, 10, 20); // 10x10 cells
  GridRect rect = grid.computeGridRect(
      800, 600); // 800/10=80, 600/10=60, so cs=60 -> 600x600
  if (rect.w != 600 || rect.h != 600) {
    std::cerr
        << "Test Failed: GridRect dimensions incorrect. Expected 600x600, got "
        << rect.w << "x" << rect.h << "\n";
    return 1;
  }
  std::cout << "Grid bounds OK.\n";

  // Test ParticleSystem Initialization
  ParticleSystem ps(100);
  if (ps.activeCount() != 0) {
    std::cerr << "Test Failed: Particle system should start empty.\n";
    return 1;
  }

  // Test Basic Physics step
  ps.emitting = false;
  Boundary b(10, 10);
  // Spawn one manually if we can, or just check update doesn't crash
  ps.update(0.1f, 0.0f, 0.0f, b, rect);

  std::cout << "ParticleSystem init and update OK.\n";

  std::cout << "All tests passed!\n";
  return 0;
}
