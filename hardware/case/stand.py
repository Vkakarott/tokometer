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

# --- Display mock (0.96" SSD1306 module), never exported
PCB_T = 1.6
MODULE_W, MODULE_H = 27.3, 27.8
GLASS_W, GLASS_H, GLASS_T = 26.7, 19.3, 1.2
GLASS_OFFSET_Y = -2.0  # the glass sits below centre, away from the pin header
MODULE_FRONT_GAP = 2.0  # from the front edge of the base to the module
DISPLAY_LIFT = 4.0  # the module floats this far above the base

# --- Frame around the screen only, like the black bezel in the reference
FRAME_W = 1.0  # inner band, added outside the glass to enlarge the screen
FRAME_GAP = 2.0  # empty space between the two bands
OUTER_FRAME_W = 4.0  # second band, wrapping the first one
FRAME_PROUD = 1.0  # how far the bands stand in front of the glass
FRAME_R = 1.5  # corner radius at the glass edge, growing with each band

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


def rounded_rect(sketch, x0: float, y0: float, x1: float, y1: float, radius: float) -> None:
    """Rectangle with filleted corners, drawn in sketch space."""
    lines = sketch.sketchCurves.sketchLines.addTwoPointRectangle(
        adsk.core.Point3D.create(cm(x0), cm(y0), 0),
        adsk.core.Point3D.create(cm(x1), cm(y1), 0),
    )
    arcs = sketch.sketchCurves.sketchArcs
    for index in range(4):
        first = lines.item(index)
        second = lines.item((index + 1) % 4)
        arcs.addFillet(
            first,
            first.endSketchPoint.geometry,
            second,
            second.startSketchPoint.geometry,
            cm(radius),
        )


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


def build_ring(builder: Builder, comp, center_y, inner_half_w, inner_half_h, band, inner_r, z, depth):
    """Rounded band around a rectangle, extruded towards +Z."""
    sketch = comp.sketches.add(builder._offset_plane(comp.xYConstructionPlane, z))
    rounded_rect(
        sketch,
        -(inner_half_w + band),
        center_y - inner_half_h - band,
        inner_half_w + band,
        center_y + inner_half_h + band,
        inner_r + band,
    )
    rounded_rect(
        sketch,
        -inner_half_w,
        center_y - inner_half_h,
        inner_half_w,
        center_y + inner_half_h,
        inner_r,
    )
    ring = min(sketch.profiles, key=lambda profile: profile.areaProperties().area)
    extrudes = comp.features.extrudeFeatures
    ext_input = extrudes.createInput(ring, NEW)
    ext_input.setDistanceExtent(False, adsk.core.ValueInput.createByReal(cm(depth)))
    return extrudes.add(ext_input).bodies.item(0)


def build_display_frames(builder: Builder, comp):
    """Two concentric bands around the screen, from the glass edge outwards."""
    center_y = BASE_T + DISPLAY_LIFT + MODULE_H / 2 + GLASS_OFFSET_Y
    glass_front_z = -MODULE_FRONT_GAP + GLASS_T

    inner = build_ring(
        builder, comp, center_y, GLASS_W / 2, GLASS_H / 2, FRAME_W, FRAME_R, glass_front_z, FRAME_PROUD
    )
    outer = build_ring(
        builder,
        comp,
        center_y,
        GLASS_W / 2 + FRAME_W + FRAME_GAP,
        GLASS_H / 2 + FRAME_W + FRAME_GAP,
        OUTER_FRAME_W,
        FRAME_R + FRAME_W + FRAME_GAP,
        glass_front_z,
        FRAME_PROUD,
    )
    return inner, outer


def build_display_mock(builder: Builder):
    """The OLED module standing on the base, glass facing front."""
    bottom = BASE_T + DISPLAY_LIFT
    module = builder.panel(
        -MODULE_FRONT_GAP - PCB_T,
        -MODULE_W / 2,
        bottom,
        MODULE_W / 2,
        bottom + MODULE_H,
        PCB_T,
        NEW,
    )
    glass_center_y = bottom + MODULE_H / 2 + GLASS_OFFSET_Y
    builder.panel(
        -MODULE_FRONT_GAP,
        -GLASS_W / 2,
        glass_center_y - GLASS_H / 2,
        GLASS_W / 2,
        glass_center_y + GLASS_H / 2,
        GLASS_T,
        JOIN,
        module,
    )
    return module


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
        display = build_display_mock(builder)
        display.name = "mock_display"
        inner_frame, outer_frame = build_display_frames(builder, comp)
        inner_frame.name = "display_frame_inner"
        outer_frame.name = "display_frame_outer"
        app.activeViewport.fit()

        export(design, base, "stand_base.3mf")
        log("plinth {} x {} x {} mm".format(BASE_SIDE, BASE_T, BASE_SIDE))
        log("display mock {} x {} mm, lifted {} mm above the base".format(
            MODULE_W, MODULE_H, DISPLAY_LIFT
        ))
        log("bands {} mm + {} mm gap + {} mm: outer size {} x {} mm".format(
            FRAME_W,
            FRAME_GAP,
            OUTER_FRAME_W,
            round(GLASS_W + 2 * (FRAME_W + FRAME_GAP + OUTER_FRAME_W), 1),
            round(GLASS_H + 2 * (FRAME_W + FRAME_GAP + OUTER_FRAME_W), 1),
        ))
    except Exception:
        import traceback

        log("FAILED:\n" + traceback.format_exc())
        raise


if __name__ == "__main__":
    run(None)
