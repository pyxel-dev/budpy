#include "ClockPlugin.h"

#include <cmath>
#include <cstdio>

namespace {

static bool s_analogMode[MAX_LAYOUT_CELLS] = {};
static bool s_analogModeInitialized[MAX_LAYOUT_CELLS] = {};
static uint32_t s_configSignatures[MAX_LAYOUT_CELLS] = {};

struct AnalogRenderCache {
  bool rendered = false;
  int16_t cellLeft = 0;
  int16_t cellTop = 0;
  int16_t cellWidth = 0;
  int16_t cellHeight = 0;
  int hour = -1;
  int minute = -1;
  uint16_t faceColor = 0;
  uint16_t rimColor = 0;
  uint16_t tickColor = 0;
  uint16_t hourHandColor = 0;
  uint16_t minuteHandColor = 0;
  uint16_t centerColor = 0;
};

static AnalogRenderCache s_analogRenderCache[MAX_LAYOUT_CELLS] = {};

enum class DateOrder : uint8_t {
  DayMonthYear,
  MonthDayYear,
  YearMonthDay,
};

struct LocaleMonthNames {
  const char* locale;
  const char* const* months;
  DateOrder dateOrder;
};

static const char* const frenchMonths[] = {
  "janv.", "fevr.", "mars",  "avr.", "mai",  "juin",
  "juil.", "aout",  "sept.", "oct.", "nov.", "dec.",
};
static const char* const englishMonths[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};
static const char* const afrikaansMonths[] = {
  "Jan", "Feb", "Mrt", "Apr", "Mei", "Jun",
  "Jul", "Aug", "Sep", "Okt", "Nov", "Des",
};
static const char* const albanianMonths[] = {
  "Jan", "Shk", "Mar", "Pri", "Maj", "Qer",
  "Kor", "Gush", "Sht", "Tet", "Nen", "Dhj",
};
static const char* const arabicMonths[] = {
  "Yan", "Feb", "Mar", "Abr", "May", "Yun",
  "Yul", "Agh", "Sib", "Okt", "Nuf", "Dis",
};
static const char* const azerbaijaniMonths[] = {
  "Yan", "Fev", "Mar", "Apr", "May", "Iyn",
  "Iyl", "Avq", "Sen", "Okt", "Noy", "Dek",
};
static const char* const basqueMonths[] = {
  "Urt", "Ots", "Mar", "Api", "Mai", "Eka",
  "Uzt", "Abu", "Ira", "Urr", "Aza", "Abe",
};
static const char* const belarusianMonths[] = {
  "Stu", "Lut", "Sak", "Kra", "Mai", "Chrv",
  "Lip", "Zhn", "Ver", "Kas", "Lis", "Snez",
};
static const char* const bulgarianMonths[] = {
  "Yan", "Fev", "Mar", "Apr", "May", "Yuni",
  "Yuli", "Avg", "Sep", "Okt", "Noe", "Dek",
};
static const char* const catalanMonths[] = {
  "Gen", "Feb", "Mar", "Abr", "Mai", "Jun",
  "Jul", "Ago", "Set", "Oct", "Nov", "Des",
};
static const char* const numericMonths[] = {
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
};
static const char* const croatianMonths[] = {
  "sij", "velj", "ozuj", "tra", "svi", "lip",
  "srp", "kol",  "ruj",  "lis", "stu", "pro",
};
static const char* const czechMonths[] = {
  "led", "uno", "bre", "dub", "kve", "cer",
  "cerv", "srp", "zari", "rij", "lis", "pro",
};
static const char* const danishMonths[] = {
  "jan", "feb", "mar", "apr", "maj", "jun",
  "jul", "aug", "sep", "okt", "nov", "dec",
};
static const char* const dutchMonths[] = {
  "jan", "feb", "mrt", "apr", "mei", "jun",
  "jul", "aug", "sep", "okt", "nov", "dec",
};
static const char* const finnishMonths[] = {
  "tammi", "helmi", "maalis", "huhti", "touko", "kesa",
  "heina", "elo",   "syys",   "loka",  "marras", "joulu",
};
static const char* const galicianMonths[] = {
  "xan", "feb", "mar", "abr", "mai", "xun",
  "xul", "ago", "set", "out", "nov", "dec",
};
static const char* const germanMonths[] = {
  "Jan", "Feb", "Mar", "Apr", "Mai", "Jun",
  "Jul", "Aug", "Sep", "Okt", "Nov", "Dez",
};
static const char* const greekMonths[] = {
  "Ian", "Fev", "Mar", "Apr", "Mai", "Ioun",
  "Ioul", "Avg", "Sep", "Okt", "Noe", "Dek",
};
static const char* const hebrewMonths[] = {
  "Yan", "Feb", "Mar", "Apr", "Mai", "Yun",
  "Yul", "Aug", "Sep", "Okt", "Nov", "Dez",
};
static const char* const hindiMonths[] = {
  "Jan", "Far", "Mar", "Apr", "Mai", "Jun",
  "Jul", "Aga", "Sit", "Akt", "Nav", "Dis",
};
static const char* const hungarianMonths[] = {
  "jan", "feb", "marc", "apr", "maj", "jun",
  "jul", "aug", "szept", "okt", "nov", "dec",
};
static const char* const icelandicMonths[] = {
  "jan", "feb", "mar", "apr", "mai", "jun",
  "jul", "agu", "sep", "okt", "nov", "des",
};
static const char* const indonesianMonths[] = {
  "Jan", "Feb", "Mar", "Apr", "Mei", "Jun",
  "Jul", "Agu", "Sep", "Okt", "Nov", "Des",
};
static const char* const italianMonths[] = {
  "gen", "feb", "mar", "apr", "mag", "giu",
  "lug", "ago", "set", "ott", "nov", "dic",
};
static const char* const kurmanjiMonths[] = {
  "Rib", "Sib", "Adr", "Nis", "Gul", "Pus",
  "Tir", "Teb", "Ilo", "Cot", "Ser", "Ber",
};
static const char* const latvianMonths[] = {
  "Jan", "Feb", "Mar", "Apr", "Mai", "Jun",
  "Jul", "Aug", "Sep", "Okt", "Nov", "Dec",
};
static const char* const lithuanianMonths[] = {
  "Sau", "Vas", "Kov", "Bal", "Geg", "Bir",
  "Lie", "Rgp", "Rgs", "Spa", "Lap", "Grd",
};
static const char* const macedonianMonths[] = {
  "Jan", "Fev", "Mar", "Apr", "Maj", "Jun",
  "Jul", "Avg", "Sep", "Okt", "Noe", "Dek",
};
static const char* const norwegianMonths[] = {
  "jan", "feb", "mar", "apr", "mai", "jun",
  "jul", "aug", "sep", "okt", "nov", "des",
};
static const char* const polishMonths[] = {
  "sty", "lut", "mar", "kwi", "maj", "cze",
  "lip", "sie", "wrz", "paz", "lis", "gru",
};
static const char* const portugueseMonths[] = {
  "jan", "fev", "mar", "abr", "mai", "jun",
  "jul", "ago", "set", "out", "nov", "dez",
};
static const char* const romanianMonths[] = {
  "ian", "feb", "mar", "apr", "mai", "iun",
  "iul", "aug", "sep", "oct", "nov", "dec",
};
static const char* const russianMonths[] = {
  "yanv", "fev", "mar", "apr", "mai", "iyun",
  "iyul", "avg", "sen", "okt", "noy", "dek",
};
static const char* const southSlavicMonths[] = {
  "jan", "feb", "mar", "apr", "maj", "jun",
  "jul", "avg", "sep", "okt", "nov", "dec",
};
static const char* const spanishMonths[] = {
  "ene", "feb", "mar", "abr", "may", "jun",
  "jul", "ago", "sep", "oct", "nov", "dic",
};
static const char* const swedishMonths[] = {
  "jan", "feb", "mar", "apr", "maj", "jun",
  "jul", "aug", "sep", "okt", "nov", "dec",
};
static const char* const turkishMonths[] = {
  "Oca", "Sub", "Mar", "Nis", "May", "Haz",
  "Tem", "Agu", "Eyl", "Eki", "Kas", "Ara",
};
static const char* const ukrainianMonths[] = {
  "sich", "lyut", "ber", "kvit", "trav", "cherv",
  "lyp",  "serp", "ver", "zhovt", "lyst", "grud",
};
static const char* const vietnameseMonths[] = {
  "Th1", "Th2", "Th3", "Th4", "Th5", "Th6",
  "Th7", "Th8", "Th9", "Th10", "Th11", "Th12",
};
static const char* const zuluMonths[] = {
  "Jan", "Feb", "Mas", "Eph", "Mey", "Jun",
  "Jul", "Aga", "Sep", "Okt", "Nov", "Dis",
};

const LocaleMonthNames* localeMonthNamesFor(const String& locale) {
  static const LocaleMonthNames locales[] = {
    {"fr-FR", frenchMonths, DateOrder::DayMonthYear},
    {"en-US", englishMonths, DateOrder::MonthDayYear},
    {"en-GB", englishMonths, DateOrder::DayMonthYear},
    {"af-ZA", afrikaansMonths, DateOrder::DayMonthYear},
    {"sq-AL", albanianMonths, DateOrder::DayMonthYear},
    {"ar-SA", arabicMonths, DateOrder::DayMonthYear},
    {"az-AZ", azerbaijaniMonths, DateOrder::DayMonthYear},
    {"eu-ES", basqueMonths, DateOrder::DayMonthYear},
    {"be-BY", belarusianMonths, DateOrder::DayMonthYear},
    {"bg-BG", bulgarianMonths, DateOrder::DayMonthYear},
    {"ca-ES", catalanMonths, DateOrder::DayMonthYear},
    {"zh-CN", numericMonths, DateOrder::YearMonthDay},
    {"zh-TW", numericMonths, DateOrder::YearMonthDay},
    {"hr-HR", croatianMonths, DateOrder::DayMonthYear},
    {"cs-CZ", czechMonths, DateOrder::DayMonthYear},
    {"da-DK", danishMonths, DateOrder::DayMonthYear},
    {"nl-NL", dutchMonths, DateOrder::DayMonthYear},
    {"fi-FI", finnishMonths, DateOrder::DayMonthYear},
    {"gl-ES", galicianMonths, DateOrder::DayMonthYear},
    {"de-DE", germanMonths, DateOrder::DayMonthYear},
    {"el-GR", greekMonths, DateOrder::DayMonthYear},
    {"he-IL", hebrewMonths, DateOrder::DayMonthYear},
    {"hi-IN", hindiMonths, DateOrder::DayMonthYear},
    {"hu-HU", hungarianMonths, DateOrder::YearMonthDay},
    {"is-IS", icelandicMonths, DateOrder::DayMonthYear},
    {"id-ID", indonesianMonths, DateOrder::DayMonthYear},
    {"it-IT", italianMonths, DateOrder::DayMonthYear},
    {"ja-JP", numericMonths, DateOrder::YearMonthDay},
    {"ko-KR", numericMonths, DateOrder::YearMonthDay},
    {"ku-TR", kurmanjiMonths, DateOrder::DayMonthYear},
    {"lv-LV", latvianMonths, DateOrder::DayMonthYear},
    {"lt-LT", lithuanianMonths, DateOrder::DayMonthYear},
    {"mk-MK", macedonianMonths, DateOrder::DayMonthYear},
    {"no-NO", norwegianMonths, DateOrder::DayMonthYear},
    {"fa-IR", englishMonths, DateOrder::DayMonthYear},
    {"pl-PL", polishMonths, DateOrder::DayMonthYear},
    {"pt-PT", portugueseMonths, DateOrder::DayMonthYear},
    {"pt-BR", portugueseMonths, DateOrder::DayMonthYear},
    {"ro-RO", romanianMonths, DateOrder::DayMonthYear},
    {"ru-RU", russianMonths, DateOrder::DayMonthYear},
    {"sr-RS", southSlavicMonths, DateOrder::DayMonthYear},
    {"sk-SK", southSlavicMonths, DateOrder::DayMonthYear},
    {"sl-SI", southSlavicMonths, DateOrder::DayMonthYear},
    {"es-ES", spanishMonths, DateOrder::DayMonthYear},
    {"sv-SE", swedishMonths, DateOrder::DayMonthYear},
    {"th-TH", englishMonths, DateOrder::DayMonthYear},
    {"tr-TR", turkishMonths, DateOrder::DayMonthYear},
    {"uk-UA", ukrainianMonths, DateOrder::DayMonthYear},
    {"vi-VN", vietnameseMonths, DateOrder::DayMonthYear},
    {"zu-ZA", zuluMonths, DateOrder::DayMonthYear},
  };

  for (const LocaleMonthNames& entry : locales) {
  if (locale == entry.locale) return &entry;
  }

  return nullptr;
}

const LocaleMonthNames fallbackLocaleMonthNames() {
  return {"en-GB", englishMonths, DateOrder::DayMonthYear};
}

String formatTimeText(const ClockPluginConfig& config,
                      const struct tm& timeInfo) {
  char buffer[16];
  if (config.hourCycle == "h12") {
    int displayHour = timeInfo.tm_hour % 12;
    if (displayHour == 0) {
      displayHour = 12;
    }

    const char* suffix = timeInfo.tm_hour >= 12 ? "PM" : "AM";
    if (config.showSeconds) {
      std::snprintf(buffer, sizeof(buffer), "%d:%02d:%02d %s", displayHour,
                    timeInfo.tm_min, timeInfo.tm_sec, suffix);
    } else {
      std::snprintf(buffer, sizeof(buffer), "%d:%02d %s", displayHour,
                    timeInfo.tm_min, suffix);
    }

    return String(buffer);
  }

  if (config.showSeconds) {
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", timeInfo.tm_hour,
                  timeInfo.tm_min, timeInfo.tm_sec);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", timeInfo.tm_hour,
                  timeInfo.tm_min);
  }

  return String(buffer);
}

