# SpeedBike RPM Gauge Implementation Plan

> For agentic workers: use superpowers:subagent-driven-development or superpowers:executing-plans; steps use - [ ] checkboxes

## Goal

Replace the existing TokEsp application on `feat/speed-gauge` with a focused ESP32/JD9853 SpeedBike RPM display: boot logo, automatic BLE connection and authentication, and a live animated gauge.

## Architecture

Hardware-free protocol, decoder, smoothing, geometry, and session-state code lives in `firmware/src/core` so native Unity tests can own the behavioral contract. `speedbike_link` is the only NimBLE boundary; it supplies bytes and link transitions to `app_controller`, which wires the pure session outputs to BLE and display. `display_manager` owns boot/status screens and SPI region pushes, while `gauge_renderer` owns only gauge pixels.

## Tech Stack

- C++17, Arduino framework, PlatformIO
- ESP32 DevKit + JD9853 SPI display
- h2zero/NimBLE-Arduino 2.x, Adafruit GFX, Pillow, Unity

## Spec path

`docs/superpowers/specs/2026-10-01-speedbike-rpm-gauge-design.md`

## Global Constraints

Device name: `speedbike_050102`.

Password bytes: `TF W X 07 0B 01 01 01 01 01 01`.

Timeouts: 15s / 10s / 2s / 3s.

Scale: 0-12000.

Red start: 10000.

Smoothing: 0.25 / 20 RPM / ~33ms.

pt-BR UI strings: `PROCURANDO`, `SPEEDBIKE_050102`, `CONECTANDO...`, `AUTENTICANDO...`, `SEM DADOS`.

Code: English.

Commits: Conventional Commits.

Commit trailers: no Co-authored-by.

Git remote: never push.

Before each commit: run `pio test -e native`.

## Review Focus

| Risky input | Owning task and required test |
| --- | --- |
| Password request split across notifications | Task 3: feed `TF R X 01 01 BA` in two chunks and assert one emitted frame. |
| Dashboard frame arriving before or without a password request | Task 6: dispatch a dashboard frame in `AwaitingPasswordRequest`, assert transition to `Streaming` and RPM output. |
| Disconnect during `Authenticating` | Task 6: dispatch `linkDisconnected`, then tick before/after two seconds and assert rescan behavior. |
| `millis()` wraparound during timeouts | Task 6: start near `UINT32_MAX`, tick across zero, and assert expiry exactly with unsigned subtraction. |
| Ring-buffer overflow during a notification burst | Task 7: inject more than capacity, assert oldest bytes are dropped and a trailing valid frame resynchronizes. |

## File Structure

| Action | Path | Responsibility |
| --- | --- | --- |
| Modify | `platformio.ini` | Retain one ESP32 environment, add NimBLE and logo embedding, retain the core-only native filter. |
| Create | `firmware/src/config/speedbike_config.h` | Immutable device, BLE, timing, scale, and display constants. |
| Create | `firmware/src/core/speedbike_protocol.h/.cpp` | Hardware-free framing, CRC, frame decoder, classification, and RPM parser. |
| Create | `firmware/src/core/rpm_smoother.h/.cpp` | Hardware-free RPM target/needle animation. |
| Create | `firmware/src/core/gauge_geometry.h/.cpp` | Hardware-free RPM-to-angle conversion. |
| Create | `firmware/src/core/speedbike_session.h/.cpp` | Hardware-free state machine and commands. |
| Create | `firmware/src/ble/speedbike_link.h/.cpp` | NimBLE scan/connect/subscribe/write and guarded byte ring. |
| Replace | `firmware/src/app/app_controller.h/.cpp` | Non-blocking integration of link, session, and display. |
| Replace | `firmware/src/display/display_manager.h/.cpp` | Display initialization, logo/status rendering, and gauge-region SPI push. |
| Create | `firmware/src/display/gauge_renderer.h/.cpp` | Gauge-only indexed-canvas drawing. |
| Replace | `firmware/src/main.cpp` | 20 ms startup and application tick. |
| Create | `tools/convert_logo.py` | Reproducibly converts the source PNG into the SPI-ready logo header. |
| Keep | `firmware/assets/logo-speed-bike.png` | User-provided static boot-logo source image. |
| Create | `firmware/src/assets/speedbike_logo.h` | Generated PROGMEM RGB565 big-endian logo bytes and dimensions. |
| Create | `firmware/test/test_speedbike_protocol/test_speedbike_protocol.cpp` | Protocol, builder, decoder, and parser Unity tests. |
| Create | `firmware/test/test_rpm_smoother/test_rpm_smoother.cpp` | Smoother Unity tests. |
| Create | `firmware/test/test_gauge_geometry/test_gauge_geometry.cpp` | Geometry Unity tests. |
| Create | `firmware/test/test_speedbike_session/test_speedbike_session.cpp` | State, timeout, and early-dashboard Unity tests. |
| Delete | `firmware/src/network/`, `firmware/src/timekeeping/`, `firmware/src/input/`, `firmware/src/storage/` | Removed Wi-Fi/backend/clock/input/persistence behavior. |
| Delete | `firmware/src/config/api_config.h`, `firmware/src/config/clock_config.h`, `firmware/src/config/board_esp32_i2c.h`, `firmware/src/config/board_heltec_v2.h` | Removed configuration and board variants. |
| Delete | `firmware/src/core/api_codec.*`, `firmware/src/core/text_format.*`, `firmware/src/core/usage_types.h`, `firmware/test/test_api_codec/`, `firmware/test/test_text_format/` | Removed TokEsp data model and tests. |
| Delete | `firmware/src/display/tft_renderer.*`, `screen_renderer.*`, `status_screens.*`, `usage_layouts.*`, `widgets.*`, `firmware/src/assets/provider_icons.h`, `firmware/src/assets/wifi_icon.h` | Removed usage dashboard UI. |

