#pragma once

#include <cstdint>

class RpmSmoother {
public:
    void setTarget(uint16_t rpm);
    uint16_t target() const;
    uint16_t displayed() const;
    uint16_t step();

private:
    uint16_t target_ = 0;
    uint16_t displayed_ = 0;
};
