#pragma once

#include "AppConfig.h"
#include "PluginRuntime.h"
#include "Renderer.h"

bool isRegisteredPluginId(const String& pluginId);

void renderPlugins(Renderer& renderer, const AppConfig& config,
                   bool forceClear = false);

void pollPluginTouch(Renderer& renderer, const AppConfig& config,
                     bool& renderDirty);