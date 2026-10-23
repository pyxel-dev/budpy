#pragma once

#include "AppConfig.h"

#include <ArduinoJson.h>

bool configStoreBegin();
bool loadAppConfig(AppConfig& config, String& error);
// Checks that the stored config is still valid for this firmware.
bool checkStoredAppConfig(String& error);
// Streams the stored config JSON as-is, without building a copy in RAM.
bool writeStoredAppConfigJson(Print& out, String& error);
bool saveAppConfig(JsonVariantConst config, String& error);