String formatDateText(const ClockPluginConfig& config,
                      const struct tm& timeInfo) {
  char buffer[24];
  const int day = timeInfo.tm_mday;
  const int year = timeInfo.tm_year + 1900;
  const int safeMonth = timeInfo.tm_mon >= 0 && timeInfo.tm_mon < 12
                            ? timeInfo.tm_mon
                            : 0;
  const LocaleMonthNames fallback = fallbackLocaleMonthNames();
  const LocaleMonthNames* localeMonths = localeMonthNamesFor(config.locale);
  if (localeMonths == nullptr) localeMonths = &fallback;
  const char* month = localeMonths->months[safeMonth];

  if (localeMonths->dateOrder == DateOrder::MonthDayYear) {
    std::snprintf(buffer, sizeof(buffer), "%s %d, %d", month, day, year);
    return String(buffer);
  }

  if (localeMonths->dateOrder == DateOrder::YearMonthDay) {
    std::snprintf(buffer, sizeof(buffer), "%d %s %d", year, month, day);
    return String(buffer);
  }

  std::snprintf(buffer, sizeof(buffer), "%d %s %d", day, month, year);
  return String(buffer);
}

int16_t gridCoordinate(int16_t size, uint8_t position, uint8_t divisions) {
  if (divisions == 0) {
    return 0;
  }

  return static_cast<int16_t>((static_cast<int32_t>(size) * position) /
                              divisions);
}

