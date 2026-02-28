#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <cstdint>
#include <future>
#include <string>
#include <vector>

struct WindData {
  std::vector<float> speeds;
  std::vector<float> degrees;
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
  static std::future<WindData>
  fetchWindDataGridAsync(const std::vector<double> &lats,
                         const std::vector<double> &lons);
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

  // Calculates 9 coordinates (3x3 grid) spanning the current Mapbox viewport
  static void calculateGridCoordinates(double centerLat, double centerLon,
                                       int mapZoom, int windowWidth,
                                       int windowHeight,
                                       std::vector<double> &outLats,
                                       std::vector<double> &outLons);
};

#endif // API_CLIENT_H
