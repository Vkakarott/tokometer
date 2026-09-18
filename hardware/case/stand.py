"""Generates the tokometer desk stand, one part at a time.

Built in steps instead of as a closed box: a solid base first, with the board
and the OLED module added on top later. Run inside Fusion (Scripts and Add-Ins,
or the Fusion MCP); it builds into the active scratch design and rebuilds in
place on every run.

All dimensions are millimetres; Fusion's API works in centimetres, so every
value goes through `cm()`. Coordinates: X to the viewer's right, Y up,
Z towards the viewer. The base sits on Y = 0 and is centred on X = 0, with its
front edge on Z = 0 growing towards -Z.
"""

import adsk.core
import adsk.fusion

# --- Board: ESP32 DevKit 30-pin (ESP-WROOM-32), USB-C on a short edge
BOARD_L, BOARD_W = 51.5, 28.5

# --- Base: square, sized by the board's longest side plus a skirt
PADDING = 3.0
BASE_T = 4.0

BASE_SIDE = BOARD_L + 2 * PADDING
BASE_W = BASE_SIDE
BASE_D = BASE_SIDE

LOG_PATH = "/tmp/tokometer_stand.log"
EXPORT_DIR = "/Users/lucas/Documents/Projetos/Pessoal/harware/tokEsp/hardware/case"

NEW = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation
CUT = adsk.fusion.FeatureOperations.CutFeatureOperation


def cm(mm: float) -> float:
    return mm / 10.0


def log(message: str) -> None:
    """Fusion swallows stdout, and a message box would block the script."""
    with open(LOG_PATH, "a") as handle:
        handle.write(message + "\n")


class Builder:
    """Thin helpers over the Fusion API, all taking millimetres."""

    def __init__(self, comp: adsk.fusion.Component) -> None:
        self.comp = comp

    def _plane_at_y(self, y: float) -> adsk.fusion.ConstructionPlane:
        planes = self.comp.constructionPlanes
        plane_input = planes.createInput()
        plane_input.setByOffset(
            self.comp.xZConstructionPlane, adsk.core.ValueInput.createByReal(cm(y))
        )
        return planes.add(plane_input)

    def slab(self, y: float, x0: float, z0: float, x1: float, z1: float, height: float, op, target=None):
        """Footprint on the horizontal plane at y, extruded `height` upwards."""
        sketch = self.comp.sketches.add(self._plane_at_y(y))
        # On the XZ plane the sketch Y axis runs along -Z of the model.
        sketch.sketchCurves.sketchLines.addTwoPointRectangle(
            adsk.core.Point3D.create(cm(x0), cm(-z0), 0),
            adsk.core.Point3D.create(cm(x1), cm(-z1), 0),
        )
        extrudes = self.comp.features.extrudeFeatures
        ext_input = extrudes.createInput(sketch.profiles.item(0), op)
        ext_input.setDistanceExtent(False, adsk.core.ValueInput.createByReal(cm(height)))
        if target is not None and op != NEW:
            ext_input.participantBodies = [target]
        feature = extrudes.add(ext_input)
        return feature.bodies.item(0) if op == NEW else target


def build_base(builder: Builder):
    """Plain solid plate: the board footprint plus a skirt on every side."""
    return builder.slab(0.0, -BASE_W / 2, 0.0, BASE_W / 2, -BASE_D, BASE_T, NEW)


def export(design: adsk.fusion.Design, body, filename: str) -> None:
    manager = design.exportManager
    options = manager.createC3MFExportOptions(body, "{}/{}".format(EXPORT_DIR, filename))
    manager.execute(options)


def run(context):
    app = adsk.core.Application.get()
    try:
        open(LOG_PATH, "w").close()
        design = adsk.fusion.Design.cast(app.activeProduct)
        # Always build into an empty scratch design. Deleting bodies would break
        # the Fusion MCP, which holds references across the call and rolls the
        # timeline back when it fails, so start a fresh document instead.
        if design is None or app.activeDocument.isSaved or design.rootComponent.bRepBodies.count:
            app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
            design = adsk.fusion.Design.cast(app.activeProduct)

        builder = Builder(design.rootComponent)
        base = build_base(builder)
        base.name = "stand_base"
        app.activeViewport.fit()
        export(design, base, "stand_base.3mf")
        log("base {} x {} x {} mm".format(BASE_W, BASE_T, BASE_D))
    except Exception:
        import traceback

        log("FAILED:\n" + traceback.format_exc())
        raise


if __name__ == "__main__":
    run(None)
