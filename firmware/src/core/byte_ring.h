#pragma once

#include <cstddef>
#include <cstdint>

class ByteRing {
public:
    void push(const uint8_t *bytes, size_t length);
    bool pop(uint8_t &byte);

private:
    static constexpr size_t Capacity = 256;
    uint8_t bytes_[Capacity]{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t size_ = 0;
};
