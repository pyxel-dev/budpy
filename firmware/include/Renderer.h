#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

enum class TextHorizontalAlign : uint8_t {
  Left,
  Center,
  Right,
};

class Renderer {
public:
  explicit Renderer(TFT_eSPI& display);

  void setBackgroundColor(uint16_t color);
  uint16_t backgroundColor() const;
  void clear();
  int16_t width();
  int16_t height();
  int16_t fontHeight(uint8_t font);
  void textCenter(int16_t x, int16_t y, const String& value, uint8_t font,
                  uint16_t color);
  void textCenterWithin(int16_t x, int16_t y, int16_t width,
                        const String& value, uint8_t font, uint16_t color);
  void textWithin(int16_t left, int16_t y, int16_t width, const String& value,
                  uint8_t font, uint16_t color, TextHorizontalAlign align,
                  int16_t offsetX = 0);
  void fillRect(int16_t left, int16_t top, int16_t width, int16_t height);
  void fillRect(int16_t left, int16_t top, int16_t width, int16_t height,
                uint16_t color);
  void pushImage(int16_t x, int16_t y, int16_t width, int16_t height,
                 const uint16_t* pixels);
  int16_t textWidth(const String& value, uint8_t font);
  void textAt(int16_t x, int16_t y, const String& value, uint8_t font,
              uint16_t color);
  void textLeft(int16_t x, int16_t y, const String& value, uint8_t font,
                uint16_t color);

  void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
  void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  void drawThickLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                     uint8_t thickness, uint16_t color);

private:
  TFT_eSPI& tft;
  uint16_t backgroundColorValue = TFT_BLACK;
};