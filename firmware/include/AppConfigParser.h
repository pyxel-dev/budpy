#pragma once

#include "AppConfig.h"

#include <ArduinoJson.h>

bool parseAppConfig(JsonVariantConst config, AppConfig& out, String& error);

// Releases everything a parsed config holds (cell configs can carry large
// image payloads) by restoring the defaults.
void resetAppConfig(AppConfig& config);
