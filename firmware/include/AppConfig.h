#pragma once

#include <Arduino.h>

struct ClockPluginConfig {
  String timezone = "Europe/Paris";
  bool hasTimezoneOffsetMinutes = false;
  int16_t timezoneOffsetMinutes = 0;
  String locale = "fr-FR";
  String hourCycle = "h23";
  String dateTimeOrder = "time-date";
  bool defaultAnalogMode = false;
  String title = "";
  bool showTitle = false;
  bool showDate = true;
  bool showSeconds = false;
  uint8_t titleFont = 2;
  uint8_t timeFont = 4;
  uint8_t dateFont = 2;
  String titleHorizontalAlign = "center";
  String timeHorizontalAlign = "center";
  String dateHorizontalAlign = "center";
  String verticalAlign = "center";
  int16_t offsetX = 0;
  int16_t offsetY = 0;
  uint16_t titleColor = 0x07FF;
  uint16_t timeColor = 0xFFFF;
  uint16_t dateColor = 0xD69A;
  uint16_t analogFaceColor = 0x2104;
  uint16_t analogRimColor = 0xD69A;
  uint16_t analogTickColor = 0xD69A;
  uint16_t analogHourHandColor = 0xFFFF;
  uint16_t analogMinuteHandColor = 0xFFFF;
  uint16_t analogCenterColor = 0xFFFF;
};

struct LayoutCellConfig {
  String pluginId;
  String configJson = "{}";
  uint8_t page = 0;
  uint8_t col = 0;
  uint8_t row = 0;
  uint8_t colSpan = 1;
  uint8_t rowSpan = 1;
  ClockPluginConfig clock;
};

constexpr uint8_t MAX_LAYOUT_CELLS = 48;
constexpr uint8_t MAX_LAYOUT_PAGES = 8;

struct AppConfig {
  String ssid;
  String password;
  String timezone = "Europe/Paris";
  String locale = "fr-FR";
  String orientation = "0";
  uint16_t backgroundColor = 0x0000;
  uint8_t brightness = 255;
  bool automaticBrightness = false;
  uint8_t cols = 3;
  uint8_t rows = 4;
  uint8_t pageCount = 1;
  LayoutCellConfig cells[MAX_LAYOUT_CELLS];
  uint8_t cellCount = 0;
};