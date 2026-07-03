#include "ImagePlugin.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>

namespace {

constexpr size_t MAX_UPLOAD_DECODED_BYTES = 40960;
constexpr size_t MAX_URL_IMAGE_BYTES = 81920;
constexpr uint32_t FETCH_READ_TIMEOUT_MS = 10000;

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

struct ImageCellConfig {
  bool fromUrl;
  const char* imageData; // points into the JsonDocument
  String url;
  bool cover;
  uint16_t backgroundColor;
  uint32_t refreshMinutes;
};

uint16_t parseHexColor(const char* value, uint16_t fallback) {
  if (value == nullptr || value[0] != '#' || strlen(value) != 7) {
    return fallback;
  }

  char* end = nullptr;
  const uint32_t rgb = strtoul(value + 1, &end, 16);
  if (end == nullptr || *end != '\0') {
    return fallback;
  }

  return static_cast<uint16_t>(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) |
                               ((rgb >> 3) & 0x001F));
}

bool parseCellConfig(JsonDocument& doc, const String& configJson,
                     ImageCellConfig& out) {
  if (deserializeJson(doc, configJson) != DeserializationError::Ok) {
    return false;
  }

  const char* source = doc["source"] | "upload";
  out.fromUrl = strcmp(source, "url") == 0;
  out.imageData = doc["imageData"] | "";
  out.url = String(doc["url"] | "");
  const char* fit = doc["fit"] | "contain";
  out.cover = strcmp(fit, "cover") == 0;
  out.backgroundColor = parseHexColor(doc["backgroundColor"] | "#000000", 0x0000);
  const long refresh = doc["refreshMinutes"] | 0L;
  out.refreshMinutes =
      refresh > 0 ? min(static_cast<uint32_t>(refresh), static_cast<uint32_t>(1440))
                  : 0;
  return true;
}

// ---------------------------------------------------------------------------
// Cell geometry
// ---------------------------------------------------------------------------

int16_t gridCoordinate(int16_t size, uint8_t position, uint8_t divisions) {
  if (divisions == 0) return 0;
  return static_cast<int16_t>((static_cast<int32_t>(size) * position) /
                              divisions);
}

// ---------------------------------------------------------------------------
// Per-cell render cache
// ---------------------------------------------------------------------------

struct ImageRenderCache {
  bool drawn;          // an image is currently on screen for this cell
  bool errorShown;     // the error label is currently on screen
  uint32_t configHash; // djb2 of configJson
  uint32_t lastFetchMs;
  bool fetchedOnce;
  bool fromUrl;
  uint32_t refreshMinutes;
};

static ImageRenderCache renderCache[MAX_LAYOUT_CELLS];

uint32_t hashConfig(const String& configJson) {
  uint32_t hash = 5381;
  for (size_t i = 0; i < static_cast<size_t>(configJson.length()); i++) {
    hash = ((hash << 5) + hash) + static_cast<uint8_t>(configJson[i]);
  }
  return hash;
}

// ---------------------------------------------------------------------------
// TJpg output callback: clip to the cell rectangle
// ---------------------------------------------------------------------------

static Renderer* activeRenderer = nullptr;
static int16_t clipLeft = 0;
static int16_t clipTop = 0;
static int16_t clipRight = 0;
static int16_t clipBottom = 0;

bool tjpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h,
                uint16_t* bitmap) {
  if (activeRenderer == nullptr) return false;

  const int16_t blockLeft = x;
  const int16_t blockTop = y;
  const int16_t blockRight = static_cast<int16_t>(x + w);
  const int16_t blockBottom = static_cast<int16_t>(y + h);

  const int16_t left = max(blockLeft, clipLeft);
  const int16_t top = max(blockTop, clipTop);
  const int16_t right = min(blockRight, clipRight);
  const int16_t bottom = min(blockBottom, clipBottom);
  if (left >= right || top >= bottom) return true;

  for (int16_t row = top; row < bottom; row++) {
    const uint16_t* src =
        bitmap + static_cast<size_t>(row - blockTop) * w + (left - blockLeft);
    activeRenderer->pushImage(left, row, static_cast<int16_t>(right - left), 1,
                              src);
  }

  return true;
}

// ---------------------------------------------------------------------------
// JPEG drawing
// ---------------------------------------------------------------------------

