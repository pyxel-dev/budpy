#include "TemplatePlugin.h"

#include <ArduinoJson.h>

namespace {
String readLabel(const String& configJson) {
  JsonDocument doc;
  if (deserializeJson(doc, configJson)) {
    return "Hello Budpy";
  }

  const char* label = doc["label"] | "Hello Budpy";
  return String(label);
}

int16_t gridCoordinate(int16_t size, uint8_t position, uint8_t divisions) {
  if (divisions == 0) {
    return 0;
  }

  return static_cast<int16_t>(
    (static_cast<int32_t>(size) * position) / divisions
  );
}
}

void renderTemplatePlugin(PluginRenderContext& context) {
  Renderer& renderer = context.renderer;
  const LayoutCellConfig& cell = context.cell;
  const uint8_t gridCols = context.config.cols;
  const uint8_t gridRows = context.config.rows;

  const int16_t cellLeft = gridCoordinate(renderer.width(), cell.col, gridCols);
  const int16_t cellRight = gridCoordinate(
    renderer.width(),
    cell.col + cell.colSpan,
    gridCols
  );
  const int16_t cellTop = gridCoordinate(renderer.height(), cell.row, gridRows);
  const int16_t cellBottom = gridCoordinate(
    renderer.height(),
    cell.row + cell.rowSpan,
    gridRows
  );
  const int16_t cellWidth = cellRight - cellLeft;
  const int16_t cellHeight = cellBottom - cellTop;
  const int16_t centerX = cellLeft + cellWidth / 2;
  const int16_t textY = cellTop + (cellHeight - renderer.fontHeight(2)) / 2;
  const String label = readLabel(cell.configJson);

  renderer.textCenterWithin(centerX, textY, cellWidth, label, 2, TFT_WHITE);
}

bool templatePluginNeedsSecondTicks(const AppConfig&) {
  return false;
}