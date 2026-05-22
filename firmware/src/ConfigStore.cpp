#include "ConfigStore.h"

#include "AppConfigParser.h"

#include <LittleFS.h>

static const char* CONFIG_PATH = "/budpy-config.json";
static AppConfig validationConfig;

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

  return parseAppConfig(doc, config, error);
}

bool loadAppConfigJson(String& json, String& error) {
  File file = LittleFS.open(CONFIG_PATH, "r");
  if (!file) {
    error = "No valid configuration";
    return false;
  }

  json = file.readString();
  file.close();

  JsonDocument doc;
  DeserializationError parseError = deserializeJson(doc, json);
  if (parseError) {
    error = parseError.c_str();
    return false;
  }

  return parseAppConfig(doc, validationConfig, error);
}

bool saveAppConfigJson(const String& json, String& error) {
  JsonDocument doc;
  DeserializationError parseError = deserializeJson(doc, json);
  if (parseError) {
    error = parseError.c_str();
    return false;
  }

  if (!parseAppConfig(doc, validationConfig, error)) {
    return false;
  }

  File file = LittleFS.open(CONFIG_PATH, "w");
  if (!file) {
    error = "Cannot open config for writing";
    return false;
  }

  const size_t bytesWritten = file.print(json);
  file.close();
  if (bytesWritten != json.length()) {
    error = "Could not write complete config";
    return false;
  }

  error = "";
  return true;
}