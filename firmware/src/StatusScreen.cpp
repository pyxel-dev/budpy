#include "StatusScreen.h"

void showStatus(TFT_eSPI& tft, const String& title, const String& message,
                uint16_t color) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextWrap(true, true);

  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(title, 12, 16, 4);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(12, 72, 2);
  tft.print(message);
}