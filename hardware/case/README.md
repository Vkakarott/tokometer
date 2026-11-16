# Tokometer enclosure revisions

The latest revision is **v7** (see the last section): an ESP32 devkit lying
upside down at the rear with its USB-C through a smaller rear opening, a base
with only screw joints and the ESP's supports, and notched front screw tabs.
Before it, **v6, the layout A prototype** (display on four posts, battery
module with its USB-C toward the rear, ESP32-C3 in front of it) reuses the v5 shell, joints and fasteners; **v5 stays preserved** as the
serviceable assembly prototype with physical guides, locating pins, screw holes
and captive-nut pockets, and the v4 exterior study is preserved too. The
enclosure is not yet released for final printing: measured electronics
footprints are used, but module thicknesses, offsets, connectors and printed
tolerances still need verification. Current measured values live in
`../dimensions.md`.

## Preserved baseline

`stand.py`, `stand_body.3mf`, and `stand_base.3mf` are the original revision.
The actual Fusion document before the aesthetic changes is archived in
`backups/tokometer_stand_2026-10-07.f3d`. The cloud copy is named
`tokometer_stand_backup_2026-10-07` in Default Project.

## Refined revision (v2)

`stand_v2.py` generates `tokometer_stand_v2` using the original generator's
mechanical geometry. Its outputs are isolated in `v2/`:

- `tokometer_stand_v2.f3d`: native Fusion design, including the feature timeline.
- `stand_v2_body.3mf` and `stand_v2_base.3mf`: the two printable parts.
- `preview.png`: actual Fusion viewport image of the assembled enclosure.
- `validation.json`: solid dimensions, timeline health, and assembly contact checks.

The revision makes the screen bezel smaller and shallower, softens exterior
edges, raises and simplifies the decorative drive, and adds an inset lower
fascia. The base has a sloping front and a rounded lip; its width increases
from 52.0 to 52.8 mm. Both sides have a low grille of 4 × 9 rounded recesses.
These recesses are 0.55 mm deep and do **not** provide through ventilation.

The 52 mm body width, 57.7 mm body height, ESP32 location, header-pin slots,
USB opening, OLED aperture and mounting rails, and snap interface retain
their baseline dimensions. The horizontal OLED dictates the display aspect
ratio, so this is an adaptation of the visual reference to the real hardware.

Ivory, warm grey, graphite, and terracotta are Fusion appearance previews.
The printable exports contain geometry only; the accent colors require
painting or a separate slicer color assignment. The display and ESP32 mock
bodies are excluded from the printable exports.

### Regenerate

Open a new empty Fusion design. Run `stand_v2.py` using Fusion's Python API
(Scripts and Add-Ins, or the Python Text Commands console):

```python
import runpy
runpy.run_path("/absolute/path/to/tokEsp/hardware/case/stand_v2.py", run_name="__main__")
```

The script refuses to modify an existing/saved design. It creates the geometry
in the empty document and overwrites only the revision's files in `v2/`.
Save the new Fusion document separately after inspecting it. The script also
captures the preview; it can be refreshed after changing the view with:

```python
adsk.core.Application.get().activeViewport.saveAsImageFile("/absolute/path/to/tokEsp/hardware/case/v2/preview.png", 1200, 1000)
```

### Validation

The generated revision has 119 healthy timeline entries and four solid bodies
(two printable parts and two component mocks). Assembly analysis found no
unexpected interference. The inherited OLED retention nubs intentionally
overlap the mock board by approximately 2.2765 mm³ in total; the script checks
that every contact vertex stays within those four nub regions.

Both 3MF meshes were checked for valid triangle indices and closed edges
(each undirected edge belongs to exactly two triangles). Both the baseline
and v2 native archives were reopened in Fusion with healthy timelines. Printed tolerances,
snap fit, surface finish, and OLED retention still require a physical test.

## Two-slope rear and molded edges (v3)

