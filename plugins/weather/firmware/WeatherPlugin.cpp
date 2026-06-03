#include "WeatherPlugin.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

#include <cmath>
#include <cstring>
#include <ctime>

namespace {

constexpr uint8_t MAX_WEATHER_CACHE = 4;
constexpr uint8_t MAX_WEATHER_DAYS = 5;
constexpr uint32_t FETCH_INTERVAL_MS = 60UL * 60UL * 1000UL;
constexpr uint32_t ERROR_RETRY_INTERVAL_MS = 60UL * 60UL * 1000UL;

constexpr uint16_t DEFAULT_ICON_COLOR = 0xFE8C;
constexpr uint16_t DEFAULT_LABEL_COLOR = 0x07FF;
constexpr uint16_t DEFAULT_TEMPERATURE_COLOR = 0xFFFF;

enum class WeatherIcon : uint8_t {
  Clear,
  Clouds,
  Rain,
  Thunderstorm,
  Snow,
  Atmosphere,
  Unknown,
};

struct WeatherConfig {
  String apiKey;
  String city = "Paris,FR";
  uint8_t dayOffset = 0;
  String hour = "any";
  String title;
  bool showIcon = true;
  String value1 = "none";
  String value2 = "none";
  String value3 = "temp_min";
  String value4 = "temp_max";
  uint16_t iconColor = DEFAULT_ICON_COLOR;
  uint16_t labelColor = DEFAULT_LABEL_COLOR;
  uint16_t valueColor = DEFAULT_TEMPERATURE_COLOR;
};

struct WeatherDay {
  String dateKey;
  int16_t minTemp = 0;
  int16_t maxTemp = 0;
  int16_t minFeelsLike = 0;
  int16_t maxFeelsLike = 0;
  WeatherIcon icon = WeatherIcon::Unknown;
  uint8_t iconPriority = 0;
  bool available = false;
  uint8_t humidity = 0;
  uint16_t pressure = 0;
  float maxWindSpeed = 0.0f;
  uint16_t windDeg = 0;
  float totalRain = 0.0f;
  uint8_t maxPop = 0;
  uint8_t avgClouds = 0;
  uint8_t slotCount = 0;
  uint32_t humiditySum = 0;
  uint32_t pressureSum = 0;
  uint32_t cloudsSum = 0;
};

struct WeatherCache {
  String city;
  String apiKey;
  String hour;
  WeatherDay days[MAX_WEATHER_DAYS];
  uint8_t dayCount = 0;
  String errorCode;
  uint32_t lastFetchMs = 0;
  bool initialised = false;
  bool hasError = false;
};

struct WeatherRenderCache {
  bool rendered = false;
  int16_t cellLeft = 0;
  int16_t cellTop = 0;
  int16_t cellWidth = 0;
  int16_t cellHeight = 0;
  String configJson;
  String displayKey;
};

static WeatherCache weatherCache[MAX_WEATHER_CACHE];
static WeatherRenderCache weatherRenderCache[MAX_LAYOUT_CELLS];
static uint8_t weatherCacheCount = 0;

int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

uint16_t parseColor(const char* text, uint16_t fallback) {
  if (!text || text[0] != '#') return fallback;
  size_t len = 0;
  while (text[len]) len++;
  if (len != 7) return fallback;

  uint8_t channels[3] = {0, 0, 0};
  for (uint8_t i = 0; i < 3; i++) {
    const int hi = hexNibble(text[1 + i * 2]);
    const int lo = hexNibble(text[2 + i * 2]);
    if (hi < 0 || lo < 0) return fallback;
    channels[i] = static_cast<uint8_t>((hi << 4) | lo);
  }

  return static_cast<uint16_t>(((channels[0] & 0xF8) << 8) |
                               ((channels[1] & 0xFC) << 3) |
                               (channels[2] >> 3));
}

uint8_t parseDayOffset(JsonVariantConst value) {
  if (value.is<int>()) {
    const int day = value.as<int>();
    return day >= 0 && day < MAX_WEATHER_DAYS ? static_cast<uint8_t>(day) : 0;
  }

  if (!value.is<const char*>()) return 0;
  const char* text = value.as<const char*>();
  if (!text || text[1] != '\0') return 0;
  if (text[0] < '0' || text[0] > '4') return 0;
  return static_cast<uint8_t>(text[0] - '0');
}

bool parseRelativeHour(const char* value, uint8_t& hoursAhead) {
  if (value == nullptr || value[0] != 'h' || value[1] != '+') return false;

  uint16_t parsed = 0;
  bool hasDigit = false;
  for (const char* digit = value + 2; *digit != '\0'; digit++) {
    if (*digit < '0' || *digit > '9') return false;

    hasDigit = true;
    parsed = static_cast<uint16_t>(parsed * 10 + (*digit - '0'));
    if (parsed > 96) return false;
  }

  if (!hasDigit || parsed == 0) return false;

  hoursAhead = static_cast<uint8_t>(parsed);
  return true;
}

WeatherConfig readConfig(const String& configJson) {
  WeatherConfig cfg;

  JsonDocument doc;
  if (deserializeJson(doc, configJson)) return cfg;

  cfg.apiKey = doc["apiKey"] | "";
  cfg.apiKey.trim();
  cfg.city = doc["city"] | "Paris,FR";
  cfg.city.trim();

  cfg.dayOffset = parseDayOffset(doc["dayOffset"]);

  if (doc["hour"].is<const char*>()) {
    const char* h = doc["hour"].as<const char*>();
    if (h) {
      const size_t hlen = strlen(h);
      bool validHour = false;
      if (hlen == 2) {
        validHour = strcmp(h, "00") == 0 || strcmp(h, "03") == 0 ||
                    strcmp(h, "06") == 0 || strcmp(h, "09") == 0 ||
                    strcmp(h, "12") == 0 || strcmp(h, "15") == 0 ||
                    strcmp(h, "18") == 0 || strcmp(h, "21") == 0;
      } else {
        uint8_t relativeHour = 0;
        validHour = parseRelativeHour(h, relativeHour);
      }
      if (validHour) cfg.hour = h;
    }
  }

  if (doc["title"].is<const char*>()) {
    cfg.title = doc["title"].as<const char*>();
  }
  if (doc["showIcon"].is<bool>()) {
    cfg.showIcon = doc["showIcon"].as<bool>();
  }
  if (doc["value1"].is<const char*>()) {
    cfg.value1 = doc["value1"].as<const char*>();
  }
  if (doc["value2"].is<const char*>()) {
    cfg.value2 = doc["value2"].as<const char*>();
  }
  if (doc["value3"].is<const char*>()) {
    cfg.value3 = doc["value3"].as<const char*>();
  }
  if (doc["value4"].is<const char*>()) {
    cfg.value4 = doc["value4"].as<const char*>();
  }
  if (doc["iconColor"].is<const char*>()) {
    cfg.iconColor =
        parseColor(doc["iconColor"].as<const char*>(), DEFAULT_ICON_COLOR);
  }
  if (doc["labelColor"].is<const char*>()) {
    cfg.labelColor =
        parseColor(doc["labelColor"].as<const char*>(), DEFAULT_LABEL_COLOR);
  }
  if (doc["valueColor"].is<const char*>()) {
    cfg.valueColor = parseColor(doc["valueColor"].as<const char*>(),
                                DEFAULT_TEMPERATURE_COLOR);
  }

  return cfg;
}

bool isUrlUnreserved(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

String urlEncode(const String& value) {
  static const char* const hex = "0123456789ABCDEF";
  String encoded;
  encoded.reserve(value.length() * 3);

  for (size_t i = 0; i < static_cast<size_t>(value.length()); i++) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if (isUrlUnreserved(static_cast<char>(c))) {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%';
      encoded += hex[c >> 4];
      encoded += hex[c & 0x0F];
    }
  }

  return encoded;
}

WeatherCache* findOrCreateCache(const String& city, const String& apiKey,
                                const String& hour) {
  for (uint8_t i = 0; i < weatherCacheCount; i++) {
    if (weatherCache[i].city == city && weatherCache[i].apiKey == apiKey &&
        weatherCache[i].hour == hour) {
      return &weatherCache[i];
    }
  }

  if (weatherCacheCount >= MAX_WEATHER_CACHE) return nullptr;

  WeatherCache& entry = weatherCache[weatherCacheCount++];
  entry.city = city;
  entry.apiKey = apiKey;
  entry.hour = hour;
  entry.dayCount = 0;
  entry.errorCode = "";
  entry.lastFetchMs = 0;
  entry.initialised = false;
  entry.hasError = false;
  return &entry;
}

WeatherIcon iconForCondition(int conditionId) {
  if (conditionId >= 200 && conditionId < 300) return WeatherIcon::Thunderstorm;
  if (conditionId >= 300 && conditionId < 600) return WeatherIcon::Rain;
  if (conditionId >= 600 && conditionId < 700) return WeatherIcon::Snow;
  if (conditionId >= 700 && conditionId < 800) return WeatherIcon::Atmosphere;
  if (conditionId == 800) return WeatherIcon::Clear;
  if (conditionId > 800 && conditionId < 900) return WeatherIcon::Clouds;
  return WeatherIcon::Unknown;
}

uint8_t iconPriority(WeatherIcon icon) {
  switch (icon) {
  case WeatherIcon::Thunderstorm:
    return 6;
  case WeatherIcon::Rain:
    return 5;
  case WeatherIcon::Snow:
    return 4;
  case WeatherIcon::Atmosphere:
    return 3;
  case WeatherIcon::Clouds:
    return 2;
  case WeatherIcon::Clear:
    return 1;
  case WeatherIcon::Unknown:
    return 0;
  }

  return 0;
}

int8_t findDayIndex(WeatherCache& cache, const String& dateKey) {
  for (uint8_t i = 0; i < cache.dayCount; i++) {
    if (cache.days[i].dateKey == dateKey) {
      return static_cast<int8_t>(i);
    }
  }

  if (cache.dayCount >= MAX_WEATHER_DAYS) return -1;

  WeatherDay& day = cache.days[cache.dayCount];
  day = WeatherDay();
  day.dateKey = dateKey;
  return static_cast<int8_t>(cache.dayCount++);
}

bool parseForecastPayload(const String& payload, WeatherCache& cache,
                          const String& hourFilter, String& outError) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    outError = "json";
    return false;
  }

  JsonArrayConst list = doc["list"].as<JsonArrayConst>();
  if (list.isNull()) {
    outError = "json";
    return false;
  }

  cache.dayCount = 0;
  for (uint8_t i = 0; i < MAX_WEATHER_DAYS; i++) {
    cache.days[i] = WeatherDay();
  }

  // H+N mode: find the single API slot closest to now + N hours
  uint8_t relativeHour = 0;
  if (parseRelativeHour(hourFilter.c_str(), relativeHour)) {
    const int32_t targetTs =
        static_cast<int32_t>(time(nullptr)) +
        static_cast<int32_t>(relativeHour) * static_cast<int32_t>(3600);

    int32_t bestDiff = 0;
    bool bestFound = false;
    float bestMinTemp = 0, bestMaxTemp = 0, bestFeels = 0;
    int bestConditionId = 0;
    float bestWindSpeed = 0;
    uint16_t bestWindDeg = 0;
    float bestRain = 0, bestPop = 0;
    uint8_t bestClouds = 0, bestHumidity = 0;
    uint16_t bestPressure = 0;
    String bestDateKey;

    for (JsonVariantConst item : list) {
      const int32_t dt = item["dt"] | static_cast<int32_t>(0);
      if (dt == 0) continue;
      const int32_t diff = dt > targetTs ? dt - targetTs : targetTs - dt;
      if (!bestFound || diff < bestDiff) {
        bestDiff = diff;
        bestFound = true;
        const float fallback = item["main"]["temp"] | 0.0f;
        bestMinTemp = item["main"]["temp_min"] | fallback;
        bestMaxTemp = item["main"]["temp_max"] | fallback;
        bestFeels = item["main"]["feels_like"] | fallback;
        bestConditionId = item["weather"][0]["id"] | 0;
        bestWindSpeed = item["wind"]["speed"] | 0.0f;
        bestWindDeg =
            static_cast<uint16_t>(static_cast<int>(item["wind"]["deg"] | 0));
        bestRain = item["rain"]["3h"] | 0.0f;
        bestPop = item["pop"] | 0.0f;
        bestClouds =
            static_cast<uint8_t>(static_cast<int>(item["clouds"]["all"] | 0));
        bestHumidity = static_cast<uint8_t>(
            static_cast<int>(item["main"]["humidity"] | 0));
        bestPressure = static_cast<uint16_t>(
            static_cast<int>(item["main"]["pressure"] | 0));
        const char* dtText = item["dt_txt"] | "";
        bestDateKey = "";
        for (uint8_t i = 0; i < 10 && dtText[i]; i++) bestDateKey += dtText[i];
      }
    }

    if (!bestFound) {
      outError = "json";
      return false;
    }

    WeatherDay& day = cache.days[0];
    day = WeatherDay();
    day.dateKey = bestDateKey;
    day.minTemp = static_cast<int16_t>(lroundf(bestMinTemp));
    day.maxTemp = static_cast<int16_t>(lroundf(bestMaxTemp));
    day.minFeelsLike = static_cast<int16_t>(lroundf(bestFeels));
    day.maxFeelsLike = static_cast<int16_t>(lroundf(bestFeels));
    day.icon = iconForCondition(bestConditionId);
    day.iconPriority = iconPriority(day.icon);
    day.available = true;
    day.maxWindSpeed = bestWindSpeed;
    day.windDeg = bestWindDeg;
    day.totalRain = bestRain;
    day.maxPop = static_cast<uint8_t>(lroundf(bestPop * 100.0f));
    day.humidity = bestHumidity;
    day.pressure = bestPressure;
    day.avgClouds = bestClouds;
    day.slotCount = 1;
    cache.dayCount = 1;
    return true;
  }

  for (JsonVariantConst item : list) {
    const char* dtText = item["dt_txt"] | "";
    if (!dtText || strlen(dtText) < 10) continue;

    String dateKey;
    dateKey.reserve(10);
    for (uint8_t i = 0; i < 10; i++) {
      dateKey += dtText[i];
    }

    if (hourFilter.length() == 2) {
      if (strlen(dtText) < 13 || dtText[11] != hourFilter[0] ||
          dtText[12] != hourFilter[1])
        continue;
    }

    const int8_t dayIndex = findDayIndex(cache, dateKey);
    if (dayIndex < 0) continue;

    WeatherDay& day = cache.days[dayIndex];
    const float fallbackTemp = item["main"]["temp"] | 0.0f;
    const float minTemp = item["main"]["temp_min"] | fallbackTemp;
    const float maxTemp = item["main"]["temp_max"] | fallbackTemp;
    const float feelsLike = item["main"]["feels_like"] | fallbackTemp;
    const int16_t roundedMin = static_cast<int16_t>(lroundf(minTemp));
    const int16_t roundedMax = static_cast<int16_t>(lroundf(maxTemp));
    const int16_t roundedFeels = static_cast<int16_t>(lroundf(feelsLike));
    const int conditionId = item["weather"][0]["id"] | 0;
    const WeatherIcon entryIcon = iconForCondition(conditionId);
    const uint8_t priority = iconPriority(entryIcon);
    const float windSpeed = item["wind"]["speed"] | 0.0f;
    const uint16_t windDeg =
        static_cast<uint16_t>(static_cast<int>(item["wind"]["deg"] | 0));
    const float rain3h = item["rain"]["3h"] | 0.0f;
    const float pop = item["pop"] | 0.0f;
    const uint8_t clouds =
        static_cast<uint8_t>(static_cast<int>(item["clouds"]["all"] | 0));
    const uint8_t humidityVal =
        static_cast<uint8_t>(static_cast<int>(item["main"]["humidity"] | 0));
    const uint16_t pressureVal =
        static_cast<uint16_t>(static_cast<int>(item["main"]["pressure"] | 0));

    if (!day.available) {
      day.minTemp = roundedMin;
      day.maxTemp = roundedMax;
      day.minFeelsLike = roundedFeels;
      day.maxFeelsLike = roundedFeels;
      day.icon = entryIcon;
      day.iconPriority = priority;
      day.available = true;
    } else {
      if (roundedMin < day.minTemp) day.minTemp = roundedMin;
      if (roundedMax > day.maxTemp) day.maxTemp = roundedMax;
      if (roundedFeels < day.minFeelsLike) day.minFeelsLike = roundedFeels;
      if (roundedFeels > day.maxFeelsLike) day.maxFeelsLike = roundedFeels;
      if (priority > day.iconPriority) {
        day.icon = entryIcon;
        day.iconPriority = priority;
      }
    }

    if (windSpeed > day.maxWindSpeed) {
      day.maxWindSpeed = windSpeed;
      day.windDeg = windDeg;
    }
    day.totalRain += rain3h;
    const uint8_t popPct = static_cast<uint8_t>(lroundf(pop * 100.0f));
    if (popPct > day.maxPop) day.maxPop = popPct;
    day.humiditySum += humidityVal;
    day.pressureSum += pressureVal;
    day.cloudsSum += clouds;
    day.slotCount++;
  }

  for (uint8_t i = 0; i < cache.dayCount; i++) {
    WeatherDay& day = cache.days[i];
    if (day.slotCount > 0) {
      day.humidity = static_cast<uint8_t>(day.humiditySum / day.slotCount);
      day.pressure = static_cast<uint16_t>(day.pressureSum / day.slotCount);
      day.avgClouds = static_cast<uint8_t>(day.cloudsSum / day.slotCount);
    }
  }

  if (cache.dayCount == 0) {
    outError = "json";
    return false;
  }

  return true;
}

