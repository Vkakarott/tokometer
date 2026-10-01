#include "app/app_controller.h"

#include <Arduino.h>

#include "ble/speedbike_link.h"
#include "core/rpm_smoother.h"
#include "core/speedbike_protocol.h"
#include "core/speedbike_session.h"
#include "display/display_manager.h"
#include "display/gauge_renderer.h"

namespace {
constexpr uint32_t BootLogoMs = 2000;
constexpr uint32_t GaugeFrameIntervalMs = 33;

SpeedBikeLink speedbikeLink;
SpeedBikeSession session;
FrameDecoder decoder;
RpmSmoother smoother;
uint32_t bootStartedAtMs = 0;
uint32_t lastGaugeFrameAtMs = 0;
bool noData = false;

void sendFrame(size_t (*build)(uint8_t *, size_t)) {
    uint8_t frame[16];
    const size_t length = build(frame, sizeof frame);
    if (length == 0 || !speedbikeLink.write(frame, length)) {
        log_w("speedbike write failed");
    }
}

void feedLinkEvents(uint32_t nowMs) {
    if (speedbikeLink.takeDeviceFound()) session.deviceFound(nowMs);
    if (speedbikeLink.takeConnected()) session.linkConnected(nowMs);
    if (speedbikeLink.takeDisconnected()) session.linkDisconnected(nowMs);
    uint8_t byte = 0;
    while (speedbikeLink.popByte(byte)) {
        decoder.push(&byte, 1);
        DecodedFrame frame{};
        while (decoder.next(frame)) session.frame(frame, nowMs);
    }
}

void applyOutput(const SessionOutput &output) {
    if (output.startScan) speedbikeLink.startScan();
    if (output.sendNotice) sendFrame(buildSessionNotice);
    if (output.sendPassword) sendFrame(buildPasswordFrame);
    if (output.hasRpm) smoother.setTarget(output.rpm);
    noData = output.noData;
    if (noData) smoother.setTarget(0);
}

void render(uint32_t nowMs) {
    if (nowMs - bootStartedAtMs < BootLogoMs) return;
    switch (session.state()) {
        case SessionState::Connecting:
            showStatus("CONECTANDO...", nullptr);
            break;
        case SessionState::AwaitingPasswordRequest:
        case SessionState::Authenticating:
            showStatus("AUTENTICANDO...", nullptr);
            break;
        case SessionState::Streaming:
            if (nowMs - lastGaugeFrameAtMs >= GaugeFrameIntervalMs) {
                lastGaugeFrameAtMs = nowMs;
                drawGauge(gaugeCanvas(), smoother.step(), smoother.target(), noData);
                pushRegion(GaugeRegionX, GaugeRegionY, GaugeRegionWidth, GaugeRegionHeight);
            }
            break;
        default:
            showStatus("PROCURANDO", "SPEEDBIKE_050102");
            break;
    }
}

void connectIfRequested(const SessionOutput &output, uint32_t nowMs) {
    if (!output.connect) return;
    if (!speedbikeLink.connect()) {
        log_w("speedbike connect failed");
        session.linkDisconnected(nowMs);
    }
}
}

void initializeApp() {
    showBootLogo();
    bootStartedAtMs = millis();
    speedbikeLink.begin();
}

void updateApp(uint32_t nowMs) {
    feedLinkEvents(nowMs);
    session.tick(nowMs);
    const SessionOutput output = session.takeOutput();
    applyOutput(output);
    render(nowMs);
    connectIfRequested(output, nowMs);
}
