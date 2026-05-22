#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

void showStatus(TFT_eSPI& tft, const String& title, const String& message,
                uint16_t color);