## Task 1 — Reduce the firmware to the SpeedBike build surface

> **Prerequisite:** The user’s uncommitted `firmware/src/display/display_manager.cpp` and `firmware/src/main.cpp` edits must be committed or stashed by the user before this task begins.

Files

- Create: `firmware/src/config/speedbike_config.h`.
- Modify: `platformio.ini`, `firmware/src/config/hardware_config.h`, `firmware/src/main.cpp`, `firmware/src/display/display_manager.h/.cpp`, `firmware/src/display/indexed_canvas.h`.
- Delete: every removal listed in File Structure, plus `firmware/src/state/app_state.h`.

Interfaces

- Consumes: `void initializeDisplay();`
- Produces: `void initializeDisplay();`, `void updateApp(uint32_t nowMs);`, `void drawApp(uint32_t nowMs);`.

- [ ] Write the failing build check by replacing `platformio.ini` with this complete environment definition, then run `pio run -e esp32_jd9853` and expect **FAIL** until the obsolete includes and source references are removed:

```ini
[platformio]
default_envs = esp32_jd9853
src_dir = firmware/src
include_dir = firmware/include
lib_dir = firmware/lib
test_dir = firmware/test

[env:esp32_jd9853]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    h2zero/NimBLE-Arduino@^2.3.6
    adafruit/Adafruit GFX Library@^1.12.4
build_flags = -DCORE_DEBUG_LEVEL=3 -DTOKESP_BOARD_ESP32_JD9853
monitor_speed = 115200
test_ignore = *

[env:native]
platform = native
build_src_filter = +<core/>
test_build_src = yes
build_flags = -std=c++17
```

- [ ] Keep the existing JD9853 `display_manager` initialization, SPI write path, `IndexedCanvas`, and `drawCenteredText`; remove only usage, pairing, Wi-Fi, and clock screen dependencies. Do not replace the display manager with an empty stub. Delete the listed obsolete files/directories and all `TOKESP_BOARD_*` branches except `TOKESP_BOARD_ESP32_JD9853`.

```cpp
// firmware/src/config/hardware_config.h
#pragma once
#include "config/board_esp32_jd9853.h"

// firmware/src/config/speedbike_config.h
#pragma once
#include <cstdint>
namespace SpeedBikeConfig {
constexpr char DeviceName[] = "speedbike_050102";
constexpr uint8_t Password[] = {1, 1, 1, 1, 1, 1};
constexpr uint32_t PasswordRequestTimeoutMs = 15000;
constexpr uint32_t AuthenticationTimeoutMs = 10000;
constexpr uint32_t ReconnectDelayMs = 2000;
constexpr uint32_t NoDataTimeoutMs = 3000;
constexpr uint16_t MinRpm = 0;
constexpr uint16_t MaxRpm = 12000;
constexpr uint16_t RedZoneRpm = 10000;
}

// firmware/src/display/display_manager.h
#pragma once
#include <cstdint>
void initializeDisplay();
void drawApp(uint32_t nowMs);

// firmware/src/display/display_manager.cpp
// Preserve the existing JD9853 initialization, SPI write helpers, IndexedCanvas
// expansion, and drawCenteredText implementation. Remove only the old screen
// renderer, usage-layout, pairing, Wi-Fi, and clock calls.

// firmware/src/app/app_controller.h
#pragma once
#include <cstdint>
void initializeApp();
void updateApp(uint32_t nowMs);

// firmware/src/app/app_controller.cpp
#include "app/app_controller.h"
void initializeApp() {}
void updateApp(uint32_t) {}

// firmware/src/main.cpp
#include <Arduino.h>
#include "app/app_controller.h"
#include "display/display_manager.h"
namespace { constexpr uint32_t LoopIntervalMs = 20; }
void setup() { Serial.begin(115200); initializeDisplay(); initializeApp(); }
void loop() { const uint32_t nowMs = millis(); updateApp(nowMs); drawApp(nowMs); delay(LoopIntervalMs); }
```

- [ ] Run `pio run -e esp32_jd9853` and expect **PASS**.
- [ ] Run `pio test -e native` and expect **PASS**, then commit with `git add platformio.ini firmware/src/config firmware/src/main.cpp firmware/src/display firmware/src/app firmware/src/core firmware/test && git commit -m "refactor: strip tokesp firmware for speedbike"`.

## Task 2 — Add frame CRC and command builders

Files

- Create: `firmware/src/core/speedbike_protocol.h/.cpp`, `firmware/test/test_speedbike_protocol/test_speedbike_protocol.cpp`.

Interfaces

- Consumes: byte spans supplied by callers.
- Produces: `uint8_t speedbikeCrc(const uint8_t *bytes, size_t length);`, `size_t buildFrame(uint8_t op, uint8_t command, const uint8_t *payload, uint8_t length, uint8_t *out, size_t capacity);`, `size_t buildSessionNotice(uint8_t *out, size_t capacity);`, `size_t buildPasswordFrame(uint8_t *out, size_t capacity);`.

- [ ] Write this failing Unity test, run `pio test -e native -f test_speedbike_protocol`, and expect **FAIL** because the header does not yet exist:

