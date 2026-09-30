#pragma once

#include "Arduino.h"

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t value) = 0;

    size_t write(const uint8_t *data, size_t size) {
        size_t written = 0;
        while (size-- > 0) written += write(*data++);
        return written;
    }

    size_t write(const char *text) {
        return write(reinterpret_cast<const uint8_t *>(text), std::strlen(text));
    }

    size_t print(const char *text) { return write(text); }
    size_t print(const String &text) { return write(text.c_str()); }
    size_t print(char value) { return write(static_cast<uint8_t>(value)); }
    size_t print(int value) { return print(std::to_string(value)); }
    size_t print(unsigned value) { return print(std::to_string(value)); }
};
