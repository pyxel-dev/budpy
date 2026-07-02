#include "PluginRegistry.h"

#include "TimeService.h"
#include "TouchService.h"
#include "generated/PluginRegistrations.h"

namespace {
enum class RenderSurface {
  unknown,
  plugins,
  timeError,
};

static RenderSurface currentSurface = RenderSurface::unknown;
static int lastRenderedHour = -1;
static int lastRenderedMinute = -1;
static int lastRenderedSecond = -1;
static int lastRenderedYearDay = -1;
static uint8_t currentPage = 0;

const PluginRegistration* findPluginRegistration(const String& pluginId) {
  for (size_t index = 0; index < pluginRegistrationCount; index++) {
    const PluginRegistration& registration = pluginRegistrations[index];

    if (pluginId == registration.id) {
      return &registration;
    }
  }

  return nullptr;
}

bool hasPluginWithSecondTicks(const AppConfig& config) {
  for (size_t index = 0; index < pluginRegistrationCount; index++) {
    const PluginRegistration& registration = pluginRegistrations[index];

    if (registration.needsSecondTicks &&
        registration.needsSecondTicks(config)) {
      return true;
    }
  }

  return false;
}

bool shouldRenderPlugins(const AppConfig& config, const struct tm& timeInfo,
                         bool forceClear) {
  if (forceClear || currentSurface != RenderSurface::plugins) {
    return true;
  }

  if (lastRenderedHour != timeInfo.tm_hour ||
      lastRenderedMinute != timeInfo.tm_min ||
      lastRenderedYearDay != timeInfo.tm_yday) {
    return true;
  }

  return hasPluginWithSecondTicks(config) &&
         lastRenderedSecond != timeInfo.tm_sec;
}

void rememberRenderedTime(const struct tm& timeInfo) {
  lastRenderedHour = timeInfo.tm_hour;
  lastRenderedMinute = timeInfo.tm_min;
  lastRenderedSecond = timeInfo.tm_sec;
  lastRenderedYearDay = timeInfo.tm_yday;
}

void resetRenderedTime() {
  lastRenderedHour = -1;
  lastRenderedMinute = -1;
  lastRenderedSecond = -1;
  lastRenderedYearDay = -1;
}

uint8_t pageCountForConfig(const AppConfig& config) {
  return config.pageCount == 0 ? 1 : config.pageCount;
}

bool normalizeCurrentPage(const AppConfig& config) {
  const uint8_t pageCount = pageCountForConfig(config);
  if (currentPage < pageCount) {
    return false;
  }

  currentPage = 0;
  resetRenderedTime();
  return true;
}

void showNextPageForConfig(const AppConfig& config) {
  const uint8_t pageCount = pageCountForConfig(config);
  currentPage = static_cast<uint8_t>((currentPage + 1) % pageCount);
  currentSurface = RenderSurface::unknown;
  resetRenderedTime();
}
} // namespace

bool isRegisteredPluginId(const String& pluginId) {
  return findPluginRegistration(pluginId) != nullptr;
}

void renderPlugins(Renderer& renderer, const AppConfig& config,
                   bool forceClear) {
  struct tm timeInfo;
  if (!getLocalTimeParts(timeInfo)) {
    if (forceClear || currentSurface != RenderSurface::timeError) {
      const int16_t centerX = renderer.width() / 2;
      const int16_t centerY = renderer.height() / 2;

      renderer.clear();
      renderer.textCenter(centerX, centerY - 20, "Time not synchronized", 2,
                          TFT_RED);
      renderer.textCenter(centerX, centerY + 6, "NTP sync required", 2,
                          TFT_LIGHTGREY);
    }

    currentSurface = RenderSurface::timeError;
    resetRenderedTime();
    return;
  }

  const bool pageWasNormalized = normalizeCurrentPage(config);
  const bool clearBeforeRender = forceClear || pageWasNormalized ||
                                 currentSurface != RenderSurface::plugins;
  if (!shouldRenderPlugins(config, timeInfo, clearBeforeRender)) {
    return;
  }

  if (clearBeforeRender) {
    renderer.clear();
  }

  for (uint8_t index = 0; index < config.cellCount; index++) {
    const LayoutCellConfig& cell = config.cells[index];
    if (cell.page != currentPage) {
      continue;
    }

    const PluginRegistration* registration =
        findPluginRegistration(cell.pluginId);
    if (!registration) {
      continue;
    }

    struct tm cellTimeInfo;
    // Named timezones keep DST rules, so only fall back to the fixed UTC
    // offset captured by the web app when the timezone is not known natively.
    const bool useUtcOffset = cell.pluginId == "clock" &&
                              cell.clock.hasTimezoneOffsetMinutes &&
                              !isNativelySupportedTimezone(cell.clock.timezone);
    const bool hasCellTime =
        useUtcOffset
            ? getTimePartsForUtcOffsetMinutes(cell.clock.timezoneOffsetMinutes,
                                              cellTimeInfo)
            : getTimePartsForTimezone(cell.pluginId == "clock"
                                          ? cell.clock.timezone
                                          : config.timezone,
                                      cellTimeInfo);

    if (!hasCellTime) {
      cellTimeInfo = timeInfo;
    }

    PluginRenderContext context = {renderer, config,       cell,
                                   index,    cellTimeInfo, clearBeforeRender};
    registration->render(context);
  }

  currentSurface = RenderSurface::plugins;
  rememberRenderedTime(timeInfo);
}

namespace {
int16_t gridCoordTouch(int16_t size, uint8_t position, uint8_t divisions) {
  if (divisions == 0) {
    return 0;
  }
  return static_cast<int16_t>((static_cast<int32_t>(size) * position) /
                              divisions);
}

static bool s_wasTouched = false;
} // namespace

void pollPluginTouch(Renderer& renderer, const AppConfig& config,
                     bool& renderDirty) {
  int16_t tx = 0;
  int16_t ty = 0;
  const bool isTouched =
      touchServiceRead(renderer.width(), renderer.height(), tx, ty);

  if (!isTouched) {
    s_wasTouched = false;
    return;
  }

  if (s_wasTouched) {
    return;
  }

  s_wasTouched = true;

  const uint8_t gridCols = config.cols;
  const uint8_t gridRows = config.rows;
  const int16_t dispWidth = renderer.width();
  const int16_t dispHeight = renderer.height();
  normalizeCurrentPage(config);

  for (uint8_t index = 0; index < config.cellCount; index++) {
    const LayoutCellConfig& cell = config.cells[index];
    if (cell.page != currentPage) {
      continue;
    }

    const int16_t cellLeft = gridCoordTouch(dispWidth, cell.col, gridCols);
    const int16_t cellRight =
        gridCoordTouch(dispWidth, cell.col + cell.colSpan, gridCols);
    const int16_t cellTop = gridCoordTouch(dispHeight, cell.row, gridRows);
    const int16_t cellBottom =
        gridCoordTouch(dispHeight, cell.row + cell.rowSpan, gridRows);

    if (tx >= cellLeft && tx < cellRight && ty >= cellTop && ty < cellBottom) {
      const PluginRegistration* registration =
          findPluginRegistration(cell.pluginId);
      if (registration && registration->handleTouch) {
        PluginTouchContext touchCtx = {config, cell, index,
                                       showNextPageForConfig};
        registration->handleTouch(touchCtx);
        renderDirty = true;
      }
      break;
    }
  }
}