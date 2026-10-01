#include "byte_ring.h"

void ByteRing::push(const uint8_t *bytes, size_t length) {
    for (size_t index = 0; index < length; ++index) {
        if (size_ == Capacity) {
            tail_ = (tail_ + 1) % Capacity;
            --size_;
        }
        bytes_[head_] = bytes[index];
        head_ = (head_ + 1) % Capacity;
        ++size_;
    }
}

bool ByteRing::pop(uint8_t &byte) {
    if (size_ == 0) return false;
    byte = bytes_[tail_];
    tail_ = (tail_ + 1) % Capacity;
    --size_;
    return true;
}
