#include <Arduino.h>
#include <TFT_eSPI.h>

#include "AppConfig.h"
#include "ConfigStore.h"
#include "PluginRegistry.h"
#include "Renderer.h"
#include "SerialProvisioning.h"
#include "StatusScreen.h"
#include "TimeService.h"
#include "TouchService.h"

namespace {
constexpr uint32_t RENDER_INTERVAL_MS = 1000;
constexpr uint8_t PORTRAIT_ROTATION = 0;
constexpr uint8_t LANDSCAPE_90_ROTATION = 1;
constexpr uint8_t PORTRAIT_180_ROTATION = 2;
constexpr uint8_t LANDSCAPE_270_ROTATION = 3;
constexpr uint8_t BACKLIGHT_PIN = TFT_BL;
constexpr uint8_t LDR_PIN = 34;
constexpr uint16_t LDR_MAX_READING = 4095;
constexpr uint16_t LDR_INITIAL_SPAN = 96;
constexpr uint16_t LDR_MIN_SPAN = 96;
constexpr uint8_t LDR_SMOOTHING_WEIGHT = 3;
constexpr uint16_t LDR_DARK_AMBIENT_DEADBAND = 10;
constexpr uint8_t AUTO_BRIGHTNESS_MIN = 4;
constexpr uint32_t BRIGHTNESS_UPDATE_INTERVAL_MS = 200;
constexpr uint8_t BRIGHTNESS_TARGET_DEADBAND = 5;
constexpr uint8_t BRIGHTNESS_APPLY_DEADBAND = 2;
constexpr uint8_t BRIGHTNESS_MIN_STEP = 2;
constexpr bool LDR_BRIGHT_READING_IS_LOW = true;

static TFT_eSPI tft;
static Renderer* renderer = nullptr;
static AppConfig appConfig;
static bool hasConfig = false;
static bool runtimeReady = false;
static bool renderDirty = true;
static uint32_t lastRenderMs = 0;
static uint32_t lastBrightnessUpdateMs = 0;
static uint8_t currentBacklightBrightness = 255;
static bool backlightBrightnessApplied = false;
static uint8_t targetBacklightBrightness = 255;
static bool automaticBrightnessTargetReady = false;
static uint32_t lastActivityMs = 0;
static bool screenAsleep = false;
static uint8_t screenSleepBrightness = 0;
static bool wakeTouchActive = false;
static uint16_t smoothedLdrReading = 0;
static uint16_t ldrMinReading = 0;
static uint16_t ldrMaxReading = LDR_MAX_READING;
static bool ldrCalibrationReady = false;

static void applyBacklightBrightness(uint8_t brightness) {
  if (backlightBrightnessApplied && currentBacklightBrightness == brightness) {
    return;
  }

  analogWrite(BACKLIGHT_PIN, brightness);
  currentBacklightBrightness = brightness;
  backlightBrightnessApplied = true;
}

static uint8_t brightnessDifference(uint8_t current, uint8_t target) {
  return current > target ? current - target : target - current;
}

static uint8_t brightnessStepForCap(uint8_t maxBrightness) {
  const uint8_t scaledStep = maxBrightness / 32;
  return scaledStep < BRIGHTNESS_MIN_STEP ? BRIGHTNESS_MIN_STEP : scaledStep;
}

static uint8_t moveBrightnessToward(uint8_t current, uint8_t target,
                                    uint8_t maxStep) {
  const uint8_t difference = brightnessDifference(current, target);
  if (difference <= BRIGHTNESS_APPLY_DEADBAND) {
    return current;
  }

  const uint8_t step = difference < maxStep ? difference : maxStep;
  return target > current ? current + step : current - step;
}

static uint8_t readAutomaticBrightness(uint8_t maxBrightness) {
  if (maxBrightness == 0) {
    return 0;
  }

  const uint16_t rawReading =
      static_cast<uint16_t>(constrain(analogRead(LDR_PIN), 0, LDR_MAX_READING));
  if (!ldrCalibrationReady) {
    smoothedLdrReading = rawReading;
    const uint16_t halfSpan = LDR_INITIAL_SPAN / 2;
    ldrMinReading = rawReading > halfSpan ? rawReading - halfSpan : 0;
    const uint32_t maxCandidate = static_cast<uint32_t>(rawReading) + halfSpan;
    ldrMaxReading = maxCandidate > LDR_MAX_READING
                        ? LDR_MAX_READING
                        : static_cast<uint16_t>(maxCandidate);
    ldrCalibrationReady = true;
  } else {
    smoothedLdrReading = static_cast<uint16_t>(
        (static_cast<uint32_t>(smoothedLdrReading) * LDR_SMOOTHING_WEIGHT +
         rawReading) /
        (LDR_SMOOTHING_WEIGHT + 1));
  }

  if (smoothedLdrReading < ldrMinReading) {
    ldrMinReading = smoothedLdrReading;
  }
  if (smoothedLdrReading > ldrMaxReading) {
    ldrMaxReading = smoothedLdrReading;
  }
  if (ldrMaxReading - ldrMinReading < LDR_MIN_SPAN) {
    const uint16_t halfSpan = LDR_MIN_SPAN / 2;
    ldrMinReading =
        smoothedLdrReading > halfSpan ? smoothedLdrReading - halfSpan : 0;
    const uint32_t maxCandidate =
        static_cast<uint32_t>(smoothedLdrReading) + halfSpan;
    ldrMaxReading = maxCandidate > LDR_MAX_READING
                        ? LDR_MAX_READING
                        : static_cast<uint16_t>(maxCandidate);
  }

  const uint16_t readingSpan =
      ldrMaxReading > ldrMinReading ? ldrMaxReading - ldrMinReading : 1;
  const uint16_t clampedReading = static_cast<uint16_t>(
      constrain(smoothedLdrReading, ldrMinReading, ldrMaxReading));
  const uint16_t clampedRawReading = static_cast<uint16_t>(
      constrain(rawReading, ldrMinReading, ldrMaxReading));
  const uint16_t rawAmbientLight = LDR_BRIGHT_READING_IS_LOW
                                       ? ldrMaxReading - clampedRawReading
                                       : clampedRawReading - ldrMinReading;
  const uint16_t measuredAmbientLight = LDR_BRIGHT_READING_IS_LOW
                                            ? ldrMaxReading - clampedReading
                                            : clampedReading - ldrMinReading;
  const uint16_t ambientLight =
      rawAmbientLight <= LDR_DARK_AMBIENT_DEADBAND ? 0 : measuredAmbientLight;
  const uint8_t scaledMinBrightness = maxBrightness / 16;
  const uint8_t minBrightness = scaledMinBrightness < AUTO_BRIGHTNESS_MIN
                                    ? AUTO_BRIGHTNESS_MIN
                                    : scaledMinBrightness;
  const uint8_t effectiveMinBrightness =
      minBrightness > maxBrightness ? maxBrightness : minBrightness;
  const uint16_t brightnessRange = maxBrightness - effectiveMinBrightness;

  return static_cast<uint8_t>(effectiveMinBrightness +
                              (brightnessRange * ambientLight) / readingSpan);
}

static void updateAutomaticBrightness(bool force = false) {
  if (!hasConfig || !appConfig.automaticBrightness) {
    return;
  }

  const uint32_t now = millis();
  if (!force && now - lastBrightnessUpdateMs < BRIGHTNESS_UPDATE_INTERVAL_MS) {
    return;
  }

  lastBrightnessUpdateMs = now;
  const uint8_t measuredBrightness =
      readAutomaticBrightness(appConfig.brightness);
  if (!automaticBrightnessTargetReady ||
      brightnessDifference(measuredBrightness, targetBacklightBrightness) >
          BRIGHTNESS_TARGET_DEADBAND) {
    targetBacklightBrightness = measuredBrightness;
    automaticBrightnessTargetReady = true;
  }

  if (!backlightBrightnessApplied) {
    applyBacklightBrightness(targetBacklightBrightness);
    return;
  }

  const uint8_t nextBrightness = moveBrightnessToward(
      currentBacklightBrightness, targetBacklightBrightness,
      brightnessStepForCap(appConfig.brightness));
  applyBacklightBrightness(nextBrightness);
}

static void applyConfiguredBrightness() {
  lastBrightnessUpdateMs = 0;
  if (hasConfig && appConfig.automaticBrightness) {
    ldrCalibrationReady = false;
    automaticBrightnessTargetReady = false;
    updateAutomaticBrightness(true);
    return;
  }

  targetBacklightBrightness = hasConfig ? appConfig.brightness : 255;
  automaticBrightnessTargetReady = false;
  applyBacklightBrightness(targetBacklightBrightness);
}

static uint8_t rotationForOrientation(const String& orientation) {
  if (orientation == "90") {
    return LANDSCAPE_90_ROTATION;
  }

  if (orientation == "180") {
    return PORTRAIT_180_ROTATION;
  }

  if (orientation == "270") {
    return LANDSCAPE_270_ROTATION;
  }

  return PORTRAIT_ROTATION;
}

static void applyDisplayOrientation(const String& orientation,
                                    uint16_t backgroundColor) {
  const uint8_t rotation = rotationForOrientation(orientation);
  tft.setRotation(rotation);
  tft.fillScreen(backgroundColor);
  touchServiceSetRotation(rotation);
}

static void loadConfigOrShowStatus() {
  String error;
  hasConfig = loadAppConfig(appConfig, error);
  runtimeReady = false;
  renderDirty = true;
  lastActivityMs = millis();
  screenAsleep = false;
  wakeTouchActive = false;

  const uint16_t backgroundColor =
      hasConfig ? appConfig.backgroundColor : TFT_BLACK;
  if (renderer != nullptr) {
    renderer->setBackgroundColor(backgroundColor);
  }

  applyConfiguredBrightness();
  applyDisplayOrientation(hasConfig ? appConfig.orientation : "0",
                          backgroundColor);

  if (hasConfig) {
    showStatus(tft, "Budpy", "WiFi connecting...", TFT_YELLOW);
    if (!connectWifiAndSyncTime(appConfig, error)) {
      showStatus(tft, "Budpy", error, TFT_RED);
      return;
    }

    runtimeReady = true;
    lastRenderMs = 0;
    renderDirty = true;
    showStatus(tft, "Budpy", "Time synchronized", TFT_GREEN);
    return;
  }

  showStatus(tft, "Budpy setup", error, TFT_YELLOW);
}
} // namespace

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(PORTRAIT_ROTATION);
  tft.fillScreen(TFT_BLACK);
  pinMode(LDR_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);
  analogWrite(BACKLIGHT_PIN, 255);
  renderer = new Renderer(tft);
  touchServiceBegin(PORTRAIT_ROTATION);

  if (!configStoreBegin()) {
    hasConfig = false;
    showStatus(tft, "Budpy error", "LittleFS unavailable", TFT_RED);
    return;
  }

  loadConfigOrShowStatus();
}

