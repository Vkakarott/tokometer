#pragma once
#include <cstddef>
#include <cstdint>
uint8_t speedbikeCrc(const uint8_t *bytes, size_t length);
size_t buildFrame(uint8_t op,uint8_t command,const uint8_t *payload,uint8_t length,uint8_t *out,size_t capacity);
size_t buildSessionNotice(uint8_t *out,size_t capacity);
size_t buildPasswordFrame(uint8_t *out,size_t capacity);
