#include "speedbike_protocol.h"

uint8_t speedbikeCrc(const uint8_t *bytes, size_t length) {
    uint16_t sum = 0;
    for (size_t index = 0; index < length; ++index) {
        sum += bytes[index];
    }
    return static_cast<uint8_t>(0x100 - (sum & 0xff));
}

size_t buildFrame(uint8_t op, uint8_t command, const uint8_t *payload, uint8_t length,
                  uint8_t *out, size_t capacity) {
    const size_t total = 6U + length;
    if (capacity < total) {
        return 0;
    }
    out[0] = 0x54;
    out[1] = 0x46;
    out[2] = op;
    out[3] = command;
    out[4] = length;
    for (uint8_t index = 0; index < length; ++index) {
        out[5 + index] = payload[index];
    }
    out[5 + length] = speedbikeCrc(out, 5 + length);
    return total;
}

size_t buildSessionNotice(uint8_t *out, size_t capacity) {
    const uint8_t payload[] = {0x0a};
    return buildFrame(0x57, 0x58, payload, sizeof payload, out, capacity);
}

size_t buildPasswordFrame(uint8_t *out, size_t capacity) {
    const uint8_t payload[] = {0x0b, 1, 1, 1, 1, 1, 1};
    return buildFrame(0x57, 0x58, payload, sizeof payload, out, capacity);
}

void FrameDecoder::push(const uint8_t *bytes, size_t length) {
    for (size_t index = 0; index < length; ++index) {
        if (size_ == sizeof bytes_) {
            discardFirst();
        }
        bytes_[size_++] = bytes[index];
    }
}

bool FrameDecoder::next(DecodedFrame &out) {
    while (size_ >= 2) {
        if (bytes_[0] != 0x54 || bytes_[1] != 0x46) {
            discardFirst();
            continue;
        }
        if (size_ < 5) {
            return false;
        }
        const size_t total = 6U + bytes_[4];
        if (bytes_[4] > sizeof out.payload) {
            discardFirst();
            continue;
        }
        if (size_ < total) {
            return false;
        }
        if (speedbikeCrc(bytes_, total - 1) != bytes_[total - 1]) {
            discardFirst();
            continue;
        }
        out.op = bytes_[2];
        out.command = bytes_[3];
        out.length = bytes_[4];
        for (uint8_t index = 0; index < out.length; ++index) {
            out.payload[index] = bytes_[5 + index];
        }
        for (size_t index = total; index < size_; ++index) {
            bytes_[index - total] = bytes_[index];
        }
        size_ -= total;
        return true;
    }
    return false;
}

void FrameDecoder::discardFirst() {
    for (size_t index = 1; index < size_; ++index) {
        bytes_[index - 1] = bytes_[index];
    }
    --size_;
}

FrameKind classifyFrame(const DecodedFrame &frame) {
    if (frame.op == 0x52 && frame.command == 0x58 && frame.length == 1 && frame.payload[0] == 1) {
        return FrameKind::PasswordRequest;
    }
    if (frame.op == 0x41 && frame.command == 0x58) {
        return FrameKind::SessionAck;
    }
    if (frame.op == 0x41 && frame.command == 0x44 && frame.length == 20) {
        return FrameKind::Dashboard;
    }
    return FrameKind::Other;
}

bool parseDashboardRpm(const DecodedFrame &frame, uint16_t &rpm) {
    if (classifyFrame(frame) != FrameKind::Dashboard) {
        return false;
    }
    rpm = static_cast<uint16_t>((frame.payload[2] << 8) | frame.payload[3]);
    return true;
}
