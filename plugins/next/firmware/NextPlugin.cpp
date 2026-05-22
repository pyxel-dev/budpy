#include "NextPlugin.h"

namespace {
int16_t gridCoordinate(int16_t size, uint8_t position, uint8_t divisions) {
  if (divisions == 0) {
    return 0;
  }

  return static_cast<int16_t>((static_cast<int32_t>(size) * position) /
                              divisions);
}
} // namespace

void renderNextPlugin(PluginRenderContext& context) {
  Renderer& renderer = context.renderer;
  const LayoutCellConfig& cell = context.cell;
  const uint8_t gridCols = context.config.cols;
  const uint8_t gridRows = context.config.rows;

  const int16_t cellLeft = gridCoordinate(renderer.width(), cell.col, gridCols);
  const int16_t cellRight =
      gridCoordinate(renderer.width(), cell.col + cell.colSpan, gridCols);
  const int16_t cellTop = gridCoordinate(renderer.height(), cell.row, gridRows);
  const int16_t cellBottom =
      gridCoordinate(renderer.height(), cell.row + cell.rowSpan, gridRows);
  const int16_t cellWidth = cellRight - cellLeft;
  const int16_t cellHeight = cellBottom - cellTop;
  const int16_t centerX = static_cast<int16_t>(cellLeft + cellWidth / 2);
  const int16_t textY =
      static_cast<int16_t>(cellTop + (cellHeight - renderer.fontHeight(4)) / 2);

  renderer.textCenterWithin(centerX, textY, cellWidth, ">", 4, TFT_WHITE);
}

void nextPluginHandleTouch(PluginTouchContext& context) {
  if (context.showNextPage) {
    context.showNextPage(context.config);
  }
}