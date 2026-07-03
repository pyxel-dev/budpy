#include "AppConfigParser.h"

#include "PluginRegistry.h"

#include <cstring>
#include <new>

namespace {
constexpr int CLOCK_MIN_OFFSET = -120;
constexpr int CLOCK_MAX_OFFSET = 120;

bool fail(String& error, const char* message) {
  error = message;
  return false;
}

bool isSupportedLocale(const String& locale) {
  static const char* const supportedLocales[] = {
      "af-ZA", "sq-AL", "ar-SA", "az-AZ", "eu-ES", "be-BY", "bg-BG",
      "ca-ES", "zh-CN", "zh-TW", "hr-HR", "cs-CZ", "da-DK", "nl-NL",
      "en-GB", "en-US", "fi-FI", "fr-FR", "gl-ES", "de-DE", "el-GR",
      "he-IL", "hi-IN", "hu-HU", "is-IS", "id-ID", "it-IT", "ja-JP",
      "ko-KR", "ku-TR", "lv-LV", "lt-LT", "mk-MK", "no-NO", "fa-IR",
      "pl-PL", "pt-PT", "pt-BR", "ro-RO", "ru-RU", "sr-RS", "sk-SK",
      "sl-SI", "es-ES", "sv-SE", "th-TH", "tr-TR", "uk-UA", "vi-VN",
      "zu-ZA",
  };

  for (const char* supportedLocale : supportedLocales) {
    if (locale == supportedLocale) return true;
  }

  return false;
}

bool isSupportedClockLocale(const String& locale) {
  return isSupportedLocale(locale);
}

bool isSupportedTimezone(const String& timezone) {
  return timezone.length() > 0 && timezone.length() <= 64;
}

bool failUnsupportedTimezone(String& error, const char* fieldName,
                             const String& timezone) {
  error = String("Unsupported ") + fieldName + ": " + timezone;
  return false;
}

bool normalizeOrientation(String& orientation) {
  if (orientation == "portrait") {
    orientation = "0";
    return true;
  }

  if (orientation == "landscape") {
    orientation = "90";
    return true;
  }

  return orientation == "0" || orientation == "90" || orientation == "180" ||
         orientation == "270";
}

bool isSupportedHourCycle(const String& hourCycle) {
  return hourCycle == "h23" || hourCycle == "h12";
}

bool isSupportedDateTimeOrder(const String& dateTimeOrder) {
  return dateTimeOrder == "time-date" || dateTimeOrder == "date-time";
}

bool isSupportedClockDisplayMode(const String& displayMode) {
  return displayMode == "digital" || displayMode == "analog";
}

bool isSupportedClockFont(uint8_t font) {
  return font == 1 || font == 2 || font == 4;
}

bool isSupportedHorizontalAlign(const String& align) {
  return align == "left" || align == "center" || align == "right";
}

bool isSupportedVerticalAlign(const String& align) {
  return align == "top" || align == "center" || align == "bottom";
}

int hexNibble(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }

  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }

  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }

  return -1;
}

bool parseHexColor(const char* text, uint16_t& color) {
  if (std::strlen(text) != 7 || text[0] != '#') {
    return false;
  }

  uint8_t channels[3] = {0, 0, 0};
  for (uint8_t channel = 0; channel < 3; channel++) {
    const int high = hexNibble(text[1 + channel * 2]);
    const int low = hexNibble(text[2 + channel * 2]);
    if (high < 0 || low < 0) {
      return false;
    }

    channels[channel] = static_cast<uint8_t>((high << 4) | low);
  }

  color =
      static_cast<uint16_t>(((channels[0] & 0xF8) << 8) |
                            ((channels[1] & 0xFC) << 3) | (channels[2] >> 3));
  return true;
}

bool readRequiredString(JsonObjectConst object, const char* key, String& value,
                        String& error, const char* fieldName) {
  JsonVariantConst variant = object[key];
  if (!variant.is<const char*>()) {
    error = String("Missing or invalid ") + fieldName;
    return false;
  }

  const char* text = variant.as<const char*>();
  if (std::strlen(text) == 0) {
    error = String("Missing or invalid ") + fieldName;
    return false;
  }

  value = text;
  return true;
}

