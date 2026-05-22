#include "Renderer.h"

namespace {
int16_t clampCoordinate(int32_t value, int16_t minValue, int16_t maxValue) {
  if (value < minValue) {
    return minValue;
  }

  if (value > maxValue) {
    return maxValue;
  }

  return static_cast<int16_t>(value);
}
} // namespace

Renderer::Renderer(TFT_eSPI& display) : tft(display) {}

void Renderer::setBackgroundColor(uint16_t color) {
  backgroundColorValue = color;
}

uint16_t Renderer::backgroundColor() const {
  return backgroundColorValue;
}

void Renderer::clear() {
  tft.fillScreen(backgroundColorValue);
}

int16_t Renderer::width() {
  return tft.width();
}

int16_t Renderer::height() {
  return tft.height();
}

int16_t Renderer::fontHeight(uint8_t font) {
  return tft.fontHeight(font);
}

void Renderer::textCenter(int16_t x, int16_t y, const String& value,
                          uint8_t font, uint16_t color) {
  textCenterWithin(x, y, tft.width(), value, font, color);
}

void Renderer::textCenterWithin(int16_t x, int16_t y, int16_t width,
                                const String& value, uint8_t font,
                                uint16_t color) {
  textWithin(static_cast<int16_t>(x - width / 2), y, width, value, font, color,
             TextHorizontalAlign::Center);
}

void Renderer::textWithin(int16_t left, int16_t y, int16_t width,
                          const String& value, uint8_t font, uint16_t color,
                          TextHorizontalAlign align, int16_t offsetX) {
  const uint16_t previousPadding = tft.getTextPadding();
  const int16_t safeWidth = width > 0 ? width : 0;
  const int16_t right = static_cast<int16_t>(left + safeWidth);
  int16_t x = left;
  uint8_t datum = TL_DATUM;

  if (align == TextHorizontalAlign::Center) {
    x = static_cast<int16_t>(left + safeWidth / 2);
    datum = TC_DATUM;
  } else if (align == TextHorizontalAlign::Right) {
    x = right;
    datum = TR_DATUM;
  }

  tft.setTextDatum(datum);
  tft.setTextWrap(false, false);
  tft.setTextColor(color, backgroundColorValue);
  tft.setTextPadding(safeWidth > 0 ? static_cast<uint16_t>(safeWidth) : 0);
  tft.drawString(
      value, clampCoordinate(static_cast<int32_t>(x) + offsetX, left, right), y,
      font);
  tft.setTextPadding(previousPadding);
}

void Renderer::fillRect(int16_t left, int16_t top, int16_t width,
                        int16_t height) {
  fillRect(left, top, width, height, backgroundColorValue);
}

void Renderer::fillRect(int16_t left, int16_t top, int16_t width,
                        int16_t height, uint16_t color) {
  if (width <= 0 || height <= 0) return;
  tft.fillRect(left, top, width, height, color);
}

int16_t Renderer::textWidth(const String& value, uint8_t font) {
  return static_cast<int16_t>(tft.textWidth(value, font));
}

void Renderer::textAt(int16_t x, int16_t y, const String& value, uint8_t font,
                      uint16_t color) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextWrap(false, false);
  tft.setTextColor(color, backgroundColorValue);
  tft.setTextPadding(0);
  tft.drawString(value, x, y, font);
}

void Renderer::textLeft(int16_t x, int16_t y, const String& value, uint8_t font,
                        uint16_t color) {
  const uint16_t previousPadding = tft.getTextPadding();
  const int16_t paddingWidth = tft.width() - x;

  tft.setTextDatum(TL_DATUM);
  tft.setTextWrap(false, false);
  tft.setTextColor(color, backgroundColorValue);
  if (paddingWidth > 0) {
    tft.setTextPadding(static_cast<uint16_t>(paddingWidth));
  }
  tft.drawString(value, x, y, font);
  tft.setTextPadding(previousPadding);
}

void Renderer::drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
  if (r <= 0) return;
  tft.drawCircle(x, y, r, color);
}

void Renderer::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
  if (r <= 0) return;
  tft.fillCircle(x, y, r, color);
}

void Renderer::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                        uint16_t color) {
  tft.drawLine(x0, y0, x1, y1, color);
}

void Renderer::drawThickLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                             uint8_t thickness, uint16_t color) {
  const int16_t dx = static_cast<int16_t>(x1 - x0);
  const int16_t dy = static_cast<int16_t>(y1 - y0);
  const int16_t half = static_cast<int16_t>(thickness / 2);

  tft.drawLine(x0, y0, x1, y1, color);

  if (thickness <= 1) return;

  if (abs(dx) >= abs(dy)) {
    for (int16_t i = 1; i <= half; i++) {
      tft.drawLine(x0, y0 + i, x1, y1 + i, color);
      tft.drawLine(x0, y0 - i, x1, y1 - i, color);
    }
  } else {
    for (int16_t i = 1; i <= half; i++) {
      tft.drawLine(x0 + i, y0, x1 + i, y1, color);
      tft.drawLine(x0 - i, y0, x1 - i, y1, color);
    }
  }
}