bool fetchWeatherForecast(const WeatherConfig& cfg, WeatherCache& cache,
                          String& outError) {
  const String url =
      String("http://api.openweathermap.org/data/2.5/forecast?q=") +
      urlEncode(cfg.city) + "&appid=" + urlEncode(cfg.apiKey) + "&units=metric";

  static WiFiClient client;
  HTTPClient http;
  http.setTimeout(8000);

  if (!http.begin(client, url)) {
    outError = "conn";
    return false;
  }

  const int code = http.GET();
  if (code <= 0) {
    outError = "net";
    http.end();
    return false;
  }
  if (code == 401) {
    outError = "401";
    http.end();
    return false;
  }
  if (code == 404) {
    outError = "404";
    http.end();
    return false;
  }
  if (code != 200) {
    outError = String(code);
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  return parseForecastPayload(payload, cache, cfg.hour, outError);
}

int16_t gridCoordinate(int16_t size, uint8_t position, uint8_t divisions) {
  if (divisions == 0) return 0;
  return static_cast<int16_t>((static_cast<int32_t>(size) * position) /
                              divisions);
}

String dayLabel(uint8_t dayOffset) {
  if (dayOffset == 0) return "Today";
  return String("D+") + String(dayOffset);
}

String weatherDayDisplayKey(const WeatherDay& day) {
  return String(static_cast<int>(day.minTemp)) + ":" +
         String(static_cast<int>(day.maxTemp)) + ":" +
         String(static_cast<int>(day.humidity)) + ":" +
         String(day.maxWindSpeed, 0) + ":" + String(day.windDeg) + ":" +
         String(day.totalRain, 1) + ":" + String(static_cast<int>(day.maxPop));
}

const char* windDirectionStr(uint16_t deg) {
  if (deg < 23 || deg >= 338) return "N";
  if (deg < 68) return "NE";
  if (deg < 113) return "E";
  if (deg < 158) return "SE";
  if (deg < 203) return "S";
  if (deg < 248) return "SW";
  if (deg < 293) return "W";
  return "NW";
}

String formatMetricValue(const String& metric, const WeatherDay& day) {
  if (metric == "temp_min") return String(static_cast<int>(day.minTemp)) + "`C";
  if (metric == "temp_max") return String(static_cast<int>(day.maxTemp)) + "`C";
  if (metric == "feels_like_min")
    return String(static_cast<int>(day.minFeelsLike)) + "`C";
  if (metric == "feels_like_max")
    return String(static_cast<int>(day.maxFeelsLike)) + "`C";
  if (metric == "humidity") return String(static_cast<int>(day.humidity)) + "%";
  if (metric == "pressure")
    return String(static_cast<int>(day.pressure)) + "hPa";
  if (metric == "wind_speed") return String(day.maxWindSpeed, 1) + "m/s";
  if (metric == "wind_dir") return String(windDirectionStr(day.windDeg));
  if (metric == "rain") return String(day.totalRain, 1) + "mm";
  if (metric == "pop") return String(static_cast<int>(day.maxPop)) + "%";
  if (metric == "clouds") return String(static_cast<int>(day.avgClouds)) + "%";
  return "";
}

void drawSun(Renderer& renderer, int16_t cx, int16_t cy, int16_t size,
             uint16_t color) {
  const int16_t radius = static_cast<int16_t>(size / 5);
  const int16_t inner = static_cast<int16_t>(radius + 3);
  const int16_t outer = static_cast<int16_t>(size / 2);

  renderer.fillCircle(cx, cy, radius, color);
  renderer.drawLine(cx, cy - inner, cx, cy - outer, color);
  renderer.drawLine(cx, cy + inner, cx, cy + outer, color);
  renderer.drawLine(cx - inner, cy, cx - outer, cy, color);
  renderer.drawLine(cx + inner, cy, cx + outer, cy, color);
  renderer.drawLine(cx - inner, cy - inner, cx - outer, cy - outer, color);
  renderer.drawLine(cx + inner, cy - inner, cx + outer, cy - outer, color);
  renderer.drawLine(cx - inner, cy + inner, cx - outer, cy + outer, color);
  renderer.drawLine(cx + inner, cy + inner, cx + outer, cy + outer, color);
}

void drawCloud(Renderer& renderer, int16_t cx, int16_t cy, int16_t size,
               uint16_t color) {
  const int16_t small = static_cast<int16_t>(size / 6);
  const int16_t medium = static_cast<int16_t>(size / 5);
  const int16_t baseY = static_cast<int16_t>(cy + size / 8);

  renderer.fillCircle(static_cast<int16_t>(cx - size / 5), baseY, small, color);
  renderer.fillCircle(cx, static_cast<int16_t>(cy - size / 12), medium, color);
  renderer.fillCircle(static_cast<int16_t>(cx + size / 5), baseY, small, color);
  renderer.drawLine(static_cast<int16_t>(cx - size / 3),
                    static_cast<int16_t>(baseY + small),
                    static_cast<int16_t>(cx + size / 3),
                    static_cast<int16_t>(baseY + small), color);
}

void drawRain(Renderer& renderer, int16_t cx, int16_t cy, int16_t size,
              uint16_t color) {
  drawCloud(renderer, cx, static_cast<int16_t>(cy - size / 10), size, color);
  const int16_t top = static_cast<int16_t>(cy + size / 5);
  const int16_t bottom = static_cast<int16_t>(cy + size / 2);
  renderer.drawLine(static_cast<int16_t>(cx - size / 5), top,
                    static_cast<int16_t>(cx - size / 4), bottom, color);
  renderer.drawLine(cx, top, static_cast<int16_t>(cx - size / 12), bottom,
                    color);
  renderer.drawLine(static_cast<int16_t>(cx + size / 5), top,
                    static_cast<int16_t>(cx + size / 8), bottom, color);
}

void drawThunderstorm(Renderer& renderer, int16_t cx, int16_t cy, int16_t size,
                      uint16_t color) {
  drawCloud(renderer, cx, static_cast<int16_t>(cy - size / 10), size, color);
  const int16_t y0 = static_cast<int16_t>(cy + size / 7);
  const int16_t x0 = static_cast<int16_t>(cx + size / 10);
  renderer.drawLine(x0, y0, static_cast<int16_t>(x0 - size / 6),
                    static_cast<int16_t>(y0 + size / 4), color);
  renderer.drawLine(static_cast<int16_t>(x0 - size / 6),
                    static_cast<int16_t>(y0 + size / 4),
                    static_cast<int16_t>(x0 + size / 8),
                    static_cast<int16_t>(y0 + size / 4), color);
  renderer.drawLine(static_cast<int16_t>(x0 + size / 8),
                    static_cast<int16_t>(y0 + size / 4),
                    static_cast<int16_t>(x0 - size / 8),
                    static_cast<int16_t>(y0 + size / 2), color);
}

void drawSnow(Renderer& renderer, int16_t cx, int16_t cy, int16_t size,
              uint16_t color) {
  const int16_t radius = static_cast<int16_t>(size / 3);
  renderer.drawLine(cx, static_cast<int16_t>(cy - radius), cx,
                    static_cast<int16_t>(cy + radius), color);
  renderer.drawLine(static_cast<int16_t>(cx - radius), cy,
                    static_cast<int16_t>(cx + radius), cy, color);
  renderer.drawLine(static_cast<int16_t>(cx - radius / 2),
                    static_cast<int16_t>(cy - radius / 2),
                    static_cast<int16_t>(cx + radius / 2),
                    static_cast<int16_t>(cy + radius / 2), color);
  renderer.drawLine(static_cast<int16_t>(cx + radius / 2),
                    static_cast<int16_t>(cy - radius / 2),
                    static_cast<int16_t>(cx - radius / 2),
                    static_cast<int16_t>(cy + radius / 2), color);
  renderer.fillCircle(cx, cy, 2, color);
}

void drawAtmosphere(Renderer& renderer, int16_t cx, int16_t cy, int16_t size,
                    uint16_t color) {
  const int16_t half = static_cast<int16_t>(size / 3);
  renderer.drawLine(
      static_cast<int16_t>(cx - half), static_cast<int16_t>(cy - 6),
      static_cast<int16_t>(cx + half), static_cast<int16_t>(cy - 6), color);
  renderer.drawLine(static_cast<int16_t>(cx - half / 2), cy,
                    static_cast<int16_t>(cx + half), cy, color);
  renderer.drawLine(
      static_cast<int16_t>(cx - half), static_cast<int16_t>(cy + 6),
      static_cast<int16_t>(cx + half / 2), static_cast<int16_t>(cy + 6), color);
}

void drawWeatherIcon(Renderer& renderer, WeatherIcon icon, int16_t cx,
                     int16_t cy, int16_t size, uint16_t color) {
  switch (icon) {
  case WeatherIcon::Clear:
    drawSun(renderer, cx, cy, size, color);
    return;
  case WeatherIcon::Clouds:
    drawCloud(renderer, cx, cy, size, color);
    return;
  case WeatherIcon::Rain:
    drawRain(renderer, cx, cy, size, color);
    return;
  case WeatherIcon::Thunderstorm:
    drawThunderstorm(renderer, cx, cy, size, color);
    return;
  case WeatherIcon::Snow:
    drawSnow(renderer, cx, cy, size, color);
    return;
  case WeatherIcon::Atmosphere:
    drawAtmosphere(renderer, cx, cy, size, color);
    return;
  case WeatherIcon::Unknown:
    drawCloud(renderer, cx, cy, size, color);
    return;
  }
}

bool renderCacheMatches(const WeatherRenderCache& cache,
                        const LayoutCellConfig& cell, const String& displayKey,
                        int16_t cellLeft, int16_t cellTop, int16_t cellWidth,
                        int16_t cellHeight) {
  return cache.rendered && cache.cellLeft == cellLeft &&
         cache.cellTop == cellTop && cache.cellWidth == cellWidth &&
         cache.cellHeight == cellHeight &&
         cache.configJson == cell.configJson && cache.displayKey == displayKey;
}

void rememberRender(WeatherRenderCache& cache, const LayoutCellConfig& cell,
                    const String& displayKey, int16_t cellLeft, int16_t cellTop,
                    int16_t cellWidth, int16_t cellHeight) {
  cache.rendered = true;
  cache.cellLeft = cellLeft;
  cache.cellTop = cellTop;
  cache.cellWidth = cellWidth;
  cache.cellHeight = cellHeight;
  cache.configJson = cell.configJson;
  cache.displayKey = displayKey;
}

void renderCenteredStatus(Renderer& renderer, const String& status,
                          int16_t cellLeft, int16_t cellTop, int16_t cellWidth,
                          int16_t cellHeight, uint16_t color) {
  renderer.fillRect(cellLeft, cellTop, cellWidth, cellHeight);
  const int16_t textY =
      static_cast<int16_t>(cellTop + (cellHeight - renderer.fontHeight(2)) / 2);
  renderer.textCenterWithin(static_cast<int16_t>(cellLeft + cellWidth / 2),
                            textY, cellWidth, status, 2, color);
}

void renderWeatherDay(Renderer& renderer, const WeatherConfig& cfg,
                      const WeatherDay& day, int16_t cellLeft, int16_t cellTop,
                      int16_t cellWidth, int16_t cellHeight) {
  renderer.fillRect(cellLeft, cellTop, cellWidth, cellHeight);

  uint8_t relativeHour = 0;
  const bool isRelHour = parseRelativeHour(cfg.hour.c_str(), relativeHour);
  const String label = cfg.title.length() > 0 ? cfg.title
                       : isRelHour ? (String("H+") + String(relativeHour))
                                   : dayLabel(cfg.dayOffset);
  const int16_t labelHeight = renderer.fontHeight(1);
  const String val1 = !cfg.showIcon && cfg.value1 != "none"
                          ? formatMetricValue(cfg.value1, day)
                          : String();
  const String val2 = !cfg.showIcon && cfg.value2 != "none"
                          ? formatMetricValue(cfg.value2, day)
                          : String();
  const String val3 =
      cfg.value3 != "none" ? formatMetricValue(cfg.value3, day) : String();
  const String val4 =
      cfg.value4 != "none" ? formatMetricValue(cfg.value4, day) : String();
  const int16_t valLineHeight = renderer.fontHeight(2);
  const int16_t valHeight =
      static_cast<int16_t>((val1.length() > 0 ? valLineHeight : 0) +
                           (val2.length() > 0 ? valLineHeight : 0) +
                           (val3.length() > 0 ? valLineHeight : 0) +
                           (val4.length() > 0 ? valLineHeight : 0));
  const bool showValues = valHeight > 0;
  int16_t iconSize = 0;

  if (cfg.showIcon) {
    const int16_t minCell = cellWidth < cellHeight ? cellWidth : cellHeight;
    iconSize = static_cast<int16_t>(minCell / 3);
    if (iconSize < 18) iconSize = 18;
    if (iconSize > 30) iconSize = 30;
  }

  const int16_t spacing = 2;
  const int16_t totalHeight = static_cast<int16_t>(
      labelHeight + (cfg.showIcon || showValues ? spacing : 0) + iconSize +
      (cfg.showIcon && showValues ? spacing : 0) + valHeight);
  int16_t y = static_cast<int16_t>(cellTop + (cellHeight - totalHeight) / 2);
  if (y < cellTop) y = cellTop;

  renderer.textCenterWithin(static_cast<int16_t>(cellLeft + cellWidth / 2), y,
                            cellWidth, label, 1, cfg.labelColor);
  y = static_cast<int16_t>(y + labelHeight + spacing);

  if (cfg.showIcon) {
    drawWeatherIcon(
        renderer, day.icon, static_cast<int16_t>(cellLeft + cellWidth / 2),
        static_cast<int16_t>(y + iconSize / 2), iconSize, cfg.iconColor);
    y = static_cast<int16_t>(y + iconSize + spacing);
  }

  const int16_t centerX = static_cast<int16_t>(cellLeft + cellWidth / 2);
  if (val1.length() > 0) {
    renderer.textCenterWithin(centerX, y, cellWidth, val1, 2, cfg.valueColor);
    y = static_cast<int16_t>(y + valLineHeight);
  }
  if (val2.length() > 0) {
    renderer.textCenterWithin(centerX, y, cellWidth, val2, 2, cfg.valueColor);
    y = static_cast<int16_t>(y + valLineHeight);
  }
  if (val3.length() > 0) {
    renderer.textCenterWithin(centerX, y, cellWidth, val3, 2, cfg.valueColor);
    y = static_cast<int16_t>(y + valLineHeight);
  }
  if (val4.length() > 0) {
    renderer.textCenterWithin(centerX, y, cellWidth, val4, 2, cfg.valueColor);
  }
}

} // namespace

