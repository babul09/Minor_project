#include "api_client.h"
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
  std::cout << "All tests passed!\n";
  return 0;
}
