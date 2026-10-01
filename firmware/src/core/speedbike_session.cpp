#include "speedbike_session.h"

#include "config/speedbike_config.h"

void SpeedBikeSession::tick(uint32_t nowMs) {
    if (state_ == SessionState::Boot) {
        state_ = SessionState::Scanning;
        stateSinceMs_ = nowMs;
        output_.startScan = true;
        return;
    }
    if (state_ == SessionState::AwaitingPasswordRequest) {
        if (noticeCount_ < 3 && nowMs - lastNoticeAtMs_ >= 1200) {
            sendNotice(nowMs);
        }
        if (nowMs - stateSinceMs_ >= SpeedBikeConfig::PasswordRequestTimeoutMs) {
            enterReconnectWait(nowMs);
        }
        return;
    }
    if (state_ == SessionState::Authenticating &&
        nowMs - stateSinceMs_ >= SpeedBikeConfig::AuthenticationTimeoutMs) {
        enterReconnectWait(nowMs);
        return;
    }
    if (state_ == SessionState::ReconnectWait &&
        nowMs - stateSinceMs_ >= SpeedBikeConfig::ReconnectDelayMs) {
        state_ = SessionState::Scanning;
        stateSinceMs_ = nowMs;
        output_.startScan = true;
        return;
    }
    if (state_ == SessionState::Streaming &&
        nowMs - lastDataAtMs_ >= SpeedBikeConfig::NoDataTimeoutMs) {
        output_.noData = true;
    }
}

void SpeedBikeSession::deviceFound(uint32_t nowMs) {
    if (state_ != SessionState::Scanning) {
        return;
    }
    state_ = SessionState::Connecting;
    stateSinceMs_ = nowMs;
    output_.connect = true;
}

void SpeedBikeSession::linkConnected(uint32_t nowMs) {
    if (state_ != SessionState::Connecting) {
        return;
    }
    state_ = SessionState::AwaitingPasswordRequest;
    stateSinceMs_ = nowMs;
    noticeCount_ = 0;
    sendNotice(nowMs);
}

void SpeedBikeSession::linkDisconnected(uint32_t nowMs) {
    if (state_ == SessionState::Boot || state_ == SessionState::Scanning) {
        return;
    }
    enterReconnectWait(nowMs);
}

void SpeedBikeSession::frame(const DecodedFrame &frame, uint32_t nowMs) {
    const FrameKind kind = classifyFrame(frame);
    if (kind == FrameKind::Dashboard &&
        (state_ == SessionState::AwaitingPasswordRequest || state_ == SessionState::Authenticating ||
         state_ == SessionState::Streaming)) {
        startStreaming(nowMs);
        publishDashboard(frame, nowMs);
        return;
    }
    if (state_ == SessionState::AwaitingPasswordRequest && kind == FrameKind::PasswordRequest) {
        state_ = SessionState::Authenticating;
        stateSinceMs_ = nowMs;
        if (frame.payload[0] == 1) {
            output_.sendPasswordCreate = true;
        } else {
            output_.sendPasswordLogin = true;
        }
        return;
    }
    if (state_ == SessionState::Authenticating && kind == FrameKind::SessionAck) {
        if (frame.payload[0] == 1) {
            startStreaming(nowMs);
        } else if (frame.payload[0] == 4) {
            output_.sendPasswordCreate = true;
            stateSinceMs_ = nowMs;
        } else if (frame.payload[0] == 2) {
            output_.wrongPassword = true;
            enterReconnectWait(nowMs);
        }
    }
}

void SpeedBikeSession::sendNotice(uint32_t nowMs) {
    output_.sendNotice = true;
    lastNoticeAtMs_ = nowMs;
    ++noticeCount_;
}

SessionState SpeedBikeSession::state() const {
    return state_;
}

SessionOutput SpeedBikeSession::takeOutput() {
    SessionOutput result = output_;
    output_ = {};
    return result;
}

void SpeedBikeSession::enterReconnectWait(uint32_t nowMs) {
    state_ = SessionState::ReconnectWait;
    stateSinceMs_ = nowMs;
}

void SpeedBikeSession::startStreaming(uint32_t nowMs) {
    state_ = SessionState::Streaming;
    stateSinceMs_ = nowMs;
    lastDataAtMs_ = nowMs;
}

void SpeedBikeSession::publishDashboard(const DecodedFrame &frame, uint32_t nowMs) {
    uint16_t rpm = 0;
    if (!parseDashboardRpm(frame, rpm)) {
        return;
    }
    lastDataAtMs_ = nowMs;
    output_.hasRpm = true;
    output_.rpm = rpm;
    output_.noData = false;
}