void renderWeatherPlugin(PluginRenderContext& context) {
  Renderer& renderer = context.renderer;
  const LayoutCellConfig& cell = context.cell;
  const AppConfig& appCfg = context.config;

  const int16_t cellLeft =
      gridCoordinate(renderer.width(), cell.col, appCfg.cols);
  const int16_t cellRight =
      gridCoordinate(renderer.width(), cell.col + cell.colSpan, appCfg.cols);
  const int16_t cellTop =
      gridCoordinate(renderer.height(), cell.row, appCfg.rows);
  const int16_t cellBottom =
      gridCoordinate(renderer.height(), cell.row + cell.rowSpan, appCfg.rows);
  const int16_t cellWidth = cellRight - cellLeft;
  const int16_t cellHeight = cellBottom - cellTop;

  const WeatherConfig cfg = readConfig(cell.configJson);
  String displayKey;
  const WeatherDay* selectedDay = nullptr;

  if (cfg.apiKey.length() == 0 || cfg.city.length() == 0) {
    displayKey = "SETUP";
  } else {
    WeatherCache* cache = findOrCreateCache(cfg.city, cfg.apiKey, cfg.hour);
    if (cache == nullptr) {
      displayKey = "ERR cache";
    } else {
      const uint32_t now = millis();
      const uint32_t retryInterval =
          cache->hasError ? ERROR_RETRY_INTERVAL_MS : FETCH_INTERVAL_MS;
      const bool stale =
          !cache->initialised || (now - cache->lastFetchMs >= retryInterval);

      if (stale) {
        String errorCode;
        if (fetchWeatherForecast(cfg, *cache, errorCode)) {
          cache->errorCode = "";
          cache->hasError = false;
        } else {
          cache->errorCode = errorCode.length() > 0 ? errorCode : "net";
          cache->hasError = true;
        }
        cache->lastFetchMs = now;
        cache->initialised = true;
      }

      if (cache->hasError) {
        displayKey = "ERR " + cache->errorCode;
      } else {
        uint8_t relativeHour = 0;
        const bool isRelHour =
            parseRelativeHour(cfg.hour.c_str(), relativeHour);
        const uint8_t effectiveDayIdx = isRelHour ? 0 : cfg.dayOffset;
        if (effectiveDayIdx >= cache->dayCount ||
            !cache->days[effectiveDayIdx].available) {
          displayKey = "---";
        } else {
          selectedDay = &cache->days[effectiveDayIdx];
          const String effectiveLabel =
              isRelHour ? (String("H+") + String(relativeHour))
                        : dayLabel(cfg.dayOffset);
          displayKey = String("OK:") + effectiveLabel + ":" +
                       weatherDayDisplayKey(*selectedDay) + ":" +
                       String(static_cast<int>(selectedDay->icon));
        }
      }
    }
  }

  WeatherRenderCache* renderCache = context.cellIndex < MAX_LAYOUT_CELLS
                                        ? &weatherRenderCache[context.cellIndex]
                                        : nullptr;
  if (!context.forceClear && renderCache != nullptr &&
      renderCacheMatches(*renderCache, cell, displayKey, cellLeft, cellTop,
                         cellWidth, cellHeight)) {
    return;
  }

  if (selectedDay != nullptr) {
    renderWeatherDay(renderer, cfg, *selectedDay, cellLeft, cellTop, cellWidth,
                     cellHeight);
  } else {
    renderCenteredStatus(renderer, displayKey, cellLeft, cellTop, cellWidth,
                         cellHeight, cfg.valueColor);
  }

  if (renderCache != nullptr) {
    rememberRender(*renderCache, cell, displayKey, cellLeft, cellTop, cellWidth,
                   cellHeight);
  }
}
