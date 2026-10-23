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

// Parses the next command line into doc. The raw line is released on return,
// so only the parsed copy stays alive while the command runs (configs with
// images are tens of KB).
bool readCommand(JsonDocument& doc, DeserializationError& parseError) {
  String line = readLine();
  line.trim();
  if (line.isEmpty()) {
    return false;
  }

  parseError = deserializeJson(doc, line);
  return true;
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

void writeConfigResponse() {
  String error;
  if (!checkStoredAppConfig(error)) {
    writeErrorResponse(error);
    return;
  }

  // Stream the stored file into the response instead of rebuilding it in RAM.
  Serial.print("{\"ok\":true,\"message\":\"Config loaded\",\"config\":");
  writeStoredAppConfigJson(Serial, error);
  Serial.println("}");
}

} // namespace

bool pollSerialProvisioning(bool& configChanged) {
  configChanged = false;

  JsonDocument doc;
  DeserializationError parseError = DeserializationError::Ok;
  if (!readCommand(doc, parseError)) {
    return false;
  }

  if (parseError) {
    writeErrorResponse(parseError.c_str());
    return true;
  }

  const char* type = doc["type"] | "";
  if (std::strcmp(type, "config:get") == 0) {
    writeConfigResponse();
    return true;
  }

  if (std::strcmp(type, "config:set") != 0) {
    writeErrorResponse("Unknown command");
    return true;
  }

  String error;
  if (!saveAppConfig(doc["config"].as<JsonVariantConst>(), error)) {
    writeErrorResponse(error);
    return true;
  }

  configChanged = true;
  writeSuccessResponse();
  return true;
}
