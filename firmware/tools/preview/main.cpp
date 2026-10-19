#include <array>
#include <initializer_list>
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
    result.items[0] = {ProviderId::Codex, provider(42, 26)};
    result.items[1] = {ProviderId::Claude, provider(68, 81)};
    result.items[2] = {ProviderId::Cursor, cursorProvider(12, 39, 8)};
    result.count = 3;
    return result;
}

UsageView &viewOf(ProvidersUsage &usage, ProviderId id) {
    for (uint8_t index = 0; index < usage.count; ++index) {
        if (usage.items[index].id == id) return usage.items[index].view;
    }
    throw std::runtime_error("provider not in sample");
}

// The sample keeping only `ids`, as the backend sends it when the rest are hidden.
ProvidersUsage keepOnly(const ProvidersUsage &usage, std::initializer_list<ProviderId> ids) {
    ProvidersUsage result;
    for (uint8_t index = 0; index < usage.count; ++index) {
        for (ProviderId id : ids) {
            if (usage.items[index].id == id) result.items[result.count++] = usage.items[index];
        }
    }
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
            UsageLayout layout, bool sevenDay, const FrameContext &context) {
    IndexedCanvas canvas;
    renderTftUsage(canvas, usage, resolveLayout(layout, usage), sevenDay, context);
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
        constexpr UsageLayout Overview = UsageLayout::Overview;
        constexpr UsageLayout Codex = UsageLayout::CodexDashboard;
        constexpr UsageLayout Claude = UsageLayout::ClaudeDashboard;
        constexpr UsageLayout Cursor = UsageLayout::CursorDashboard;
        render(outputDir, "codex-5h", usage, Codex, false, context);
        render(outputDir, "codex-7d", usage, Codex, true, context);
        render(outputDir, "claude-5h", usage, Claude, false, context);
        render(outputDir, "claude-7d", usage, Claude, true, context);
        render(outputDir, "cursor-plan", usage, Cursor, false, context);
        render(outputDir, "cursor-api", usage, Cursor, true, context);
        render(outputDir, "overview-5h", usage, Overview, false, context);
        render(outputDir, "overview-7d", usage, Overview, true, context);

        ProvidersUsage extremes = usage;
        viewOf(extremes, ProviderId::Codex) = provider(100, 0);
        viewOf(extremes, ProviderId::Claude) = provider(0, 100);
        viewOf(extremes, ProviderId::Cursor) = cursorProvider(99, 1, 100);
        render(outputDir, "full", extremes, Codex, false, context);
        render(outputDir, "zero", extremes, Codex, true, context);
        render(outputDir, "cursor-plan-full", extremes, Cursor, false, context);

        ProvidersUsage missing = usage;
        viewOf(missing, ProviderId::Codex) = {};
        viewOf(missing, ProviderId::Cursor) = {};
        render(outputDir, "missing-provider", missing, Codex, false, context);
        render(outputDir, "missing-overview", missing, Overview, false, context);

        const ProvidersUsage twoProviders = keepOnly(usage, {ProviderId::Codex, ProviderId::Claude});
        render(outputDir, "two-overview", twoProviders, Overview, false, context);
        render(outputDir, "two-claude-5h", twoProviders, Claude, false, context);
        render(outputDir, "two-saved-cursor", twoProviders, Cursor, false, context);
        const ProvidersUsage oneProvider = keepOnly(usage, {ProviderId::Claude});
        render(outputDir, "one-claude-5h", oneProvider, Overview, false, context);
        render(outputDir, "one-claude-7d", oneProvider, Claude, true, context);
        render(outputDir, "none-enabled", {}, Overview, false, context);

        ProvidersUsage stale = usage;
        viewOf(stale, ProviderId::Codex).stale = true;
        viewOf(stale, ProviderId::Codex).ageSeconds = 7800;
        viewOf(stale, ProviderId::Claude).stale = true;
        viewOf(stale, ProviderId::Claude).ageSeconds = 720;
        render(outputDir, "stale", stale, Codex, false, context);
        render(outputDir, "stale-overview", stale, Overview, false, context);

        ProvidersUsage reset = usage;
        viewOf(reset, ProviderId::Codex).fiveHour.known = false;
        viewOf(reset, ProviderId::Codex).sevenDay.present = false;
        render(outputDir, "reset-window", reset, Codex, false, context);
        render(outputDir, "missing-window", reset, Codex, true, context);

        FrameContext unsynced;
        render(outputDir, "clock-unsynced", usage, Codex, false, unsynced);

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