```cpp
#include <unity.h>
#include "speedbike_protocol.h"
void test_known_crc_frames() {
  const uint8_t request[] = {0x54,0x46,0x52,0x58,0x01,0x01};
  const uint8_t answer[] = {0x54,0x46,0x41,0x5a,0x01,0x01};
  TEST_ASSERT_EQUAL_HEX8(0xba, speedbikeCrc(request, sizeof request));
  TEST_ASSERT_EQUAL_HEX8(0xc9, speedbikeCrc(answer, sizeof answer));
}
void test_builders_match_captured_bytes() {
  uint8_t out[16]{};
  const uint8_t notice[] = {0x54,0x46,0x57,0x58,0x01,0x0a,0xac};
  const uint8_t password[] = {0x54,0x46,0x57,0x58,0x07,0x0b,0x01,0x01,0x01,0x01,0x01,0x01,0x9f};
  TEST_ASSERT_EQUAL_UINT(sizeof notice, buildSessionNotice(out, sizeof out));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(notice, out, sizeof notice);
  TEST_ASSERT_EQUAL_UINT(sizeof password, buildPasswordFrame(out, sizeof out));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(password, out, sizeof password);
}
int main(int, char**) { UNITY_BEGIN(); RUN_TEST(test_known_crc_frames); RUN_TEST(test_builders_match_captured_bytes); return UNITY_END(); }
```

- [ ] Implement these complete files:

```cpp
// firmware/src/core/speedbike_protocol.h
#pragma once
#include <cstddef>
#include <cstdint>
uint8_t speedbikeCrc(const uint8_t *bytes, size_t length);
size_t buildFrame(uint8_t op, uint8_t command, const uint8_t *payload, uint8_t length, uint8_t *out, size_t capacity);
size_t buildSessionNotice(uint8_t *out, size_t capacity);
size_t buildPasswordFrame(uint8_t *out, size_t capacity);

// firmware/src/core/speedbike_protocol.cpp
#include "speedbike_protocol.h"
#include "config/speedbike_config.h"
uint8_t speedbikeCrc(const uint8_t *bytes, size_t length) { uint16_t sum=0; for(size_t i=0;i<length;++i) sum+=bytes[i]; return static_cast<uint8_t>(0x100-(sum&0xff)); }
size_t buildFrame(uint8_t op,uint8_t command,const uint8_t *payload,uint8_t length,uint8_t *out,size_t capacity) { const size_t total=6u+length; if(capacity<total) return 0; out[0]=0x54; out[1]=0x46; out[2]=op; out[3]=command; out[4]=length; for(uint8_t i=0;i<length;++i) out[5+i]=payload[i]; out[5+length]=speedbikeCrc(out,5+length); return total; }
size_t buildSessionNotice(uint8_t *out, size_t capacity) { const uint8_t payload[]={0x0a}; return buildFrame(0x57,0x58,payload,sizeof payload,out,capacity); }
size_t buildPasswordFrame(uint8_t *out, size_t capacity) { const uint8_t payload[]={0x0b,1,1,1,1,1,1}; return buildFrame(0x57,0x58,payload,sizeof payload,out,capacity); }
```

- [ ] Run `pio test -e native -f test_speedbike_protocol` and expect **PASS**.
- [ ] Run `pio test -e native` and expect **PASS**, then commit with `git add firmware/src/core/speedbike_protocol.h firmware/src/core/speedbike_protocol.cpp firmware/test/test_speedbike_protocol/test_speedbike_protocol.cpp && git commit -m "feat: add speedbike protocol builders"`.

## Task 3 — Decode the BLE byte stream without allocation

Files

- Modify: `firmware/src/core/speedbike_protocol.h/.cpp`, `firmware/test/test_speedbike_protocol/test_speedbike_protocol.cpp`.

Interfaces

- Consumes: `void FrameDecoder::push(const uint8_t *bytes, size_t length);`.
- Produces: `bool FrameDecoder::next(DecodedFrame &out);`, where `struct DecodedFrame { uint8_t op; uint8_t command; uint8_t length; uint8_t payload[32]; };`.

- [ ] Append this failing test, run `pio test -e native -f test_speedbike_protocol`, and expect **FAIL** because `FrameDecoder` is undefined:

```cpp
void test_decoder_handles_split_garbage_and_bad_crc() {
  FrameDecoder decoder; DecodedFrame frame{};
  const uint8_t garbage[]={0x00,0xff,0x54}; const uint8_t first[]={0x46,0x52,0x58}; const uint8_t second[]={0x01,0x01,0xba};
  decoder.push(garbage,sizeof garbage); decoder.push(first,sizeof first); TEST_ASSERT_FALSE(decoder.next(frame));
  decoder.push(second,sizeof second); TEST_ASSERT_TRUE(decoder.next(frame)); TEST_ASSERT_EQUAL_HEX8(0x52,frame.op); TEST_ASSERT_EQUAL_HEX8(0x58,frame.command);
  const uint8_t bad[]={0x54,0x46,0x41,0x5a,0x01,0x01,0x00}; decoder.push(bad,sizeof bad); TEST_ASSERT_FALSE(decoder.next(frame));
}
void test_decoder_reassembles_dashboard_from_three_chunks() {
  FrameDecoder decoder; DecodedFrame frame{}; const uint8_t packet[]={0x54,0x46,0x41,0x44,0x14,0,0,0x0b,0xb8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x0a};
  decoder.push(packet,4); decoder.push(packet+4,11); decoder.push(packet+15,sizeof(packet)-15);
  TEST_ASSERT_TRUE(decoder.next(frame)); TEST_ASSERT_EQUAL_UINT8(20,frame.length); TEST_ASSERT_EQUAL_HEX8(0x0b,frame.payload[2]); TEST_ASSERT_EQUAL_HEX8(0xb8,frame.payload[3]);
}
```

