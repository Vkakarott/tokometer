#pragma once

#include <cstdint>

#include "speedbike_protocol.h"

enum class SessionState {
    Boot,
    Scanning,
    Connecting,
    AwaitingPasswordRequest,
    Authenticating,
    Streaming,
    ReconnectWait,
};

struct SessionOutput {
    bool startScan = false;
    bool connect = false;
    bool sendNotice = false;
    bool sendPasswordCreate = false;
    bool sendPasswordLogin = false;
    bool wrongPassword = false;
    bool hasRpm = false;
    uint16_t rpm = 0;
    bool noData = false;
};

class SpeedBikeSession {
public:
    void tick(uint32_t nowMs);
    void deviceFound(uint32_t nowMs);
    void linkConnected(uint32_t nowMs);
    void linkDisconnected(uint32_t nowMs);
    void frame(const DecodedFrame &frame, uint32_t nowMs);

    SessionState state() const;
    SessionOutput takeOutput();

private:
    void enterReconnectWait(uint32_t nowMs);
    void sendNotice(uint32_t nowMs);
    void startStreaming(uint32_t nowMs);
    void publishDashboard(const DecodedFrame &frame, uint32_t nowMs);

    SessionState state_ = SessionState::Boot;
    uint32_t stateSinceMs_ = 0;
    uint32_t lastNoticeAtMs_ = 0;
    uint32_t lastDataAtMs_ = 0;
    uint8_t noticeCount_ = 0;
    SessionOutput output_{};
};
