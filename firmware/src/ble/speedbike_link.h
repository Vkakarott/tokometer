#pragma once
#include <cstddef>
#include <cstdint>
class SpeedBikeLink { public: void begin(); void startScan(); bool takeDeviceFound(); bool connect(); bool takeConnected(); bool takeDisconnected(); bool popByte(uint8_t &byte); bool write(const uint8_t *bytes,size_t length); };