- [ ] Extend the header and implementation with this complete decoder code:

```cpp
// append to speedbike_protocol.h
struct DecodedFrame { uint8_t op; uint8_t command; uint8_t length; uint8_t payload[32]; };
class FrameDecoder { public: void push(const uint8_t *bytes,size_t length); bool next(DecodedFrame &out); private: uint8_t bytes_[96]{}; size_t size_=0; };

// append to speedbike_protocol.cpp
void FrameDecoder::push(const uint8_t *bytes,size_t length) { for(size_t i=0;i<length;++i) { if(size_==sizeof bytes_) { for(size_t j=1;j<size_;++j) bytes_[j-1]=bytes_[j]; --size_; } bytes_[size_++]=bytes[i]; } }
bool FrameDecoder::next(DecodedFrame &out) { while(size_>=2) { if(bytes_[0]!=0x54||bytes_[1]!=0x46) { for(size_t i=1;i<size_;++i) bytes_[i-1]=bytes_[i]; --size_; continue; } if(size_<5) return false; const size_t total=6+bytes_[4]; if(bytes_[4]>sizeof out.payload) { for(size_t i=1;i<size_;++i) bytes_[i-1]=bytes_[i]; --size_; continue; } if(size_<total) return false; if(speedbikeCrc(bytes_,total-1)!=bytes_[total-1]) { for(size_t i=1;i<size_;++i) bytes_[i-1]=bytes_[i]; --size_; continue; } out.op=bytes_[2]; out.command=bytes_[3]; out.length=bytes_[4]; for(size_t i=0;i<out.length;++i) out.payload[i]=bytes_[5+i]; for(size_t i=total;i<size_;++i) bytes_[i-total]=bytes_[i]; size_-=total; return true; } return false; }
```

- [ ] Run `pio test -e native -f test_speedbike_protocol` and expect **PASS**.
- [ ] Run `pio test -e native` and expect **PASS**, then commit with `git add firmware/src/core/speedbike_protocol.h firmware/src/core/speedbike_protocol.cpp firmware/test/test_speedbike_protocol/test_speedbike_protocol.cpp && git commit -m "feat: decode speedbike ble frames"`.

## Task 4 — Classify frames and parse dashboard RPM

Files

- Modify: `firmware/src/core/speedbike_protocol.h/.cpp`, `firmware/test/test_speedbike_protocol/test_speedbike_protocol.cpp`.

Interfaces

- Consumes: `const DecodedFrame &`.
- Produces: `enum class FrameKind { Other, PasswordRequest, SessionAck, Dashboard };`, `FrameKind classifyFrame(const DecodedFrame &frame);`, `bool parseDashboardRpm(const DecodedFrame &frame, uint16_t &rpm);`.

- [ ] Add this failing test, run `pio test -e native -f test_speedbike_protocol`, and expect **FAIL**:

```cpp
void test_classifies_and_parses_dashboard() {
  DecodedFrame d{0x41,0x44,20,{0,0,0x0b,0xb8}}; uint16_t rpm=0;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FrameKind::Dashboard),static_cast<int>(classifyFrame(d)));
  TEST_ASSERT_TRUE(parseDashboardRpm(d,rpm)); TEST_ASSERT_EQUAL_UINT16(3000,rpm);
  d.length=3; TEST_ASSERT_FALSE(parseDashboardRpm(d,rpm));
  DecodedFrame request{0x52,0x58,1,{1}}; TEST_ASSERT_EQUAL_INT(static_cast<int>(FrameKind::PasswordRequest),static_cast<int>(classifyFrame(request)));
}
```

- [ ] Append this complete code:

```cpp
// append to speedbike_protocol.h
enum class FrameKind { Other, PasswordRequest, SessionAck, Dashboard };
FrameKind classifyFrame(const DecodedFrame &frame);
bool parseDashboardRpm(const DecodedFrame &frame, uint16_t &rpm);

// append to speedbike_protocol.cpp
FrameKind classifyFrame(const DecodedFrame &frame) { if(frame.op==0x52&&frame.command==0x58&&frame.length==1&&frame.payload[0]==1) return FrameKind::PasswordRequest; if(frame.op==0x41&&frame.command==0x58) return FrameKind::SessionAck; if(frame.op==0x41&&frame.command==0x44&&frame.length==20) return FrameKind::Dashboard; return FrameKind::Other; }
bool parseDashboardRpm(const DecodedFrame &frame,uint16_t &rpm) { if(classifyFrame(frame)!=FrameKind::Dashboard||frame.length<4) return false; rpm=static_cast<uint16_t>((frame.payload[2]<<8)|frame.payload[3]); return true; }
```

- [ ] Run `pio test -e native -f test_speedbike_protocol` and expect **PASS**.
- [ ] Run `pio test -e native` and expect **PASS**, then commit with `git add firmware/src/core/speedbike_protocol.h firmware/src/core/speedbike_protocol.cpp firmware/test/test_speedbike_protocol/test_speedbike_protocol.cpp && git commit -m "feat: parse speedbike dashboard rpm"`.

## Task 5 — Smooth the visible needle

Files

- Create: `firmware/src/core/rpm_smoother.h/.cpp`, `firmware/test/test_rpm_smoother/test_rpm_smoother.cpp`.

Interfaces

