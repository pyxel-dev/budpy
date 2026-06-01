#include "HaSensorPlugin.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

namespace {

// ---------------------------------------------------------------------------
// ASCII sanitization
// TFT_eSPI built-in fonts only render ASCII 0x20–0x7E.
// The degree sign (U+00B0, UTF-8: 0xC2 0xB0) is mapped to 'o'.
// All other non-ASCII bytes are skipped.
// ---------------------------------------------------------------------------

String toAscii(const String& text) {
  String result;
  result.reserve(text.length());
  const size_t len = static_cast<size_t>(text.length());

  for (size_t i = 0; i < len;) {
    const uint8_t c = static_cast<uint8_t>(text[i]);

    if (c < 0x80) {
      if (c >= 0x20 && c <= 0x7E) {
        result += static_cast<char>(c);
      }
      i++;
    } else if (c == 0xC2 && i + 1 < len &&
               static_cast<uint8_t>(text[i + 1]) == 0xB0) {
      result += 'o'; // U+00B0 DEGREE SIGN → 'o'
      i += 2;
    } else {
      // Skip any other multi-byte UTF-8 sequence
      if ((c & 0xE0) == 0xC0) {
        i += 2;
      } else if ((c & 0xF0) == 0xE0) {
        i += 3;
      } else if ((c & 0xF8) == 0xF0) {
        i += 4;
      } else {
        i++;
      }
    }
  }

  return result;
}

// ---------------------------------------------------------------------------
// Parsing helpers
// ---------------------------------------------------------------------------

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