`stand_v3.py` generates the separate `tokometer_stand_v3` design. The original
and v2 documents, scripts' default behavior, and generated models are preserved.
The outputs live in `v3/`, including the native `.f3d`, body/base `.3mf` files,
three actual Fusion views (`preview.png`, `side.png`, `rear.png`), and validation.
The cloud document is saved separately as `tokometer_stand_v3` in Default Project.

The lower rear retains its approximately 1.99° inward lean, less than the
5.74° front lean. A second rear slope starts 10 mm below the top and leans
26° from vertical. These are design choices guided by the reference image,
not measurements of the photographed object. The upper cavity follows this
slope with a 2 mm normal offset; the lower cavity remains unchanged.

Exterior corners and the rear shoulder use continuous 2 mm fillets instead
of the previous intersecting front chamfers. The lower shell edge has a
0.35 mm edge break. The screen entry and throat use 0.7 and 0.5 mm fillets.
Drive/fascia borders have 0.15 mm fillets; the eject accent moves up 0.25 mm
to clear the rounded drive border. The base uses a simpler sloping profile
with a continuous 1.2 mm perimeter radius instead of the thin stepped lip.

The OLED opening/rails, ESP32 location, pin slots, USB opening, and snap
interface retain their original geometry. The display aspect ratio, recessed
decorative grilles, and color/printing caveats described for v2 still apply.

### Regenerate and validate v3

Run `stand_v3.py` through Fusion's Python API in a new empty design, as for v2.
Keep `stand.py` and `stand_v2.py` beside it: v3 imports their shared geometry
and detailing helpers. It refuses to modify an existing/saved document and
overwrites only its own exports under `v3/`.

The generated design has 117 healthy timeline entries and four solid bodies.
Assembly analysis found only the inherited OLED retention-nub contact
(2.2765 mm³); no unexpected interference was found. The exported body and
base meshes have 31,752 and 3,576 triangles respectively. The script checks
valid indices, two triangles per undirected edge, and consistent winding.
The native archive was reopened in Fusion with all four solids and a healthy
timeline. Physical fit, print finish, and snap durability remain untested.

## Macintosh-inspired exterior study (v4)

`stand_v4.py` generates the separate `tokometer_stand_v4_design` document.
The cloud copy is saved under that name in Default Project.
It keeps v3's overall design scale and two-slope rear profile while changing
the visual identity of the front:

- Separate front bezel with a broad, flat perimeter bevel and small edge breaks.
- Larger, near-square screen surround with a deeper recess and dark inner border.
- Thin floppy slot shifted right, with a stepped right-hand pocket and small accent.
- A small six-color badge at the lower left, replacing the wide inset label panel.
- Narrower, recessed grey foot and a nonfunctional keyboard-port detail.
- Five long horizontal side recesses instead of the small grille grid.
- Rear USB cable opening restored from v3: 13 × 7 mm, 1.5 mm corner radius,
  centered horizontally at 13.7 mm above the bottom of the enclosure.

The screen opening is 35.5 × 28.3 mm **at the study scale**. The dark screen
body is only an appearance placeholder. Neither its opening nor its body
represents the final JD9853 module. (When v4 was made, `../dimensions.md` held
a 40.0 × 31.8 mm window and a 48.9 × 33.0 mm PCB; both were later revised to
38.5 × 31.0 and 48.87 × 33.0.)

The four concept solids are the rear shell, front bezel, recessed foot and
screen placeholder. Old OLED rails, ESP32 DevKit mounts and snap features
are deliberately not carried into this concept. Those mechanical details
must be redesigned after the appearance is approved and
the enclosure is enlarged for the JD9853 and ESP32-C3 Mini. The final display
offset, module thickness, fasteners, wall thicknesses and tolerances still
need engineering verification. The grille recesses are decorative, not vents.
The rear USB opening is a through-cut at the inherited v3 position; alignment
and plug clearance for the ESP32-C3 Mini still require verification.

### Outputs and validation

The `v4/` directory contains `tokometer_stand_v4_design.f3d`, actual Fusion
viewport images (`preview.png`, `front.png`, `side.png`, `rear.png`), and
`validation.json`. No printable 3MF files are exported for this design study.
Appearances are illustrative; they do not define a multicolor printing setup.