bool readOptionalString(JsonObjectConst object, const char* key,
                        const String& fallback, String& value, String& error,
                        const char* fieldName) {
  JsonVariantConst variant = object[key];
  if (variant.isNull()) {
    value = fallback;
    return true;
  }

  if (!variant.is<const char*>()) {
    error = String("Invalid ") + fieldName;
    return false;
  }

  const char* text = variant.as<const char*>();
  if (std::strlen(text) == 0) {
    error = String("Invalid ") + fieldName;
    return false;
  }

  value = text;
  return true;
}

bool readOptionalText(JsonObjectConst object, const char* key,
                      const String& fallback, String& value, String& error,
                      const char* fieldName) {
  JsonVariantConst variant = object[key];
  if (variant.isNull()) {
    value = fallback;
    return true;
  }

  if (!variant.is<const char*>()) {
    error = String("Invalid ") + fieldName;
    return false;
  }

  value = variant.as<const char*>();
  return true;
}

bool readRequiredInt(JsonObjectConst object, const char* key, int& value,
                     String& error, const char* fieldName) {
  JsonVariantConst variant = object[key];
  if (!variant.is<int>()) {
    error = String("Missing or invalid ") + fieldName;
    return false;
  }

  value = variant.as<int>();
  return true;
}

bool readOptionalInt(JsonObjectConst object, const char* key, int fallback,
                     int& value, String& error, const char* fieldName) {
  JsonVariantConst variant = object[key];
  if (variant.isNull()) {
    value = fallback;
    return true;
  }

  if (!variant.is<int>()) {
    error = String("Invalid ") + fieldName;
    return false;
  }

  value = variant.as<int>();
  return true;
}

bool readOptionalIntInRange(JsonObjectConst object, const char* key,
                            int fallback, int minValue, int maxValue,
                            int16_t& value, String& error,
                            const char* fieldName) {
  int parsed = fallback;
  if (!readOptionalInt(object, key, fallback, parsed, error, fieldName)) {
    return false;
  }

  if (parsed < minValue || parsed > maxValue) {
    error = String("Invalid ") + fieldName;
    return false;
  }

  value = static_cast<int16_t>(parsed);
  return true;
}

bool readOptionalClockFont(JsonObjectConst object, const char* key,
                           uint8_t fallback, uint8_t& value, String& error,
                           const char* fieldName) {
  JsonVariantConst variant = object[key];
  if (variant.isNull()) {
    value = fallback;
    return true;
  }

  int parsed = 0;
  if (variant.is<int>()) {
    parsed = variant.as<int>();
  } else if (variant.is<const char*>()) {
    const char* text = variant.as<const char*>();
    if (std::strlen(text) != 1 || text[0] < '0' || text[0] > '9') {
      error = String("Invalid ") + fieldName;
      return false;
    }

    parsed = text[0] - '0';
  } else {
    error = String("Invalid ") + fieldName;
    return false;
  }

  if (parsed < 0 || parsed > 255 ||
      !isSupportedClockFont(static_cast<uint8_t>(parsed))) {
    error = String("Invalid ") + fieldName;
    return false;
  }

  value = static_cast<uint8_t>(parsed);
  return true;
}

bool readOptionalColor(JsonObjectConst object, const char* key,
                       uint16_t fallback, uint16_t& value, String& error,
                       const char* fieldName) {
  JsonVariantConst variant = object[key];
  if (variant.isNull()) {
    value = fallback;
    return true;
  }

  if (!variant.is<const char*>()) {
    error = String("Invalid ") + fieldName;
    return false;
  }

  uint16_t parsedColor = fallback;
  if (!parseHexColor(variant.as<const char*>(), parsedColor)) {
    error = String("Invalid ") + fieldName;
    return false;
  }

  value = parsedColor;
  return true;
}

bool readOptionalHorizontalAlign(JsonObjectConst object, const char* key,
                                 const String& fallback, String& value,
                                 String& error, const char* fieldName) {
  if (!readOptionalString(object, key, fallback, value, error, fieldName)) {
    return false;
  }

  if (!isSupportedHorizontalAlign(value)) {
    error = String("Unsupported ") + fieldName;
    return false;
  }

  return true;
}