int16_t clampCoordinate(int32_t value, int16_t minValue, int16_t maxValue) {
  if (value < minValue) {
    return minValue;
  }

  if (value > maxValue) {
    return maxValue;
  }

  return static_cast<int16_t>(value);
}

TextHorizontalAlign horizontalTextAlign(const String& align) {
  if (align == "left") {
    return TextHorizontalAlign::Left;
  }

  if (align == "right") {
    return TextHorizontalAlign::Right;
  }

  return TextHorizontalAlign::Center;
}

int16_t verticalContentOffset(const String& align, int16_t cellHeight,
                              int16_t contentHeight) {
  if (cellHeight <= contentHeight) {
    return 0;
  }

  if (align == "top") {
    return 0;
  }

  if (align == "bottom") {
    return static_cast<int16_t>(cellHeight - contentHeight);
  }

  return static_cast<int16_t>((cellHeight - contentHeight) / 2);
}

uint32_t configSignature(const String& configJson) {
  uint32_t signature = 2166136261UL;
  for (size_t index = 0; index < configJson.length(); index++) {
    signature ^= static_cast<uint8_t>(configJson[index]);
    signature *= 16777619UL;
  }

  return signature == 0 ? 1 : signature;
}

bool analogModeForCell(const PluginRenderContext& context) {
  const uint8_t index = context.cellIndex;
  if (index >= MAX_LAYOUT_CELLS) {
    return context.cell.clock.defaultAnalogMode;
  }

  const uint32_t signature = configSignature(context.cell.configJson);
  if (!s_analogModeInitialized[index] ||
      s_configSignatures[index] != signature) {
    s_analogMode[index] = context.cell.clock.defaultAnalogMode;
    s_analogModeInitialized[index] = true;
    s_configSignatures[index] = signature;
    s_analogRenderCache[index].rendered = false;
  }

  return s_analogMode[index];
}

