#include "SerialProvisioning.h"

#include "ConfigStore.h"

#include <ArduinoJson.h>
#include <cstring>

namespace {
String readLine() {
  if (!Serial.available()) {
    return "";
  }

  return Serial.readStringUntil('\n');
}

void writeErrorResponse(const String& error) {
  JsonDocument response;
  response["ok"] = false;
  response["error"] = error;
  serializeJson(response, Serial);
  Serial.println();
}

void writeSuccessResponse() {
  JsonDocument response;
  response["ok"] = true;
  response["message"] = "Config saved";
  serializeJson(response, Serial);
  Serial.println();
}

void writeConfigResponse(const String& configJson) {
  JsonDocument configDoc;
  DeserializationError parseError = deserializeJson(configDoc, configJson);
  if (parseError) {
    writeErrorResponse(parseError.c_str());
    return;
  }

  JsonDocument response;
  response["ok"] = true;
  response["message"] = "Config loaded";
  response["config"].set(configDoc.as<JsonVariantConst>());
  serializeJson(response, Serial);
  Serial.println();
}

} // namespace

bool pollSerialProvisioning(bool& configChanged) {
  configChanged = false;

  String line = readLine();
  line.trim();
  if (line.isEmpty()) {
    return false;
  }

  JsonDocument doc;
  DeserializationError parseError = deserializeJson(doc, line);
  if (parseError) {
    writeErrorResponse(parseError.c_str());
    return true;
  }

  const char* type = doc["type"] | "";
  if (std::strcmp(type, "config:get") == 0) {
    String configJson;
    String error;
    if (!loadAppConfigJson(configJson, error)) {
      writeErrorResponse(error);
      return true;
    }

    writeConfigResponse(configJson);
    return true;
  }

  if (std::strcmp(type, "config:set") != 0) {
    writeErrorResponse("Unknown command");
    return true;
  }

  String configJson;
  serializeJson(doc["config"], configJson);

  String error;
  if (!saveAppConfigJson(configJson, error)) {
    writeErrorResponse(error);
    return true;
  }

  configChanged = true;
  writeSuccessResponse();
  return true;
}