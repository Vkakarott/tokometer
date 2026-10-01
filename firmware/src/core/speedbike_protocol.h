#pragma once

#include <cstddef>
#include <cstdint>

struct DecodedFrame { uint8_t op; uint8_t command; uint8_t length; uint8_t payload[32]; };
class FrameDecoder { public: void push(const uint8_t *bytes, size_t length); bool next(DecodedFrame &out); private: void discardFirst(); uint8_t bytes_[96]{}; size_t size_ = 0; };
uint8_t speedbikeCrc(const uint8_t *bytes, size_t length);
size_t buildFrame(uint8_t op, uint8_t command, const uint8_t *payload, uint8_t length, uint8_t *out, size_t capacity);
size_t buildSessionNotice(uint8_t *out, size_t capacity);
size_t buildPasswordFrame(uint8_t *out, size_t capacity);