void renderAnalogClock(Renderer& renderer, const LayoutCellConfig& cell,
                       const struct tm& timeInfo, int16_t cellLeft,
                       int16_t cellTop, int16_t cellWidth, int16_t cellHeight,
                       bool clearCell) {
  const int16_t cx = static_cast<int16_t>(cellLeft + cellWidth / 2);
  const int16_t cy = static_cast<int16_t>(cellTop + cellHeight / 2);
  const int16_t minDim = cellWidth < cellHeight ? cellWidth : cellHeight;
  const int16_t radius = static_cast<int16_t>(minDim / 2 - 4);

  if (radius <= 8) return;

  const int16_t hourHandLen = static_cast<int16_t>(radius * 55 / 100);
  const int16_t minHandLen = static_cast<int16_t>(radius * 82 / 100);

  if (clearCell) {
    renderer.fillRect(cellLeft, cellTop, cellWidth, cellHeight);
  }
  renderer.fillCircle(cx, cy, radius, cell.clock.analogFaceColor);
  renderer.drawCircle(cx, cy, radius, cell.clock.analogRimColor);
  renderer.drawCircle(cx, cy, static_cast<int16_t>(radius - 1),
                      cell.clock.analogRimColor);

  for (int h = 0; h < 12; h++) {
    const float angle = h * (static_cast<float>(M_PI) / 6.0f);
    const float sa = sinf(angle);
    const float ca = cosf(angle);
    const bool isQuarter = (h % 3 == 0);
    const int16_t outerR = static_cast<int16_t>(radius - 2);
    const int16_t innerR = static_cast<int16_t>(radius - (isQuarter ? 9 : 5));
    const int16_t x0 = static_cast<int16_t>(cx + outerR * sa);
    const int16_t y0 = static_cast<int16_t>(cy - outerR * ca);
    const int16_t x1 = static_cast<int16_t>(cx + innerR * sa);
    const int16_t y1 = static_cast<int16_t>(cy - innerR * ca);
    renderer.drawLine(x0, y0, x1, y1, cell.clock.analogTickColor);
  }

  const float minAngle = timeInfo.tm_min * (static_cast<float>(M_PI) / 30.0f);
  renderer.drawThickLine(cx, cy,
                         static_cast<int16_t>(cx + minHandLen * sinf(minAngle)),
                         static_cast<int16_t>(cy - minHandLen * cosf(minAngle)),
                         2, cell.clock.analogMinuteHandColor);

  const float hourAngle = (timeInfo.tm_hour % 12 * 60 + timeInfo.tm_min) *
                          (static_cast<float>(M_PI) / 360.0f);
  renderer.drawThickLine(
      cx, cy, static_cast<int16_t>(cx + hourHandLen * sinf(hourAngle)),
      static_cast<int16_t>(cy - hourHandLen * cosf(hourAngle)), 3,
      cell.clock.analogHourHandColor);

  renderer.fillCircle(cx, cy, 3, cell.clock.analogCenterColor);
}