- Consumes: `void RpmSmoother::setTarget(uint16_t rpm);`.
- Produces: `uint16_t RpmSmoother::target() const;`, `uint16_t RpmSmoother::step();`, `uint16_t RpmSmoother::displayed() const;`.

- [ ] Write this failing test and run `pio test -e native -f test_rpm_smoother`, expecting **FAIL**:

```cpp
#include <unity.h>
#include "rpm_smoother.h"
void test_converges_snaps_and_clamps() { RpmSmoother s; s.setTarget(16000); TEST_ASSERT_EQUAL_UINT16(12000,s.target()); TEST_ASSERT_EQUAL_UINT16(3000,s.step()); s.setTarget(3010); TEST_ASSERT_EQUAL_UINT16(3010,s.step()); s.setTarget(0); for(int i=0;i<20;++i) s.step(); TEST_ASSERT_LESS_OR_EQUAL_UINT16(20,s.displayed()); }
int main(int,char**) { UNITY_BEGIN(); RUN_TEST(test_converges_snaps_and_clamps); return UNITY_END(); }
```

- [ ] Implement these complete files:

```cpp
// rpm_smoother.h
#pragma once
#include <cstdint>
class RpmSmoother { public: void setTarget(uint16_t rpm); uint16_t target() const; uint16_t displayed() const; uint16_t step(); private: uint16_t target_=0; uint16_t displayed_=0; };

// rpm_smoother.cpp
#include "rpm_smoother.h"
#include "config/speedbike_config.h"
void RpmSmoother::setTarget(uint16_t rpm) { target_=rpm>SpeedBikeConfig::MaxRpm?SpeedBikeConfig::MaxRpm:rpm; }
uint16_t RpmSmoother::target() const { return target_; }
uint16_t RpmSmoother::displayed() const { return displayed_; }
uint16_t RpmSmoother::step() { int32_t gap=static_cast<int32_t>(target_)-displayed_; if(gap>-20&&gap<20) displayed_=target_; else displayed_=static_cast<uint16_t>(static_cast<int32_t>(displayed_)+gap/4); return displayed_; }
```

- [ ] Run `pio test -e native -f test_rpm_smoother` and expect **PASS**.
- [ ] Run `pio test -e native` and expect **PASS**, then commit with `git add firmware/src/core/rpm_smoother.h firmware/src/core/rpm_smoother.cpp firmware/test/test_rpm_smoother/test_rpm_smoother.cpp && git commit -m "feat: smooth speedbike rpm needle"`.

## Task 6 — Model the session in pure core code

Files

- Create: `firmware/src/core/speedbike_session.h/.cpp`, `firmware/test/test_speedbike_session/test_speedbike_session.cpp`.

Interfaces

- Consumes: `void SpeedBikeSession::linkConnecting(uint32_t nowMs);`, `void linkConnected(uint32_t nowMs);`, `void linkDisconnected(uint32_t nowMs);`, `void frame(const DecodedFrame &, uint32_t nowMs);`, `void tick(uint32_t nowMs);`.
- Produces: `SessionState state() const;`, `SessionOutput takeOutput();`, where `struct SessionOutput { bool startScan; bool sendNotice; bool sendPassword; bool hasRpm; uint16_t rpm; bool noData; };`.

- [ ] Write this full failing test and run `pio test -e native -f test_speedbike_session`, expecting **FAIL**:

```cpp
#include <unity.h>
#include "speedbike_session.h"
static DecodedFrame dashboard() { return {0x41,0x44,20,{0,0,0x0b,0xb8}}; }
void test_dashboard_authenticates_without_request() { SpeedBikeSession s; s.linkConnected(10); s.frame(dashboard(),11); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::Streaming),static_cast<int>(s.state())); TEST_ASSERT_TRUE(s.takeOutput().hasRpm); }
void test_disconnect_authenticating_waits_then_scans() { SpeedBikeSession s; s.linkConnected(0); DecodedFrame request{0x52,0x58,1,{1}}; s.frame(request,1); s.linkDisconnected(2); s.tick(2001); TEST_ASSERT_FALSE(s.takeOutput().startScan); s.tick(2002); TEST_ASSERT_TRUE(s.takeOutput().startScan); }
void test_wrap_safe_and_no_data() { SpeedBikeSession s; s.linkConnected(0xfffffff0u); s.tick(0x00000000u); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::AwaitingPasswordRequest),static_cast<int>(s.state())); s.tick(0x00003a90u); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::ReconnectWait),static_cast<int>(s.state())); s.tick(0x00004260u); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::Scanning),static_cast<int>(s.state())); s.linkConnected(100); s.frame(dashboard(),101); s.tick(3102); TEST_ASSERT_TRUE(s.takeOutput().noData); }
int main(int,char**) { UNITY_BEGIN(); RUN_TEST(test_dashboard_authenticates_without_request); RUN_TEST(test_disconnect_authenticating_waits_then_scans); RUN_TEST(test_wrap_safe_and_no_data); return UNITY_END(); }
```

- [ ] Implement these complete files, using unsigned subtraction for all elapsed checks:

