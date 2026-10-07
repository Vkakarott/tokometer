#include "display/status_screens.h"

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#include "core/text_format.h"
#include "display/indexed_canvas.h"

// Every screen before the dashboard borrows the look of the classic Macintosh
// desktop: one-bit ink on paper, a menu bar and a single window.
namespace {

constexpr uint16_t Ink = TftColor::Background;
constexpr uint16_t Paper = TftColor::Text;

constexpr int16_t MENU_BAR_HEIGHT = 20;
constexpr int16_t TITLE_BAR_HEIGHT = 19;
constexpr int16_t WINDOW_X = 18;
constexpr int16_t WINDOW_Y = 34;
constexpr int16_t WINDOW_WIDTH = 260;
constexpr int16_t WINDOW_HEIGHT = 190;
constexpr int16_t CONTENT_TOP = WINDOW_Y + TITLE_BAR_HEIGHT;
constexpr int16_t CENTER_X = WINDOW_X + WINDOW_WIDTH / 2;
constexpr int16_t PROGRESS_WIDTH = 168;
constexpr int16_t PROGRESS_HEIGHT = 12;
constexpr uint32_t PROGRESS_STEP_MS = 500;

const GFXfont *const TITLE_FONT = &FreeSansBold9pt7b;
const GFXfont *const BODY_FONT = &FreeSans9pt7b;

enum class Align { Left, Center, Right };

uint16_t textWidth(Adafruit_GFX &canvas, const char *text, const GFXfont *font) {
    int16_t left, top;
    uint16_t width, height;
    canvas.setFont(font);
    canvas.setTextSize(1);
    canvas.getTextBounds(text, 0, 0, &left, &top, &width, &height);
    return width;
}

// `y` is the top of the text box, whatever the font's baseline is.
void drawText(Adafruit_GFX &canvas, const char *text, int16_t x, int16_t y, const GFXfont *font,
              Align align = Align::Left, uint16_t color = Ink) {
    int16_t left, top;
    uint16_t width, height;
    canvas.setFont(font);
    canvas.setTextSize(1);
    canvas.getTextBounds(text, 0, 0, &left, &top, &width, &height);
    const int16_t shift = align == Align::Center ? width / 2 : align == Align::Right ? width : 0;
    canvas.setTextColor(color);
    canvas.setCursor(x - left - shift, y - top);
    canvas.print(text);
}

// The 50% gray of a one-bit desktop is a checkerboard of ink on paper.
void drawDesktop(Adafruit_GFX &canvas) {
    canvas.fillScreen(Paper);
    for (int16_t y = MENU_BAR_HEIGHT + 1; y < canvas.height(); ++y) {
        for (int16_t x = y & 1; x < canvas.width(); x += 2) canvas.drawPixel(x, y, Ink);
    }
}

// The rounded screen corners of the original CRT bezel.
void drawScreenCorners(Adafruit_GFX &canvas) {
    constexpr int16_t RADIUS = 5;
    const int16_t right = canvas.width() - 1;
    const int16_t bottom = canvas.height() - 1;
    for (int16_t row = 0; row < RADIUS; ++row) {
        const int16_t length = RADIUS - row - (row == 0 ? 0 : 1);
        if (length <= 0) continue;
        canvas.drawFastHLine(0, row, length, Ink);
        canvas.drawFastHLine(right - length + 1, row, length, Ink);
        canvas.drawFastHLine(0, bottom - row, length, Ink);
        canvas.drawFastHLine(right - length + 1, bottom - row, length, Ink);
    }
}

void drawMenuBar(Adafruit_GFX &canvas, const char *clockText) {
    canvas.drawFastHLine(0, MENU_BAR_HEIGHT, canvas.width(), Ink);
    canvas.fillRect(12, 10, 3, 6, Ink);
    canvas.fillRect(17, 7, 3, 9, Ink);
    canvas.fillRect(22, 4, 3, 12, Ink);
    drawText(canvas, "Tokometer", 32, 4, TITLE_FONT);
    if (clockText[0] != '\0') drawText(canvas, clockText, canvas.width() - 12, 4, BODY_FONT, Align::Right);
}

// Striped title bar with a close box and the title on a paper gap.
void drawTitleBar(Adafruit_GFX &canvas, const char *title) {
    for (int16_t line = 0; line < 6; ++line) {
        canvas.drawFastHLine(WINDOW_X + 3, WINDOW_Y + 4 + line * 2, WINDOW_WIDTH - 6, Ink);
    }
    canvas.drawFastHLine(WINDOW_X, WINDOW_Y + TITLE_BAR_HEIGHT - 1, WINDOW_WIDTH, Ink);
    canvas.fillRect(WINDOW_X + 8, WINDOW_Y + 3, 13, 13, Paper);
    canvas.drawRect(WINDOW_X + 9, WINDOW_Y + 4, 11, 11, Ink);

    const int16_t gap = textWidth(canvas, title, TITLE_FONT) + 16;
    canvas.fillRect(CENTER_X - gap / 2, WINDOW_Y + 1, gap, TITLE_BAR_HEIGHT - 2, Paper);
    drawText(canvas, title, CENTER_X, WINDOW_Y + 3, TITLE_FONT, Align::Center);
}

void drawWindow(Adafruit_GFX &canvas, const char *title) {
    canvas.fillRect(WINDOW_X + 2, WINDOW_Y + 2, WINDOW_WIDTH, WINDOW_HEIGHT, Ink);
    canvas.fillRect(WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT, Paper);
    canvas.drawRect(WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT, Ink);
    drawTitleBar(canvas, title);
}

// A compact computer whose screen shows the tokometer bars.
void drawComputerIcon(Adafruit_GFX &canvas, int16_t x, int16_t y) {
    canvas.drawRoundRect(x, y, 32, 36, 3, Ink);
    canvas.drawRoundRect(x + 5, y + 4, 22, 17, 2, Ink);
    canvas.fillRect(x + 10, y + 13, 3, 5, Ink);
    canvas.fillRect(x + 15, y + 10, 3, 8, Ink);
    canvas.fillRect(x + 20, y + 7, 3, 11, Ink);
    canvas.drawFastHLine(x + 17, y + 27, 10, Ink);
    canvas.fillRect(x + 5, y + 26, 3, 2, Ink);
    canvas.fillRect(x + 3, y + 36, 26, 4, Ink);
}

void drawAlertIcon(Adafruit_GFX &canvas, int16_t x, int16_t y) {
    canvas.fillTriangle(x + 16, y, x, y + 30, x + 32, y + 30, Ink);
    canvas.fillTriangle(x + 16, y + 5, x + 4, y + 28, x + 28, y + 28, Paper);
    canvas.fillRect(x + 15, y + 12, 3, 9, Ink);
    canvas.fillRect(x + 15, y + 23, 3, 3, Ink);
}

// Indeterminate "barber pole" bar: the stripes walk right as time passes.
void drawProgressBar(Adafruit_GFX &canvas, int16_t y, uint32_t nowMs) {
    constexpr int16_t STRIPE = 8;
    const int16_t x = CENTER_X - PROGRESS_WIDTH / 2;
    const int16_t phase = (nowMs / PROGRESS_STEP_MS) % 4 * (STRIPE / 2);
    canvas.drawRect(x, y, PROGRESS_WIDTH, PROGRESS_HEIGHT, Ink);
    for (int16_t row = 1; row < PROGRESS_HEIGHT - 1; ++row) {
        for (int16_t column = 1; column < PROGRESS_WIDTH - 1; ++column) {
            if ((column - row - phase + 4 * STRIPE) % (2 * STRIPE) < STRIPE) canvas.drawPixel(x + column, y + row, Ink);
        }
    }
}

uint32_t pairingSecondsLeft(const AppState &state, uint32_t nowMs) {
    const uint32_t elapsed = (nowMs - state.pairingStartedAtMs) / 1000;
    return elapsed >= state.pairCode.expiresInSeconds ? 0 : state.pairCode.expiresInSeconds - elapsed;
}

// Icon, headline, one line of detail and the busy bar.
void drawBusyWindow(Adafruit_GFX &canvas, const char *title, const char *headline, const char *detail,
                    uint32_t nowMs) {
    drawWindow(canvas, title);
    drawComputerIcon(canvas, CENTER_X - 16, CONTENT_TOP + 18);
    drawText(canvas, headline, CENTER_X, CONTENT_TOP + 72, TITLE_FONT, Align::Center);
    drawText(canvas, detail, CENTER_X, CONTENT_TOP + 98, BODY_FONT, Align::Center);
    drawProgressBar(canvas, CONTENT_TOP + 134, nowMs);
}

void drawSetupWindow(Adafruit_GFX &canvas) {
    const int16_t left = WINDOW_X + 18;
    drawWindow(canvas, "Configurar Wi-Fi");
    drawComputerIcon(canvas, left, CONTENT_TOP + 14);
    drawText(canvas, "No seu Mac:", left + 46, CONTENT_TOP + 16, TITLE_FONT);
    drawText(canvas, "abra o tokometer", left + 46, CONTENT_TOP + 38, BODY_FONT);
    drawText(canvas, "Clique em", left, CONTENT_TOP + 70, BODY_FONT);
    drawText(canvas, "Conectar display", left + 86, CONTENT_TOP + 70, TITLE_FONT);

    for (int16_t x = WINDOW_X + 10; x < WINDOW_X + WINDOW_WIDTH - 10; x += 2) {
        canvas.drawPixel(x, CONTENT_TOP + 100, Ink);
    }
    drawText(canvas, "Sem o app: entre na rede Wi-Fi", CENTER_X, CONTENT_TOP + 112, nullptr, Align::Center);
    drawText(canvas, provisioningNetworkName(), CENTER_X, CONTENT_TOP + 126, TITLE_FONT, Align::Center);
    drawText(canvas, "e abra 192.168.4.1", CENTER_X, CONTENT_TOP + 148, nullptr, Align::Center);
}

// The app approves on its own; the code only matters to someone pairing by hand.
void drawPairingWindow(Adafruit_GFX &canvas, const AppState &state, uint32_t nowMs) {
    char code[12];
    char countdown[8];
    char expiry[24];
    formatUserCode(state.pairCode.userCode, code, sizeof code);
    formatCountdown(pairingSecondsLeft(state, nowMs), countdown, sizeof countdown);
    snprintf(expiry, sizeof expiry, "expira em %s", countdown);

    drawWindow(canvas, "Pareamento");
    drawText(canvas, "Aguardando o tokometer...", CENTER_X, CONTENT_TOP + 14, BODY_FONT, Align::Center);
    drawText(canvas, "Ou digite no app:", CENTER_X, CONTENT_TOP + 44, nullptr, Align::Center);
    canvas.fillRoundRect(WINDOW_X + 16, CONTENT_TOP + 60, WINDOW_WIDTH - 32, 50, 6, Ink);
    drawText(canvas, code, CENTER_X, CONTENT_TOP + 72, &FreeSansBold18pt7b, Align::Center, Paper);
    drawText(canvas, expiry, CENTER_X, CONTENT_TOP + 124, BODY_FONT, Align::Center);
    drawText(canvas, stripUrlScheme(state.pairCode.verificationUri), CENTER_X, CONTENT_TOP + 152, nullptr,
             Align::Center);
}

// Double frame, like the system alert box.
void drawErrorWindow(Adafruit_GFX &canvas, const char *message, uint32_t nowMs) {
    drawWindow(canvas, "Erro");
    canvas.drawRect(WINDOW_X + 4, CONTENT_TOP + 4, WINDOW_WIDTH - 8, WINDOW_HEIGHT - TITLE_BAR_HEIGHT - 8, Ink);
    canvas.drawRect(WINDOW_X + 5, CONTENT_TOP + 5, WINDOW_WIDTH - 10, WINDOW_HEIGHT - TITLE_BAR_HEIGHT - 10, Ink);
    drawAlertIcon(canvas, CENTER_X - 16, CONTENT_TOP + 20);
    drawText(canvas, message, CENTER_X, CONTENT_TOP + 68, TITLE_FONT, Align::Center);
    drawText(canvas, "Tentando novamente...", CENTER_X, CONTENT_TOP + 96, BODY_FONT, Align::Center);
    drawProgressBar(canvas, CONTENT_TOP + 134, nowMs);
}

}  // namespace

void renderStatusScreen(Adafruit_GFX &canvas, const AppState &state, const FrameContext &context) {
    drawDesktop(canvas);
    drawMenuBar(canvas, context.clockText);
    switch (state.screen) {
        case AppScreen::ConnectingWifi:
            if (state.wifiStatus == WifiStatus::Provisioning) {
                drawSetupWindow(canvas);
            } else {
                drawBusyWindow(canvas, "Tokometer", "Bem-vindo ao tokometer", "Conectando ao Wi-Fi...",
                               context.nowMs);
            }
            break;
        case AppScreen::RequestingPairCode:
            drawBusyWindow(canvas, "Pareamento", "Pareando o display", "Falando com o tokometer...",
                           context.nowMs);
            break;
        case AppScreen::WaitingPairApproval:
            drawPairingWindow(canvas, state, context.nowMs);
            break;
        case AppScreen::LoadingUsage:
            drawBusyWindow(canvas, "Tokometer", "Tudo pronto", "Carregando o consumo...", context.nowMs);
            break;
        case AppScreen::Error:
            drawErrorWindow(canvas, state.errorMessage, context.nowMs);
            break;
        case AppScreen::ShowingUsage:
            break;
    }
    drawScreenCorners(canvas);
    canvas.setFont();
}