bool analogCacheMatches(const AnalogRenderCache& cache,
                        const LayoutCellConfig& cell, const struct tm& timeInfo,
                        int16_t cellLeft, int16_t cellTop, int16_t cellWidth,
                        int16_t cellHeight) {
  return cache.rendered && cache.cellLeft == cellLeft &&
         cache.cellTop == cellTop && cache.cellWidth == cellWidth &&
         cache.cellHeight == cellHeight && cache.hour == timeInfo.tm_hour &&
         cache.minute == timeInfo.tm_min &&
         cache.faceColor == cell.clock.analogFaceColor &&
         cache.rimColor == cell.clock.analogRimColor &&
         cache.tickColor == cell.clock.analogTickColor &&
         cache.hourHandColor == cell.clock.analogHourHandColor &&
         cache.minuteHandColor == cell.clock.analogMinuteHandColor &&
         cache.centerColor == cell.clock.analogCenterColor;
}

void rememberAnalogRender(AnalogRenderCache& cache,
                          const LayoutCellConfig& cell,
                          const struct tm& timeInfo, int16_t cellLeft,
                          int16_t cellTop, int16_t cellWidth,
                          int16_t cellHeight) {
  cache.rendered = true;
  cache.cellLeft = cellLeft;
  cache.cellTop = cellTop;
  cache.cellWidth = cellWidth;
  cache.cellHeight = cellHeight;
  cache.hour = timeInfo.tm_hour;
  cache.minute = timeInfo.tm_min;
  cache.faceColor = cell.clock.analogFaceColor;
  cache.rimColor = cell.clock.analogRimColor;
  cache.tickColor = cell.clock.analogTickColor;
  cache.hourHandColor = cell.clock.analogHourHandColor;
  cache.minuteHandColor = cell.clock.analogMinuteHandColor;
  cache.centerColor = cell.clock.analogCenterColor;
}

} // namespace

