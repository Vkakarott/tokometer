#include "display/tft_renderer.h"

#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
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
constexpr Provider Providers[] = {
    {"Codex", Mint, MintDim, ProviderIcons::Codex},
    {"Claude", Orange, OrangeDim, ProviderIcons::Claude},
    {"Cursor", Purple, PurpleDim, ProviderIcons::Cursor},
};

const UsageView &viewFor(const ProvidersUsage &usage, uint8_t id) {
    if (id == 1) return usage.claude;
    if (id == 2) return usage.cursor;
    return usage.codex;
}

UsageWindow windowFor(const UsageView &view, uint8_t id, bool secondary) {
    if (!view.hasData) return {};
    if (id == 2) return secondary ? view.api : view.included;
    return secondary ? view.sevenDay : view.fiveHour;
}

const char *periodFor(uint8_t id, bool secondary) {
    if (id == 2) return secondary ? "API" : "PLANO";
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

void smallCard(Adafruit_GFX &c, int y, const ProvidersUsage &usage, uint8_t id, bool secondary,
               const FrameContext &context) {
    const auto &p = Providers[id];
    const auto &view = viewFor(usage, id);
    const auto window = windowFor(view, id, secondary);
    panel(c, 198, y, 84, 76, p);
    icon(c, 205, y + 8, p);
    text(c, p.name, 231, y + 11, Text);
    text(c, periodFor(id, secondary), 231, y + 22, Muted);
    percentage(c, window, 240, y + 32, p, false);
    char status[24];
    statusText(view, window, context.nowSeconds, status, sizeof status);
    text(c, status, 240, y + 59, view.stale ? Orange : Muted, nullptr, true);
    bar(c, 207, y + 68, 66, window, p);
}

void periodTabs(Adafruit_GFX &c, const UsageView &view, uint8_t id, bool secondary) {
    for (uint8_t i = 0; i < 2; ++i) {
        const bool active = secondary == (i != 0);
        const int x = 23 + i * 128;
        chamfer(c, x, 209, 122, 22, 6, active ? Cyan : Border);
        if (active) chamfer(c, x + 1, 210, 120, 20, 5, CyanDim);
        char value[12], label[24];
        formatPrimaryValue(windowFor(view, id, i != 0), value, sizeof value);
        snprintf(label, sizeof label, "%s  %s", periodFor(id, i != 0), value);
        text(c, label, x + 61, 217, active ? Text : Muted, nullptr, true);
    }
}

void dashboard(Adafruit_GFX &c, const ProvidersUsage &usage, uint8_t id, bool secondary,
               const FrameContext &context) {
    const auto &p = Providers[id];
    const auto &view = viewFor(usage, id);
    const auto window = windowFor(view, id, secondary);
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
    text(c, id == 2 ? "CICLO DO PLANO" : (secondary ? "ULTIMOS 7 DIAS" : "ULTIMAS 5 HORAS"),
         100, 171, Muted, nullptr, true);
    char status[24];
    statusText(view, window, context.nowSeconds, status, sizeof status);
    text(c, status, 100, 189, view.stale ? Orange : p.bright, nullptr, true);
    int side = 0;
    for (uint8_t other = 0; other < 3; ++other) {
        if (other != id) smallCard(c, 43 + side++ * 82, usage, other, secondary, context);
    }
    periodTabs(c, view, id, secondary);
}

void overview(Adafruit_GFX &c, const ProvidersUsage &usage, const FrameContext &context) {
    for (uint8_t id = 0; id < 3; ++id) {
        const auto &p = Providers[id];
        const auto &view = viewFor(usage, id);
        const int y = 44 + id * 53;
        panel(c, 16, y, 264, 47, p);
        icon(c, 26, y + 10, p);
        text(c, p.name, 54, y + 8, Text);
        char status[24];
        statusText(view, windowFor(view, id, false), context.nowSeconds, status, sizeof status);
        text(c, status, 54, y + 24, view.stale ? Orange : Muted);
        for (uint8_t period = 0; period < 2; ++period) {
            const auto w = windowFor(view, id, period != 0);
            const int x = 153 + period * 77;
            text(c, periodFor(id, period != 0), x, y + 5, Muted, nullptr, true);
            char value[12];
            formatPrimaryValue(w, value, sizeof value);
            text(c, value, x, y + 16, w.known ? p.bright : Muted, &FreeSansBold9pt7b, true);
            bar(c, x - 28, y + 36, 56, w, p);
        }
    }
    text(c, "CODEX / CLAUDE / CURSOR", 148, 216, Muted, nullptr, true);
}
}

void renderTftUsage(Adafruit_GFX &canvas, const ProvidersUsage &usage, uint8_t selectedProvider,
                    bool showOverview, bool sevenDay, const FrameContext &context) {
    header(canvas, context);
    if (showOverview) overview(canvas, usage, context);
    else dashboard(canvas, usage, selectedProvider < 3 ? selectedProvider : 0, sevenDay, context);
    canvas.setFont();
    canvas.setTextSize(1);
}
