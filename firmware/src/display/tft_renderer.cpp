#include "display/tft_renderer.h"

#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "assets/provider_icons.h"
#include "core/text_format.h"
#include "display/indexed_canvas.h"

namespace {
using namespace TftColor;
constexpr float Pi = 3.14159265358979323846f;

struct Provider {
    const char *name;
    uint16_t bright;
    uint16_t dim;
    const uint8_t *icon;
};
// Indexed by ProviderId.
constexpr Provider Providers[PROVIDER_COUNT] = {
    {"Codex", Mint, MintDim, ProviderIcons::Codex},
    {"Claude", Orange, OrangeDim, ProviderIcons::Claude},
    {"Cursor", Purple, PurpleDim, ProviderIcons::Cursor},
};

const Provider &style(ProviderId id) {
    return Providers[static_cast<uint8_t>(id)];
}

// Cursor bills by plan cycle instead of five-hour and seven-day windows.
bool billsByCycle(ProviderId id) {
    return id == ProviderId::Cursor;
}

UsageWindow windowFor(const ProviderUsage &entry, bool secondary) {
    const UsageView &view = entry.view;
    if (!view.hasData) return {};
    if (billsByCycle(entry.id)) return secondary ? view.api : view.included;
    return secondary ? view.sevenDay : view.fiveHour;
}

const char *periodFor(ProviderId id, bool secondary) {
    if (billsByCycle(id)) return secondary ? "API" : "PLANO";
    return secondary ? "7D" : "5H";
}

uint16_t textWidth(Adafruit_GFX &c, const char *s, const GFXfont *font) {
    c.setFont(font);
    c.setTextSize(1);
    int16_t x, y;
    uint16_t w, h;
    c.getTextBounds(s, 0, 0, &x, &y, &w, &h);
    return w;
}

void text(Adafruit_GFX &c, const char *s, int x, int y, uint16_t color,
          const GFXfont *font = nullptr, bool centered = false) {
    c.setFont(font);
    c.setTextSize(1);
    c.setTextWrap(false);
    int16_t left, top;
    uint16_t w, h;
    c.getTextBounds(s, 0, 0, &left, &top, &w, &h);
    c.setTextColor(color);
    c.setCursor(x - left - (centered ? w / 2 : 0), y - top);
    c.print(s);
}

void chamfer(Adafruit_GFX &c, int x, int y, int w, int h, int cut, uint16_t color) {
    c.drawFastHLine(x + cut, y, w - 2 * cut, color);
    c.drawLine(x + w - cut - 1, y, x + w - 1, y + cut, color);
    c.drawFastVLine(x + w - 1, y + cut, h - 2 * cut, color);
    c.drawLine(x + w - 1, y + h - cut - 1, x + w - cut - 1, y + h - 1, color);
    c.drawFastHLine(x + cut, y + h - 1, w - 2 * cut, color);
    c.drawLine(x + cut, y + h - 1, x, y + h - cut - 1, color);
    c.drawFastVLine(x, y + cut, h - 2 * cut, color);
    c.drawLine(x, y + cut, x + cut, y, color);
}

void panel(Adafruit_GFX &c, int x, int y, int w, int h, const Provider &p) {
    c.fillRect(x + 1, y + 7, w - 2, h - 14, Surface);
    c.fillRect(x + 7, y + 1, w - 14, h - 2, Surface);
    chamfer(c, x - 1, y - 1, w + 2, h + 2, 8, Border);
    chamfer(c, x, y, w, h, 7, p.dim);
    c.drawLine(x + 1, y + 7, x + 7, y + 1, p.bright);
    c.drawLine(x + 2, y + 8, x + 8, y + 2, p.bright);
    c.drawFastHLine(x + 7, y + 1, 24, p.bright);
    c.drawFastVLine(x + 1, y + 7, 20, p.bright);
    c.drawLine(x + w - 8, y + h - 2, x + w - 2, y + h - 8, p.bright);
}

void icon(Adafruit_GFX &c, int x, int y, const Provider &p) {
    for (int row = 0; row < ProviderIcons::Size; ++row) {
        for (int col = 0; col < ProviderIcons::Size; ++col) {
            const int index = row * ProviderIcons::Size + col;
            const uint8_t alpha = (p.icon[index / 4] >> (6 - (index % 4) * 2)) & 3;
            if (alpha != 0) c.drawPixel(x + col, y + row, alpha == 1 ? p.dim : p.bright);
        }
    }
}

void bar(Adafruit_GFX &c, int x, int y, int width, const UsageWindow &window, const Provider &p) {
    c.fillRoundRect(x, y, width, 5, 2, Track);
    const int fill = static_cast<int>(std::round(width * usageFillFraction(window)));
    if (fill > 0) {
        c.fillRect(x, y + 1, fill, 3, p.bright);
        c.drawFastHLine(x + 1, y, std::max(0, fill - 2), p.dim);
        c.drawFastHLine(x + 1, y + 4, std::max(0, fill - 2), p.dim);
    }
}

void ringBand(Adafruit_GFX &c, int cx, int cy, int inner, int outer, float start, float end, uint16_t color) {
    for (float a = start; a < end; a += 2) {
        const float b = std::min(a + 2, end);
        const float aRad = a * Pi / 180, bRad = b * Pi / 180;
        const int ax = cx + std::lround(std::cos(aRad) * outer);
        const int ay = cy + std::lround(std::sin(aRad) * outer);
        const int bx = cx + std::lround(std::cos(bRad) * outer);
        const int by = cy + std::lround(std::sin(bRad) * outer);
        const int ix = cx + std::lround(std::cos(aRad) * inner);
        const int iy = cy + std::lround(std::sin(aRad) * inner);
        const int jx = cx + std::lround(std::cos(bRad) * inner);
        const int jy = cy + std::lround(std::sin(bRad) * inner);
        c.fillTriangle(ax, ay, bx, by, ix, iy, color);
        c.fillTriangle(bx, by, jx, jy, ix, iy, color);
    }
}

void gauge(Adafruit_GFX &c, const UsageWindow &window, const Provider &p) {
    constexpr int cx = 100, cy = 135;
    const float fillEnd = 135 + 270 * usageFillFraction(window);
    ringBand(c, cx, cy, 65, 65, 135, 405, Border);
    for (int segment = 0; segment < 36; ++segment) {
        const float start = 135 + segment * 7.5f, end = start + 5.5f;
        ringBand(c, cx, cy, 53, 62, start, end, Track);
        if (fillEnd > start) {
            ringBand(c, cx, cy, 53, 62, start, std::min(end, fillEnd), p.bright);
            ringBand(c, cx, cy, 61, 62, start, std::min(end, fillEnd), p.dim);
        }
    }
    for (int dot = 0; dot <= 40; ++dot) {
        const float degrees = 135 + dot * 6.75f;
        const float angle = degrees * Pi / 180;
        c.drawPixel(cx + std::lround(std::cos(angle) * 47), cy + std::lround(std::sin(angle) * 47),
                    degrees <= fillEnd ? p.bright : Border);
    }
}

void percentage(Adafruit_GFX &c, const UsageWindow &w, int cx, int top, const Provider &p, bool large) {
    if (!w.present || !w.known) {
        text(c, "--%", cx, top + (large ? 8 : 4), Muted, &FreeSansBold18pt7b, true);
        return;
    }
    char number[8];
    snprintf(number, sizeof number, "%d", static_cast<int>(w.usedPercentage + .5f));
    const GFXfont *font = large ? &FreeSansBold24pt7b : &FreeSansBold18pt7b;
    if (large && textWidth(c, number, font) > 76) font = &FreeSansBold18pt7b;
    if (!large && textWidth(c, number, font) > 47) font = &FreeSansBold12pt7b;
    const int numberWidth = textWidth(c, number, font);
    const int suffixWidth = textWidth(c, "%", &FreeSansBold9pt7b);
    const int left = cx - (numberWidth + suffixWidth + 3) / 2;
    text(c, number, left, top, w.known ? p.bright : Muted, font);
    const int suffixTop = font == &FreeSansBold24pt7b ? 20 : font == &FreeSansBold18pt7b ? 12 : 4;
    text(c, "%", left + numberWidth + 3, top + suffixTop,
         w.known ? p.bright : Muted, &FreeSansBold9pt7b);
}

void statusText(const UsageView &view, const UsageWindow &window, int64_t now, char *out, size_t size) {
    if (!view.hasData || !window.present) snprintf(out, size, "Sem dados");
    else if (view.stale) {
        char age[12];
        formatAge(view.ageSeconds, age, sizeof age);
        snprintf(out, size, "! %s", age);
    } else if (!window.known) snprintf(out, size, "Aguardando");
    else if (now <= 0 || window.resetsAt <= 0) snprintf(out, size, "Sem horario");
    else formatResetIn(window, now, out, size);
}

void header(Adafruit_GFX &c, const FrameContext &context) {
    c.fillScreen(Background);
    chamfer(c, 5, 5, 286, 230, 23, Border);
    c.drawLine(28, 6, 107, 6, CyanDim);
    c.drawLine(107, 6, 114, 13, CyanDim);
    c.drawLine(114, 13, 198, 13, Border);
    c.drawLine(198, 13, 205, 6, CyanDim);
    c.drawFastHLine(205, 6, 63, CyanDim);
    c.fillRect(24, 23, 3, 6, Cyan);
    c.fillRect(29, 19, 3, 10, Cyan);
    c.fillRect(34, 15, 3, 14, Cyan);
    text(c, "TOKOMETER", 44, 15, Text, &FreeSansBold9pt7b);
    for (int r = 5; r <= 10; r += 4) ringBand(c, 223, 26, r, r, 222, 318, Cyan);
    c.fillCircle(223, 26, 1, Cyan);
    text(c, context.clockText, 236, 17, Muted, nullptr);
    c.drawFastHLine(17, 35, 261, Border);
}

// A side card of the dashboard: one window of one provider. A tall card fills
// the whole column when there is no second card to share it with.
void windowCard(Adafruit_GFX &c, int y, int h, const ProviderUsage &entry, bool secondary,
                const FrameContext &context) {
    const auto &p = style(entry.id);
    const auto window = windowFor(entry, secondary);
    const bool tall = h > 76;
    panel(c, 198, y, 84, h, p);
    icon(c, 205, y + 8, p);
    text(c, p.name, 231, y + 11, Text);
    text(c, periodFor(entry.id, secondary), 231, y + 22, Muted);
    percentage(c, window, 240, y + (tall ? 62 : 32), p, false);
    char status[24];
    statusText(entry.view, window, context.nowSeconds, status, sizeof status);
    text(c, status, 240, y + h - (tall ? 40 : 17), entry.view.stale ? Orange : Muted, nullptr, true);
    bar(c, 207, y + h - 8, 66, window, p);
}

// Other providers first; a lone provider shows its other window instead.
void sideColumn(Adafruit_GFX &c, const ProvidersUsage &usage, const ProviderUsage &shown, bool secondary,
                const FrameContext &context) {
    const ProviderUsage *others[PROVIDER_COUNT];
    uint8_t count = 0;
    for (uint8_t index = 0; index < usage.count; ++index) {
        if (usage.items[index].id != shown.id) others[count++] = &usage.items[index];
    }
    if (count == 0) {
        windowCard(c, 43, 158, shown, !secondary, context);
        return;
    }
    if (count == 1) {
        windowCard(c, 43, 158, *others[0], secondary, context);
        return;
    }
    for (uint8_t index = 0; index < 2; ++index) {
        windowCard(c, 43 + index * 82, 76, *others[index], secondary, context);
    }
}

void periodTabs(Adafruit_GFX &c, const ProviderUsage &entry, bool secondary) {
    for (uint8_t i = 0; i < 2; ++i) {
        const bool active = secondary == (i != 0);
        const int x = 23 + i * 128;
        chamfer(c, x, 209, 122, 22, 6, active ? Cyan : Border);
        if (active) chamfer(c, x + 1, 210, 120, 20, 5, CyanDim);
        char value[12], label[24];
        formatPrimaryValue(windowFor(entry, i != 0), value, sizeof value);
        snprintf(label, sizeof label, "%s  %s", periodFor(entry.id, i != 0), value);
        text(c, label, x + 61, 217, active ? Text : Muted, nullptr, true);
    }
}

void dashboard(Adafruit_GFX &c, const ProvidersUsage &usage, const ProviderUsage &entry, bool secondary,
               const FrameContext &context) {
    const auto &p = style(entry.id);
    const auto window = windowFor(entry, secondary);
    panel(c, 14, 43, 174, 158, p);
    chamfer(c, 15, 44, 172, 156, 6, p.bright);
    const int labelWidth = 28 + textWidth(c, p.name, &FreeSansBold9pt7b);
    const int labelLeft = 101 - labelWidth / 2;
    icon(c, labelLeft, 50, p);
    text(c, p.name, labelLeft + 28, 54, Text, &FreeSansBold9pt7b);
    gauge(c, window, p);
    percentage(c, window, 100, 111, p, true);
    text(c, window.present ? "UTILIZADO" : "SEM DADOS", 100, 151, Muted, nullptr, true);
    c.drawFastHLine(65, 163, 71, Border);
    text(c, billsByCycle(entry.id) ? "CICLO DO PLANO" : (secondary ? "ULTIMOS 7 DIAS" : "ULTIMAS 5 HORAS"),
         100, 171, Muted, nullptr, true);
    char status[24];
    statusText(entry.view, window, context.nowSeconds, status, sizeof status);
    text(c, status, 100, 189, entry.view.stale ? Orange : p.bright, nullptr, true);
    sideColumn(c, usage, entry, secondary, context);
    periodTabs(c, entry, secondary);
}

// Rows share the space the three-provider layout uses, so fewer providers get
// taller rows with larger numbers.
constexpr int OVERVIEW_TOP = 44;
constexpr int OVERVIEW_HEIGHT = 153;
constexpr int OVERVIEW_GAP = 6;
constexpr int COMPACT_ROW_HEIGHT = 47;

void overviewRow(Adafruit_GFX &c, int y, int h, const ProviderUsage &entry, const FrameContext &context) {
    const auto &p = style(entry.id);
    const int top = y + (h - COMPACT_ROW_HEIGHT) / 2;
    const bool tall = h > COMPACT_ROW_HEIGHT + 10;
    panel(c, 16, y, 264, h, p);
    icon(c, 26, top + 10, p);
    text(c, p.name, 54, top + 8, Text, tall ? &FreeSansBold9pt7b : nullptr);
    char status[24];
    statusText(entry.view, windowFor(entry, false), context.nowSeconds, status, sizeof status);
    text(c, status, 54, top + (tall ? 28 : 24), entry.view.stale ? Orange : Muted);
    for (uint8_t period = 0; period < 2; ++period) {
        const auto w = windowFor(entry, period != 0);
        const int x = 153 + period * 77;
        text(c, periodFor(entry.id, period != 0), x, top + (tall ? 1 : 5), Muted, nullptr, true);
        char value[12];
        formatPrimaryValue(w, value, sizeof value);
        text(c, value, x, top + (tall ? 13 : 16), w.known ? p.bright : Muted,
             tall ? &FreeSansBold12pt7b : &FreeSansBold9pt7b, true);
        bar(c, x - 28, top + (tall ? 39 : 36), 56, w, p);
    }
}

void noProviders(Adafruit_GFX &c) {
    text(c, "Nenhum provedor ativo", 148, 100, Text, &FreeSansBold9pt7b, true);
    text(c, "Ative um nos Ajustes do app", 148, 126, Muted, nullptr, true);
}

void overview(Adafruit_GFX &c, const ProvidersUsage &usage, const FrameContext &context) {
    if (usage.count == 0) {
        noProviders(c);
        return;
    }
    const int rowHeight = (OVERVIEW_HEIGHT - OVERVIEW_GAP * (usage.count - 1)) / usage.count;
    char names[40] = "";
    for (uint8_t index = 0; index < usage.count; ++index) {
        const ProviderUsage &entry = usage.items[index];
        overviewRow(c, OVERVIEW_TOP + index * (rowHeight + OVERVIEW_GAP), rowHeight, entry, context);
        if (index > 0) strncat(names, " / ", sizeof names - strlen(names) - 1);
        strncat(names, style(entry.id).name, sizeof names - strlen(names) - 1);
    }
    for (char *letter = names; *letter != '\0'; ++letter) *letter = toupper(*letter);
    text(c, names, 148, 216, Muted, nullptr, true);
}
}

void renderTftUsage(Adafruit_GFX &canvas, const ProvidersUsage &usage, UsageLayout layout,
                    bool sevenDay, const FrameContext &context) {
    header(canvas, context);
    const ProviderUsage *shown = nullptr;
    for (uint8_t index = 0; index < usage.count; ++index) {
        if (layout != UsageLayout::Overview && usage.items[index].id == providerOf(layout)) shown = &usage.items[index];
    }
    if (shown != nullptr) dashboard(canvas, usage, *shown, sevenDay, context);
    else overview(canvas, usage, context);
    canvas.setFont();
    canvas.setTextSize(1);
}