uint8_t chooseScale(uint16_t imgW, uint16_t imgH, int16_t cellW, int16_t cellH,
                    bool cover) {
  if (cover) {
    // Largest downscale that still covers the cell.
    for (uint8_t scale = 8; scale > 1; scale /= 2) {
      if (imgW / scale >= static_cast<uint16_t>(cellW) &&
          imgH / scale >= static_cast<uint16_t>(cellH)) {
        return scale;
      }
    }
    return 1;
  }

  // Smallest downscale that fits inside the cell.
  for (uint8_t scale = 1; scale <= 8; scale *= 2) {
    if (imgW / scale <= static_cast<uint16_t>(cellW) &&
        imgH / scale <= static_cast<uint16_t>(cellH)) {
      return scale;
    }
  }
  return 8;
}

bool drawJpegInCell(Renderer& renderer, const uint8_t* jpeg, size_t jpegLen,
                    int16_t cellLeft, int16_t cellTop, int16_t cellWidth,
                    int16_t cellHeight, bool cover) {
  uint16_t imgW = 0;
  uint16_t imgH = 0;
  if (TJpgDec.getJpgSize(&imgW, &imgH, jpeg, jpegLen) != JDR_OK || imgW == 0 ||
      imgH == 0) {
    return false;
  }

  const uint8_t scale = chooseScale(imgW, imgH, cellWidth, cellHeight, cover);
  const int16_t scaledW = static_cast<int16_t>(imgW / scale);
  const int16_t scaledH = static_cast<int16_t>(imgH / scale);
  const int16_t drawX =
      static_cast<int16_t>(cellLeft + (cellWidth - scaledW) / 2);
  const int16_t drawY =
      static_cast<int16_t>(cellTop + (cellHeight - scaledH) / 2);

  activeRenderer = &renderer;
  clipLeft = cellLeft;
  clipTop = cellTop;
  clipRight = static_cast<int16_t>(cellLeft + cellWidth);
  clipBottom = static_cast<int16_t>(cellTop + cellHeight);

  TJpgDec.setCallback(tjpgOutput);
  TJpgDec.setJpgScale(scale);
  const JRESULT result = TJpgDec.drawJpg(drawX, drawY, jpeg, jpegLen);

  activeRenderer = nullptr;
  return result == JDR_OK;
}

// ---------------------------------------------------------------------------
// Sources
// ---------------------------------------------------------------------------

uint8_t* decodeBase64(const char* input, size_t& outLen) {
  const size_t inputLen = strlen(input);
  if (inputLen == 0) return nullptr;

  size_t needed = 0;
  mbedtls_base64_decode(nullptr, 0, &needed,
                        reinterpret_cast<const unsigned char*>(input),
                        inputLen);
  if (needed == 0 || needed > MAX_UPLOAD_DECODED_BYTES) return nullptr;

  uint8_t* buffer = static_cast<uint8_t*>(malloc(needed));
  if (buffer == nullptr) return nullptr;

  if (mbedtls_base64_decode(buffer, needed, &outLen,
                            reinterpret_cast<const unsigned char*>(input),
                            inputLen) != 0 ||
      outLen == 0) {
    free(buffer);
    return nullptr;
  }

  return buffer;
}

