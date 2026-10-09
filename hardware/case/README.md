# Tokometer enclosure revisions

The latest revision is **v4, an exterior design study**. It is not a JD9853
hardware-fit model or a print-ready enclosure. The user requested aesthetic
approval before enlarging the case and adapting the electronics.

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

The screen opening is 35.5 × 28.3 mm **at the study scale**. The dark screen
body is only an appearance placeholder. Neither its opening nor its body
represents the final JD9853 module. The existing 40.0 × 31.8 mm window and
48.9 × 33.0 mm PCB measurements in `../dimensions.md` remain unchanged.

The four concept solids are the rear shell, front bezel, recessed foot and
screen placeholder. Old OLED rails, ESP32 DevKit mounts, snap features and
the old USB opening are deliberately not carried into this concept. Those
mechanical details must be redesigned after the appearance is approved and
the enclosure is enlarged for the JD9853 and ESP32-C3 Mini. The final display
offset, module thickness, fasteners, wall thicknesses and tolerances still
need engineering verification. The grille recesses are decorative, not vents.

### Outputs and validation

The `v4/` directory contains `tokometer_stand_v4_design.f3d`, actual Fusion
viewport images (`preview.png`, `front.png`, `side.png`, `rear.png`), and
`validation.json`. No printable 3MF files are exported for this design study.
Appearances are illustrative; they do not define a multicolor printing setup.

Run `stand_v4.py` through Fusion's Python API in a new empty document. Keep
`stand.py`, `stand_v2.py` and `stand_v3.py` beside it for shared helpers.
The generator refuses to modify an existing/saved document and overwrites
only its own outputs under `v4/`. Prior revision outputs are preserved.

The concept has 65 healthy timeline entries and four valid solids. Native
interference analysis found no volumetric overlaps. The exported native
archive was reopened successfully with all four solids and a healthy timeline.
These checks validate
the CAD study, not fit, assembly, ventilation, printability or manufacturability.
