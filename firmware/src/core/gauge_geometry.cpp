#include "gauge_geometry.h"
#include <cmath>
#include "config/speedbike_config.h"
float rpmToAngleDegrees(uint16_t rpm) { const uint16_t capped=rpm>SpeedBikeConfig::MaxRpm?SpeedBikeConfig::MaxRpm:rpm; return 150.0f+240.0f*capped/SpeedBikeConfig::MaxRpm; }
GaugePoint pointOnArc(int16_t cx,int16_t cy,int16_t radius,float degrees){const float radians=degrees*3.14159265358979323846f/180.0f;return {static_cast<int16_t>(lroundf(cx+radius*cosf(radians))),static_cast<int16_t>(lroundf(cy+radius*sinf(radians)))};}
