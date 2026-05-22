#pragma once

#include "PluginRuntime.h"

void renderClockPlugin(PluginRenderContext& context);

bool clockPluginNeedsSecondTicks(const AppConfig& config);

void clockPluginHandleTouch(PluginTouchContext& context);