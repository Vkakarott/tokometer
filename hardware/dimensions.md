# Hardware dimensions

Measured dimensions of the parts the case has to fit. All values are in
millimetres. Add a row only for something that was actually measured; leave
unknowns in the "Still to measure" list instead of estimating them here.

## Display module (JD9853 TFT, 240×296)

Width is the long side, matching the landscape orientation the firmware draws
in (296 px wide, 240 px tall).

| Item | Width | Height | Notes |
|---|---|---|---|
| Case window (visible display area) | 40.0 | 31.8 | Opening the case front must have. |
| Display board (PCB) | 48.9 | 33.0 | Footprint the case must house behind the window. |

The board is larger than the window by 8.9 mm in width and 1.2 mm in height in
total. How that excess is split between the sides is not recorded yet, so the
window cannot be assumed to be centred on the board.

## Still to measure

- Position of the visible area on the display board (left, right, top and
  bottom margins).
- Display module thickness: PCB, glass stack and any components on the back.
- Position and size of the display header or flex connector.
- ESP32-C3 Mini board: length, width, thickness, USB connector position and
  BOOT / RESET button positions.
