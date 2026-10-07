# Tokometer enclosure revisions

## Preserved baseline

`stand.py`, `stand_body.3mf`, and `stand_base.3mf` are the original revision.
The actual Fusion document before the aesthetic changes is archived in
`backups/tokometer_stand_2026-10-07.f3d`. The cloud copy is named
`tokometer_stand_backup_2026-10-07` in Default Project.

## Refined revision

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
