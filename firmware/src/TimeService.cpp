#include "TimeService.h"

#include <Arduino.h>
#include <WiFi.h>

#include <cstdlib>

namespace {
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t NTP_SYNC_TIMEOUT_MS = 15000;

// Returns the POSIX TZ string for natively supported timezones, or nullptr.
const char* findPosixTimezone(const String& timezone) {
  if (timezone == "Europe/Paris") {
    return "CET-1CEST,M3.5.0/2,M10.5.0/3";
  }

  if (timezone == "UTC" || timezone == "Etc/UTC" || timezone == "Etc/GMT" ||
      timezone == "GMT") {
    return "UTC0";
  }

  if (timezone == "Europe/London") {
    return "GMT0BST,M3.5.0/1,M10.5.0";
  }

  if (timezone == "America/New_York") {
    return "EST5EDT,M3.2.0/2,M11.1.0/2";
  }

  if (timezone == "America/Los_Angeles") {
    return "PST8PDT,M3.2.0/2,M11.1.0/2";
  }

  if (timezone == "Asia/Tokyo") {
    return "JST-9";
  }

  if (timezone == "Australia/Sydney") {
    return "AEST-10AEDT,M10.1.0/2,M4.1.0/3";
  }

  return nullptr;
}

const char* timezoneToPosix(const String& timezone) {
  const char* posixTimezone = findPosixTimezone(timezone);
  return posixTimezone != nullptr ? posixTimezone : "UTC0";
}

bool readTimePartsForPosixTimezone(const char* posixTimezone,
                                   struct tm& timeInfo) {
  const time_t now = time(nullptr);
  if (now < 1609459200) {
    return false;
  }

  // localtime_r reads the process-wide TZ value, so restore it immediately.
  const char* previousTimezone = getenv("TZ");
  const String previousTimezoneValue = previousTimezone ? previousTimezone : "";

  setenv("TZ", posixTimezone, 1);
  tzset();
  localtime_r(&now, &timeInfo);

  if (previousTimezone) {
    setenv("TZ", previousTimezoneValue.c_str(), 1);
  } else {
    unsetenv("TZ");
  }
  tzset();

  return true;
}
} // namespace

bool connectWifiAndSyncTime(const AppConfig& config, String& error) {
  error = "";

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(config.ssid.c_str(), config.password.c_str());

  const uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - wifiStart < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    error = "WiFi connection failed";
    return false;
  }

  configTzTime(timezoneToPosix(config.timezone), "pool.ntp.org",
               "time.nist.gov");

  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, NTP_SYNC_TIMEOUT_MS)) {
    error = "NTP synchronization failed";
    return false;
  }

  return true;
}

bool getLocalTimeParts(struct tm& timeInfo) {
  return getLocalTime(&timeInfo, 250);
}

bool isNativelySupportedTimezone(const String& timezone) {
  return findPosixTimezone(timezone) != nullptr;
}

bool getTimePartsForTimezone(const String& timezone, struct tm& timeInfo) {
  return readTimePartsForPosixTimezone(timezoneToPosix(timezone), timeInfo);
}

bool getTimePartsForUtcOffsetMinutes(int16_t offsetMinutes,
                                     struct tm& timeInfo) {
  const time_t now = time(nullptr);
  if (now < 1609459200) {
    return false;
  }

  const time_t shifted = now + static_cast<time_t>(offsetMinutes) * 60;
  gmtime_r(&shifted, &timeInfo);
  return true;
}