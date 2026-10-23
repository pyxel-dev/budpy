#include "ConfigStore.h"

#include "AppConfigParser.h"

#include <LittleFS.h>

static const char* CONFIG_PATH = "/budpy-config.json";
static AppConfig validationConfig;

namespace {
bool validateAppConfig(JsonVariantConst config, String& error) {
  const bool valid = parseAppConfig(config, validationConfig, error);
  // Only the verdict matters: do not keep a second copy of every cell config
  // alive after validation.
  resetAppConfig(validationConfig);
  return valid;
}
} // namespace

bool configStoreBegin() {
  return LittleFS.begin(true);
}

bool loadAppConfig(AppConfig& config, String& error) {
  File file = LittleFS.open(CONFIG_PATH, "r");
  if (!file) {
    error = "No valid configuration";
    return false;
  }

  JsonDocument doc;
  DeserializationError parseError = deserializeJson(doc, file);
  file.close();
  if (parseError) {
    error = parseError.c_str();
    return false;
  }

  return parseAppConfig(doc.as<JsonVariantConst>(), config, error);
}

bool checkStoredAppConfig(String& error) {
  File file = LittleFS.open(CONFIG_PATH, "r");
  if (!file) {
    error = "No valid configuration";
    return false;
  }

  JsonDocument doc;
  DeserializationError parseError = deserializeJson(doc, file);
  file.close();
  if (parseError) {
    error = parseError.c_str();
    return false;
  }

  return validateAppConfig(doc.as<JsonVariantConst>(), error);
}

bool writeStoredAppConfigJson(Print& out, String& error) {
  File file = LittleFS.open(CONFIG_PATH, "r");
  if (!file) {
    error = "No valid configuration";
    return false;
  }

  while (file.available()) {
    const int value = file.read();
    if (value < 0) {
      break;
    }

    // The serial protocol is line based: never forward a raw line break.
    if (value != '\n' && value != '\r') {
      out.write(static_cast<uint8_t>(value));
    }
  }

  file.close();
  return true;
}

bool saveAppConfig(JsonVariantConst config, String& error) {
  if (!validateAppConfig(config, error)) {
    return false;
  }

  File file = LittleFS.open(CONFIG_PATH, "w");
  if (!file) {
    error = "Cannot open config for writing";
    return false;
  }

  const size_t expectedBytes = measureJson(config);
  const size_t bytesWritten = serializeJson(config, file);
  file.close();
  if (bytesWritten != expectedBytes) {
    error = "Could not write complete config";
    return false;
  }

  error = "";
  return true;
}