void loop() {
  bool configChanged = false;
  if (pollSerialProvisioning(configChanged) && configChanged) {
    loadConfigOrShowStatus();
  }

  const uint32_t now = millis();
  const bool touched = touchServiceTouched();

  // Le geste de réveil ne doit pas aussi activer un widget: on supprime la
  // distribution tactile jusqu'à ce que le doigt se relève après un réveil.
  if (wakeTouchActive && !touched) {
    wakeTouchActive = false;
  }

  if (hasConfig && appConfig.screenIdleMinutes > 0) {
    const uint32_t idleTimeoutMs =
        static_cast<uint32_t>(appConfig.screenIdleMinutes) * 60000UL;
    if (touched) {
      lastActivityMs = now;
      if (screenAsleep) {
        screenAsleep = false;
        wakeTouchActive = true;
        applyConfiguredBrightness();
        renderDirty = true;
      }
    } else if (!screenAsleep && now - lastActivityMs >= idleTimeoutMs) {
      screenAsleep = true;
      screenSleepBrightness = appConfig.screenSleepDim
                                  ? appConfig.screenSleepDimBrightness
                                  : 0;
      applyBacklightBrightness(screenSleepBrightness);
    }
  }

  if (!screenAsleep) {
    updateAutomaticBrightness();
  }

  // In "Dim" mode the screen stays visible (just dimmed), so rendering must
  // keep running; only a full sleep (backlight off) suspends it.
  const bool renderingSuspended = screenAsleep && !appConfig.screenSleepDim;

  if (!renderingSuspended) {
    if (runtimeReady && renderer != nullptr &&
        (renderDirty || now - lastRenderMs >= RENDER_INTERVAL_MS)) {
      lastRenderMs = now;
      renderPlugins(*renderer, appConfig, renderDirty);
      renderDirty = false;
    }

    if (runtimeReady && renderer != nullptr && !wakeTouchActive &&
        !screenAsleep) {
      pollPluginTouch(*renderer, appConfig, renderDirty);
    }
  }
}