"""Generates the tokometer desk stand: a retro-computer shell on a plinth.

Built in steps. The cover is an extruded side profile (front leaning back, flat
top, straight back), filleted and then shelled to a 2 mm wall, open at the
bottom. The display window and its bezel are cut on the leaning face, so the
screen tilts back like the reference. The plinth carries four feet.

Run inside Fusion (Scripts and Add-Ins, or the Fusion MCP). It always builds in
a fresh scratch document: deleting bodies breaks the MCP, which holds
references across the call and rolls the timeline back when it fails. Check
LOG_PATH and the exported files rather than the tool's reply.

All dimensions are millimetres; Fusion's API works in centimetres, so every
value goes through `cm()`. Coordinates: X to the viewer's right, Y up,
Z towards the viewer. The stand stands on Y = 0, centred on X = 0, with its
front face on Z = 0 and the body growing towards -Z.
"""

import adsk.core
import adsk.fusion

# --- Board: ESP32 DevKit 30-pin (ESP-WROOM-32), USB-C on a short edge
BOARD_L, BOARD_W = 51.5, 28.5

# --- Plinth
PADDING = 3.0
BASE_T = 8.0
CORNER_R = 6.0  # the board still clears up to about 14 mm
BASE_SIDE = BOARD_L + 2 * PADDING

# --- Board reference body: shows where the ESP32 sits, never exported
PCB_T = 1.6
CAN_W, CAN_L, CAN_T = 18.0, 25.5, 3.1  # the ESP-WROOM-32 metal can
CAN_FRONT_GAP = 3.0  # from the front edge of the board to the can
USB_W, USB_H, USB_L = 9.0, 3.2, 7.0  # the USB-C receptacle at the back edge

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

    def _offset_plane(self, base_plane, distance: float) -> adsk.fusion.ConstructionPlane:
        planes = self.comp.constructionPlanes
        plane_input = planes.createInput()
        plane_input.setByOffset(base_plane, adsk.core.ValueInput.createByReal(cm(distance)))
        return planes.add(plane_input)

    def _extrude(self, profile, distance: float, op, target=None):
        extrudes = self.comp.features.extrudeFeatures
        ext_input = extrudes.createInput(profile, op)
        ext_input.setDistanceExtent(False, adsk.core.ValueInput.createByReal(cm(distance)))
        if target is not None and op != NEW:
            ext_input.participantBodies = [target]
        feature = extrudes.add(ext_input)
        return feature.bodies.item(0) if op == NEW else target

    def slab(self, y, x0, z0, x1, z1, height, op, target=None):
        """Footprint on the horizontal plane at y, extruded upwards."""
        sketch = self.comp.sketches.add(self._offset_plane(self.comp.xZConstructionPlane, y))
        # On the XZ plane the sketch Y axis runs along -Z of the model.
        sketch.sketchCurves.sketchLines.addTwoPointRectangle(
            adsk.core.Point3D.create(cm(x0), cm(-z0), 0),
            adsk.core.Point3D.create(cm(x1), cm(-z1), 0),
        )
        return self._extrude(sketch.profiles.item(0), height, op, target)

    def panel(self, z, x0, y0, x1, y1, depth, op, target=None):
        """Upright rectangle on the plane at z, extruded towards +Z."""
        sketch = self.comp.sketches.add(self._offset_plane(self.comp.xYConstructionPlane, z))
        sketch.sketchCurves.sketchLines.addTwoPointRectangle(
            adsk.core.Point3D.create(cm(x0), cm(y0), 0),
            adsk.core.Point3D.create(cm(x1), cm(y1), 0),
        )
        return self._extrude(sketch.profiles.item(0), depth, op, target)

    def side_profile(self, x: float, points, width: float, op, target=None):
        """Closed polyline on the YZ plane at x, extruded along +X."""
        sketch = self.comp.sketches.add(self._offset_plane(self.comp.yZConstructionPlane, x))
        lines = sketch.sketchCurves.sketchLines
        # On the YZ plane the sketch X axis runs along model -Z and sketch Y
        # along model +Y, so a (y, z) point maps to (-z, y).
        sketch_points = [adsk.core.Point3D.create(cm(-z), cm(y), 0) for y, z in points]
        for start, end in zip(sketch_points, sketch_points[1:] + sketch_points[:1]):
            lines.addByTwoPoints(start, end)
        return self._extrude(sketch.profiles.item(0), width, op, target)

    def cylinder(self, y: float, x: float, z: float, diameter: float, height: float, op, target=None):
        sketch = self.comp.sketches.add(self._offset_plane(self.comp.xZConstructionPlane, y))
        sketch.sketchCurves.sketchCircles.addByCenterRadius(
            adsk.core.Point3D.create(cm(x), cm(-z), 0), cm(diameter / 2)
        )
        return self._extrude(sketch.profiles.item(0), height, op, target)


def find_faces(body, axis: str, sign: float):
    """Planar faces whose outward normal points along the given axis."""
    found = []
    for face in body.faces:
        geometry = face.geometry
        if geometry.objectType != adsk.core.Plane.classType():
            continue
        normal = geometry.normal
        value = {"x": normal.x, "y": normal.y, "z": normal.z}[axis]
        if value * sign > 0.7:
            found.append(face)
    return found