Run `stand_v4.py` through Fusion's Python API in a new empty document. Keep
`stand.py`, `stand_v2.py` and `stand_v3.py` beside it for shared helpers.
The generator refuses to modify an existing/saved document and overwrites
only its own outputs under `v4/`. Prior revision outputs are preserved.

The concept has 68 healthy timeline entries and four valid solids. Native
interference analysis found no volumetric overlaps. The exported native
archive was reopened successfully with all four solids and a healthy timeline.
These checks validate
the CAD study, not fit, assembly, ventilation, printability or manufacturability.

## Serviceable enclosure prototype (v5)

Open **`tokometer_stand_v5_assembly`**, not the older v4 design study, to inspect
the mechanical closure. `stand_v5.py` and `stand_v5_spec.py` generate a separate
document and write only to `v5/`; all earlier revisions remain unchanged.

### Mechanical closure

- The front frame attaches to the base using two underside M3 screws. Two
  2.4 mm locating pins on the base enter 3.0 mm blind sockets in the frame.
- Two 1.8 mm wide, 2.2 mm tall longitudinal rails on the base engage channels
  in the rear cover with 0.3 mm clearance per side and above the rail.
- Front side/top tongues align the cover with the bezel. The seam has a
  nominal 0.25 mm separation; the cover's screw tabs seat on the base.
- Four additional underside M3 screws lock the cover to the base. Each screw
  threads into a metal hex nut inserted into an inward-facing pocket; there
  are no printed threads or flexible snap hooks.
- The underside holes are 3.4 mm through, with 6.2 mm diameter, 3.4 mm deep
  head recesses. The six closure screw references are M3 × 16, modeled without
  threads, with 5.5 mm diameter × 3 mm heads. Match actual fastener dimensions.
