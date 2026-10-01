# SpeedBike RPM Gauge Design

**Date:** 2026-10-01

**Branch:** `feat/speed-gauge`

**Status:** Approved

## Goal

Build a minimal ESP32 + JD9853 replacement for the SpeedBike app. When powered on near the bike, the device must show a boot logo, connect to `speedbike_050102`, authenticate automatically with the default password `111111`, and animate an RPM gauge from streamed dashboard frames. No user interaction is required.

This branch is independent from TokEsp. It reuses the firmware structure only.

## Success Criteria

1. The device powers on near the bike.
2. It discovers and connects to `speedbike_050102` over BLE.
3. It authenticates automatically using the default password.
4. The displayed gauge needle follows the bike RPM stream.

## Scope

### Included

- Boot logo.
- BLE scanning, connection, notification subscription, and authentication.
- Dashboard-frame reassembly and RPM extraction.
- Animated RPM gauge and connection/data status.

### Excluded

- Firmware updates and bootloader support.
- Wi-Fi, backend, synchronization, maps, settings, and timekeeping.
- Other dashboard fields.
- Buttons/input.
- Multiple-device support.

## BLE Protocol

Protocol sources are `Work/jiga-ble speedbike-*.ts` and `Work/mobile-app speedbike-protocol`.

| Item | Value |
| --- | --- |
| Device name | `speedbike_050102` |
| Service UUID | `fff0` |
| Notification characteristic | `fff1` |
| Write characteristic | `fff2` |
| Read operation | `0x52` (`R`) |
| Write operation | `0x57` (`W`) |
| Answer operation | `0x41` (`A`) |
| Session command | `0x58` (`X`) |
| Dashboard command | `0x44` (`D`) |

Each frame has the following layout:

```text
54 46 op cmd len payload crc
```

The CRC is calculated across every preceding frame byte:

```text
crc = (0x100 - (sum(previous_bytes) & 0xff)) & 0xff
```

### Authentication Exchange

The runtime authentication flow is sourced from `mobile-app` `usePasswordFlow.ts` and `useUpdateFlow.ts`. The `jiga-ble` flow is for the factory jig only.

1. Send the session notice `TF W X 01 0A` initially, then resend it every 1.2 seconds for up to three total sends.
2. The module answers `TF R X 01 <code>`.
3. For request code `1`, create the password with `TF W X 07 0B` followed by `111111`. For every other request code, log in with `TF W X 07 0C` followed by `111111`.
4. The module acknowledges with `TF A X 01 <code>`: code `1` completes authentication, code `2` means the password was rejected and causes a reconnect, and code `4` requests the create-password frame again. Ignore other acknowledgement codes.

A valid dashboard frame received before the password exchange starts streaming directly because the module is already authenticated.

### Dashboard Frames

Dashboard responses have the form:

```text
TF A D 14 <20-byte payload> crc
```

They are 26 bytes in total and may be split across BLE notifications. RPM is big-endian at payload offsets 2 and 3:

```text
rpm = (payload[2] << 8) | payload[3]
```

## Application State Machine

```text
Boot -> Scanning -> Connecting -> AwaitingPasswordRequest -> Authenticating -> Streaming
```

| State | Behavior | Exit / timeout |
| --- | --- | --- |
| Boot | Show the logo for about two seconds while scanning is already active. | Move to Scanning after the logo period. |
| Scanning | Scan for the exact configured device name. | Matching device found: Connecting. |
| Connecting | Connect and subscribe to `fff1`. | Subscription ready: AwaitingPasswordRequest. |
| AwaitingPasswordRequest | Send the session notice and wait for `TF R X`. | Password request: Authenticating; 15-second timeout: reconnect flow. |
| Authenticating | Send the password and wait for `TF A X` or a dashboard frame. | Success: Streaming; 10-second timeout: reconnect flow. |
| Streaming | Decode dashboard frames and render RPM. | Disconnect: reconnect flow. |

On any disconnect or connection/authentication timeout, wait two seconds and then resume scanning indefinitely. If no dashboard frame arrives for three seconds, set the gauge target to zero, show `SEM DADOS`, and retain the BLE connection.

