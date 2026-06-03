#include "HaCoverPlugin.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

namespace {

// ---------------------------------------------------------------------------
// Cover type & state enums
// ---------------------------------------------------------------------------

enum class CoverIconType : uint8_t {
  None = 0,
  Awning,
  Blind,
  Curtain,
  Damper,
  Door,
  Garage,
  Gate,
  Shade,
  Shutter,
  Window,
};

enum class CoverState : uint8_t {
  Unknown = 0,
  Open,
  Closed,
  Opening,
  Closing,
  Unavailable,
};

// ---------------------------------------------------------------------------
// ASCII sanitisation (mirrors ha-sensor)
// ---------------------------------------------------------------------------

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
      result += 'o';
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

String trimAndUnquote(String value) {
  value.trim();
  const size_t len = static_cast<size_t>(value.length());
  if (len >= 2) {
    const char first = value[0];
    const char last = value[value.length() - 1];
    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
      value.remove(value.length() - 1);
      value.remove(0, 1);
      value.trim();
    }
  }
  return value;
}

void normalizeHaBaseUrl(String& url) {
  url = trimAndUnquote(url);
  while (url.endsWith("/")) {
    url.remove(url.length() - 1);
  }
  // Users often paste URLs ending with /api. Keep only the host base.
  if (url.endsWith("/api")) {
    url.remove(url.length() - 4);
  }
  while (url.endsWith("/")) {
    url.remove(url.length() - 1);
  }
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

CoverIconType parseCoverType(const String& s) {
  if (s == "awning")  return CoverIconType::Awning;
  if (s == "blind")   return CoverIconType::Blind;
  if (s == "curtain") return CoverIconType::Curtain;
  if (s == "damper")  return CoverIconType::Damper;
  if (s == "door")    return CoverIconType::Door;
  if (s == "garage")  return CoverIconType::Garage;
  if (s == "gate")    return CoverIconType::Gate;
  if (s == "shade")   return CoverIconType::Shade;
  if (s == "shutter") return CoverIconType::Shutter;
  if (s == "window")  return CoverIconType::Window;
  return CoverIconType::None;
}

CoverState parseCoverState(const String& s) {
  if (s == "open")        return CoverState::Open;
  if (s == "closed")      return CoverState::Closed;
  if (s == "opening")     return CoverState::Opening;
  if (s == "closing")     return CoverState::Closing;
  if (s == "unavailable") return CoverState::Unavailable;
  return CoverState::Unknown;
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

struct HaCoverConfig {
  String haUrl;
  String haToken;
  String entityId;
  String title;
  CoverIconType coverType;
  uint8_t iconSize;        // pixels
  uint16_t iconOpenColor;
  uint16_t iconClosedColor;
  bool showStateText;
  uint8_t titleFont;
  uint16_t titleColor;
  String titleHorizontalAlign;
  uint8_t stateFont;
  uint16_t stateColor;
  String verticalAlign;
  int16_t offsetX;
  int16_t offsetY;
};

HaCoverConfig readConfig(const String& configJson) {
  HaCoverConfig cfg;
  cfg.coverType        = CoverIconType::None;
  cfg.iconSize         = 36;
  cfg.iconOpenColor    = parseColor("#00cc44", 0x0648);
  cfg.iconClosedColor  = parseColor("#ff4400", 0xFA00);
  cfg.showStateText    = true;
  cfg.titleFont        = 2;
  cfg.titleColor       = 0x07FF; // cyan
  cfg.titleHorizontalAlign = "center";
  cfg.stateFont        = 2;
  cfg.stateColor       = 0xFFFF; // white
  cfg.verticalAlign    = "center";
  cfg.offsetX          = 0;
  cfg.offsetY          = 0;

  JsonDocument doc;
  if (deserializeJson(doc, configJson)) return cfg;

  cfg.haUrl    = doc["haUrl"]    | "";
  cfg.haToken  = doc["haToken"]  | "";
  cfg.entityId = doc["entityId"] | "";
  cfg.title    = doc["title"]    | "";

  normalizeHaBaseUrl(cfg.haUrl);
  cfg.haToken = trimAndUnquote(cfg.haToken);
  cfg.entityId = trimAndUnquote(cfg.entityId);

  if (doc["coverType"].is<const char*>())
    cfg.coverType = parseCoverType(doc["coverType"].as<String>());

  if (doc["iconSize"].is<const char*>()) {
    const String sz = doc["iconSize"].as<String>();
    if (sz == "24") cfg.iconSize = 24;
    else if (sz == "48") cfg.iconSize = 48;
    else cfg.iconSize = 36;
  }

  if (doc["iconOpenColor"].is<const char*>())
    cfg.iconOpenColor = parseColor(doc["iconOpenColor"].as<const char*>(), cfg.iconOpenColor);
  if (doc["iconClosedColor"].is<const char*>())
    cfg.iconClosedColor = parseColor(doc["iconClosedColor"].as<const char*>(), cfg.iconClosedColor);

  if (doc["showStateText"].is<const char*>())
    cfg.showStateText = (String(doc["showStateText"].as<const char*>()) != "no");

  if (doc["titleFont"].is<const char*>())
    cfg.titleFont = parseFont(doc["titleFont"].as<const char*>(), 2);
  if (doc["titleColor"].is<const char*>())
    cfg.titleColor = parseColor(doc["titleColor"].as<const char*>(), cfg.titleColor);
  if (doc["titleHorizontalAlign"].is<const char*>())
    cfg.titleHorizontalAlign = doc["titleHorizontalAlign"].as<const char*>();

  if (doc["stateFont"].is<const char*>())
    cfg.stateFont = parseFont(doc["stateFont"].as<const char*>(), 2);
  if (doc["stateColor"].is<const char*>())
    cfg.stateColor = parseColor(doc["stateColor"].as<const char*>(), cfg.stateColor);

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

constexpr uint8_t  MAX_HA_COVER_CACHE = 12;
constexpr uint32_t FETCH_INTERVAL_MS  = 60000UL;
constexpr uint32_t TOUCH_DEBOUNCE_MS  = 700UL;

struct HaCoverCache {
  String entityId;
  CoverState state;
  String errorCode;
  uint32_t lastFetchMs;
  bool initialised;
  bool hasError;
};

struct HaCoverRenderCache {
  bool rendered;
  int16_t cellLeft, cellTop, cellWidth, cellHeight;
  String configJson;
  CoverState state;
};

static HaCoverCache      haCoverCache[MAX_HA_COVER_CACHE];
static HaCoverRenderCache haCoverRenderCache[MAX_LAYOUT_CELLS];
static uint32_t haCoverLastTouchMs[MAX_LAYOUT_CELLS] = {0};
static uint8_t haCoverCacheCount = 0;

HaCoverCache* findOrCreateCache(const String& entityId) {
  for (uint8_t i = 0; i < haCoverCacheCount; i++) {
    if (haCoverCache[i].entityId == entityId) return &haCoverCache[i];
  }
  if (haCoverCacheCount >= MAX_HA_COVER_CACHE) return nullptr;
  HaCoverCache& e = haCoverCache[haCoverCacheCount++];
  e.entityId    = entityId;
  e.state       = CoverState::Unknown;
  e.errorCode   = "";
  e.lastFetchMs = 0;
  e.initialised = false;
  e.hasError    = false;
  return &e;
}

bool renderCacheMatches(const HaCoverRenderCache& c,
                        const LayoutCellConfig& cell, CoverState state,
                        int16_t l, int16_t t, int16_t w, int16_t h) {
  return c.rendered && c.cellLeft == l && c.cellTop == t &&
         c.cellWidth == w && c.cellHeight == h &&
         c.configJson == cell.configJson && c.state == state;
}

void rememberRender(HaCoverRenderCache& c, const LayoutCellConfig& cell,
                    CoverState state,
                    int16_t l, int16_t t, int16_t w, int16_t h) {
  c.rendered   = true;
  c.cellLeft   = l; c.cellTop    = t;
  c.cellWidth  = w; c.cellHeight = h;
  c.configJson = cell.configJson;
  c.state      = state;
}

// ---------------------------------------------------------------------------
// HTTP fetch
// ---------------------------------------------------------------------------

bool fetchCoverState(const HaCoverConfig& cfg, CoverState& outState,
                     String& outError) {
  const String url = cfg.haUrl + "/api/states/" + cfg.entityId;
  const bool useTls = cfg.haUrl.startsWith("https");

  HTTPClient http;
  http.setTimeout(8000);

  if (useTls) {
    static WiFiClientSecure tlsClient;
    tlsClient.setInsecure();
    if (!http.begin(tlsClient, url)) { outError = "conn"; return false; }
  } else {
    static WiFiClient plainClient;
    if (!http.begin(plainClient, url)) { outError = "conn"; return false; }
  }

  http.addHeader("Authorization", "Bearer " + cfg.haToken);
  http.addHeader("Content-Type", "application/json");

  const int code = http.GET();
  if (code <= 0) { outError = "net";         http.end(); return false; }
  if (code == 401) { outError = "401";       http.end(); return false; }
  if (code == 404) { outError = "404";       http.end(); return false; }
  if (code != 200) { outError = String(code); http.end(); return false; }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) { outError = "json"; return false; }

  outState = parseCoverState(doc["state"] | "unknown");
  return true;
}

bool toggleCover(const HaCoverConfig& cfg, String& outError) {
  const String url = cfg.haUrl + "/api/services/cover/toggle";
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

  JsonDocument body;
  body["entity_id"] = cfg.entityId;
  String payload;
  serializeJson(body, payload);

  http.addHeader("Authorization", "Bearer " + cfg.haToken);
  http.addHeader("Content-Type", "application/json");

  const int code = http.POST(payload);
  http.end();

  if (code <= 0) {
    outError = "net";
    return false;
  }
  if (code == 401) {
    outError = "401";
    return false;
  }
  if (code == 404) {
    outError = "404";
    return false;
  }
  if (code < 200 || code >= 300) {
    outError = String(code);
    return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// Layout helper
// ---------------------------------------------------------------------------

int16_t gridCoordinate(int16_t size, uint8_t position, uint8_t divisions) {
  if (divisions == 0) return 0;
  return static_cast<int16_t>((static_cast<int32_t>(size) * position) /
                              divisions);
}

// ---------------------------------------------------------------------------
// Icon drawing
// ---------------------------------------------------------------------------

void drawRectOutline(Renderer& r, int16_t x, int16_t y, int16_t w,
                     int16_t h, uint16_t color) {
  r.drawLine(x,         y,         x + w - 1, y,         color);
  r.drawLine(x,         y + h - 1, x + w - 1, y + h - 1, color);
  r.drawLine(x,         y,         x,         y + h - 1, color);
  r.drawLine(x + w - 1, y,         x + w - 1, y + h - 1, color);
}

// Draw a cover icon centred at (cx, cy) in a iSize×iSize bounding box.
// isOpen == true  → show open state (open / opening)
// isOpen == false → show closed state (closed / closing)
void drawCoverIcon(Renderer& r, int16_t cx, int16_t cy, int16_t iSize,
                   CoverIconType type, bool isOpen, uint16_t color) {
  const int16_t half = iSize / 2;
  const int16_t ix   = cx - half;
  const int16_t iy   = cy - half;
  const int16_t iw   = iSize;
  const int16_t ih   = iSize;

  switch (type) {

    // ── None (generic) ─────────────────────────────────────────────────
    case CoverIconType::None: {
      r.drawCircle(cx, cy, half - 1, color);
      if (isOpen) {
        // Cross = air/light passing through
        const int16_t a = half / 2;
        r.drawLine(cx - a, cy,     cx + a, cy,     color);
        r.drawLine(cx,     cy - a, cx,     cy + a, color);
      } else {
        r.fillCircle(cx, cy, half / 2, color);
      }
      break;
    }

    // ── Awning ─────────────────────────────────────────────────────────
    // Closed: flat wall bar only.
    // Open:   triangular awning extending downward from the wall.
    case CoverIconType::Awning: {
      const int16_t barH = 3;
      r.fillRect(ix, iy, iw, barH, color); // wall bar
      if (isOpen) {
        const int16_t extY = iy + ih * 2 / 3;
        r.drawLine(ix,          iy + barH, ix + iw - 1, extY,      color); // top slope
        r.drawLine(ix,          extY,      ix + iw - 1, extY,      color); // front edge
        r.drawLine(ix,          iy + barH, ix,          extY,      color); // left side
      } else {
        // Small folded bundle near wall
        r.fillRect(ix, iy + barH, iw * 2 / 5, barH, color);
      }
      break;
    }

    // ── Blind (venetian) ───────────────────────────────────────────────
    // Open:   few widely-spaced slat lines (slats tilted open, light passes).
    // Closed: many dense slat lines (slats flat, light blocked).
    case CoverIconType::Blind: {
      drawRectOutline(r, ix, iy, iw, ih, color);
      const int16_t pad   = 2;
      const int16_t inner = ih - pad * 2;
      const int16_t slats = isOpen ? 3 : 7;
      for (int16_t i = 1; i <= slats; i++) {
        const int16_t slY = iy + pad + (inner * i) / (slats + 1);
        r.drawLine(ix + pad, slY, ix + iw - pad - 1, slY, color);
      }
      break;
    }

    // ── Curtain ────────────────────────────────────────────────────────
    // Open:   two narrow fabric panels gathered at the sides.
    // Closed: two half-width panels meeting at the centre.
    case CoverIconType::Curtain: {
      if (isOpen) {
        const int16_t pw = iw / 4;
        r.fillRect(ix,            iy, pw, ih, color);
        r.fillRect(ix + iw - pw,  iy, pw, ih, color);
      } else {
        const int16_t pw = iw / 2 - 1;
        r.fillRect(ix,            iy, pw, ih, color);
        r.fillRect(ix + iw - pw,  iy, pw, ih, color);
      }
      break;
    }

    // ── Damper ─────────────────────────────────────────────────────────
    // Open:   circle outline with a horizontal line through it (airflow).
    // Closed: thick circle with filled centre (damper blocking flow).
    case CoverIconType::Damper: {
      r.drawCircle(cx, cy, half - 1, color);
      r.drawCircle(cx, cy, half - 2, color); // double-ring for thickness
      if (isOpen) {
        const int16_t a = half - 4;
        r.drawLine(cx - a, cy, cx + a, cy, color); // horizontal airflow line
      } else {
        r.fillCircle(cx, cy, half - 4, color);
      }
      break;
    }

    // ── Door ───────────────────────────────────────────────────────────
    // Open:   door-frame outline + perspective lines showing door ajar.
    // Closed: full door outline with a handle dot.
    case CoverIconType::Door: {
      drawRectOutline(r, ix, iy, iw, ih, color);
      if (isOpen) {
        const int16_t depth = iw / 3;
        // Perspective edge of open door panel
        r.drawLine(ix + 1,     iy + 1,        ix + depth, iy + depth / 2,      color);
        r.drawLine(ix + 1,     iy + ih - 2,   ix + depth, iy + ih - depth / 2, color);
        r.drawLine(ix + depth, iy + depth / 2, ix + depth, iy + ih - depth / 2, color);
      } else {
        r.fillCircle(ix + iw * 3 / 4, cy, 2, color); // door handle
      }
      break;
    }

    // ── Garage ─────────────────────────────────────────────────────────
    // Open:   wide rectangle with only a thick bar at the top (door rolled up).
    // Closed: wide rectangle with 3 horizontal panel lines.
    case CoverIconType::Garage: {
      const int16_t gH = ih * 2 / 3;
      const int16_t gY = iy + (ih - gH) / 2;
      drawRectOutline(r, ix, gY, iw, gH, color);
      if (isOpen) {
        r.drawLine(ix + 2, gY + 2, ix + iw - 3, gY + 2, color);
        r.drawLine(ix + 2, gY + 3, ix + iw - 3, gY + 3, color);
      } else {
        for (int16_t i = 1; i <= 3; i++) {
          const int16_t lY = gY + (gH * i) / 4;
          r.drawLine(ix + 2, lY, ix + iw - 3, lY, color);
        }
      }
      break;
    }

    // ── Gate ───────────────────────────────────────────────────────────
    // Open:   two vertical posts with a diagonal bar (gate swung open).
    // Closed: two posts with 3 horizontal bars.
    case CoverIconType::Gate: {
      const int16_t pw = 3;
      r.fillRect(ix,           iy, pw, ih, color); // left post
      r.fillRect(ix + iw - pw, iy, pw, ih, color); // right post
      if (isOpen) {
        r.drawLine(ix + pw + 1, iy + 2,
                   ix + iw - pw - 2, iy + ih - 3, color);
      } else {
        for (int16_t i = 1; i <= 3; i++) {
          const int16_t bY = iy + (ih * i) / 4;
          r.drawLine(ix + pw, bY, ix + iw - pw - 1, bY, color);
        }
      }
      break;
    }

    // ── Shade (roller shade) ────────────────────────────────────────────
    // Open:   shade rolled up — small bar at top + a weight-bar just below.
    // Closed: shade fully extended — filled panel + weight-bar at bottom.
    case CoverIconType::Shade: {
      drawRectOutline(r, ix, iy, iw, ih, color);
      if (isOpen) {
        r.fillRect(ix + 1, iy + 1,      iw - 2, 3, color); // rolled shade
        r.fillRect(ix + 1, iy + 4,      iw - 2, 2, color); // weight bar
      } else {
        r.fillRect(ix + 1, iy + 1,      iw - 2, ih - 5, color); // shade panel
        r.fillRect(ix + 1, iy + ih - 4, iw - 2, 2,      color); // weight bar
      }
      break;
    }

    // ── Shutter (exterior shutters) ─────────────────────────────────────
    // Open:   two narrow panels folded to each side, each with slat lines.
    // Closed: single panel covering the opening, centre seam + slat lines.
    case CoverIconType::Shutter: {
      if (isOpen) {
        const int16_t pw = iw / 4;
        drawRectOutline(r, ix,           iy, pw, ih, color);
        drawRectOutline(r, ix + iw - pw, iy, pw, ih, color);
        for (int16_t i = 1; i <= 3; i++) {
          const int16_t slY = iy + (ih * i) / 4;
          r.drawLine(ix + 1,           slY, ix + pw - 2,      slY, color);
          r.drawLine(ix + iw - pw + 1, slY, ix + iw - 2,      slY, color);
        }
      } else {
        drawRectOutline(r, ix, iy, iw, ih, color);
        r.drawLine(ix + iw / 2, iy, ix + iw / 2, iy + ih - 1, color); // seam
        for (int16_t i = 1; i <= 4; i++) {
          const int16_t slY = iy + (ih * i) / 5;
          r.drawLine(ix + 1, slY, ix + iw - 2, slY, color);
        }
      }
      break;
    }

    // ── Window ─────────────────────────────────────────────────────────
    // A 4-pane window frame. Open: a diagonal on the top-left pane indicates
    // that pane is tilted/opened.
    case CoverIconType::Window: {
      drawRectOutline(r, ix, iy, iw, ih, color);
      const int16_t midX = ix + iw / 2;
      const int16_t midY = iy + ih / 2;
      r.drawLine(midX, iy,         midX, iy + ih - 1, color); // vertical divider
      r.drawLine(ix,   midY, ix + iw - 1, midY,       color); // horizontal divider
      if (isOpen) {
        // Diagonal in top-left pane: open indicator
        r.drawLine(ix + 2, iy + 2, midX - 2, midY - 2, color);
      }
      break;
    }

    default:
      r.drawCircle(cx, cy, half - 1, color);
      break;
  }
}

} // namespace

// ---------------------------------------------------------------------------
// Render entry point
// ---------------------------------------------------------------------------

void renderHaCoverPlugin(PluginRenderContext& context) {
  Renderer& renderer             = context.renderer;
  const LayoutCellConfig& cell   = context.cell;
  const AppConfig& appCfg        = context.config;

  const int16_t cellLeft   = gridCoordinate(renderer.width(),  cell.col,              appCfg.cols);
  const int16_t cellRight  = gridCoordinate(renderer.width(),  cell.col + cell.colSpan, appCfg.cols);
  const int16_t cellTop    = gridCoordinate(renderer.height(), cell.row,              appCfg.rows);
  const int16_t cellBottom = gridCoordinate(renderer.height(), cell.row + cell.rowSpan, appCfg.rows);
  const int16_t cellWidth  = cellRight  - cellLeft;
  const int16_t cellHeight = cellBottom - cellTop;

  const HaCoverConfig cfg = readConfig(cell.configJson);

  // ── Cache refresh ────────────────────────────────────────────────────
  HaCoverCache* cache = findOrCreateCache(cfg.entityId);
  const uint32_t now  = millis();

  if (cache != nullptr && cfg.entityId.length() > 0 &&
      cfg.haUrl.length() > 0 && cfg.haToken.length() > 0) {
    const bool stale = !cache->initialised ||
                       (now - cache->lastFetchMs >= FETCH_INTERVAL_MS);
    if (stale) {
      CoverState fetched;
      String errCode;
      if (fetchCoverState(cfg, fetched, errCode)) {
        cache->state    = fetched;
        cache->errorCode = "";
        cache->hasError  = false;
      } else {
        cache->errorCode = errCode;
        cache->hasError  = true;
      }
      cache->lastFetchMs = now;
      cache->initialised = true;
    }
  }

  // ── Determine current state ──────────────────────────────────────────
  const bool hasError = (cache != nullptr && cache->initialised && cache->hasError);
  const CoverState currentState =
      (cache != nullptr && cache->initialised && !cache->hasError)
          ? cache->state
          : CoverState::Unknown;

  // ── Render-cache check ───────────────────────────────────────────────
  HaCoverRenderCache* renderCache =
      context.cellIndex < MAX_LAYOUT_CELLS
          ? &haCoverRenderCache[context.cellIndex]
          : nullptr;

  if (!context.forceClear && renderCache != nullptr &&
      renderCacheMatches(*renderCache, cell, currentState,
                         cellLeft, cellTop, cellWidth, cellHeight)) {
    return;
  }

  // ── Layout math ──────────────────────────────────────────────────────
  const bool showTitle = cfg.title.length() > 0;
  const bool showState = cfg.showStateText;

  const int16_t titleH = showTitle ? renderer.fontHeight(cfg.titleFont) : 0;
  const int16_t iconH  = cfg.iconSize;
  const int16_t stateH = showState ? renderer.fontHeight(cfg.stateFont) : 0;
  const int16_t gap    = 4;

  int16_t totalH = titleH + iconH + stateH;
  if (showTitle && (iconH > 0 || showState)) totalH += gap;
  if (showState && iconH > 0) totalH += gap;

  int16_t startY;
  if (cfg.verticalAlign == "top") {
    startY = cellTop;
  } else if (cfg.verticalAlign == "bottom") {
    startY = cellBottom - totalH;
  } else {
    startY = cellTop + (cellHeight - totalH) / 2;
  }
  startY += cfg.offsetY;

  // ── Draw ─────────────────────────────────────────────────────────────
  renderer.fillRect(cellLeft, cellTop, cellWidth, cellHeight);

  int16_t curY = startY;

  if (showTitle) {
    renderer.textWithin(cellLeft, curY, cellWidth, toAscii(cfg.title),
                        cfg.titleFont, cfg.titleColor,
                        parseHAlign(cfg.titleHorizontalAlign), cfg.offsetX);
    curY += titleH + gap;
  }

  // Icon (or error text)
  if (hasError) {
    const String errText = cache->errorCode.length() > 0
                               ? "ERR " + cache->errorCode
                               : "ERR";
    renderer.textWithin(cellLeft, curY + iconH / 4, cellWidth, errText,
                        cfg.stateFont, 0xF800 /* red */,
                        TextHorizontalAlign::Center, cfg.offsetX);
  } else {
    const bool isOpen = (currentState == CoverState::Open ||
                         currentState == CoverState::Opening);
    const uint16_t iconColor = isOpen ? cfg.iconOpenColor : cfg.iconClosedColor;
    const int16_t iconCX = cellLeft + cellWidth / 2 + cfg.offsetX;
    const int16_t iconCY = curY + iconH / 2;
    drawCoverIcon(renderer, iconCX, iconCY, cfg.iconSize, cfg.coverType,
                  isOpen, iconColor);
  }
  curY += iconH + gap;

  // State text
  if (showState) {
    String stateStr;
    if (cache == nullptr || !cache->initialised) {
      stateStr = "---";
    } else if (hasError) {
      stateStr = "";
    } else {
      switch (currentState) {
        case CoverState::Open:        stateStr = "open";    break;
        case CoverState::Closed:      stateStr = "closed";  break;
        case CoverState::Opening:     stateStr = "opening"; break;
        case CoverState::Closing:     stateStr = "closing"; break;
        case CoverState::Unavailable: stateStr = "unavail"; break;
        default:                      stateStr = "unknown"; break;
      }
    }
    if (stateStr.length() > 0) {
      renderer.textWithin(cellLeft, curY, cellWidth, stateStr,
                          cfg.stateFont, cfg.stateColor,
                          parseHAlign(cfg.titleHorizontalAlign), cfg.offsetX);
    }
  }

  // ── Save render cache ─────────────────────────────────────────────────
  if (renderCache != nullptr) {
    rememberRender(*renderCache, cell, currentState,
                   cellLeft, cellTop, cellWidth, cellHeight);
  }
}

void handleTouchHaCoverPlugin(PluginTouchContext& context) {
  const HaCoverConfig cfg = readConfig(context.cell.configJson);
  if (cfg.haUrl.length() == 0 || cfg.haToken.length() == 0 ||
      cfg.entityId.length() == 0) {
    return;
  }

  const uint32_t now = millis();
  if (context.cellIndex < MAX_LAYOUT_CELLS) {
    const uint32_t lastTouch = haCoverLastTouchMs[context.cellIndex];
    if (now - lastTouch < TOUCH_DEBOUNCE_MS) {
      return;
    }
    haCoverLastTouchMs[context.cellIndex] = now;
  }

  String errCode;
  const bool ok = toggleCover(cfg, errCode);

  HaCoverCache* cache = findOrCreateCache(cfg.entityId);
  if (!cache) {
    return;
  }

  if (!ok) {
    cache->hasError = true;
    cache->errorCode = errCode;
    cache->initialised = true;
    cache->lastFetchMs = now;
    return;
  }

  cache->hasError = false;
  cache->errorCode = "";
  cache->initialised = true;
  cache->lastFetchMs = 0;
}
