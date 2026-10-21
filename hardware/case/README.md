# Tokometer enclosure revisions

The latest revision is **v5, a serviceable assembly prototype**. It includes
physical guides, locating pins, screw holes and captive-nut pockets. The v4
exterior study remains preserved. The enclosure is not yet released for final
printing: measured electronics footprints are used, but module thicknesses,
offsets, connectors and printed tolerances still need verification.

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
represents the final JD9853 module. The existing 40.0 × 31.8 mm window and
48.9 × 33.0 mm PCB measurements in `../dimensions.md` remain unchanged.

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
deep. The display window is 40.0 × 31.8 mm, with reference PCB footprints of
48.9 × 33.0 mm for the JD9853 and 16.0 × 20.7 mm for the ESP32-C3.

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
