#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

#include "display/indexed_canvas.h"
#include "display/status_screens.h"
#include "display/tft_renderer.h"

// The firmware asks the Wi-Fi manager for this name; the preview has no radio.
const char *provisioningNetworkName() { return "tokEsp-9F10"; }

namespace {

constexpr int64_t NOW_SECONDS = 1790789220;

UsageWindow window(float percentage, int64_t resetInSeconds) {
    UsageWindow result;
    result.present = true;
    result.known = true;
    result.usedPercentage = percentage;
    result.resetsAt = NOW_SECONDS + resetInSeconds;
    return result;
}

UsageView provider(float fiveHour, float sevenDay) {
    UsageView result;
    result.fiveHour = window(fiveHour, 2 * 3600 + 14 * 60);
    result.sevenDay = window(sevenDay, 3 * 86400 + 12 * 3600);
    result.ageSeconds = 24;
    result.hasData = true;
    return result;
}

UsageView cursorProvider(float included, float api, float onDemand) {
    UsageView result;
    result.included = window(included, 17 * 86400 + 6 * 3600);
    result.api = window(api, 17 * 86400 + 6 * 3600);
    result.onDemand = window(onDemand, 17 * 86400 + 6 * 3600);
    result.ageSeconds = 24;
    result.hasData = true;
    return result;
}

ProvidersUsage sampleUsage() {
    ProvidersUsage result;
    result.codex = provider(42, 26);
    result.claude = provider(68, 81);
    result.cursor = cursorProvider(12, 39, 8);
    return result;
}

void savePpm(const IndexedCanvas &canvas, const std::string &path) {
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("Cannot open " + path);
    output << "P6\n" << canvas.width() << ' ' << canvas.height() << "\n255\n";
    for (int16_t y = 0; y < canvas.height(); ++y) {
        for (int16_t x = 0; x < canvas.width(); ++x) {
            const uint16_t pixel = canvas.pixelColor(x, y);
            const uint8_t r = (pixel >> 11) & 31;
            const uint8_t g = (pixel >> 5) & 63;
            const uint8_t b = pixel & 31;
            const std::array<uint8_t, 3> rgb = {
                static_cast<uint8_t>((r << 3) | (r >> 2)),
                static_cast<uint8_t>((g << 2) | (g >> 4)),
                static_cast<uint8_t>((b << 3) | (b >> 2)),
            };
            output.write(reinterpret_cast<const char *>(rgb.data()), rgb.size());
        }
    }
    if (!output) throw std::runtime_error("Cannot write " + path);
}

void render(const std::string &outputDir, const char *name, const ProvidersUsage &usage,
            uint8_t selectedProvider, bool overview, bool sevenDay,
            const FrameContext &context) {
    IndexedCanvas canvas;
    renderTftUsage(canvas, usage, selectedProvider, overview, sevenDay, context);
    savePpm(canvas, outputDir + '/' + name + ".ppm");
    std::printf("%s\n", name);
}

void renderStatus(const std::string &outputDir, const char *name, AppScreen screen,
                  WifiStatus wifiStatus, const FrameContext &context) {
    AppState state;
    state.screen = screen;
    state.wifiStatus = wifiStatus;
    state.errorMessage = "Sem resposta do tokometer";
    state.pairCode.expiresInSeconds = 300;
    std::snprintf(state.pairCode.userCode, sizeof(state.pairCode.userCode), "K7QM3F9A");
    std::snprintf(state.pairCode.verificationUri, sizeof(state.pairCode.verificationUri),
                  "http://tok.local/pair");

    IndexedCanvas canvas;
    renderStatusScreen(canvas, state, context);
    savePpm(canvas, outputDir + '/' + name + ".ppm");
    std::printf("%s\n", name);
}

}  // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: render-preview OUTPUT_DIRECTORY\n");
        return 2;
    }

    try {
        const std::string outputDir = argv[1];
        FrameContext context;
        context.nowSeconds = NOW_SECONDS;
        std::snprintf(context.clockText, sizeof(context.clockText), "14:27");
        const ProvidersUsage usage = sampleUsage();
        const std::array<const char *, 3> names = {"codex", "claude", "cursor"};
        for (uint8_t selected = 0; selected < names.size(); ++selected) {
            for (bool sevenDay : {false, true}) {
                const char *period = selected == 2 ? (sevenDay ? "-api" : "-plan")
                                                  : (sevenDay ? "-7d" : "-5h");
                const std::string name = std::string(names[selected]) + period;
                render(outputDir, name.c_str(), usage, selected, false, sevenDay, context);
            }
        }
        render(outputDir, "overview-5h", usage, 0, true, false, context);
        render(outputDir, "overview-7d", usage, 0, true, true, context);

        ProvidersUsage extremes = usage;
        extremes.codex = provider(100, 0);
        extremes.claude = provider(0, 100);
        extremes.cursor = cursorProvider(99, 1, 100);
        render(outputDir, "full", extremes, 0, false, false, context);
        render(outputDir, "zero", extremes, 0, false, true, context);
        render(outputDir, "cursor-plan-full", extremes, 2, false, false, context);

        ProvidersUsage missing = usage;
        missing.codex = {};
        missing.cursor = {};
        render(outputDir, "missing-provider", missing, 0, false, false, context);
        render(outputDir, "missing-overview", missing, 0, true, false, context);
        render(outputDir, "all-missing", {}, 0, false, false, context);

        ProvidersUsage stale = usage;
        stale.codex.stale = true;
        stale.codex.ageSeconds = 7800;
        stale.claude.stale = true;
        stale.claude.ageSeconds = 720;
        render(outputDir, "stale", stale, 0, false, false, context);
        render(outputDir, "stale-overview", stale, 0, true, false, context);

        ProvidersUsage reset = usage;
        reset.codex.fiveHour.known = false;
        reset.codex.sevenDay.present = false;
        render(outputDir, "reset-window", reset, 0, false, false, context);
        render(outputDir, "missing-window", reset, 0, false, true, context);

        FrameContext unsynced;
        render(outputDir, "clock-unsynced", usage, 0, false, false, unsynced);

        renderStatus(outputDir, "status-welcome", AppScreen::ConnectingWifi, WifiStatus::Connecting, unsynced);
        renderStatus(outputDir, "status-setup", AppScreen::ConnectingWifi, WifiStatus::Provisioning, unsynced);
        renderStatus(outputDir, "status-pairing-request", AppScreen::RequestingPairCode, WifiStatus::Connected, context);
        renderStatus(outputDir, "status-pairing-code", AppScreen::WaitingPairApproval, WifiStatus::Connected, context);
        renderStatus(outputDir, "status-loading", AppScreen::LoadingUsage, WifiStatus::Connected, context);
        renderStatus(outputDir, "status-error", AppScreen::Error, WifiStatus::Connected, context);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