NimBLE callbacks must only append received bytes to a guarded ring buffer and report link state. `updateApp()` drains that buffer on the 20 ms application tick, evaluates timeouts using `millis()`, and performs all protocol/state work. No callback or application path may block.

## Frame Reassembly and Validation

The decoder operates on a byte stream rather than individual notifications:

1. Resynchronize by finding the `0x54 0x46` (`TF`) header.
2. Once a header is found, wait until at least `5 + len + 1` bytes are buffered.
3. Validate the CRC.
4. Emit valid frames to the application.
5. Drop garbage bytes and invalid-CRC frames, then continue searching for a header.

This supports both fragmented dashboard messages and arbitrary garbage before or between valid frames.

## Display Design

### Boot and Status

- Embed the logo GIF with `embed_files` and render it using AnimatedGIF. A single-frame GIF is acceptable.
- Use pt-BR status strings:
  - `PROCURANDO`
  - `SPEEDBIKE_050102`
  - `CONECTANDO...`
  - `AUTENTICANDO...`
  - `SEM DADOS`

### Gauge

Render a 296 x 240 gauge region with:

- A 240-degree arc covering 0 to 12,000 RPM.
- Major ticks and labels at every 1,000 RPM (`0` through `12`).
- Minor ticks at every 500 RPM.
- A red zone on the arc and labels from 10,000 RPM upward.
- A thick needle that turns red above 10,000 RPM.
- A large digital RPM readout and the `RPM` label.
- The normal arc, ticks, labels, and needle use Cyan/CyanDim; the 10,000+ zone and the needle above 10,000 use Red/RedDim. Palette entries Purple/PurpleDim (Cursor-only, removed) are replaced by Red/RedDim, keeping 16 entries.

The displayed needle is smoothed approximately every 33 ms by moving 0.25 of the remaining distance to the target; it snaps to the target when the difference is below 20 RPM. Both target and displayed values are clamped to 0--12,000 RPM. The digital readout displays the real target RPM, not the smoothed needle value.

Redraw the gauge canvas and push only the gauge rectangle over SPI at approximately 30 FPS. A 40 MHz SPI setting can be considered later if required.

## Firmware Structure

### Keep

- `display_manager`, including region push support; it owns the boot logo and status screens via `drawCenteredText`.
- Indexed canvas support.
- `drawCenteredText`.
- `board_esp32_jd9853.h`.
- The native environment.
- The 20 ms main loop.

### Add

| Path | Responsibility |
| --- | --- |
| `config/speedbike_config.h` | Device name, password, timeout values, RPM scale, and red-zone start. |
| `core/speedbike_protocol` | Hardware-free frame builder/CRC, stream decoder, and RPM parser. |
| `core/rpm_smoother` | Hardware-free needle smoothing and clamping. |
| `ble/speedbike_link` | NimBLE scan/connect/subscribe/write operations, received bytes, and link state. |
| `app/app_controller` | Non-blocking application state machine. |
| `display/gauge_renderer` | RPM gauge rendering only (arc, ticks, labels, needle, readout). |

Add the `h2zero/NimBLE-Arduino` dependency. It is preferred over Bluedroid to preserve memory beside the approximately 35 KB canvas and GIF requirements.

### Remove

- `network/`, `timekeeping/`, `api_config.h`, and `clock_config.h`.
- `core/api_codec`, `usage_types`, `text_format` and its tests.
- `tft_renderer`, `usage_layouts`, `widgets`, `provider_icons.h`, and `wifi_icon.h`.
- Input/button handling and `storage/device_store`.
- Heltec and `esp32_i2c` environments, their board headers, and `TOKESP_BOARD_*` conditionals.
- ArduinoJson and U8g2.

## Tests and Verification

Add native Unity tests:

| Test | Required coverage |
| --- | --- |
| `test_speedbike_protocol` | CRCs for `TF R X 01 01 BA` and `TF A Z 01 01 C9`; notice and password bytes matching jiga-ble; dashboard reassembly from two and three chunks; garbage and bad-CRC rejection; `0B B8` parsing to 3000 RPM. |
| `test_rpm_smoother` | Convergence, snap threshold, and clamping. |

Manual verification:

```sh
pio run -e esp32_jd9853
```

Flash the device and test it with the bike: after power-on near the bike, confirm automatic connection/authentication and that the needle tracks RPM without interaction.
