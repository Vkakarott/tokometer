#include <Arduino.h>
#include "display/display_manager.h"
namespace { constexpr uint32_t LoopIntervalMs=20; }
void setup() { Serial.begin(115200); initializeDisplay(); showBootLogo(); delay(2000); showStatus("PROCURANDO", "SPEEDBIKE_050102"); }
void loop() { delay(LoopIntervalMs); }
