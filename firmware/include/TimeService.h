#pragma once

#include "AppConfig.h"

#include <time.h>

bool connectWifiAndSyncTime(const AppConfig& config, String& error);
// Non-blocking follow-up after connectWifiAndSyncTime() failed: periodically
// retries WiFi, starts SNTP once connected, and returns true when the clock is
// set.
bool pollWifiAndTimeRecovery(const AppConfig& config);
bool getLocalTimeParts(struct tm& timeInfo);
bool isNativelySupportedTimezone(const String& timezone);
bool getTimePartsForTimezone(const String& timezone, struct tm& timeInfo);
bool getTimePartsForPosixTimezone(const String& posixTimezone,
                                  struct tm& timeInfo);
bool getTimePartsForUtcOffsetMinutes(int16_t offsetMinutes,
                                     struct tm& timeInfo);