```cpp
// speedbike_session.h
#pragma once
#include <cstdint>
#include "speedbike_protocol.h"
enum class SessionState { Boot, Scanning, Connecting, AwaitingPasswordRequest, Authenticating, Streaming, ReconnectWait };
struct SessionOutput { bool startScan=false,sendNotice=false,sendPassword=false,hasRpm=false,noData=false; uint16_t rpm=0; };
class SpeedBikeSession { public: void linkConnecting(uint32_t nowMs); void linkConnected(uint32_t nowMs); void linkDisconnected(uint32_t nowMs); void frame(const DecodedFrame &frame,uint32_t nowMs); void tick(uint32_t nowMs); SessionState state() const; SessionOutput takeOutput(); private: void reconnect(uint32_t nowMs); SessionState state_=SessionState::Boot; uint32_t enteredAt_=0,lastDataAt_=0; SessionOutput output_{}; };

// speedbike_session.cpp
#include "speedbike_session.h"
#include "config/speedbike_config.h"
void SpeedBikeSession::linkConnecting(uint32_t nowMs){state_=SessionState::Connecting;enteredAt_=nowMs;}
void SpeedBikeSession::linkConnected(uint32_t nowMs){state_=SessionState::AwaitingPasswordRequest;enteredAt_=nowMs;output_.sendNotice=true;}
void SpeedBikeSession::reconnect(uint32_t nowMs){state_=SessionState::ReconnectWait;enteredAt_=nowMs;}
void SpeedBikeSession::linkDisconnected(uint32_t nowMs){reconnect(nowMs);}
void SpeedBikeSession::frame(const DecodedFrame &f,uint32_t nowMs){const FrameKind k=classifyFrame(f); if(k==FrameKind::Dashboard){uint16_t rpm; if(parseDashboardRpm(f,rpm)){state_=SessionState::Streaming;lastDataAt_=nowMs;output_.hasRpm=true;output_.rpm=rpm;}} else if(state_==SessionState::AwaitingPasswordRequest&&k==FrameKind::PasswordRequest){state_=SessionState::Authenticating;enteredAt_=nowMs;output_.sendPassword=true;} else if(state_==SessionState::Authenticating&&k==FrameKind::SessionAck){state_=SessionState::Streaming;lastDataAt_=nowMs;}}
void SpeedBikeSession::tick(uint32_t nowMs){if(state_==SessionState::Boot){state_=SessionState::Scanning;output_.startScan=true;} else if(state_==SessionState::ReconnectWait&&nowMs-enteredAt_>=SpeedBikeConfig::ReconnectDelayMs){state_=SessionState::Scanning;output_.startScan=true;} else if(state_==SessionState::AwaitingPasswordRequest&&nowMs-enteredAt_>=SpeedBikeConfig::PasswordRequestTimeoutMs) reconnect(nowMs); else if(state_==SessionState::Authenticating&&nowMs-enteredAt_>=SpeedBikeConfig::AuthenticationTimeoutMs) reconnect(nowMs); else if(state_==SessionState::Streaming&&nowMs-lastDataAt_>=SpeedBikeConfig::NoDataTimeoutMs){output_.hasRpm=true;output_.rpm=0;output_.noData=true;}}
SessionState SpeedBikeSession::state()const{return state_;} SessionOutput SpeedBikeSession::takeOutput(){SessionOutput r=output_;output_={};return r;}
```

- [ ] Run `pio test -e native -f test_speedbike_session` and expect **PASS**.
- [ ] Run `pio test -e native` and expect **PASS**, then commit with `git add firmware/src/core/speedbike_session.h firmware/src/core/speedbike_session.cpp firmware/test/test_speedbike_session/test_speedbike_session.cpp && git commit -m "feat: add speedbike session state machine"`.

## Task 7 — Build the NimBLE transport boundary and core byte ring

Files

- Create: `firmware/src/core/byte_ring.h`, `firmware/test/test_byte_ring/test_byte_ring.cpp`, `firmware/src/ble/speedbike_link.h/.cpp`.
- Modify: `firmware/src/app/app_controller.h/.cpp`.

Interfaces

- Consumes: `void SpeedBikeLink::startScan();`, `void SpeedBikeLink::poll();`, `bool SpeedBikeLink::popByte(uint8_t &byte);`, `bool SpeedBikeLink::takeConnecting();`, `bool SpeedBikeLink::takeConnected();`, `bool SpeedBikeLink::takeDisconnected();`, `bool SpeedBikeLink::write(const uint8_t *bytes, size_t length);`.
- Produces: exact service `fff0`, notify `fff1`, and write `fff2` transport behavior.

- [ ] Write the failing compile integration in `app_controller.cpp`, run `pio run -e esp32_jd9853`, and expect **FAIL** because `SpeedBikeLink` is unavailable:

```cpp
#include "ble/speedbike_link.h"
namespace { SpeedBikeLink link; }
```

- [ ] Implement the transport. Before coding, inspect the NimBLE-Arduino 2.x headers resolved by PlatformIO and use their exact `NimBLEScan`, `NimBLEClient`, `subscribe`, and `writeValue` signatures. Implement this complete project API (callback override declarations must match those inspected 2.x headers):

```cpp
// speedbike_link.h
#pragma once
#include <cstddef>
#include <cstdint>
class SpeedBikeLink { public: void begin(); void startScan(); void poll(); bool popByte(uint8_t &byte); bool takeConnected(); bool takeDisconnected(); bool write(const uint8_t *bytes,size_t length); private: static constexpr size_t RingCapacity=256; uint8_t ring_[RingCapacity]{}; volatile size_t head_=0,tail_=0; volatile bool connected_=false,disconnected_=false; void pushBytes(const uint8_t *bytes,size_t length); };
```

Move storage into the following hardware-free `ByteRing`, injected into the link under its ESP32 critical section. The scan callback must accept only the exact configured name; emit a Connecting event before connecting; connection setup discovers service `fff0`, subscribes to `fff1`, and caches `fff2` for writes. A disconnect callback sets only `disconnected_`; the notification callback calls only the guarded ring push.

