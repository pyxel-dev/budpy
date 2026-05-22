#pragma once

#include "AppConfig.h"

#include <ArduinoJson.h>

bool parseAppConfig(const JsonDocument& doc, AppConfig& out, String& error);