uint8_t* fetchJpeg(const String& url, size_t& outLen) {
  if (url.length() == 0) return nullptr;

  const bool useTls = url.startsWith("https");
  HTTPClient http;
  http.setTimeout(8000);

  bool began = false;
  if (useTls) {
    static WiFiClientSecure tlsClient;
    tlsClient.setInsecure();
    began = http.begin(tlsClient, url);
  } else {
    static WiFiClient plainClient;
    began = http.begin(plainClient, url);
  }
  if (!began) return nullptr;

  if (http.GET() != 200) {
    http.end();
    return nullptr;
  }

  const int size = http.getSize();
  if (size <= 0 || static_cast<size_t>(size) > MAX_URL_IMAGE_BYTES) {
    http.end();
    return nullptr;
  }

  uint8_t* buffer = static_cast<uint8_t*>(malloc(static_cast<size_t>(size)));
  if (buffer == nullptr) {
    http.end();
    return nullptr;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t offset = 0;
  const uint32_t start = millis();
  while (offset < static_cast<size_t>(size) &&
         millis() - start < FETCH_READ_TIMEOUT_MS) {
    const size_t available = stream->available();
    if (available > 0) {
      const size_t toRead =
          min(available, static_cast<size_t>(size) - offset);
      offset += stream->readBytes(buffer + offset, toRead);
    } else {
      delay(10);
    }
  }
  http.end();

  if (offset != static_cast<size_t>(size)) {
    free(buffer);
    return nullptr;
  }

  outLen = static_cast<size_t>(size);
  return buffer;
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

void drawError(Renderer& renderer, int16_t cellLeft, int16_t cellTop,
               int16_t cellWidth, int16_t cellHeight,
               uint16_t backgroundColor) {
  renderer.fillRect(cellLeft, cellTop, cellWidth, cellHeight, backgroundColor);
  const int16_t centerX = static_cast<int16_t>(cellLeft + cellWidth / 2);
  const int16_t textY =
      static_cast<int16_t>(cellTop + (cellHeight - renderer.fontHeight(2)) / 2);
  renderer.textCenterWithin(centerX, textY, cellWidth, "Image error", 2,
                            TFT_LIGHTGREY);
}

} // namespace

void renderImagePlugin(PluginRenderContext& context) {
  Renderer& renderer = context.renderer;
  const LayoutCellConfig& cell = context.cell;

  const int16_t cellLeft =
      gridCoordinate(renderer.width(), cell.col, context.config.cols);
  const int16_t cellRight = gridCoordinate(
      renderer.width(), cell.col + cell.colSpan, context.config.cols);
  const int16_t cellTop =
      gridCoordinate(renderer.height(), cell.row, context.config.rows);
  const int16_t cellBottom = gridCoordinate(
      renderer.height(), cell.row + cell.rowSpan, context.config.rows);
  const int16_t cellWidth = static_cast<int16_t>(cellRight - cellLeft);
  const int16_t cellHeight = static_cast<int16_t>(cellBottom - cellTop);

  if (context.cellIndex >= MAX_LAYOUT_CELLS) return;
  ImageRenderCache& cache = renderCache[context.cellIndex];

  const uint32_t configHash = hashConfig(cell.configJson);
  const bool configChanged = cache.configHash != configHash;
  if (configChanged) {
    cache.configHash = configHash;
    cache.drawn = false;
    cache.errorShown = false;
    cache.fetchedOnce = false;
    cache.lastFetchMs = 0;
  }

  const uint32_t now = millis();

  if (!configChanged) {
    // Config is unchanged: decide whether a draw is needed from the cached
    // values alone, before paying for a JSON parse.
    const uint32_t cachedRefreshIntervalMs = cache.refreshMinutes * 60000UL;
    const bool cachedRefreshDue = cache.fromUrl && cache.fetchedOnce &&
                                  cachedRefreshIntervalMs > 0 &&
                                  now - cache.lastFetchMs >= cachedRefreshIntervalMs;
    const bool cachedNeedsDraw = context.forceClear ||
                                 (!cache.drawn && !cache.errorShown) ||
                                 cachedRefreshDue;
    if (!cachedNeedsDraw) return;
  }

  JsonDocument doc;
  ImageCellConfig config;
  if (!parseCellConfig(doc, cell.configJson, config)) {
    if (cache.drawn && !context.forceClear) return;
    if (context.forceClear || !cache.errorShown) {
      drawError(renderer, cellLeft, cellTop, cellWidth, cellHeight, 0x0000);
      cache.errorShown = true;
      cache.drawn = false;
    }
    return;
  }

  cache.fromUrl = config.fromUrl;
  cache.refreshMinutes = config.refreshMinutes;

  const uint32_t refreshIntervalMs = config.refreshMinutes * 60000UL;
  const bool refreshDue = config.fromUrl && cache.fetchedOnce &&
                          refreshIntervalMs > 0 &&
                          now - cache.lastFetchMs >= refreshIntervalMs;
  const bool needsDraw =
      context.forceClear || (!cache.drawn && !cache.errorShown) || refreshDue;
  if (!needsDraw) return;

  // Acquire the JPEG bytes.
  uint8_t* jpeg = nullptr;
  size_t jpegLen = 0;
  if (config.fromUrl) {
    jpeg = fetchJpeg(config.url, jpegLen);
    cache.fetchedOnce = true;
    cache.lastFetchMs = now;
  } else {
    jpeg = decodeBase64(config.imageData, jpegLen);
  }

  if (jpeg == nullptr) {
    // URL refresh failure keeps the previous frame; anything else shows the
    // error state.
    if (config.fromUrl && cache.drawn && !context.forceClear) return;
    drawError(renderer, cellLeft, cellTop, cellWidth, cellHeight,
              config.backgroundColor);
    cache.errorShown = true;
    cache.drawn = false;
    return;
  }

  renderer.fillRect(cellLeft, cellTop, cellWidth, cellHeight,
                    config.backgroundColor);
  const bool drawnOk = drawJpegInCell(renderer, jpeg, jpegLen, cellLeft,
                                      cellTop, cellWidth, cellHeight,
                                      config.cover);
  free(jpeg);

  if (!drawnOk) {
    drawError(renderer, cellLeft, cellTop, cellWidth, cellHeight,
              config.backgroundColor);
    cache.errorShown = true;
    cache.drawn = false;
    return;
  }

  cache.drawn = true;
  cache.errorShown = false;
}
