# Native display preview

Run from the repository root after PlatformIO has installed the JD9853 dependencies:

```sh
sh firmware/tools/preview/render.sh
```

The output goes to the ignored `.pio/display-preview/` directory. Pass an absolute output directory as the first argument to save elsewhere. A C++17 compiler is required. On macOS, `sips` also converts each PPM into a PNG.

The harness compiles the actual firmware renderer, indexed canvas, text formatters, and installed Adafruit GFX implementation. The host stubs only replace Arduino types and text output; they do not implement rendering. The 296×240 output therefore uses the same geometry, fonts, and RGB565 palette as the display. It does not model the physical LCD's brightness, viewing angle, or SPI transport.

Fixtures cover Codex, Claude, Cursor, overview, both usage windows, 0%, 100%, missing providers and windows, reset values, stale data, and an unsynchronized clock. Cursor fixtures use its actual included-plan, API, and on-demand fields; they do not assign 5-hour or 7-day subscription windows. The usage values and clock are deterministic synthetic inputs, not collected account data. The `status-*` images cover the screens shown before the dashboard: welcome, Wi-Fi setup, pairing, loading and error.

For a 4× nearest-neighbor contact sheet of the generated output, run `python3 firmware/tools/preview/contact_sheet.py`. This optional step requires Pillow and uses only the native preview images.