- [ ] Write `test_byte_ring` first; it must push 300 garbage bytes followed by the complete `TF R X 01 01 BA`, drain the ring into `FrameDecoder`, and assert that the decoder emits the trailing frame. Run `pio test -e native -f test_byte_ring` and expect **FAIL** until `ByteRing` exists. Implement this complete core header:

```cpp
// firmware/src/core/byte_ring.h
#pragma once
#include <cstddef>
#include <cstdint>
template <size_t Capacity> class ByteRing { public: void push(uint8_t byte) { const size_t next=(head_+1)%Capacity; if(next==tail_) tail_=(tail_+1)%Capacity; bytes_[head_]=byte; head_=next; } bool pop(uint8_t &byte) { if(tail_==head_) return false; byte=bytes_[tail_]; tail_=(tail_+1)%Capacity; return true; } private: uint8_t bytes_[Capacity]{}; size_t head_=0,tail_=0; };
```

- [ ] Run `pio run -e esp32_jd9853` and expect **PASS**.
- [ ] Run `pio test -e native` and expect **PASS**, then commit with `git add firmware/src/core/byte_ring.h firmware/test/test_byte_ring/test_byte_ring.cpp firmware/src/ble/speedbike_link.h firmware/src/ble/speedbike_link.cpp firmware/src/app/app_controller.h firmware/src/app/app_controller.cpp && git commit -m "feat: add speedbike nimble link"`.

## Task 8 — Own boot/status rendering in the display manager

Files

- Modify: `firmware/src/display/indexed_canvas.h`, `firmware/src/display/display_manager.h/.cpp`.
- Create: `tools/convert_logo.py`, `firmware/src/assets/speedbike_logo.h`.
- Keep: `firmware/assets/logo-speed-bike.png`.

Interfaces

- Consumes: `void setDisplayStatus(const char *text);`, `void drawBootLogo(uint32_t nowMs);`.
- Produces: `IndexedCanvas &gaugeCanvas();`, `void pushGaugeRegion();`.

- [ ] Write the failing hardware compile call in `app_controller.cpp`, run `pio run -e esp32_jd9853`, and expect **FAIL** until the display manager exports the interfaces:

```cpp
setDisplayStatus("PROCURANDO");
drawBootLogo(nowMs);
```

- [ ] Implement the complete palette declaration replacement and public display interface:

```cpp
// indexed_canvas.h: replace the final four palette enum entries and values
Orange, OrangeDim, Mint, MintDim, Red, RedDim, OrangeHot, DimText
// Palette entries 12 and 13 become rgb565(255,64,64), rgb565(128,24,24); total entries remain 16.

// display_manager.h
#pragma once
#include <cstdint>
#include "display/indexed_canvas.h"
void initializeDisplay();
void drawApp(uint32_t nowMs);
void setDisplayStatus(const char *text);
void drawBootLogo(uint32_t nowMs);
IndexedCanvas &gaugeCanvas();
void pushGaugeRegion();
```

`tools/convert_logo.py` uses Pillow with type hints and `pathlib`: crop transparent or empty borders, resize to fit 296x240 while preserving aspect ratio, center on the Background color, and emit `firmware/src/assets/speedbike_logo.h` with the smallest cropped dimensions, width/height constants, and a PROGMEM RGB565 array encoded as big-endian bytes for direct SPI use. `display_manager.cpp` streams that array row-by-row to the panel without canvas allocation, holds it for roughly two seconds, then continues. Use the existing `drawCenteredText` implementation for `PROCURANDO`, `SPEEDBIKE_050102`, `CONECTANDO...`, and `AUTENTICANDO...`. `pushGaugeRegion()` must transfer only the 296x240 gauge rectangle, not the entire screen.

- [ ] Run `pio run -e esp32_jd9853` and expect **PASS**.
- [ ] Run `python3 tools/convert_logo.py`, record the generated byte count, run `pio test -e native` and expect **PASS**, then commit with `git add tools/convert_logo.py firmware/assets/logo-speed-bike.png firmware/src/assets/speedbike_logo.h firmware/src/display/indexed_canvas.h firmware/src/display/display_manager.h firmware/src/display/display_manager.cpp && git commit -m "feat: add speedbike boot and status display"`.

## Task 9 — Render and test the RPM gauge

Files

- Create: `firmware/src/core/gauge_geometry.h/.cpp`, `firmware/test/test_gauge_geometry/test_gauge_geometry.cpp`, `firmware/src/display/gauge_renderer.h/.cpp`.

Interfaces

- Consumes: `float rpmToAngle(uint16_t rpm);`, `void drawGauge(IndexedCanvas &canvas, uint16_t needleRpm, uint16_t targetRpm, bool noData);`.
- Produces: a 240-degree 0--12000 RPM gauge with 1000-RPM major marks, 500-RPM minor marks, and a red zone from 10000.

- [ ] Write the failing native geometry test and run `pio test -e native -f test_gauge_geometry`, expecting **FAIL**:

```cpp
#include <unity.h>
#include "gauge_geometry.h"
void test_rpm_to_angle_clamps_endpoints() { TEST_ASSERT_FLOAT_WITHIN(0.001f,-210.0f,rpmToAngle(0)); TEST_ASSERT_FLOAT_WITHIN(0.001f,30.0f,rpmToAngle(12000)); TEST_ASSERT_FLOAT_WITHIN(0.001f,30.0f,rpmToAngle(65535)); }
int main(int,char**) { UNITY_BEGIN(); RUN_TEST(test_rpm_to_angle_clamps_endpoints); return UNITY_END(); }
```

