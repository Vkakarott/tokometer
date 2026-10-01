#include "rpm_smoother.h"

#include "config/speedbike_config.h"

void RpmSmoother::setTarget(uint16_t rpm) {
    target_ = rpm > SpeedBikeConfig::MaxRpm ? SpeedBikeConfig::MaxRpm : rpm;
}

uint16_t RpmSmoother::target() const {
    return target_;
}

uint16_t RpmSmoother::displayed() const {
    return displayed_;
}

uint16_t RpmSmoother::step() {
    const int32_t gap = static_cast<int32_t>(target_) - displayed_;
    if (gap > -20 && gap < 20) {
        displayed_ = target_;
    } else {
        displayed_ = static_cast<uint16_t>(static_cast<int32_t>(displayed_) + gap / 4);
    }
    return displayed_;
}