def fillet_corner_edges(comp, body, radius: float) -> None:
    """Fillets the four upright corner edges: those on a side wall that rise."""
    edges = adsk.core.ObjectCollection.create()
    for edge in body.edges:
        geometry = edge.geometry
        if geometry.objectType != adsk.core.Line3D.classType():
            continue
        start, end = geometry.startPoint, geometry.endPoint
        rises = abs(start.y - end.y) > cm(1.0)
        on_side = abs(abs(start.x) - cm(BASE_SIDE / 2)) < 1e-6 and abs(abs(end.x) - cm(BASE_SIDE / 2)) < 1e-6
        if rises and on_side:
            edges.add(edge)
    if not edges.count:
        return
    fillets = comp.features.filletFeatures
    fillet_input = fillets.createInput()
    fillet_input.addConstantRadiusEdgeSet(edges, adsk.core.ValueInput.createByReal(cm(radius)), True)
    fillets.add(fillet_input)


def shell_open_bottom(comp, body, thickness: float) -> None:
    faces = adsk.core.ObjectCollection.create()
    for face in find_faces(body, "y", -1.0):
        faces.add(face)
    shells = comp.features.shellFeatures
    shell_input = shells.createInput(faces, False)
    shell_input.insideThickness = adsk.core.ValueInput.createByReal(cm(thickness))
    shells.add(shell_input)


def smallest_profile(sketch):
    """A rectangle drawn inside a face yields the rectangle and the ring around
    it; the rectangle is the smaller one."""
    return min(sketch.profiles, key=lambda profile: profile.areaProperties().area)


def front_faces(body):
    """Forward-facing planar faces, largest first."""
    faces = find_faces(body, "z", 1.0)
    return sorted(faces, key=lambda face: face.area, reverse=True)


def cut_on_face(comp, body, face, width: float, height: float, depth: float, offset_y: float = 0.0):
    """Rectangle centred on a face, cut into the body along the face normal."""
    sketch = comp.sketches.add(face)
    box = face.boundingBox
    center = adsk.core.Point3D.create(
        (box.minPoint.x + box.maxPoint.x) / 2,
        (box.minPoint.y + box.maxPoint.y) / 2,
        (box.minPoint.z + box.maxPoint.z) / 2,
    )
    local = sketch.modelToSketchSpace(center)
    corner_a = adsk.core.Point3D.create(local.x - cm(width / 2), local.y - cm(height / 2) + cm(offset_y), 0)
    corner_b = adsk.core.Point3D.create(local.x + cm(width / 2), local.y + cm(height / 2) + cm(offset_y), 0)
    sketch.sketchCurves.sketchLines.addTwoPointRectangle(corner_a, corner_b)

    extrudes = comp.features.extrudeFeatures
    ext_input = extrudes.createInput(smallest_profile(sketch), CUT)
    ext_input.setDistanceExtent(False, adsk.core.ValueInput.createByReal(cm(-depth)))
    ext_input.participantBodies = [body]
    extrudes.add(ext_input)


def build_base(builder: Builder):
    """Plain plinth standing on the table."""
    half = BASE_SIDE / 2
    return builder.slab(0.0, -half, 0.0, half, -BASE_SIDE, BASE_T, NEW)


def build_board_reference(builder: Builder):
    """The ESP32 DevKit laid on the base: board, module can and USB-C."""
    top = BASE_T
    half_w, half_l = BOARD_W / 2, BOARD_L / 2
    front_z = -(BASE_SIDE / 2 - half_l)
    back_z = front_z - BOARD_L

    board = builder.slab(top, -half_w, front_z, half_w, back_z, PCB_T, NEW)

    can_front = front_z - CAN_FRONT_GAP
    builder.slab(top + PCB_T, -CAN_W / 2, can_front, CAN_W / 2, can_front - CAN_L, CAN_T, JOIN, board)
    builder.slab(
        top + PCB_T, -USB_W / 2, back_z + USB_L, USB_W / 2, back_z - 1.0, USB_H, JOIN, board
    )
    return board


def export(design: adsk.fusion.Design, body, filename: str) -> None:
    manager = design.exportManager
    options = manager.createC3MFExportOptions(body, "{}/{}".format(EXPORT_DIR, filename))
    manager.execute(options)


def run(context):
    app = adsk.core.Application.get()
    try:
        open(LOG_PATH, "w").close()
        design = adsk.fusion.Design.cast(app.activeProduct)
        if design is None or app.activeDocument.isSaved or design.rootComponent.bRepBodies.count:
            app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
            design = adsk.fusion.Design.cast(app.activeProduct)
        comp = design.rootComponent

        builder = Builder(comp)
        base = build_base(builder)
        base.name = "stand_base"
        fillet_corner_edges(comp, base, CORNER_R)
        board = build_board_reference(builder)
        board.name = "reference_board"
        app.activeViewport.fit()

        export(design, base, "stand_base.3mf")
        log("plinth {} x {} x {} mm".format(BASE_SIDE, BASE_T, BASE_SIDE))
        log("board {} x {} mm resting at {} mm, {} mm clear at each end".format(
            BOARD_L, BOARD_W, BASE_T, round((BASE_SIDE - BOARD_L) / 2, 2)
        ))
    except Exception:
        import traceback

        log("FAILED:\n" + traceback.format_exc())
        raise


if __name__ == "__main__":
    run(None)
