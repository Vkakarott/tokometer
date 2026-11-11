# Hardware dimensions

Measured dimensions of the parts the case has to fit. All values are in
millimetres. Values tagged MEASURED were taken by the user with a caliper;
DERIVED values are arithmetic on measured ones; STANDARD values are typical
market figures, not measured; ASSUMPTION values are modeling choices and live
in `case/stand_v6_spec.py`, not here.

**Full re-measurement, 2026-11-11.** The user re-measured every part and
reported that the earlier figures were wrong. All earlier display, ESP32-C3,
battery and mounting-hole values (including 48.87 × 33.0, 43 / 27.8,
20 × 16 and 42 × 24 × 16, and all photo estimates of the charging-board holes)
are discarded. Only the values below are current.

## Display module (JD9853 TFT, 240×296)

Width is the long side, matching the landscape orientation the firmware draws
in (296 px wide, 240 px tall).

| Item | Width | Height | Notes |
|---|---|---|---|
| Display board (PCB) | 51.0 | 35.5 | MEASURED. Footprint the case must house behind the opening. |
| Case opening | 40.0 | 33.5 | MEASURED. The opening of the case, slightly larger than the lit area. Centred on the PCB. |

| Item | Value (mm) | Notes |
|---|---|---|
| Display PCB thickness | 1.1 | MEASURED. PCB only. |
| Total thickness | 3.25 | MEASURED. Front of the glass to the highest point on the back, with components. |
| Opening margin to the PCB edge, long side | 5.5 per side | DERIVED: (51.0 − 40.0) / 2, opening centred on the PCB. |
| Opening margin to the PCB edge, short side | 1.0 per side | DERIVED: (35.5 − 33.5) / 2. |

The lit area is slightly smaller than the 40.0 × 33.5 opening; its size is not
measured. The 8-pin header is irrelevant for the case and is ignored.

### Display mounting holes

Four holes in a rectangle near the corners of the PCB, all MEASURED.

| Item | Value (mm) | Notes |
|---|---|---|
| Hole diameter | 2.0 | MEASURED. |
| Long side, inner edge to inner edge | 45.5 | MEASURED, edge of one hole to the nearest edge of the other (convention confirmed by the user). |
| Short side, inner edge to inner edge | 29.7 | MEASURED, same convention (confirmed by the user). |
| Long side, centre to centre | 47.5 | DERIVED: 45.5 + 2.0. |
| Short side, centre to centre | 31.7 | DERIVED: 29.7 + 2.0. |
| Hole centre to PCB edge | 1.75 (long side), 1.9 (short side) | DERIVED: (51.0 − 47.5) / 2 and (35.5 − 31.7) / 2, holes symmetric about the PCB centre. |

## ESP32-C3 Mini board

| Item | Value (mm) | Notes |
|---|---|---|
| Board (PCB), long side × short side | 22.7 × 18.0 | MEASURED. |
| PCB thickness | 1.0 | MEASURED. |
| Total thickness with header and pins | 11.5 | MEASURED. Used only to reserve space. |

The ESP sits entirely inside the case: nothing on it needs to be reached or
leave the case (no USB access, BOOT, RESET or side openings). Confirmed by the
user.

## Battery with charging board

| Item | Value (mm) | Notes |
|---|---|---|
| Envelope width | 44.6 | MEASURED, wing tip to wing tip. |
| Envelope, other dimension | 25.0 | MEASURED. The USB-C tail is not inside these 25 mm. |
| Thickness with battery | 16.5 | MEASURED. |

No power switch is needed.

### USB-C board (independent)

The USB-C board is separate from the battery board.

| Item | Value (mm) | Notes |
|---|---|---|
| Board | 18.0 × 20.0 | MEASURED. |
| Thickness | 1.0 | MEASURED. |
| USB-C position | centred on the board width | MEASURED (stated by the user). It is free to exit at the height the case is designed for. |
| Mounting holes | 3 × Ø1.0 | MEASURED. |
| Two holes on the USB-C line, edge to edge | 15.9 | MEASURED. |
| Two holes on the USB-C line, centre to centre | 16.9 | DERIVED: 15.9 + 1.0. |
| Third hole | centred, 3.4 from the bottom of the board | MEASURED. Which edge "bottom" is, is an assumption (see the spec). |

The neck width of the USB-C tail is not needed.

**Fixation (user's decision, 2026-11-11):** the board is held only by three
locating pins (Ø0.9 mm pins in the Ø1.0 mm holes); no glue and no heat
stake.

## USB cable plug (STANDARD, not measured)

The user has no specific cable, so the rear opening is sized for a generic
USB-C plug with a typical moulded cover. These are market-typical values, not
measurements:

| Item | Value (mm) | Notes |
|---|---|---|
| Plug metal shell | about 8.4 × 2.6 | STANDARD. |
| Moulded cover | about 12 × 6.5 | STANDARD. |
| Clearance to the opening | about 0.5 per side | Chosen by the user. |
| Rear opening | 13.0 × 7.5 | DERIVED: 12 + 1.0 and 6.5 + 1.0, centred on the USB-C. The previous provisional 13 × 7 did not cover the height. |

## Still to measure

- Lit area size and its margins to the opening (only known to be smaller).
- Real USB-C receptacle of the USB-C board: height, length and how far it
  protrudes beyond the board edge, and its height above the board.
- Which edge the third USB-C-board hole is measured from, and where the two
  holes sit along the board length.
- Wire routing between the battery, the USB-C board and the ESP32-C3 (wire
  length, pass-through position, clearance).
- The actual cable plug, only if a specific cable is chosen later.