  uint8_t ch[3] = {0, 0, 0};
  for (uint8_t i = 0; i < 3; i++) {
    const int hi = hexNibble(text[1 + i * 2]);
    const int lo = hexNibble(text[2 + i * 2]);
    if (hi < 0 || lo < 0) return fallback;
    ch[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return static_cast<uint16_t>(((ch[0] & 0xF8) << 8) | ((ch[1] & 0xFC) << 3) |
                               (ch[2] >> 3));
}

uint8_t parseFont(const char* text, uint8_t fallback) {
  if (!text) return fallback;
  if (text[0] == '1' && text[1] == '\0') return 1;
  if (text[0] == '2' && text[1] == '\0') return 2;
  if (text[0] == '4' && text[1] == '\0') return 4;
  return fallback;
}

TextHorizontalAlign parseHAlign(const String& s) {
  if (s == "left") return TextHorizontalAlign::Left;
  if (s == "right") return TextHorizontalAlign::Right;
  return TextHorizontalAlign::Center;
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

struct HaSensorConfig {
  String haUrl;
  String haToken;
  String entityId;
  String title;
  String unit;
  uint8_t titleFont;
  uint16_t titleColor;
  String titleHorizontalAlign;
  uint8_t valueFont;
  uint16_t valueColor;
  String valueHorizontalAlign;
  uint8_t unitFont;
  uint16_t unitColor;
  String unitPosition; // "before" | "after"
  String verticalAlign;
  int16_t offsetX;
  int16_t offsetY;
};

HaSensorConfig readConfig(const String& configJson) {
  HaSensorConfig cfg;
  cfg.titleFont = 2;
  cfg.titleColor = 0x07FF; // TFT_CYAN ≈ #00ffff
  cfg.titleHorizontalAlign = "center";
  cfg.valueFont = 4;
  cfg.valueColor = 0xFFFF; // TFT_WHITE
  cfg.valueHorizontalAlign = "center";
  cfg.unitFont = 2;
  cfg.unitColor = 0xFFFF; // TFT_WHITE
  cfg.unitPosition = "after";
  cfg.verticalAlign = "center";
  cfg.offsetX = 0;
  cfg.offsetY = 0;

  JsonDocument doc;
  if (deserializeJson(doc, configJson)) return cfg;

  cfg.haUrl = doc["haUrl"] | "";
  cfg.haToken = doc["haToken"] | "";
  cfg.entityId = doc["entityId"] | "";
  cfg.title = doc["title"] | "";
  cfg.unit = doc["unit"] | "";

  while (cfg.haUrl.endsWith("/")) {
    cfg.haUrl.remove(cfg.haUrl.length() - 1);
  }

  if (doc["titleFont"].is<const char*>())
    cfg.titleFont = parseFont(doc["titleFont"].as<const char*>(), 2);
  if (doc["titleColor"].is<const char*>())
    cfg.titleColor = parseColor(doc["titleColor"].as<const char*>(), 0x07FF);
  if (doc["titleHorizontalAlign"].is<const char*>())
    cfg.titleHorizontalAlign = doc["titleHorizontalAlign"].as<const char*>();

  if (doc["valueFont"].is<const char*>())
    cfg.valueFont = parseFont(doc["valueFont"].as<const char*>(), 4);
  if (doc["valueColor"].is<const char*>())
    cfg.valueColor = parseColor(doc["valueColor"].as<const char*>(), 0xFFFF);
  if (doc["valueHorizontalAlign"].is<const char*>())
    cfg.valueHorizontalAlign = doc["valueHorizontalAlign"].as<const char*>();

  if (doc["unitFont"].is<const char*>())
    cfg.unitFont = parseFont(doc["unitFont"].as<const char*>(), 2);
  if (doc["unitColor"].is<const char*>())
    cfg.unitColor = parseColor(doc["unitColor"].as<const char*>(), 0xFFFF);
  if (doc["unitPosition"].is<const char*>())
    cfg.unitPosition = doc["unitPosition"].as<const char*>();

  if (doc["verticalAlign"].is<const char*>())
    cfg.verticalAlign = doc["verticalAlign"].as<const char*>();

  if (doc["offsetX"].is<int>())
    cfg.offsetX = static_cast<int16_t>(doc["offsetX"].as<int>());
  if (doc["offsetY"].is<int>())
    cfg.offsetY = static_cast<int16_t>(doc["offsetY"].as<int>());

  return cfg;
}

// ---------------------------------------------------------------------------
// Cache
// ---------------------------------------------------------------------------

constexpr uint8_t MAX_HA_CACHE = 12;
constexpr uint32_t FETCH_INTERVAL_MS = 60000UL;

struct HaCache {
  String entityId;
  String value;
  String errorCode;
  uint32_t lastFetchMs;
  bool initialised;
  bool hasError;
};

struct HaRenderCache {
  bool rendered;
  int16_t cellLeft;
  int16_t cellTop;
  int16_t cellWidth;
  int16_t cellHeight;
  String configJson;
  String displayValue;
};

static HaCache haCache[MAX_HA_CACHE];
static HaRenderCache haRenderCache[MAX_LAYOUT_CELLS];
static uint8_t haCacheCount = 0;

HaCache* findOrCreateCache(const String& entityId) {
  for (uint8_t i = 0; i < haCacheCount; i++) {
    if (haCache[i].entityId == entityId) return &haCache[i];
  }
  if (haCacheCount >= MAX_HA_CACHE) return nullptr;

  HaCache& entry = haCache[haCacheCount++];
  entry.entityId = entityId;
  entry.value = "";
  entry.errorCode = "";
  entry.lastFetchMs = 0;
  entry.initialised = false;
  entry.hasError = false;
  return &entry;
}

bool renderCacheMatches(const HaRenderCache& cache,
                        const LayoutCellConfig& cell,
                        const String& displayValue, int16_t cellLeft,
                        int16_t cellTop, int16_t cellWidth,
                        int16_t cellHeight) {
  return cache.rendered && cache.cellLeft == cellLeft &&
         cache.cellTop == cellTop && cache.cellWidth == cellWidth &&
         cache.cellHeight == cellHeight &&
         cache.configJson == cell.configJson &&
         cache.displayValue == displayValue;
}

void rememberRender(HaRenderCache& cache, const LayoutCellConfig& cell,
                    const String& displayValue, int16_t cellLeft,
                    int16_t cellTop, int16_t cellWidth, int16_t cellHeight) {
  cache.rendered = true;
  cache.cellLeft = cellLeft;
  cache.cellTop = cellTop;
  cache.cellWidth = cellWidth;
  cache.cellHeight = cellHeight;
  cache.configJson = cell.configJson;
  cache.displayValue = displayValue;
}

// ---------------------------------------------------------------------------
// HTTP fetch
// ---------------------------------------------------------------------------

bool fetchSensorState(const HaSensorConfig& cfg, String& outValue,
                      String& outError) {
  const String url = cfg.haUrl + "/api/states/" + cfg.entityId;
  const bool useTls = cfg.haUrl.startsWith("https");

  HTTPClient http;
  http.setTimeout(8000);

  if (useTls) {
    static WiFiClientSecure tlsClient;
    tlsClient.setInsecure();
    if (!http.begin(tlsClient, url)) {
      outError = "conn";
      return false;
    }
  } else {
    static WiFiClient plainClient;
    if (!http.begin(plainClient, url)) {
      outError = "conn";
      return false;
    }
  }

  http.addHeader("Authorization", "Bearer " + cfg.haToken);
  http.addHeader("Content-Type", "application/json");

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

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    outError = "json";
    return false;
  }

  const char* state = doc["state"] | "";
  outValue = String(state);
  return true;
}

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------

int16_t gridCoordinate(int16_t size, uint8_t position, uint8_t divisions) {
  if (divisions == 0) return 0;
  return static_cast<int16_t>((static_cast<int32_t>(size) * position) /
                              divisions);
}

} // namespace

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void renderHaSensorPlugin(PluginRenderContext& context) {
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

  const HaSensorConfig cfg = readConfig(cell.configJson);

  // -----------------------------------------------------------------------
  // Cache refresh
  // -----------------------------------------------------------------------
  HaCache* cache = findOrCreateCache(cfg.entityId);
  const uint32_t now = millis();

  if (cache != nullptr && cfg.entityId.length() > 0 && cfg.haUrl.length() > 0 &&
      cfg.haToken.length() > 0) {
    const bool stale =
        !cache->initialised || (now - cache->lastFetchMs >= FETCH_INTERVAL_MS);
    if (stale) {
      String fetched, errCode;
      if (fetchSensorState(cfg, fetched, errCode)) {
        cache->value = fetched;
        cache->errorCode = "";
        cache->hasError = false;
      } else {
        cache->errorCode = errCode;
        cache->hasError = true;
      }
      cache->lastFetchMs = now;
      cache->initialised = true;
    }
  }

  // -----------------------------------------------------------------------
  // Build display strings (ASCII-safe)
  // -----------------------------------------------------------------------
  String displayValue;
  if (cache == nullptr || !cache->initialised) {
    displayValue = "---";
  } else if (cache->hasError) {
    displayValue =
        cache->errorCode.length() > 0 ? "ERR " + cache->errorCode : "ERR";
  } else {
    displayValue = cache->value;
  }

  HaRenderCache* renderCache = context.cellIndex < MAX_LAYOUT_CELLS
                                   ? &haRenderCache[context.cellIndex]
                                   : nullptr;
  if (!context.forceClear && renderCache != nullptr &&
      renderCacheMatches(*renderCache, cell, displayValue, cellLeft, cellTop,
                         cellWidth, cellHeight)) {
    return;
  }

  const String valueText = toAscii(displayValue);
  const bool showTitle = cfg.title.length() > 0;
  const bool showUnit = cfg.unit.length() > 0;

  // -----------------------------------------------------------------------
  // Vertical positioning (unit is on the same line as value)
  // -----------------------------------------------------------------------
  const int16_t titleH = showTitle ? renderer.fontHeight(cfg.titleFont) : 0;
  const int16_t valueH = renderer.fontHeight(cfg.valueFont);
  const int16_t unitH = showUnit ? renderer.fontHeight(cfg.unitFont) : 0;
  const int16_t lineH = valueH > unitH ? valueH : unitH;
  const int16_t totalH = titleH + lineH;

  int16_t startY;
  if (cfg.verticalAlign == "top") {
    startY = cellTop;
  } else if (cfg.verticalAlign == "bottom") {
    startY = cellBottom - totalH;
  } else {
    startY = cellTop + (cellHeight - totalH) / 2;
  }
  startY += cfg.offsetY;

  // -----------------------------------------------------------------------
  // Draw
  // -----------------------------------------------------------------------
  renderer.fillRect(cellLeft, cellTop, cellWidth, cellHeight);

  if (showTitle) {
    renderer.textWithin(cellLeft, startY, cellWidth, toAscii(cfg.title),
                        cfg.titleFont, cfg.titleColor,
                        parseHAlign(cfg.titleHorizontalAlign), cfg.offsetX);
  }

  // Value + unit side by side
  const String unitStr = showUnit ? toAscii(cfg.unit) : String("");
  const int16_t valW = renderer.textWidth(valueText, cfg.valueFont);
  const int16_t unitW =
      showUnit ? renderer.textWidth(unitStr, cfg.unitFont) : 0;
  const int16_t pairW = valW + unitW;
  const int16_t valueY = startY + titleH;
  const int16_t unitY = valueY;

  int16_t pairLeft;
  const TextHorizontalAlign valueAlign = parseHAlign(cfg.valueHorizontalAlign);
  if (valueAlign == TextHorizontalAlign::Center) {
    pairLeft = cellLeft + (cellWidth - pairW) / 2 + cfg.offsetX;
  } else if (valueAlign == TextHorizontalAlign::Right) {
    pairLeft = cellRight - pairW + cfg.offsetX;
  } else {
    pairLeft = cellLeft + cfg.offsetX;
  }

  int16_t valX, unitX;
  if (showUnit && cfg.unitPosition == "before") {
    unitX = pairLeft;
    valX = pairLeft + unitW;
  } else {
    valX = pairLeft;
    unitX = pairLeft + valW;
  }

  renderer.textAt(valX, valueY, valueText, cfg.valueFont, cfg.valueColor);
  if (showUnit) {
    renderer.textAt(unitX, unitY, unitStr, cfg.unitFont, cfg.unitColor);
  }

  if (renderCache != nullptr) {
    rememberRender(*renderCache, cell, displayValue, cellLeft, cellTop,
                   cellWidth, cellHeight);
  }
}