- Nut channels are 5.8 mm wide and 2.8 mm high, sized around M3 nuts with
  5.5 mm across flats and 2.4 mm thickness. These nominal nut dimensions are
  documented by [Accu's DIN 934 specification](https://www.accu.co.uk/hexagon-nuts/766459-NUT203M3).
  Check actual hardware and a printed fit sample before production printing.

The guides locate the parts; **the metal screws and nuts provide retention**.
The cover will not lock without the four cover screws installed.

### Assembly and opening

1. Insert the two front-frame nuts through their inward-facing slots. Seat
   the frame on the two base pins and install its two underside screws.
2. Fit the display behind the bezel and secure its separate retaining frame.
   Fit the ESP32-C3 on the base and secure its separate retaining bracket.
   These supports are provisional until module depths, offsets and headers
   are measured; do not clamp the glass or unknown rear components.
3. Connect and route the wiring inside, away from the rails, screw tips and
   the moving cover. Keep the external USB cable disconnected.
4. Insert four nuts into the cover pockets. Lower the cover onto the rail
   entries behind the front assembly, then slide it forward to meet the bezel.
5. Install the four cover screws from underneath. The two frame screws stay
   installed for normal servicing.
6. To open, disconnect USB, remove only the four cover screws, and slide the
   cover rearward. The front, base and electronics supports remain together.

The display retainer uses four M3 clearances/nut pockets and the ESP retainer
uses two. Their nominal fasteners are M3 × 6 and M3 × 8 respectively; confirm
the final stack after measurement. A nominal 0.2 mm gap behind each mock PCB
allows a fitted insulating shim; it is not a verified PCB clamping fit.

### Size and unresolved measurements

The new design envelope is approximately 68 mm wide × 84 mm tall × 65 mm
deep. The display window is 38.5 × 31.0 mm, with reference PCB footprints of
48.87 × 33.0 mm for the JD9853 and 16.0 × 20.0 mm for the ESP32-C3 (current
measurements; `v5/validation.json` was generated earlier with the previous
40.0 × 31.8 / 20.7 values and has not been regenerated).

`stand_v5_spec.py` explicitly labels the **unmeasured modeling assumptions**:
4 mm display stack, 1.6 mm PCB thicknesses, centered display PCB, ESP PCB
underside at 14 mm and USB center at 17.2 mm above the bottom. The 13 × 7 mm
rear opening is retained as a provisional cable envelope. None of those
assumptions is added to the measured record in `../dimensions.md`.

Still required: display margins/stack/connector envelope; ESP headers, buttons,
component heights and USB projection; actual cable clearance and print-fit
testing. The internal retainers must be revised as necessary from those
measurements. Decorative side grooves are not through-ventilation.

### Outputs and checks

- `v5/tokometer_stand_v5_assembly.f3d`: editable native assembly prototype.
- `v5/open.png`: cover hidden, exposing base rails and internal supports.
- `v5/cover_inside.png`: cover alone, exposing guide channels and nut pockets.
- `v5/underside.png`: six recessed closure screw locations.
- `v5/front.png`, `v5/rear.png`, `v5/preview.png`: assembled views.
- `v5/validation.json`: native solid, interference and sampled opening checks.

The generator checks all solids and the Fusion timeline, then samples the
rearward cover travel at 85 positions between 0 and 80 mm. The cover screws
and nuts are excluded from that motion test; the front fasteners remain.
The test includes modeled supports and PCB references, not unmeasured wires,
headers or connectors. It is a sampled CAD test, not a physical fit guarantee.
No final-print meshes are exported. Hardware reference bodies must never be
included in any later print export.

Run the generator in a **new empty Fusion design**, with `stand.py` through
`stand_v4.py` beside it. Run the local checks with:

```sh
python3 -m unittest discover -s hardware/case -p 'test_*.py'
```

## Layout A prototype (v6)

Open **`tokometer_stand_v6_assembly`**. `stand_v6.py` and `stand_v6_spec.py`
generate a separate document and write only to `v6/`. The outer shell, base
joints, cover guides, six M3 closure screws and the opening sequence are the
v5 ones (the generator imports `stand_v5.py`); only the interior is new.
Coordinates: x across, y up from the underside, z negative toward the rear;
the front face leans, so display positions are measured along it.

**All dimensions were re-measured on 2026-11-11** and the earlier ones were
wrong (see `../dimensions.md`). v5 was generated with the superseded values
(38.5 × 31.0 window, 48.87 × 33.0 PCB, 20 × 16 ESP); it stays as a prototype
and is not updated.

### Layout

| Part | Position | Status |
|---|---|---|
| Front opening | 40.0 × 33.5 in the front plate, centred on the PCB (5.5 mm per side horizontally, 1.0 mm vertically) | MEASURED; slightly larger than the lit area |
| Display PCB 51.0 × 35.5 × 1.1 | 6.15–7.25 mm behind the front face | PCB and 3.25 mm total stack MEASURED; the stack is taken as glass front to PCB back (ASSUMED) |
| Display posts | 4 × Ø3.0 at 47.5 × 31.7 mm centres (PCB holes Ø2.0, 45.5 × 29.7 edge to edge), Ø1.3 × 2.8 mm pilot holes for M1.6 self-tapping screws | holes MEASURED, convention confirmed; post and pilot ASSUMED |
| Battery 44.6 × 25.0 × 16.5 | x ±22.3, z −61 to −36, **y 18.5 to 35.0**, on two ribs (x ±15) with a front stop | envelope MEASURED; height and position ASSUMED |
| USB-C board 18 × 20 × 1 | z −62.55 to −42.55, y 11.4–12.4, on 3 bosses with Ø0.9 locating pins; its rear corners stop 0.33 mm short of the inner rear wall; a 20 × 2.0 mm stop (z −42.5 to −40.5, up to y 14.0) sits behind its front edge | board and holes MEASURED; hole pair 3.5 mm from the connector edge and "bottom" = opposite edge CONFIRMED; z position ASSUMED |
| USB-C receptacle | 9.3 × 3.2 × 7.35, centred (x 0), centre at y 14.0, 1.0 mm beyond the board edge; the tip (z −63.55) sits inside the opening tunnel, **1.28 mm** from the outer face | typical values; no measurement needed (user's decision) |
| Rear opening | 13.0 × 7.5, centre y 14.0 (y 10.25–17.75), x 0 | STANDARD plug, see below |
| ESP32-C3 | board 18.0 × 22.7 × 1.0, reserved envelope 11.5 tall, z −31.5 to −8.8, y 9.0–20.5, x ±9, inside a 6 mm rim (0.3 mm clearance) | footprint and 11.5 MEASURED; nothing leaves the case |

The rear opening is sized for a generic USB-C plug with a typical moulded
cover, not a measured cable: cover about 12 × 6.5 mm (metal about 8.4 × 2.6)
plus 0.5 mm per side gives 13.0 × 7.5. The earlier provisional 13 × 7 did not
cover the height.

### Why the battery is raised

The battery is 44.6 mm wide but the cover's screw tabs (|x| ≥ 22.0, up to
y 18.0) sweep the whole length when the cover slides off, leaving only 44.0 mm
between their inner edges. The battery therefore rides on two ribs with its bottom at
y 18.5, 0.5 mm above the tabs; its wings overhang the tab inner edge by 0.3 mm
per side. The USB-C board sits under it, and the 11.5 mm ESP (it would reach
y 20.5 and touch the battery) sits in front of it instead.

### Clearances

- USB-C tip to the rear outer face: 1.28 mm, inside the opening tunnel (it was
  2.83 mm). Flush is not possible without notching the wall: the 18 mm board
  is wider than the 13 mm opening, so its rear corners stop 0.33 mm short of
  the 2 mm wall. The connector has 1.85 mm of clearance per side and 2.15 mm
  above and below inside the opening.
- Battery bottom over the cover tabs: 0.5 mm. USB-C top to battery bottom:
  2.9 mm. Rear opening top to battery bottom: 0.75 mm.
- Battery to the front stop: 0.5 mm. ESP rim to the front stop: 1.5 mm. ESP
  rim to the front screw tabs: 1.0 mm per side.
- The 11.5 mm ESP (y 9.0–20.5) and the display PCB (bottom at about y 38)
  never overlap in height or depth.
- Display screws: M1.6 self-tapping (0.4 mm clearance in the Ø2.0 holes; M2
  would have none). The screws, the display header (irrelevant to the case)
  and all wiring are not modeled.
- **USB-C board fixation (user's decision, plus the stop):** the board is held
  by its three Ø0.9 locating pins in the Ø1.0 holes; no glue and no heat stake.
  A stop wall behind the board (x ±10, z −42.5 to −40.5, y 9 to 14, 2.0 mm
  thick) takes the plug's insertion thrust. Its gap to the board's front edge
  is 0.05 mm, equal to the pins' play per side (0.05 mm); the stop is much
  stiffer than the pins, so it carries the load. It sits 1.6 mm above the board
  top, 4.5 mm below the battery, 5.0 mm from the battery's front stop and 7.5 mm
  from the ESP rim. The gap must be tuned in the printed fit test. The stop only
  helps insertion: pulling the plug out loads the pins and friction alone, and
  nothing holds the board down vertically.
- **Battery retention:** nothing holds the raised battery down or sideways
  except gravity, two ribs, the front stop and the rear wall. The cover tabs
  top out at y 18.0, below the battery bottom at y 18.5, so they do not contain
  it either; only the cover's side walls (|x| = 32) limit sideways travel.

### Outputs and checks

`v6/` holds `tokometer_stand_v6_assembly.f3d`, `open.png` (cover hidden),
`layout_top.png` (interior from above), `cover_inside.png`, `underside.png`,
`front.png`, `rear.png`, `preview.png` and `validation.json` (solids, layout
positions, clearances, measured/standard/assumed values, missing
measurements). The model has 367 healthy timeline entries, 20 solids and no
interference; the cover slides rearward 0–80 mm (85 samples) without touching
any other solid. This is a CAD check, not a physical fit guarantee; no print
meshes are exported.

Run the generator in a **new empty Fusion design**, with `stand.py` through
`stand_v5.py` beside it. If a single call times out, pass `OPTIONS` through
`runpy.run_path(..., init_globals={"OPTIONS": {...}})` to split the stages
(`opening`, `views`, `export`, `finalize_only`, `log`). Local checks:

```sh
python3 -m unittest discover -s hardware/case -p 'test_*.py'
```

### STL files for printing

`hardware/case/stl/` holds the three printable parts, exported from the model
at commit `fa88b80`. Units are mm; the origin is on the bed (each part is centred
on X/Y with its lowest point at Z = 0). The meshes are binary STL, closed and
consistently oriented (every edge shared by exactly two triangles); the volumes
match the native solids within 0.1%. `stl/stl_report.json` has the full check.
No supports are baked in, and the hardware reference bodies are not included.

| File | Orientation | Bounding box X × Y × Z (mm) | Triangles | Bed contact (mm²) |
|---|---|---|---|---|
| `hardware/case/stl/v6_front_frame.stl` | front face down | 67.600 × 74.589 × 14.427 | 26592 | 2399 |
| `hardware/case/stl/v6_rear_cover.stl` | upside down, top on the bed | 68.000 × 60.594 × 75.000 | 9110 | 2996 |
| `hardware/case/stl/v6_service_base.stl` | underside on the bed | 66.000 × 61.343 × 24.500 | 8424 | 3672 |

Print notes: the front frame lies face down, so its funnel (about 58° from
vertical) and the outer bevel (about 47°) are overhangs and may need supports on
the visible face; four zero-area slivers (< 0.0001 mm) were dropped from its mesh.
The cover is upside down with its flat top on the bed, and the base sits on its
underside. These orientations are suggestions for the first test print.

To regenerate: with the v6 model open in Fusion, run `stl_v6.export_raw(<raw
folder>)` through Fusion's Python API (it writes the three bodies in model
coordinates without editing the design), then run the orientation step outside
Fusion (`stl_v6.process(<raw folder>, "hardware/case/stl")`, from
`hardware/case`). The tests in `test_stl_v6.py` check the mesh tools and the
exported files.

### Remaining work

Nothing else needs to be measured (USB-C receptacle, wiring, lit area and the
3.25 mm stack split were closed by the user's decisions; see
`../dimensions.md`). Only the **physical fit test of a printed sample**
remains.

## v7: ESP32 devkit at the rear, cleaned base

Open **`tokometer_stand_v7_assembly`**. `stand_v7.py` and `stand_v7_spec.py`
generate it in a new empty Fusion design and write only to `v7/`; the v6 files
are unchanged. v7 calls the v5/v6 builders, so everything not listed here is
as in v6. Values are tagged MEASURED / DERIVED / STANDARD / ASSUMPTION in the
spec, as before.

### What changed from v6

| Part | Change |
|---|---|
| Rear cover | Only the USB opening: **9.0 x 3.5** (was 13.0 x 7.5), same centre (x 0, y 14.0), reduced symmetrically, 1.5 corner radius. Only the plug's metal shell passes (user's intent). |
| Front frame | A notch in each front screw tab where the ESP PCB passes: x 11.5 to 14.55 (mirrored), z -14.5 to -10.39, y 15.3 to the tab top (18.0), open on top, 0.3 clear of the PCB. It opens up to 0.19 mm into the 3.4 screw hole and unroofs about a quarter of the nut pocket (the nut still bears on its floor). |
| Base | All v6 electronics supports removed (battery ribs and front stop, USB-C board bosses, pins and stop, ESP32-C3 cradle). Added: two **4.2** standoffs under the ESP's rear holes, y 9.0 to 15.6, with **2.2 x 7.0** pilots for M2.5 self-tapping screws (ASSUMPTION), and one plain **8.0 x 1.2** pad under the PCB's front edge (no screw, user's decision). Kept: the six M3 screw holes and head pockets, and, for the user to decide, the v5 guide rails (x +-30) and the two front-frame locating pins (x +-16, z -5.5). |
| References | Only the ESP PCB (28.5 x 51.7 x 1.5, four 3.0 holes), the display references and the fasteners. No battery, no separate USB-C board, no ESP components or USB-C bodies. |

### ESP32 devkit

The board lies flat and **upside down**: components and USB-C face down, the
USB-C edge at the rear wall with the USB-C centred on the rear opening (its
3.2 mm typical body fills y 12.4 to 15.6), so the PCB is at **y 15.6 to 17.1**,
x +-14.25, z -62.39 to -10.69, 0.3 mm from the rear wall (perpendicular to it).
Holes (MEASURED with a caliper): 20.0 between inner edges across (centres
x +-11.5); along the long side the hole edges are 0.9 from the rear edge and
0.7 from the front edge (read as edge distances, so centres 2.4 and 2.2 in;
read to the centre they would be impossible). The rear standoffs leave **18.8**
free between them (the user's limit is 18.0, checked by the spec and the
generator). The front holes have no screw: they sit over the front frame's
screw tabs.

**Rear-wall formula fix.** The v6 spec puts the inner face of the rear wall
about 0.03 mm too far back (its line starts at y 9 instead of y 8, where
`stand_v5.build_shell` starts it). v7 uses the real line (tested against the
v5 profile); v6's wall clearances are therefore about 0.03 mm optimistic. The
v6 files are left as they are.

### Checks

Measured directly on the Fusion solids, independently of the generator: 349
healthy timeline entries, 18 single-lump solids, rear opening 9.0 x 3.5 at
(0, 14.0), both notches empty, PCB to cover **0.300** and to front frame 0.300,
rear standoffs to cover 0.651, **no interference** between any bodies (the PCB
only touches the tops of the standoffs and pad), and no collision while the
cover slides back 0 to 80 mm (85 samples). Volumes (mm3): front frame
15485.19, rear cover 35058.30, base 35684.39, ESP PCB 2167.76.

Not checked by the CAD (listed in `UNVERIFIED_WITHOUT_REFERENCES` and
`PENDING_MEASUREMENTS`): the ESP components under the PCB (3.2 mm assumed)
against the floor, the front nuts and the standoffs' neighbourhood (BOOT/EN
buttons); the ESP's USB-C (9.3 typical, wider than the 9.0 opening by 0.15 per
side); the free strip at the PCB's front edge used by the pad (2.0 assumed from
a photo; it needs at least 1.7); the battery and the separate USB-C board (not
in v7). Still to confirm with the user: 0.9 / 0.7 as edge distances. Still to
test on a print: the notched tabs, the M2.5 pilots and all tolerances.

