#include "api_client.h"
#include <cmath>
#include <curl/curl.h>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
size_t WriteCallbackString(void *contents, size_t size, size_t nmemb,
                           void *userp) {
  size_t totalSize = size * nmemb;
  auto *str = static_cast<std::string *>(userp);
  str->append(static_cast<char *>(contents), totalSize);
  return totalSize;
}

size_t WriteCallbackVector(void *contents, size_t size, size_t nmemb,
                           void *userp) {
  size_t totalSize = size * nmemb;
  auto *vec = static_cast<std::vector<uint8_t> *>(userp);
  auto *data = static_cast<uint8_t *>(contents);
  vec->insert(vec->end(), data, data + totalSize);
  return totalSize;
}
} // namespace

std::future<WindData> ApiClient::fetchWindDataAsync(double lat, double lon,
                                                    const std::string &apiKey) {
  return std::async(std::launch::async, [lat, lon, apiKey]() -> WindData {
    WindData result;
    CURL *curl = curl_easy_init();
    if (curl) {
      std::string url = "http://api.openweathermap.org/data/2.5/weather?lat=" +
                        std::to_string(lat) + "&lon=" + std::to_string(lon) +
                        "&appid=" + apiKey;
      std::string readBuffer;
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackString);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
      CURLcode res = curl_easy_perform(curl);

      if (res == CURLE_OK) {
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code >= 200 && response_code < 300) {
          try {
            auto j = json::parse(readBuffer);
            if (j.contains("wind")) {
              result.speed = j["wind"]["speed"].get<float>();
              result.degree = j["wind"]["deg"].get<float>();
              result.valid = true;
            } else {
              result.errorMessage = "Missing 'wind' object in response.";
            }
          } catch (const std::exception &e) {
            result.errorMessage =
                "JSON parse error (Wind): " + std::string(e.what());
            std::cerr << result.errorMessage << std::endl;
          }
        } else {
          result.errorMessage = "HTTP Error: " + std::to_string(response_code) +
                                " - " + readBuffer;
        }
      } else {
        result.errorMessage =
            "CURL Error: " + std::string(curl_easy_strerror(res));
      }
      curl_easy_cleanup(curl);
    }
    return result;
  });
}

std::future<PollutionData>
ApiClient::fetchPollutionDataAsync(double lat, double lon,
                                   const std::string &apiKey) {
  return std::async(std::launch::async, [lat, lon, apiKey]() -> PollutionData {
    PollutionData result;
    CURL *curl = curl_easy_init();
    if (curl) {
      std::string url =
          "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" +
          std::to_string(lat) + "&longitude=" + std::to_string(lon) +
          "&current=pm2_5";
      std::string readBuffer;

      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackString);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
      CURLcode res = curl_easy_perform(curl);

      if (res == CURLE_OK) {
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code >= 200 && response_code < 300) {
          try {
            auto j = json::parse(readBuffer);
            if (j.contains("current") && j["current"].contains("pm2_5")) {
              result.pm25 = j["current"]["pm2_5"].get<float>();
              result.valid = true;
            } else {
              result.errorMessage = "No PM2.5 data found in response.";
            }
          } catch (const std::exception &e) {
            result.errorMessage =
                "JSON parse error (Pollution): " + std::string(e.what());
            std::cerr << result.errorMessage << std::endl;
          }
        } else {
          result.errorMessage = "HTTP Error: " + std::to_string(response_code) +
                                " - " + readBuffer;
        }
      } else {
        result.errorMessage =
            "CURL Error: " + std::string(curl_easy_strerror(res));
      }
      curl_easy_cleanup(curl);
    }
    return result;
  });
}

std::future<MapData> ApiClient::fetchMapImageAsync(double lat, double lon,
                                                   int width, int height,
                                                   int zoom,
                                                   const std::string &apiKey) {
  return std::async(
      std::launch::async, [lat, lon, width, height, zoom, apiKey]() -> MapData {
        MapData result;
        CURL *curl = curl_easy_init();
        if (curl) {
          int clampedWidth = std::min(1280, std::max(1, width));
          int clampedHeight = std::min(1280, std::max(1, height));
          std::string url =
              "https://api.mapbox.com/styles/v1/mapbox/dark-v11/static/" +
              std::to_string(lon) + "," + std::to_string(lat) + "," +
              std::to_string(zoom) + ",0/" + std::to_string(clampedWidth) +
              "x" + std::to_string(clampedHeight) + "?access_token=" + apiKey;
          curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
          curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackVector);
          curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.imageBytes);
          CURLcode res = curl_easy_perform(curl);

          if (res == CURLE_OK) {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code >= 200 && response_code < 300) {
              if (!result.imageBytes.empty()) {
                result.valid = true;
              } else {
                result.errorMessage = "Received empty image from Mapbox.";
              }
            } else {
              std::string errStr(result.imageBytes.begin(),
                                 result.imageBytes.end());
              result.errorMessage =
                  "Mapbox HTTP Error: " + std::to_string(response_code) +
                  " - " + errStr;
            }
          } else {
            result.errorMessage =
                "CURL Error: " + std::string(curl_easy_strerror(res));
          }
          curl_easy_cleanup(curl);
        }
        return result;
      });
}

void CoordinateTransformer::latLonToScreen(double lat, double lon,
                                           int windowWidth, int windowHeight,
                                           double &outX, double &outY) {
  outX = (lon + 180.0) * (static_cast<double>(windowWidth) / 360.0);
  const double PI = 3.14159265358979323846;
  outY = (static_cast<double>(windowHeight) / 2.0) -
         (static_cast<double>(windowWidth) / (2.0 * PI)) *
             std::log(std::tan(PI / 4.0 + (lat * PI) / 360.0));
}
