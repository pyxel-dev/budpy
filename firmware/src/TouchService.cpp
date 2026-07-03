#include "TouchService.h"

#include <Arduino.h>
#include <SPI.h>

namespace {
// Broches HSPI du contrôleur XPT2046 sur la ESP32-2432S028R (CYD)
constexpr uint8_t TOUCH_CLK_PIN = 25;
constexpr uint8_t TOUCH_MISO_PIN = 39;
constexpr uint8_t TOUCH_MOSI_PIN = 32;
constexpr uint8_t TOUCH_CS_PIN = 33;
constexpr uint8_t TOUCH_IRQ_PIN = 36;

// Valeurs de calibration brutes XPT2046 (0-4095).
// Ajuster si la position tactile est décalée.
constexpr int16_t CAL_X_MIN = 349;
constexpr int16_t CAL_X_MAX = 3859;
constexpr int16_t CAL_Y_MIN = 247;
constexpr int16_t CAL_Y_MAX = 3871;

// Commandes XPT2046 (12-bit, mode différentiel, power-down entre lectures)
constexpr uint8_t CMD_READ_X = 0xD0;
constexpr uint8_t CMD_READ_Y = 0x90;

static SPIClass s_spi(HSPI);
static uint8_t s_rotation = 0;

// Lit une valeur ADC 12-bit sur le canal demandé (CS doit être LOW)
uint16_t readRaw(uint8_t cmd) {
  s_spi.transfer(cmd);
  const uint16_t hi = s_spi.transfer(0x00);
  const uint16_t lo = s_spi.transfer(0x00);
  return static_cast<uint16_t>((hi << 5) | (lo >> 3)) & 0x0FFF;
}
} // namespace

void touchServiceBegin(uint8_t rotation) {
  s_rotation = rotation;
  pinMode(TOUCH_IRQ_PIN, INPUT);
  pinMode(TOUCH_CS_PIN, OUTPUT);
  digitalWrite(TOUCH_CS_PIN, HIGH);
  s_spi.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
}

void touchServiceSetRotation(uint8_t rotation) {
  s_rotation = rotation;
}

bool touchServiceRead(int16_t displayWidth, int16_t displayHeight, int16_t& x,
                      int16_t& y) {
  if (digitalRead(TOUCH_IRQ_PIN) != LOW) {
    return false;
  }

  s_spi.beginTransaction(SPISettings(2500000, MSBFIRST, SPI_MODE0));
  digitalWrite(TOUCH_CS_PIN, LOW);
  delayMicroseconds(10);

  // Moyenne de plusieurs échantillons pour la stabilité
  int32_t sumX = 0;
  int32_t sumY = 0;
  constexpr uint8_t SAMPLES = 4;
  for (uint8_t i = 0; i < SAMPLES; i++) {
    sumX += readRaw(CMD_READ_X);
    sumY += readRaw(CMD_READ_Y);
  }
  s_spi.transfer(0x00); // power down

  digitalWrite(TOUCH_CS_PIN, HIGH);
  s_spi.endTransaction();

  // Vérifier que le doigt est toujours posé
  if (digitalRead(TOUCH_IRQ_PIN) != LOW) {
    return false;
  }

  const int16_t rawX = static_cast<int16_t>(sumX / SAMPLES);
  const int16_t rawY = static_cast<int16_t>(sumY / SAMPLES);

  // Mappage des coordonnées brutes vers l'écran selon la rotation.
  // Sur la CYD, l'axe X tactile est miroir en portrait natif:
  // une valeur rawX élevée correspond au bord gauche de l'écran.
  int16_t mappedX;
  int16_t mappedY;
  switch (s_rotation) {
  case 1: // Paysage (USB à droite)
    mappedX = map(rawY, CAL_Y_MIN, CAL_Y_MAX, 0, displayWidth - 1);
    mappedY = map(rawX, CAL_X_MIN, CAL_X_MAX, 0, displayHeight - 1);
    break;
  case 2: // Portrait retourné
    mappedX = map(rawX, CAL_X_MIN, CAL_X_MAX, 0, displayWidth - 1);
    mappedY = map(rawY, CAL_Y_MAX, CAL_Y_MIN, 0, displayHeight - 1);
    break;
  case 3: // Paysage retourné (USB à gauche)
    mappedX = map(rawY, CAL_Y_MAX, CAL_Y_MIN, 0, displayWidth - 1);
    mappedY = map(rawX, CAL_X_MAX, CAL_X_MIN, 0, displayHeight - 1);
    break;
  default: // 0: Portrait
    mappedX = map(rawX, CAL_X_MAX, CAL_X_MIN, 0, displayWidth - 1);
    mappedY = map(rawY, CAL_Y_MIN, CAL_Y_MAX, 0, displayHeight - 1);
    break;
  }

  x = static_cast<int16_t>(constrain(mappedX, 0, displayWidth - 1));
  y = static_cast<int16_t>(constrain(mappedY, 0, displayHeight - 1));
  return true;
}

bool touchServiceTouched() {
  return digitalRead(TOUCH_IRQ_PIN) == LOW;
}