void renderClockPlugin(PluginRenderContext& context) {
  Renderer& renderer = context.renderer;
  const LayoutCellConfig& cell = context.cell;
  const struct tm& timeInfo = context.timeInfo;
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
  const bool analogMode = analogModeForCell(context);

  if (analogMode) {
    AnalogRenderCache& cache = s_analogRenderCache[context.cellIndex];
    if (!context.forceClear &&
        analogCacheMatches(cache, cell, timeInfo, cellLeft, cellTop, cellWidth,
                           cellHeight)) {
      return;
    }

    const bool clearAnalogCell =
        context.forceClear || !cache.rendered || cache.cellLeft != cellLeft ||
        cache.cellTop != cellTop || cache.cellWidth != cellWidth ||
        cache.cellHeight != cellHeight;

    renderAnalogClock(renderer, cell, timeInfo, cellLeft, cellTop, cellWidth,
                      cellHeight, clearAnalogCell);
    rememberAnalogRender(cache, cell, timeInfo, cellLeft, cellTop, cellWidth,
                         cellHeight);
    return;
  }

  if (context.cellIndex < MAX_LAYOUT_CELLS) {
    s_analogRenderCache[context.cellIndex].rendered = false;
  }

  const bool hasTitle = cell.clock.showTitle && cell.clock.title.length() > 0;
  const uint8_t titleFont = cell.clock.titleFont;
  const uint8_t timeFont = cell.clock.timeFont;
  const uint8_t dateFont = cell.clock.dateFont;
  const int16_t titleHeight = hasTitle ? renderer.fontHeight(titleFont) : 0;
  const int16_t titleGap = hasTitle ? 3 : 0;
  const int16_t timeHeight = renderer.fontHeight(timeFont);
  const int16_t dateHeight =
      cell.clock.showDate ? renderer.fontHeight(dateFont) : 0;
  const int16_t lineGap = cell.clock.showDate ? 4 : 0;
  const int16_t contentHeight =
      titleHeight + titleGap + timeHeight + lineGap + dateHeight;
  const int16_t baseOffsetY = verticalContentOffset(cell.clock.verticalAlign,
                                                    cellHeight, contentHeight);
  const int16_t maxStartY =
      contentHeight < cellHeight
          ? static_cast<int16_t>(cellBottom - contentHeight)
          : cellTop;
  const int16_t startY = clampCoordinate(static_cast<int32_t>(cellTop) +
                                             baseOffsetY + cell.clock.offsetY,
                                         cellTop, maxStartY);
  const TextHorizontalAlign titleTextAlign =
      horizontalTextAlign(cell.clock.titleHorizontalAlign);
  const TextHorizontalAlign timeTextAlign =
      horizontalTextAlign(cell.clock.timeHorizontalAlign);
  const TextHorizontalAlign dateTextAlign =
      horizontalTextAlign(cell.clock.dateHorizontalAlign);
  const String timeText = formatTimeText(cell.clock, timeInfo);
  const bool renderDateFirst =
      cell.clock.showDate && cell.clock.dateTimeOrder == "date-time";
  int16_t cursorY = startY;

  if (hasTitle) {
    renderer.textWithin(cellLeft, cursorY, cellWidth, cell.clock.title,
                        titleFont, cell.clock.titleColor, titleTextAlign,
                        cell.clock.offsetX);
    cursorY += titleHeight + titleGap;
  }

  if (renderDateFirst) {
    const String dateText = formatDateText(cell.clock, timeInfo);
    renderer.textWithin(cellLeft, cursorY, cellWidth, dateText, dateFont,
                        cell.clock.dateColor, dateTextAlign,
                        cell.clock.offsetX);
    cursorY += dateHeight + lineGap;
  }

  renderer.textWithin(cellLeft, cursorY, cellWidth, timeText, timeFont,
                      cell.clock.timeColor, timeTextAlign, cell.clock.offsetX);

  if (cell.clock.showDate && !renderDateFirst) {
    const String dateText = formatDateText(cell.clock, timeInfo);
    renderer.textWithin(cellLeft, cursorY + timeHeight + lineGap, cellWidth,
                        dateText, dateFont, cell.clock.dateColor, dateTextAlign,
                        cell.clock.offsetX);
  }
}

bool clockPluginNeedsSecondTicks(const AppConfig& config) {
  for (uint8_t index = 0; index < config.cellCount; index++) {
    const LayoutCellConfig& cell = config.cells[index];
    if (cell.pluginId == "clock") {
      const bool analogMode = s_analogModeInitialized[index]
                                  ? s_analogMode[index]
                                  : cell.clock.defaultAnalogMode;
      if (!analogMode && cell.clock.showSeconds) {
        return true;
      }
    }
  }

  return false;
}

void clockPluginHandleTouch(PluginTouchContext& context) {
  const uint8_t idx = context.cellIndex;
  if (idx < MAX_LAYOUT_CELLS) {
    s_analogModeInitialized[idx] = true;
    s_analogMode[idx] = !s_analogMode[idx];
    s_analogRenderCache[idx].rendered = false;
  }
}