- [ ] Implement the complete geometry files and renderer interface:

```cpp
// gauge_geometry.h
#pragma once
#include <cstdint>
float rpmToAngle(uint16_t rpm);

// gauge_geometry.cpp
#include "gauge_geometry.h"
#include "config/speedbike_config.h"
float rpmToAngle(uint16_t rpm) { const uint16_t capped=rpm>SpeedBikeConfig::MaxRpm?SpeedBikeConfig::MaxRpm:rpm; return -210.0f+240.0f*capped/SpeedBikeConfig::MaxRpm; }

// gauge_renderer.h
#pragma once
#include <cstdint>
#include "display/indexed_canvas.h"
void drawGauge(IndexedCanvas &canvas,uint16_t needleRpm,uint16_t targetRpm,bool noData);
```

`gauge_renderer.cpp` must draw the 296x240 canvas from scratch: a 240-degree arc; major ticks/labels 0--12 at 1000 RPM; minor ticks at 500 RPM; Cyan/CyanDim normal arc/ticks/labels/needle; Red/RedDim 10000+ zone and needle; and a large target RPM readout with `RPM`. It must call `rpmToAngle`, use thick needle strokes, and have no boot-logo or status responsibility.

- [ ] Run `pio test -e native -f test_gauge_geometry` and expect **PASS**.
- [ ] Run `pio test -e native` and expect **PASS**, then commit with `git add firmware/src/core/gauge_geometry.h firmware/src/core/gauge_geometry.cpp firmware/test/test_gauge_geometry/test_gauge_geometry.cpp firmware/src/display/gauge_renderer.h firmware/src/display/gauge_renderer.cpp && git commit -m "feat: render speedbike rpm gauge"`.

## Task 10 — Wire the app and verify on the bike

Files

- Modify: `firmware/src/app/app_controller.h/.cpp`, `firmware/src/main.cpp`.

Interfaces

- Consumes: `SpeedBikeLink`, `SpeedBikeSession`, `FrameDecoder`, `RpmSmoother`, `setDisplayStatus`, `drawGauge`, `pushGaugeRegion`.
- Produces: `void initializeApp();`, `void updateApp(uint32_t nowMs);`, with a non-blocking 20 ms loop and approximately 33 ms gauge cadence.

- [ ] Write the failing hardware compile integration, run `pio run -e esp32_jd9853`, and expect **FAIL** until all production objects are wired:

```cpp
namespace { constexpr uint32_t GaugeIntervalMs=33; uint32_t lastGaugeAtMs=0; }
```

- [ ] Implement this complete `updateApp` control flow (the functions named here are the exact signatures established in earlier tasks):

```cpp
void updateApp(uint32_t nowMs) {
  link.poll();
  if (link.takeConnected()) session.linkConnected(nowMs);
  if (link.takeDisconnected()) session.linkDisconnected(nowMs);
  uint8_t byte=0; while (link.popByte(byte)) { decoder.push(&byte,1); DecodedFrame frame{}; while (decoder.next(frame)) session.frame(frame,nowMs); }
  session.tick(nowMs); const SessionOutput out=session.takeOutput();
  if (out.startScan) link.startScan();
  if (out.sendNotice) { uint8_t frame[16]; link.write(frame,buildSessionNotice(frame,sizeof frame)); }
  if (out.sendPassword) { uint8_t frame[16]; link.write(frame,buildPasswordFrame(frame,sizeof frame)); }
  if (out.hasRpm) smoother.setTarget(out.rpm);
  if (session.state()==SessionState::Scanning) setDisplayStatus("PROCURANDO\nSPEEDBIKE_050102");
  else if (session.state()==SessionState::Connecting) setDisplayStatus("CONECTANDO...");
  else if (session.state()==SessionState::AwaitingPasswordRequest || session.state()==SessionState::Authenticating) setDisplayStatus("AUTENTICANDO...");
  else if (session.state()==SessionState::Streaming && nowMs-lastGaugeAtMs>=GaugeIntervalMs) { lastGaugeAtMs=nowMs; if(out.noData) smoother.setTarget(0); drawGauge(gaugeCanvas(),smoother.step(),smoother.target(),out.noData); pushGaugeRegion(); }
}
```

- [ ] Run `pio run -e esp32_jd9853` and expect **PASS**.
- [ ] Flash and manually verify: boot logo appears for about two seconds while scanning starts; device selects only `speedbike_050102`; it subscribes to `fff1`, writes to `fff2`, sends notice and password; it reconnects after each disconnect/timeout; `SEM DADOS` appears after three seconds without a dashboard frame while the link remains; and the needle follows bike RPM without interaction.
- [ ] Run `pio test -e native` and expect **PASS**, then commit with `git add firmware/src/app/app_controller.h firmware/src/app/app_controller.cpp firmware/src/main.cpp && git commit -m "feat: wire speedbike rpm application"`.

## Self-review

- Spec coverage: boot, exact BLE transport, captured protocol bytes, reassembly, state/timeouts, data loss, smoother, palette, gauge, scope removals, native tests, and bike verification each have a task.
- Placeholder scan: no `TBD`, `TODO`, ellipsis, or cross-task implementation reference is present; the logo asset is explicitly user-supplied before Task 8.
- Signature consistency: protocol interfaces are consumed by Session and Link; Session outputs are consumed by App Controller; renderer and display interfaces are consumed only by App Controller; all natively tested interfaces reside under `firmware/src/core` without Arduino headers.
