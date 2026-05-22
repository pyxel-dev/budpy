#pragma once

#include "AppConfig.h"

bool configStoreBegin();
bool loadAppConfig(AppConfig& config, String& error);
bool loadAppConfigJson(String& json, String& error);
bool saveAppConfigJson(const String& json, String& error);