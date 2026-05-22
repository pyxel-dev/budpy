#pragma once

#include "AppConfig.h"
#include "Renderer.h"

#include <time.h>

struct PluginRenderContext {
  Renderer& renderer;
  const AppConfig& config;
  const LayoutCellConfig& cell;
  uint8_t cellIndex;
  const struct tm& timeInfo;
  bool forceClear;
};

struct PluginTouchContext {
  const AppConfig& config;
  const LayoutCellConfig& cell;
  uint8_t cellIndex;
  void (*showNextPage)(const AppConfig& config);
};

using PluginRenderFunction = void (*)(PluginRenderContext& context);
using PluginNeedsSecondTicksFunction = bool (*)(const AppConfig& config);
using PluginHandleTouchFunction = void (*)(PluginTouchContext& context);

struct PluginRegistration {
  const char* id;
  PluginRenderFunction render;
  PluginNeedsSecondTicksFunction needsSecondTicks;
  PluginHandleTouchFunction handleTouch;
};