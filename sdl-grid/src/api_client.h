#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <cstdint>
#include <future>
#include <string>
#include <vector>

struct WindData {
  float speed = 0.0f;
  float degree = 0.0f;
  bool valid = false;
  std::string errorMessage;
};

struct PollutionData {
  float pm25 = 0.0f;
  bool valid = false;
  std::string errorMessage;
};

struct MapData {
  std::vector<uint8_t> imageBytes;
  bool valid = false;
  std::string errorMessage;
};

class ApiClient {
public:
  static std::future<WindData> fetchWindDataAsync(double lat, double lon,
                                                  const std::string &apiKey);
  static std::future<PollutionData>
  fetchPollutionDataAsync(double lat, double lon, const std::string &apiKey);
  static std::future<MapData> fetchMapImageAsync(double lat, double lon,
                                                 int width, int height,
                                                 int zoom,
                                                 const std::string &apiKey);
};

class CoordinateTransformer {
public:
  // Transforms Latitude/Longitude to screen X/Y using Mercator Projection
  static void latLonToScreen(double lat, double lon, int windowWidth,
                             int windowHeight, double &outX, double &outY);
};

#endif // API_CLIENT_H
