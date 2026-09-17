"""Generates the tokometer desk case for an ESP32 DevKit with an external OLED.

A plain two-part box. The board lies flat on side rails with its USB-C edge
against the back wall, so the cable leaves at the back; the 0.96" OLED module
stands against the front wall, held by side ribs. The bottom lid presses into
the cavity by friction.

Run inside Fusion (Scripts and Add-Ins, or the Fusion MCP); it builds into a
new document. All dimensions are millimetres; Fusion's API works in
centimetres, so every value goes through `cm()`. Coordinates: X to the viewer's
right, Y up, Z towards the viewer. The front outer face sits on Z = 0 and the
body grows towards -Z; Y = 0 is where the lid meets the body.
"""

import adsk.core
import adsk.fusion

# --- Board: ESP32 DevKit 30-pin (ESP-WROOM-32), USB-C on a short edge
BOARD_L, BOARD_W, PCB_T = 51.5, 28.5, 1.6
PIN_DEPTH = 7.0  # soldered headers below the PCB
BOARD_LIFT = PIN_DEPTH + 0.5  # PCB underside above the lid
USB_SLOT = (13.0, 7.0)  # the cable overmould, not just the receptacle
BUTTON_OFFSET_X = 11.0  # EN and BOOT sit one each side of the USB connector
BUTTON_HOLE_D = 5.0
BACK_FEATURE_Y = BOARD_LIFT + PCB_T + 1.4  # centre of the USB slot and holes

# --- Display module: 0.96" SSD1306, 4-pin header
MODULE_W, MODULE_H, MODULE_T = 27.3, 27.8, 4.4
WINDOW_W, WINDOW_H = 24.0, 14.5
WINDOW_OFFSET_Y = 0.0  # tune after the test print if the glass sits off-centre

# --- Case
WALL, CLEARANCE = 2.0, 0.3
WIRE_GAP = 16.0  # DuPont housings between module and board; 8 mm if soldered
RAIL_W, RAIL_H = 2.5, 1.5
RIB_T = 0.7  # side ribs that pinch the module; must fit the width slack
STOP_W, STOP_T, STOP_H = 3.0, 1.5, 6.0  # corner stops behind the module
LID_T, LID_LIP_H, LID_LIP_T, LID_FIT = 2.0, 3.0, 1.2, 0.25
LID_OFFSET_X = 60.0  # the lid body sits beside the case, ready to print

# --- Derived cavity and depth stations
INNER_W = max(BOARD_W, MODULE_W) + 2 * CLEARANCE
INNER_H = MODULE_H + 2 * CLEARANCE
INNER_D = MODULE_T + CLEARANCE + WIRE_GAP + BOARD_L + CLEARANCE
OUTER_W = INNER_W + 2 * WALL
BODY_H = INNER_H + WALL
OUTER_D = INNER_D + 2 * WALL

CAVITY_BACK_Z = -(WALL + INNER_D)
MODULE_BACK_Z = -(WALL + MODULE_T + CLEARANCE)
BOARD_FRONT_Z = MODULE_BACK_Z - WIRE_GAP
BOARD_BACK_Z = BOARD_FRONT_Z - BOARD_L - CLEARANCE
MODULE_CENTER_Y = INNER_H / 2

NEW = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation
CUT = adsk.fusion.FeatureOperations.CutFeatureOperation


def cm(mm: float) -> float:
    return mm / 10.0


class Builder:
    """Thin helpers over the Fusion API, all taking millimetres."""

    def __init__(self, comp: adsk.fusion.Component) -> None:
        self.comp = comp

    def _plane_at_z(self, z: float) -> adsk.fusion.ConstructionPlane:
        planes = self.comp.constructionPlanes
        plane_input = planes.createInput()
        plane_input.setByOffset(
            self.comp.xYConstructionPlane, adsk.core.ValueInput.createByReal(cm(z))
        )
        return planes.add(plane_input)

    def _extrude(self, profile, depth: float, op, target=None):
        extrudes = self.comp.features.extrudeFeatures
        ext_input = extrudes.createInput(profile, op)
        ext_input.setDistanceExtent(False, adsk.core.ValueInput.createByReal(cm(depth)))
        if target is not None and op != NEW:
            ext_input.participantBodies = [target]
        return extrudes.add(ext_input)

    def box(self, z, x0, y0, x1, y1, depth, op, target=None):
        """Rectangle on the plane at z, extruded `depth` along Z (negative = back)."""
        sketch = self.comp.sketches.add(self._plane_at_z(z))
        sketch.sketchCurves.sketchLines.addTwoPointRectangle(
            adsk.core.Point3D.create(cm(x0), cm(y0), 0),
            adsk.core.Point3D.create(cm(x1), cm(y1), 0),
        )
        feature = self._extrude(sketch.profiles.item(0), depth, op, target)
        return feature.bodies.item(0) if op == NEW else target

    def hole(self, z, x, y, diameter, depth, target):
        sketch = self.comp.sketches.add(self._plane_at_z(z))
        sketch.sketchCurves.sketchCircles.addByCenterRadius(
            adsk.core.Point3D.create(cm(x), cm(y), 0), cm(diameter / 2)
        )
        self._extrude(sketch.profiles.item(0), depth, CUT, target)


