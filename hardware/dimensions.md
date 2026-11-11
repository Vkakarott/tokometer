# Hardware dimensions

Measured dimensions of the parts the case has to fit. All values are in
millimetres. Add a row only for something that was actually measured; leave
unknowns in the "Still to measure" list instead of estimating them here.

## Display module (JD9853 TFT, 240×296)

Width is the long side, matching the landscape orientation the firmware draws
in (296 px wide, 240 px tall).

| Item | Width | Height | Notes |
|---|---|---|---|
| Case window (opening in the case front) | 38.5 | 31.0 | MEASURED by user (caliper). CONFIRMED by the user as the case opening, slightly larger than the area that lights up. |
| Display board (PCB) | 48.87 | 33.0 | MEASURED by user (caliper). Footprint the case must house behind the window. |

**Revised 2026-11-11.** The window was previously recorded as 40.0 × 31.8 and
the PCB as 48.9 × 33.0. The new measurements are 38.5 × 31.0 for the visible
area (1.5 mm narrower, 0.8 mm shorter) and 48.87 × 33.0 for the PCB (width
changed by 0.03 mm). Where a photo reading disagrees with a value the user measured, the user's
value prevails; photo readings are only used for what the user did not
measure.

The board is larger than the window by 10.37 mm in width and 2.0 mm in height
in total.

### PCB thickness

| Item | Value (mm) | Notes |
|---|---|---|
| Display PCB thickness | 1.1 | MEASURED by user (caliper). PCB only, no glass or components. |

### Window and lit area

**Resolved 2026-11-11 (user's answer).** 38.5 × 31.0 mm is the **opening of
the case**. It is slightly **larger than the area that lights up**, which is
smaller than the opening. The case window therefore needs no further
assumption: the opening is exactly what the front must have.

| Item | Value | Status |
|---|---|---|
| Case opening | 38.5 × 31.0 mm | MEASURED, CONFIRMED as the opening. |
| Lit area | smaller than 38.5 × 31.0 mm | Size not measured; only known to be smaller. |
| Margin per side, long side, PCB edge to the visible region | ~3 mm | MEASURED by user (approximate, "~3 mm"); the user confirms it is correct. |
| Margin per side, short side | 1.0 mm | DERIVED: (33.0 − 31.0) / 2, symmetric. Not measured. |

Note: ~3 mm per side on a 48.87 mm PCB leaves ~42.9 mm, wider than the
opening. That figure is therefore not the lit area and not the opening; it
presumably bounds the glass or module region behind the opening. This is not
confirmed. The earlier 5.19 mm per side (derived from 38.5 mm) stays withdrawn.

## ESP32-C3 Mini board

| Item | Long side | Short side | Notes |
|---|---|---|---|
| Board (PCB) | 20.0 | 16.0 | MEASURED by user (caliper). USB Type-C connector for power and flashing; its USB port does not need to be reachable from outside. |

**Revised 2026-11-11.** The long side was previously recorded as 20.7 mm; the
user's caliper measurement is 20.0 mm and replaces it.

## Battery with charging board

The battery **goes inside the case**, so the device is no longer USB-only. It
is a LiPo cell with a charging board and a USB-C charge connector.

| Item | Value (mm) | Notes |
|---|---|---|
| Width | 42 | MEASURED by user (caliper), wing tip to wing tip. |
| Smaller dimension | 24 | MEASURED by user (caliper). |
| Thickness | 16 | MEASURED by user (caliper). Includes the battery. |

42 × 24 × 16 mm is the real envelope of the battery with its charging board
and is the space the case reserves for it. An earlier photo reading of ~45.65
mm across the wide board is superseded by the user's measurement.

The battery and charging board are expected to fit in the space left over by
the display and the ESP32-C3 Mini; that fit is not checked yet.

## Mounting holes (ESTIMATED from photos, not measured)

Everything in this section is **ESTIMATED** by proportion from photos, using
the known PCB size as the scale. None of it is a caliper measurement. Replace
each value when it is measured; do not design final fits on these numbers.

Display PCB (48.87 × 33.0 mm), 4 holes in a rectangle near the corners:

| Item | ESTIMATED | Uncertainty (mm) |
|---|---|---|
| Hole spacing along the long side | 45.9 | ± 0.6 |
| Hole spacing along the short side | 29.6 | ± 0.8 |
| Hole centre to PCB edge | 1.5 (long side), 1.7 (short side) | ± 0.4 |
| Hole diameter | 1.7 (solder ring about 2.5) | ± 0.4 |

The holes are assumed symmetric about the PCB centre (offset ± 0.5 mm). The
display has an 8-pin header on one short edge; its pin height and direction are
unknown.

Battery charging board, the USB-C tail. The two holes are in its ears. They are
not the wing tips of the wide board, which the 42 mm width refers to:

| Item | ESTIMATED | Uncertainty (mm) |
|---|---|---|
| Hole centre to centre | 17.0 | ± 0.6 |
| Hole diameter | 1.3 | ± 0.4 |
| Hole line behind the USB-C front face | 3.3 | ± 0.8 |
| USB-C connector body width | 9.3 | ± 0.5 |
| Connector beyond the board edge | 1.0 | ± 0.7 |
| Tail length, junction to connector front face | 20 | ± 1.5 |
| Neck width, narrowest / at the junction | 6.3 / 11.8 | ± 0.7 |

Photo reading (ESTIMATED, about ± 0.3 mm): ear to ear of the USB-C tail 20.0 mm.
The wide board also has 5 more holes (2 per wing and 1 in
the middle right); they are not used unless the user decides otherwise.

## Still to measure

- Lit area size and its margins to the opening (only known to be smaller).
- Display module total thickness: front of the glass to the highest point on the
  back (the PCB alone is measured: 1.1 mm).
- Position and size of the display header or flex connector.
- ESP32-C3 Mini board: thickness with components, which edge the USB Type-C
  connector sits on and how far it protrudes, and BOOT / RESET button
  positions.
- USB Type-C opening the case needs for the plug.
- Display PCB and charging-board ear holes: measured diameters and centre
  spacing (the values above are only estimated from photos).
- Display 8-pin header: pin height, direction and position along the edge.
- Battery envelope: whether the USB-C tail lies inside the 24 mm dimension or
  extends beyond it.
- Battery with charging board: position of the USB-C charge connector (which
  edge, offset from the corner, how far it protrudes, connector body size).
- Battery with charging board: power on/off switch (whether one is needed,
  type, position and opening size).
- Battery with charging board: wire routing between the battery, the charging
  board and the ESP32-C3 (wire length, pass-through position, clearance).
