#pragma once

#include "AppConfig.h"

#include <time.h>

bool connectWifiAndSyncTime(const AppConfig& config, String& error);
bool getLocalTimeParts(struct tm& timeInfo);
bool getTimePartsForTimezone(const String& timezone, struct tm& timeInfo);
bool getTimePartsForUtcOffsetMinutes(int16_t offsetMinutes,
                                     struct tm& timeInfo);