def build_body(builder: Builder):
    half_outer, half_inner = OUTER_W / 2, INNER_W / 2

    body = builder.box(0, -half_outer, 0, half_outer, BODY_H, -OUTER_D, NEW)
    # Cavity, open at the bottom so the lid closes it.
    builder.box(-WALL, -half_inner, 0, half_inner, INNER_H, -INNER_D, CUT, body)

    # Face: the OLED window in the front wall.
    window_bottom = MODULE_CENTER_Y + WINDOW_OFFSET_Y - WINDOW_H / 2
    builder.box(
        0, -WINDOW_W / 2, window_bottom, WINDOW_W / 2, window_bottom + WINDOW_H, -WALL, CUT, body
    )

    # Back wall: the USB cable and the two board buttons.
    builder.box(
        CAVITY_BACK_Z,
        -USB_SLOT[0] / 2,
        BACK_FEATURE_Y - USB_SLOT[1] / 2,
        USB_SLOT[0] / 2,
        BACK_FEATURE_Y + USB_SLOT[1] / 2,
        -WALL,
        CUT,
        body,
    )
    for side in (-1, 1):
        builder.hole(
            CAVITY_BACK_Z, side * BUTTON_OFFSET_X, BACK_FEATURE_Y, BUTTON_HOLE_D, -WALL, body
        )

    # Rails the board rests on, leaving room for the soldered pins underneath.
    rail_depth = BOARD_BACK_Z - BOARD_FRONT_Z
    for side in (-1, 1):
        outer_x = side * half_inner
        inner_x = side * (half_inner - RAIL_W)
        builder.box(
            BOARD_FRONT_Z,
            min(outer_x, inner_x),
            BOARD_LIFT - RAIL_H,
            max(outer_x, inner_x),
            BOARD_LIFT,
            rail_depth,
            JOIN,
            body,
        )

    # Ribs that pinch the module against the front wall, plus corner stops.
    for side in (-1, 1):
        outer_x = side * half_inner
        inner_x = side * (half_inner - RIB_T)
        builder.box(
            -WALL,
            min(outer_x, inner_x),
            0,
            max(outer_x, inner_x),
            INNER_H,
            MODULE_BACK_Z + WALL,
            JOIN,
            body,
        )
        for y0 in (0.0, INNER_H - STOP_H):
            stop_inner = side * (half_inner - STOP_W)
            builder.box(
                MODULE_BACK_Z,
                min(outer_x, stop_inner),
                y0,
                max(outer_x, stop_inner),
                y0 + STOP_H,
                -STOP_T,
                JOIN,
                body,
            )
    return body


def build_lid(builder: Builder):
    half_outer = OUTER_W / 2
    lid = builder.box(
        0, LID_OFFSET_X - half_outer, -LID_T, LID_OFFSET_X + half_outer, 0, -OUTER_D, NEW
    )

    # A lip along the sides and the back presses into the cavity; the front is
    # left open so it never fights the module.
    half_lip = INNER_W / 2 - LID_FIT
    lip_front_z = -(WALL + LID_FIT)
    lip_back_z = CAVITY_BACK_Z + LID_FIT
    for side in (-1, 1):
        outer_x = LID_OFFSET_X + side * half_lip
        inner_x = LID_OFFSET_X + side * (half_lip - LID_LIP_T)
        builder.box(
            lip_front_z,
            min(outer_x, inner_x),
            -LID_T,
            max(outer_x, inner_x),
            LID_LIP_H,
            lip_back_z - lip_front_z,
            JOIN,
            lid,
        )
    builder.box(
        lip_back_z + LID_LIP_T,
        LID_OFFSET_X - half_lip,
        -LID_T,
        LID_OFFSET_X + half_lip,
        LID_LIP_H,
        -LID_LIP_T,
        JOIN,
        lid,
    )
    return lid


LOG_PATH = "/tmp/tokometer_case.log"
EXPORT_DIR = "/Users/lucas/Documents/Projetos/Pessoal/harware/tokEsp/hardware/case"


def log(message: str) -> None:
    """Fusion swallows stdout, and a message box would block the script."""
    with open(LOG_PATH, "a") as handle:
        handle.write(message + "\n")


def clear(comp: adsk.fusion.Component) -> None:
    """Empties a scratch design so the script can run again in place."""
    for collection in (comp.bRepBodies, comp.sketches, comp.constructionPlanes):
        while collection.count:
            collection.item(0).deleteMe()


def export(design: adsk.fusion.Design, body, filename: str) -> None:
    """Writes one printable part beside this script."""
    manager = design.exportManager
    options = manager.createC3MFExportOptions(body, "{}/{}".format(EXPORT_DIR, filename))
    manager.execute(options)


def run(context):
    app = adsk.core.Application.get()
    try:
        open(LOG_PATH, "w").close()
        design = adsk.fusion.Design.cast(app.activeProduct)
        # Never build into a saved document: open a scratch one instead. The
        # Fusion MCP reports an error when a script switches documents, because
        # it holds references across the call; the build still completes, so
        # check LOG_PATH and the exported files rather than the tool's reply.
        if design is None or app.activeDocument.isSaved:
            app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
            design = adsk.fusion.Design.cast(app.activeProduct)
        clear(design.rootComponent)
        # The root component takes its name from the document, so it is named
        # on save, not here.
        root = design.rootComponent

        builder = Builder(root)
        log("building body")
        body = build_body(builder)
        body.name = "case_body"
        log("building lid")
        lid = build_lid(builder)
        lid.name = "case_lid"
        app.activeViewport.fit()

        export(design, body, "box_case_body.3mf")
        export(design, lid, "box_case_lid.3mf")
        log("done: case {} x {} x {} mm".format(OUTER_W, BODY_H + LID_T, OUTER_D))
    except Exception:
        import traceback

        log("FAILED:\n" + traceback.format_exc())
        raise


if __name__ == "__main__":
    run(None)
