#include "GetDataPlugin.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <cstring>

namespace {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

constexpr uint8_t MAX_GET_DATA_CACHE = 4;
constexpr uint32_t DEFAULT_FETCH_INTERVAL_MS = 60UL * 1000UL;
constexpr uint32_t ERROR_RETRY_INTERVAL_MS = 30UL * 1000UL;
constexpr size_t MAX_PAYLOAD_BYTES = 4096;

constexpr uint16_t DEFAULT_TITLE_COLOR = 0x07FF; // #00ffff
constexpr uint16_t DEFAULT_LABEL_COLOR = 0x07FF; // #00ffff
constexpr uint16_t DEFAULT_VALUE_COLOR = 0xFFFF; // #ffffff

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

struct GetDataConfig {
  String url;
  String param1Key;
  String param1Value;
  String param2Key;
  String param2Value;
  String param3Key;
  String param3Value;
  uint32_t refreshIntervalMs = DEFAULT_FETCH_INTERVAL_MS;
  String title;
  String value1Key;
  String value1Label;
  String value2Key;
  String value2Label;
  String value3Key;
  String value3Label;
  String value4Key;
  String value4Label;
  String valueLayout = "horizontal";
  uint8_t titleFont = 2;
  uint8_t valueFont = 2;
  uint16_t titleColor = DEFAULT_TITLE_COLOR;
  uint16_t labelColor = DEFAULT_LABEL_COLOR;
  uint16_t valueColor = DEFAULT_VALUE_COLOR;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

uint8_t parseFont(const char* text, uint8_t fallback) {
  if (!text) return fallback;
  if (text[0] == '1' && text[1] == '\0') return 1;
  if (text[0] == '2' && text[1] == '\0') return 2;
  if (text[0] == '4' && text[1] == '\0') return 4;
  return fallback;
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

bool isUrlUnreserved(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
         c == '~';
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

// Strip non-printable and non-ASCII bytes; map UTF-8 degree sign → 'o'.
String toAscii(const String& text) {
  String result;
  result.reserve(text.length());
  const size_t len = static_cast<size_t>(text.length());
  for (size_t i = 0; i < len;) {
    const uint8_t c = static_cast<uint8_t>(text[i]);
    if (c < 0x80) {
      if (c >= 0x20 && c <= 0x7E) result += static_cast<char>(c);
      i++;
    } else if (c == 0xC2 && i + 1 < len &&
               static_cast<uint8_t>(text[i + 1]) == 0xB0) {
      result += 'o'; // U+00B0 DEGREE SIGN → 'o'
      i += 2;
    } else {
      if ((c & 0xE0) == 0xC0) i += 2;
      else if ((c & 0xF0) == 0xE0) i += 3;
      else if ((c & 0xF8) == 0xF0) i += 4;
      else i++;
    }
  }
  return result;
}

String ellipsizeToWidth(Renderer& renderer, const String& text, uint8_t font,
                        int16_t maxWidth) {
  if (maxWidth <= 0) return "";
  if (renderer.textWidth(text, font) <= maxWidth) return text;

  const String ellipsis = "...";
  if (renderer.textWidth(ellipsis, font) > maxWidth) {
    String dots = ellipsis;
    while (dots.length() > 0 && renderer.textWidth(dots, font) > maxWidth) {
      dots.remove(dots.length() - 1);
    }
    return dots;
  }

  String truncated = text;
  while (truncated.length() > 0) {
    truncated.remove(truncated.length() - 1);
    const String candidate = truncated + ellipsis;
    if (renderer.textWidth(candidate, font) <= maxWidth) return candidate;
  }

  return ellipsis;
}

// ---------------------------------------------------------------------------
// Config parsing
// ---------------------------------------------------------------------------

GetDataConfig readConfig(const String& configJson) {
  GetDataConfig cfg;
  JsonDocument doc;
  if (deserializeJson(doc, configJson)) return cfg;

  if (doc["url"].is<const char*>()) cfg.url = doc["url"].as<const char*>();
  if (doc["param1Key"].is<const char*>())
    cfg.param1Key = doc["param1Key"].as<const char*>();
  if (doc["param1Value"].is<const char*>())
    cfg.param1Value = doc["param1Value"].as<const char*>();
  if (doc["param2Key"].is<const char*>())
    cfg.param2Key = doc["param2Key"].as<const char*>();
  if (doc["param2Value"].is<const char*>())
    cfg.param2Value = doc["param2Value"].as<const char*>();
  if (doc["param3Key"].is<const char*>())
    cfg.param3Key = doc["param3Key"].as<const char*>();
  if (doc["param3Value"].is<const char*>())
    cfg.param3Value = doc["param3Value"].as<const char*>();

  if (doc["refreshInterval"].is<int>()) {
    const int ri = doc["refreshInterval"].as<int>();
    if (ri >= 10 && ri <= 3600)
      cfg.refreshIntervalMs = static_cast<uint32_t>(ri) * 1000UL;
  }

  if (doc["title"].is<const char*>()) cfg.title = doc["title"].as<const char*>();
  if (doc["value1Key"].is<const char*>())
    cfg.value1Key = doc["value1Key"].as<const char*>();
  if (doc["value1Label"].is<const char*>())
    cfg.value1Label = doc["value1Label"].as<const char*>();
  if (doc["value2Key"].is<const char*>())
    cfg.value2Key = doc["value2Key"].as<const char*>();
  if (doc["value2Label"].is<const char*>())
    cfg.value2Label = doc["value2Label"].as<const char*>();
  if (doc["value3Key"].is<const char*>())
    cfg.value3Key = doc["value3Key"].as<const char*>();
  if (doc["value3Label"].is<const char*>())
    cfg.value3Label = doc["value3Label"].as<const char*>();
  if (doc["value4Key"].is<const char*>())
    cfg.value4Key = doc["value4Key"].as<const char*>();
  if (doc["value4Label"].is<const char*>())
    cfg.value4Label = doc["value4Label"].as<const char*>();
  if (doc["valueLayout"].is<const char*>()) {
    const char* layout = doc["valueLayout"].as<const char*>();
    if (layout && strcmp(layout, "vertical") == 0) {
      cfg.valueLayout = "vertical";
    }
  }

  if (doc["titleFont"].is<const char*>())
    cfg.titleFont = parseFont(doc["titleFont"].as<const char*>(), 2);
  if (doc["valueFont"].is<const char*>())
    cfg.valueFont = parseFont(doc["valueFont"].as<const char*>(), 2);
  if (doc["titleColor"].is<const char*>())
    cfg.titleColor =
        parseColor(doc["titleColor"].as<const char*>(), DEFAULT_TITLE_COLOR);
  if (doc["labelColor"].is<const char*>())
    cfg.labelColor =
        parseColor(doc["labelColor"].as<const char*>(), DEFAULT_LABEL_COLOR);
  if (doc["valueColor"].is<const char*>())
    cfg.valueColor =
        parseColor(doc["valueColor"].as<const char*>(), DEFAULT_VALUE_COLOR);

  return cfg;
}

// Build the full request URL including encoded query parameters.
String buildUrl(const GetDataConfig& cfg) {
  String url = cfg.url;

  bool hasQuery = false;
  for (size_t i = 0; i < static_cast<size_t>(url.length()); i++) {
    if (url[i] == '?') {
      hasQuery = true;
      break;
    }
  }

  struct Param {
    const String& key;
    const String& value;
  };
  const Param params[3] = {
      {cfg.param1Key, cfg.param1Value},
      {cfg.param2Key, cfg.param2Value},
      {cfg.param3Key, cfg.param3Value},
  };

  for (uint8_t i = 0; i < 3; i++) {
    if (params[i].key.length() == 0) continue;
    url += hasQuery ? '&' : '?';
    url += urlEncode(params[i].key);
    url += '=';
    url += urlEncode(params[i].value);
    hasQuery = true;
  }

  return url;
}

// ---------------------------------------------------------------------------
// Cache
// ---------------------------------------------------------------------------

struct GetDataCache {
  String cacheKey; // built URL (url + encoded params)
  String payload;  // raw JSON response
  String errorCode;
  uint32_t lastFetchMs;
  bool initialised;
  bool hasError;
};

struct GetDataRenderCache {
  bool rendered;
  int16_t cellLeft;
  int16_t cellTop;
  int16_t cellWidth;
  int16_t cellHeight;
  String configJson;
  String displayKey;
};

static GetDataCache dataCache[MAX_GET_DATA_CACHE];
static GetDataRenderCache renderCacheArr[MAX_LAYOUT_CELLS];
static uint8_t dataCacheCount = 0;

GetDataCache* findOrCreateCache(const String& cacheKey) {
  for (uint8_t i = 0; i < dataCacheCount; i++) {
    if (dataCache[i].cacheKey == cacheKey) return &dataCache[i];
  }
  if (dataCacheCount >= MAX_GET_DATA_CACHE) return nullptr;

  GetDataCache& entry = dataCache[dataCacheCount++];
  entry.cacheKey = cacheKey;
  entry.payload = "";
  entry.errorCode = "";
  entry.lastFetchMs = 0;
  entry.initialised = false;
  entry.hasError = false;
  return &entry;
}

// ---------------------------------------------------------------------------
// HTTP fetch
// ---------------------------------------------------------------------------

bool fetchData(const String& url, String& outPayload, String& outError) {
  const bool useTls = url.startsWith("https");

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

  const int code = http.GET();
  if (code <= 0) {
    outError = "net";
    http.end();
    return false;
  }
  if (code == 401) { outError = "401"; http.end(); return false; }
  if (code == 403) { outError = "403"; http.end(); return false; }
  if (code == 404) { outError = "404"; http.end(); return false; }
  if (code != 200) {
    outError = String(code);
    http.end();
    return false;
  }

  const size_t contentLen =
      static_cast<size_t>(http.getSize() < 0 ? 0 : http.getSize());
  if (contentLen > MAX_PAYLOAD_BYTES) {
    outError = "big";
    http.end();
    return false;
  }

  outPayload = http.getString();
  http.end();

  if (static_cast<size_t>(outPayload.length()) > MAX_PAYLOAD_BYTES) {
    outError = "big";
    return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// JSON dot-notation path resolution
// e.g. "data.key" on {"data":{"key":"value"}} → "value"
// ---------------------------------------------------------------------------

String resolveJsonPath(JsonVariantConst root, const String& path) {
  if (path.length() == 0) return "";

  JsonVariantConst current = root;
  size_t start = 0;
  const size_t pathLen = static_cast<size_t>(path.length());

  for (size_t i = 0; i <= pathLen; i++) {
    if (i == pathLen || path[i] == '.') {
      const size_t segLen = i - start;
      if (segLen == 0) {
        start = i + 1;
        continue;
      }

      // Stack-allocate the segment (max 63 chars + null terminator)
      char segment[64];
      if (segLen >= sizeof(segment)) return "";
      for (size_t j = 0; j < segLen; j++) segment[j] = path[start + j];
      segment[segLen] = '\0';

      if (!current.is<JsonObjectConst>()) return "";
      current = current[segment];
      if (current.isNull()) return "";

      start = i + 1;
    }
  }

  if (current.is<const char*>()) return String(current.as<const char*>());
  if (current.is<long>()) return String(current.as<long>());
  if (current.is<int>()) return String(current.as<int>());
  if (current.is<double>()) return String(current.as<double>(), 4);
  if (current.is<float>()) return String(current.as<float>(), 4);
  if (current.is<bool>()) return current.as<bool>() ? "true" : "false";
  return "";
}

// ---------------------------------------------------------------------------
// Render helpers
// ---------------------------------------------------------------------------

int16_t gridCoordinate(int16_t size, uint8_t position, uint8_t divisions) {
  if (divisions == 0) return 0;
  return static_cast<int16_t>((static_cast<int32_t>(size) * position) /
                              divisions);
}

bool renderCacheMatches(const GetDataRenderCache& cache,
                        const LayoutCellConfig& cell,
                        const String& displayKey, int16_t cellLeft,
                        int16_t cellTop, int16_t cellWidth,
                        int16_t cellHeight) {
  return cache.rendered && cache.cellLeft == cellLeft &&
         cache.cellTop == cellTop && cache.cellWidth == cellWidth &&
         cache.cellHeight == cellHeight &&
         cache.configJson == cell.configJson &&
         cache.displayKey == displayKey;
}

void rememberRender(GetDataRenderCache& cache, const LayoutCellConfig& cell,
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

} // namespace

// ---------------------------------------------------------------------------
// Render entry point
// ---------------------------------------------------------------------------

void renderGetDataPlugin(PluginRenderContext& context) {
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

  const GetDataConfig cfg = readConfig(cell.configJson);
  const bool verticalValues = cfg.valueLayout == "vertical";
  const uint8_t valueLimit = verticalValues ? 2 : 4;

  // -------------------------------------------------------------------------
  // Resolve values and build the display key
  // -------------------------------------------------------------------------
  String displayKey;
  String resolvedValues[4];

  if (cfg.url.length() == 0) {
    displayKey = "SETUP";
  } else {
    const String builtUrl = buildUrl(cfg);
    GetDataCache* cache = findOrCreateCache(builtUrl);

    if (cache == nullptr) {
      displayKey = "ERR cache";
    } else {
      const uint32_t now = millis();
      const uint32_t interval =
          cache->hasError ? ERROR_RETRY_INTERVAL_MS : cfg.refreshIntervalMs;
      const bool stale =
          !cache->initialised || (now - cache->lastFetchMs >= interval);

      if (stale) {
        String payload, errCode;
        if (fetchData(builtUrl, payload, errCode)) {
          cache->payload = payload;
          cache->errorCode = "";
          cache->hasError = false;
        } else {
          cache->errorCode = errCode.length() > 0 ? errCode : "net";
          cache->hasError = true;
        }
        cache->lastFetchMs = now;
        cache->initialised = true;
      }

      if (cache->hasError) {
        displayKey =
            cache->errorCode.length() > 0 ? "ERR " + cache->errorCode : "ERR";
      } else {
        // Parse payload and resolve the 4 configured JSON paths.
        JsonDocument doc;
        if (deserializeJson(doc, cache->payload)) {
          displayKey = "ERR json";
        } else {
          const String* const keys[4] = {
              &cfg.value1Key, &cfg.value2Key, &cfg.value3Key, &cfg.value4Key};
          for (uint8_t i = 0; i < valueLimit; i++) {
            if (keys[i]->length() > 0) {
              resolvedValues[i] =
                  resolveJsonPath(doc.as<JsonVariantConst>(), *keys[i]);
            }
          }
          displayKey = cfg.valueLayout + ":" + resolvedValues[0] + "|" +
                       resolvedValues[1] + "|" + resolvedValues[2] + "|" +
                       resolvedValues[3];
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  // Render cache check
  // -------------------------------------------------------------------------
  GetDataRenderCache* rc = context.cellIndex < MAX_LAYOUT_CELLS
                               ? &renderCacheArr[context.cellIndex]
                               : nullptr;
  if (!context.forceClear && rc != nullptr &&
      renderCacheMatches(*rc, cell, displayKey, cellLeft, cellTop, cellWidth,
                         cellHeight)) {
    return;
  }

  renderer.fillRect(cellLeft, cellTop, cellWidth, cellHeight);

  // -------------------------------------------------------------------------
  // Draw
  // -------------------------------------------------------------------------
  if (displayKey == "SETUP" || displayKey.startsWith("ERR")) {
    const int16_t textY = static_cast<int16_t>(
        cellTop + (cellHeight - renderer.fontHeight(2)) / 2);
    const String statusText =
      ellipsizeToWidth(renderer, displayKey, 2, cellWidth);
    renderer.textCenterWithin(static_cast<int16_t>(cellLeft + cellWidth / 2),
                  textY, cellWidth, statusText, 2,
                  cfg.valueColor);
  } else {
    const bool showTitle = cfg.title.length() > 0;
    const int16_t titleH =
        showTitle ? renderer.fontHeight(cfg.titleFont) : static_cast<int16_t>(0);
    const int16_t rowH = renderer.fontHeight(cfg.valueFont);
    const int16_t labelH = renderer.fontHeight(1);

    constexpr int16_t TITLE_TOP_PADDING = 6;
    int16_t y = static_cast<int16_t>(cellTop + TITLE_TOP_PADDING);

    const int16_t centerX = static_cast<int16_t>(cellLeft + cellWidth / 2);

    // Title
    if (showTitle) {
      if (y + titleH > cellBottom) {
        if (rc != nullptr) {
          rememberRender(*rc, cell, displayKey, cellLeft, cellTop, cellWidth,
                         cellHeight);
        }
        return;
      }
      const String titleText = ellipsizeToWidth(
          renderer, toAscii(cfg.title), cfg.titleFont, cellWidth);
      renderer.textCenterWithin(centerX, y, cellWidth, titleText, cfg.titleFont,
                                cfg.titleColor);
      y = static_cast<int16_t>(y + titleH);
    }

    // Values
    const String* const labels[4] = {
        &cfg.value1Label, &cfg.value2Label, &cfg.value3Label, &cfg.value4Label};

    for (uint8_t i = 0; i < valueLimit; i++) {
      if (resolvedValues[i].length() == 0) continue;

      const String valueText = toAscii(resolvedValues[i]);
      const bool hasLabel = labels[i]->length() > 0;

      if (verticalValues) {
        if (hasLabel) {
        if (y + labelH > cellBottom) break;
        const String labelText =
          ellipsizeToWidth(renderer, toAscii(*labels[i]), 1, cellWidth);
        renderer.textCenterWithin(centerX, y, cellWidth, labelText, 1,
                    cfg.labelColor);
          y = static_cast<int16_t>(y + labelH);
        }
      if (y + rowH > cellBottom) break;
      const String clippedValue =
        ellipsizeToWidth(renderer, valueText, cfg.valueFont, cellWidth);
      renderer.textCenterWithin(centerX, y, cellWidth, clippedValue,
                    cfg.valueFont, cfg.valueColor);
      } else if (hasLabel) {
      if (y + rowH > cellBottom) break;
        const String labelText = toAscii(*labels[i]) + ":";
        // Render label (font 1, labelColor) and value (font 2, valueColor)
        // side by side, centered as a pair within the cell.
        constexpr int16_t sep = 3; // px gap between label and value
      String clippedLabel = labelText;
      int16_t labelW = renderer.textWidth(clippedLabel, 1);
      const int16_t ellipsisW = renderer.textWidth("...", cfg.valueFont);
      if (labelW + sep + ellipsisW > cellWidth) {
        const int16_t labelMaxWidth =
          cellWidth > sep + ellipsisW
            ? static_cast<int16_t>(cellWidth - sep - ellipsisW)
            : cellWidth;
        clippedLabel =
          ellipsizeToWidth(renderer, clippedLabel, 1, labelMaxWidth);
        labelW = renderer.textWidth(clippedLabel, 1);
      }

      int16_t valueMaxWidth = static_cast<int16_t>(cellWidth - labelW - sep);
      if (valueMaxWidth < 0) valueMaxWidth = 0;
      const String clippedValue =
        ellipsizeToWidth(renderer, valueText, cfg.valueFont, valueMaxWidth);
      const int16_t valW = renderer.textWidth(clippedValue, cfg.valueFont);
      const int16_t sepW = valW > 0 && labelW > 0 ? sep : 0;
        const int16_t pairW =
        static_cast<int16_t>(labelW + sepW + valW);

        int16_t pairLeft =
            static_cast<int16_t>(cellLeft + (cellWidth - pairW) / 2);
        if (pairLeft < cellLeft) pairLeft = cellLeft;

        // Vertically center the smaller font-1 label within the row height
        const int16_t labelY =
            static_cast<int16_t>(y + (rowH - labelH) / 2);

        renderer.textAt(pairLeft, labelY, clippedLabel, 1, cfg.labelColor);
        if (clippedValue.length() > 0) {
          renderer.textAt(static_cast<int16_t>(pairLeft + labelW + sepW), y,
                          clippedValue, cfg.valueFont, cfg.valueColor);
        }
      } else {
        if (y + rowH > cellBottom) break;
        const String clippedValue =
            ellipsizeToWidth(renderer, valueText, cfg.valueFont, cellWidth);
        renderer.textCenterWithin(centerX, y, cellWidth, clippedValue,
                                  cfg.valueFont, cfg.valueColor);
      }

      y = static_cast<int16_t>(y + rowH);
    }
  }

  if (rc != nullptr) {
    rememberRender(*rc, cell, displayKey, cellLeft, cellTop, cellWidth,
                   cellHeight);
  }
}