### STL files

`stl_v7.py` reuses the `stl_v6.py` tools and orientations. With the model open
in Fusion, run `stl_v7.export_raw(<raw folder>)` there, then
`python3 stl_v7.py <raw folder> stl` from `hardware/case`. The files are in
`hardware/case/stl/` (report: `stl/stl_report_v7.json`); units mm, each part
centred on X/Y with its lowest point at Z = 0; closed and consistently oriented
meshes, volumes within 0.11% of the solids.

| File | Orientation | Bounding box X x Y x Z (mm) | Triangles | Bed contact (mm2) |
|---|---|---|---|---|
| `hardware/case/stl/v7_front_frame.stl` | front face down | 67.600 x 74.589 x 14.427 | 26608 | 2399 |
| `hardware/case/stl/v7_rear_cover.stl` | upside down, top on the bed | 68.000 x 60.594 x 75.000 | 9110 | 2996 |
| `hardware/case/stl/v7_service_base.stl` | underside on the bed | 66.000 x 61.343 x 15.600 | 8200 | 3672 |

`v7/validation.json` is written by the generator before the STL step, so its
`print_meshes_exported` stays false; `stl/stl_report_v7.json` is the record of
the exported meshes. Four zero-area slivers were dropped from the front frame
mesh, as in v6.

Run the checks with
`python3 -m unittest discover -s hardware/case -p 'test_*.py'`.