bool readOptionalBool(JsonObjectConst object, const char* key, bool fallback,
                      bool& value, String& error, const char* fieldName) {
  JsonVariantConst variant = object[key];
  if (variant.isNull()) {
    value = fallback;
    return true;
  }

  if (!variant.is<bool>()) {
    error = String("Invalid ") + fieldName;
    return false;
  }

  value = variant.as<bool>();
  return true;
}

bool readObject(JsonObjectConst parent, const char* key, JsonObjectConst& value,
                String& error, const char* fieldName) {
  JsonVariantConst variant = parent[key];
  if (!variant.is<JsonObjectConst>()) {
    error = String("Missing or invalid ") + fieldName;
    return false;
  }

  value = variant.as<JsonObjectConst>();
  return true;
}

void resetAppConfig(AppConfig& config) {
  config.~AppConfig();
  new (&config) AppConfig();
}
} // namespace

bool parseAppConfig(const JsonDocument& doc, AppConfig& out, String& error) {
  JsonObjectConst root = doc.as<JsonObjectConst>();
  if (root.isNull()) {
    return fail(error, "Config root must be an object");
  }

  JsonVariantConst version = root["version"];
  if (!version.is<int>() || version.as<int>() != 1) {
    return fail(error, "Unsupported config version");
  }

  JsonObjectConst wifi;
  if (!readObject(root, "wifi", wifi, error, "wifi")) {
    return false;
  }

  resetAppConfig(out);
  AppConfig& parsed = out;
  if (!readRequiredString(wifi, "ssid", parsed.ssid, error, "wifi.ssid")) {
    return false;
  }
  if (!readRequiredString(wifi, "password", parsed.password, error,
                          "wifi.password")) {
    return false;
  }
  if (parsed.password.length() < 8) {
    return fail(error, "Invalid WiFi password length");
  }

  if (!readRequiredString(root, "locale", parsed.locale, error, "locale")) {
    return false;
  }
  if (!isSupportedLocale(parsed.locale)) {
    return fail(error, "Unsupported locale");
  }

  if (!readRequiredString(root, "timezone", parsed.timezone, error,
                          "timezone")) {
    return false;
  }
  if (!isSupportedTimezone(parsed.timezone)) {
    return failUnsupportedTimezone(error, "timezone", parsed.timezone);
  }

  if (!readOptionalColor(root, "backgroundColor", 0x0000,
                         parsed.backgroundColor, error, "backgroundColor")) {
    return false;
  }

  {
    int16_t brightnessValue = 255;
    if (!readOptionalIntInRange(root, "brightness", 255, 0, 255,
                                brightnessValue, error, "brightness")) {
      return false;
    }
    parsed.brightness = static_cast<uint8_t>(brightnessValue);
  }

  String brightnessMode = "manual";
  if (!readOptionalString(root, "brightnessMode", "manual", brightnessMode,
                          error, "brightnessMode")) {
    return false;
  }
  if (brightnessMode == "manual") {
    parsed.automaticBrightness = false;
  } else if (brightnessMode == "auto") {
    parsed.automaticBrightness = true;
  } else {
    return fail(error, "Unsupported brightnessMode");
  }

  {
    int16_t idleMinutes = 0;
    if (!readOptionalIntInRange(root, "screenIdleMinutes", 0, 0, 1440,
                                idleMinutes, error, "screenIdleMinutes")) {
      return false;
    }
    parsed.screenIdleMinutes = static_cast<uint16_t>(idleMinutes);
  }

  String screenSleepMode = "off";
  if (!readOptionalString(root, "screenSleepMode", "off", screenSleepMode,
                          error, "screenSleepMode")) {
    return false;
  }
  if (screenSleepMode == "off") {
    parsed.screenSleepDim = false;
  } else if (screenSleepMode == "dim") {
    parsed.screenSleepDim = true;
  } else {
    return fail(error, "Unsupported screenSleepMode");
  }

  {
    int16_t dimBrightness = 12;
    if (!readOptionalIntInRange(root, "screenSleepDimBrightness", 12, 0, 255,
                                dimBrightness, error,
                                "screenSleepDimBrightness")) {
      return false;
    }
    parsed.screenSleepDimBrightness = static_cast<uint8_t>(dimBrightness);
  }

  JsonObjectConst device;
  if (!readObject(root, "device", device, error, "device")) {
    return false;
  }
  if (!readRequiredString(device, "orientation", parsed.orientation, error,
                          "device.orientation")) {
    return false;
  }
  if (!normalizeOrientation(parsed.orientation)) {
    return fail(error, "Unsupported orientation");
  }

  JsonObjectConst layout;
  if (!readObject(root, "layout", layout, error, "layout")) {
    return false;
  }

  int cols = 0;
  int rows = 0;
  if (!readRequiredInt(layout, "cols", cols, error, "layout.cols")) {
    return false;
  }
  if (!readRequiredInt(layout, "rows", rows, error, "layout.rows")) {
    return false;
  }
  if (cols < 1 || cols > 4 || rows < 1 || rows > 4) {
    return fail(error, "Invalid grid dimensions");
  }

  parsed.cols = static_cast<uint8_t>(cols);
  parsed.rows = static_cast<uint8_t>(rows);

  const bool hasPageCount = !layout["pageCount"].isNull();
  int pageCount = 1;
  if (!readOptionalInt(layout, "pageCount", 1, pageCount, error,
                       "layout.pageCount")) {
    return false;
  }
  if (pageCount < 1 || pageCount > MAX_LAYOUT_PAGES) {
    return fail(error, "Invalid layout.pageCount");
  }

  uint8_t computedPageCount = 1;

  JsonVariantConst cellsVariant = layout["cells"];
  if (!cellsVariant.isNull()) {
    if (!cellsVariant.is<JsonArrayConst>()) {
      return fail(error, "Missing or invalid layout.cells");
    }

    JsonArrayConst cells = cellsVariant.as<JsonArrayConst>();
    if (cells.size() > MAX_LAYOUT_CELLS) {
      return fail(error, "Too many layout cells");
    }

    for (JsonObjectConst cell : cells) {
      LayoutCellConfig parsedCell;
      if (!readRequiredString(cell, "pluginId", parsedCell.pluginId, error,
                              "layout.cells[].pluginId")) {
        return false;
      }
      if (!isRegisteredPluginId(parsedCell.pluginId)) {
        error = String("Unknown pluginId: ") + parsedCell.pluginId;
        return false;
      }

      int col = 0;
      int row = 0;
      int colSpan = 0;
      int rowSpan = 0;
      int page = 0;
      if (!readOptionalInt(cell, "page", 0, page, error,
                           "layout.cells[].page")) {
        return false;
      }
      if (page < 0 || page >= MAX_LAYOUT_PAGES) {
        return fail(error, "Invalid layout cell page");
      }
      if (hasPageCount && page >= pageCount) {
        return fail(error, "Cell outside page range");
      }
      if (!readRequiredInt(cell, "col", col, error, "layout.cells[].col")) {
        return false;
      }
      if (!readRequiredInt(cell, "row", row, error, "layout.cells[].row")) {
        return false;
      }
      if (!readRequiredInt(cell, "colSpan", colSpan, error,
                           "layout.cells[].colSpan")) {
        return false;
      }
      if (!readRequiredInt(cell, "rowSpan", rowSpan, error,
                           "layout.cells[].rowSpan")) {
        return false;
      }
      if (col < 0 || row < 0 || colSpan < 1 || rowSpan < 1 || colSpan > 4 ||
          rowSpan > 4) {
        return fail(error, "Invalid layout cell bounds");
      }
      if (col + colSpan > cols || row + rowSpan > rows) {
        return fail(error, "Cell outside grid");
      }

      parsedCell.page = static_cast<uint8_t>(page);
      parsedCell.col = static_cast<uint8_t>(col);
      parsedCell.row = static_cast<uint8_t>(row);
      parsedCell.colSpan = static_cast<uint8_t>(colSpan);
      parsedCell.rowSpan = static_cast<uint8_t>(rowSpan);

      if (page + 1 > computedPageCount) {
        computedPageCount = static_cast<uint8_t>(page + 1);
      }

      JsonVariantConst configVariant = cell["config"];
      if (!configVariant.isNull() && !configVariant.is<JsonObjectConst>()) {
        return fail(error, "Invalid plugin config");
      }

      if (configVariant.is<JsonObjectConst>()) {
        serializeJson(configVariant, parsedCell.configJson);
      }

      if (parsedCell.pluginId == "clock") {
        JsonObjectConst clock = configVariant.as<JsonObjectConst>();
        if (!readOptionalString(clock, "timezone", parsed.timezone,
                                parsedCell.clock.timezone, error,
                                "clock.timezone")) {
          return false;
        }
        if (!isSupportedTimezone(parsedCell.clock.timezone)) {
          return failUnsupportedTimezone(error, "clock.timezone",
                                         parsedCell.clock.timezone);
        }
        int timezoneOffsetMinutes = 0;
        if (!readOptionalInt(clock, "timezoneOffsetMinutes", 0,
                             timezoneOffsetMinutes, error,
                             "clock.timezoneOffsetMinutes")) {
          return false;
        }
        if (!clock["timezoneOffsetMinutes"].isNull()) {
          if (timezoneOffsetMinutes < -14 * 60 ||
              timezoneOffsetMinutes > 14 * 60) {
            return fail(error, "Invalid clock.timezoneOffsetMinutes");
          }

          parsedCell.clock.hasTimezoneOffsetMinutes = true;
          parsedCell.clock.timezoneOffsetMinutes =
              static_cast<int16_t>(timezoneOffsetMinutes);
        }
        if (!readOptionalString(clock, "locale", parsed.locale,
                                parsedCell.clock.locale, error,
                                "clock.locale")) {
          return false;
        }
        if (!isSupportedClockLocale(parsedCell.clock.locale)) {
          return fail(error, "Unsupported clock locale");
        }
        if (!readOptionalString(clock, "hourCycle", "h23",
                                parsedCell.clock.hourCycle, error,
                                "clock.hourCycle")) {
          return false;
        }
        if (!isSupportedHourCycle(parsedCell.clock.hourCycle)) {
          return fail(error, "Unsupported clock hourCycle");
        }
        if (!readOptionalString(clock, "dateTimeOrder", "time-date",
                                parsedCell.clock.dateTimeOrder, error,
                                "clock.dateTimeOrder")) {
          return false;
        }
        if (!isSupportedDateTimeOrder(parsedCell.clock.dateTimeOrder)) {
          return fail(error, "Unsupported clock.dateTimeOrder");
        }
        String defaultDisplayMode = "digital";
        if (!readOptionalString(clock, "defaultDisplayMode", "digital",
                                defaultDisplayMode, error,
                                "clock.defaultDisplayMode")) {
          return false;
        }
        if (!isSupportedClockDisplayMode(defaultDisplayMode)) {
          return fail(error, "Unsupported clock.defaultDisplayMode");
        }
        parsedCell.clock.defaultAnalogMode = defaultDisplayMode == "analog";
        if (!readOptionalText(clock, "title", "", parsedCell.clock.title, error,
                              "clock.title")) {
          return false;
        }
        if (parsedCell.clock.title.length() > 32) {
          return fail(error, "Invalid clock.title length");
        }
        if (!readOptionalBool(clock, "showTitle", false,
                              parsedCell.clock.showTitle, error,
                              "clock.showTitle")) {
          return false;
        }
        if (!readOptionalBool(clock, "showDate", true,
                              parsedCell.clock.showDate, error,
                              "clock.showDate")) {
          return false;
        }
        if (!readOptionalBool(clock, "showSeconds", false,
                              parsedCell.clock.showSeconds, error,
                              "clock.showSeconds")) {
          return false;
        }
        if (!readOptionalClockFont(clock, "timeFont", 4,
                                   parsedCell.clock.timeFont, error,
                                   "clock.timeFont")) {
          return false;
        }
        uint8_t legacyLabelFont = 2;
        if (!readOptionalClockFont(clock, "labelFont", 2, legacyLabelFont,
                                   error, "clock.labelFont")) {
          return false;
        }
        if (!readOptionalClockFont(clock, "titleFont", legacyLabelFont,
                                   parsedCell.clock.titleFont, error,
                                   "clock.titleFont")) {
          return false;
        }
        if (!readOptionalClockFont(clock, "dateFont", legacyLabelFont,
                                   parsedCell.clock.dateFont, error,
                                   "clock.dateFont")) {
          return false;
        }
        String legacyHorizontalAlign = "center";
        if (!readOptionalString(clock, "horizontalAlign", "center",
                                legacyHorizontalAlign, error,
                                "clock.horizontalAlign")) {
          return false;
        }
        if (!isSupportedHorizontalAlign(legacyHorizontalAlign)) {
          return fail(error, "Unsupported clock.horizontalAlign");
        }
        if (!readOptionalHorizontalAlign(clock, "titleHorizontalAlign",
                                         legacyHorizontalAlign,
                                         parsedCell.clock.titleHorizontalAlign,
                                         error, "clock.titleHorizontalAlign")) {
          return false;
        }
        if (!readOptionalHorizontalAlign(clock, "timeHorizontalAlign",
                                         legacyHorizontalAlign,
                                         parsedCell.clock.timeHorizontalAlign,
                                         error, "clock.timeHorizontalAlign")) {
          return false;
        }
        if (!readOptionalHorizontalAlign(clock, "dateHorizontalAlign",
                                         legacyHorizontalAlign,
                                         parsedCell.clock.dateHorizontalAlign,
                                         error, "clock.dateHorizontalAlign")) {
          return false;
        }
        if (!readOptionalString(clock, "verticalAlign", "center",
                                parsedCell.clock.verticalAlign, error,
                                "clock.verticalAlign")) {
          return false;
        }
        if (!isSupportedVerticalAlign(parsedCell.clock.verticalAlign)) {
          return fail(error, "Unsupported clock.verticalAlign");
        }
        if (!readOptionalIntInRange(clock, "offsetX", 0, CLOCK_MIN_OFFSET,
                                    CLOCK_MAX_OFFSET, parsedCell.clock.offsetX,
                                    error, "clock.offsetX")) {
          return false;
        }
        if (!readOptionalIntInRange(clock, "offsetY", 0, CLOCK_MIN_OFFSET,
                                    CLOCK_MAX_OFFSET, parsedCell.clock.offsetY,
                                    error, "clock.offsetY")) {
          return false;
        }
        if (!readOptionalColor(clock, "titleColor", 0x07FF,
                               parsedCell.clock.titleColor, error,
                               "clock.titleColor")) {
          return false;
        }
        if (!readOptionalColor(clock, "timeColor", 0xFFFF,
                               parsedCell.clock.timeColor, error,
                               "clock.timeColor")) {
          return false;
        }
        if (!readOptionalColor(clock, "dateColor", 0xD69A,
                               parsedCell.clock.dateColor, error,
                               "clock.dateColor")) {
          return false;
        }
        if (!readOptionalColor(clock, "analogFaceColor", 0x2104,
                               parsedCell.clock.analogFaceColor, error,
                               "clock.analogFaceColor")) {
          return false;
        }
        if (!readOptionalColor(clock, "analogRimColor",
                               parsedCell.clock.dateColor,
                               parsedCell.clock.analogRimColor, error,
                               "clock.analogRimColor")) {
          return false;
        }
        if (!readOptionalColor(clock, "analogTickColor",
                               parsedCell.clock.dateColor,
                               parsedCell.clock.analogTickColor, error,
                               "clock.analogTickColor")) {
          return false;
        }
        if (!readOptionalColor(clock, "analogHourHandColor",
                               parsedCell.clock.timeColor,
                               parsedCell.clock.analogHourHandColor, error,
                               "clock.analogHourHandColor")) {
          return false;
        }
        if (!readOptionalColor(clock, "analogMinuteHandColor",
                               parsedCell.clock.timeColor,
                               parsedCell.clock.analogMinuteHandColor, error,
                               "clock.analogMinuteHandColor")) {
          return false;
        }
        if (!readOptionalColor(clock, "analogCenterColor",
                               parsedCell.clock.timeColor,
                               parsedCell.clock.analogCenterColor, error,
                               "clock.analogCenterColor")) {
          return false;
        }
      }

      parsed.cells[parsed.cellCount++] = parsedCell;
    }
  }

  parsed.pageCount =
      hasPageCount ? static_cast<uint8_t>(pageCount) : computedPageCount;

  error = "";
  return true;
}
