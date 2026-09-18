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

import math

import adsk.core
import adsk.fusion

# --- Board: ESP32 DevKit 30-pin (ESP-WROOM-32), USB-C on a short edge
BOARD_L, BOARD_W = 51.5, 28.5

# --- Plinth and body footprint: width follows the front panel, depth the board
PADDING = 3.0
BASE_T = 8.0
BASE_W = 52.0
BASE_D = BOARD_L + 2 * PADDING
# Every outer edge of the plinth and the body is left square.

# --- Body
BODY_H = 57.7  # gives a 24 mm chin under the funnel
WALL = 2.0
FRONT_LEAN = 5.8  # how far the top of the front face sits behind its bottom (~5.7 deg)
# Flat decline round the front panel's edge: (width on the front, setback on the other face).
FRONT_DECLINE_SIDE = (2.0, 1.0)
FRONT_DECLINE_TOP = (4.0, 1.5)  # top and bottom decline more than the sides

# --- Display mock (0.96" SSD1306 module), never exported
PCB_T = 1.6
MODULE_W, MODULE_H = 27.3, 27.8
GLASS_W, GLASS_H, GLASS_T = 26.7, 19.3, 1.2
ACTIVE_W, ACTIVE_H = 21.74, 10.86  # the lit area: 128 x 64 pixels, datasheet size
# From a photo of the actual module, lit: the lit area sits towards the pins
# (about 2 mm of dead glass on that side, 6.4 mm on the ribbon side), and the
# glass is centred on the board.
ACTIVE_OFFSET_Y = 2.0  # lit area centre above the glass centre, towards the pins
GLASS_OFFSET_Y = 0.0  # glass centre relative to the board centre

# --- Front panel: a Macintosh-style funnel sunk into the face
PANEL_W, PANEL_H, PANEL_T = BASE_W, BODY_H, 4.0
SCREEN_FROM_TOP = 21.5  # panel top to the screen centre; fixes where the OLED sits
WINDOW_PADDING = 0.75  # dead glass left visible round the lit area, each side
OPENING = (ACTIVE_W + 2 * WINDOW_PADDING, ACTIVE_H + 2 * WINDOW_PADDING)  # throat to the glass
CORNER_RADIUS = 1.5  # window corners
OPENING_R = CORNER_RADIUS
FUNNEL_DEPTH = 2.5  # slope depth; the rest of the wall is the throat in front of the glass
FUNNEL_RUN = 6.0  # width of the sloped band on the face: atan(2.5 / 6) = 22.6 deg
FUNNEL_OUTER = (OPENING[0] + 2 * FUNNEL_RUN, OPENING[1] + 2 * FUNNEL_RUN)
FUNNEL_OUTER_R = 0.5  # outer outline corners, almost sharp
LIP_FILLET = 0.3  # face into slope, almost sharp; kept under FUNNEL_OUTER_R
THROAT_CHAMFER = 0.5  # flat band into the throat; leaves 1 mm of straight throat wall

# --- Floppy drive detail in the chin, visual only (never cut through the wall)
FLOPPY_RIGHT_X = FUNNEL_OUTER[0] / 2  # right end lines up with the funnel's right edge
CHIN_H = BODY_H - SCREEN_FROM_TOP - FUNNEL_OUTER[1] / 2  # face below the funnel
FLOPPY_Y = BASE_T + CHIN_H / 2  # centred in the chin
FLOPPY_BAND = (22.0, 3.0)  # the long shallow recess
FLOPPY_MOUTH = (6.0, 5.5)  # the taller block at its right end
FLOPPY_RECESS_DEPTH = 0.8
FLOPPY_SLOT = (18.0, 1.0)  # dark slot running along the recess, ending in the mouth
FLOPPY_SLOT_DEPTH = 1.5
FLOPPY_EJECT_D = 0.8  # pinhole in the mouth, below the slot
FLOPPY_R = 0.3

# Fusion appearance library and its matte black plastic; ids, so any UI language works.
APPEARANCE_LIBRARY_ID = "BA5EE55E-9982-449B-9D66-9F036540E140"
DISPLAY_APPEARANCE_ID = "Prism-113"

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
    """Planar faces parallel to the leaning front, largest first.

    Matching the lean excludes the inner back wall (straight +Z) and the
    declines round the edge, which only partly face forward.
    """
    nz, ny = front_normal()
    found = []
    for face in body.faces:
        if face.geometry.objectType != adsk.core.Plane.classType():
            continue
        # The evaluator gives the outward normal; the plane's own normal can
        # point either way, which let the wall's inner face pass as the front.
        _, normal = face.evaluator.getNormalAtPoint(face.pointOnFace)
        if normal.y * ny + normal.z * nz > 0.9999:
            found.append(face)
    return sorted(found, key=lambda face: face.area, reverse=True)


def front_inset(body, edge_kind: str) -> float:
    """How far the front face now stops short of the body's outline on one edge, in mm."""
    box = front_faces(body)[0].boundingBox
    return {
        "top": BASE_T + BODY_H - box.maxPoint.y / cm(1),
        "bottom": box.minPoint.y / cm(1) - BASE_T,
        "left": box.minPoint.x / cm(1) + BASE_W / 2,
        "right": BASE_W / 2 - box.maxPoint.x / cm(1),
    }[edge_kind]


def front_edge(body, edge_kind: str):
    """One of the four straight edges round the front face."""
    target = {"top": BASE_T + BODY_H, "bottom": BASE_T}
    for edge in front_faces(body)[0].edges:
        start, end = edge.startVertex.geometry, edge.endVertex.geometry
        if edge_kind in target:
            if all(abs(point.y - cm(target[edge_kind])) < 1e-6 for point in (start, end)):
                return edge
        else:
            sign = -1.0 if edge_kind == "left" else 1.0
            if all(abs(point.x - sign * cm(BASE_W / 2)) < 1e-6 for point in (start, end)):
                return edge
    raise RuntimeError("no {} edge on the front face".format(edge_kind))


def decline_front_edges(comp, body) -> None:
    """Flat two-distance chamfers round the front, flipped if a side lands the wrong way."""
    chamfers = comp.features.chamferFeatures
    for edge_kind in ("top", "bottom", "left", "right"):
        on_front, setback = FRONT_DECLINE_TOP if edge_kind in ("top", "bottom") else FRONT_DECLINE_SIDE
        for flipped in (False, True):
            edges = adsk.core.ObjectCollection.create()
            edges.add(front_edge(body, edge_kind))
            chamfer_input = chamfers.createInput2()
            chamfer_input.chamferEdgeSets.addTwoDistancesChamferEdgeSet(
                edges,
                adsk.core.ValueInput.createByReal(cm(on_front)),
                adsk.core.ValueInput.createByReal(cm(setback)),
                flipped,
                False,
            )
            feature = chamfers.add(chamfer_input)
            if abs(front_inset(body, edge_kind) - on_front) < 0.3:
                break
            feature.deleteMe()
        else:
            raise RuntimeError("{} decline came out the wrong way round".format(edge_kind))


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
    return builder.slab(0.0, -BASE_W / 2, 0.0, BASE_W / 2, -BASE_D, BASE_T, NEW)


def rounded_profile(builder: Builder, comp, z, half_w, center_y, half_h, radius):
    """A single rounded-rectangle profile on the plane at z."""
    sketch = comp.sketches.add(builder._offset_plane(comp.xYConstructionPlane, z))
    rounded_rect(sketch, -half_w, center_y - half_h, half_w, center_y + half_h, radius)
    return sketch.profiles.item(0)


def panel_center_y():
    """Height of the screen centre, fixed from the panel top."""
    panel_top = BASE_T + PANEL_H
    return panel_top - SCREEN_FROM_TOP


def front_normal():
    """Outward normal of the leaning front face, in the ZY plane."""
    length = (FRONT_LEAN ** 2 + BODY_H ** 2) ** 0.5
    return BODY_H / length, FRONT_LEAN / length  # (z, y), pointing out and up


def screen_center_point():
    """Centre of the screen, on the leaning face."""
    y = panel_center_y()
    z = -FRONT_LEAN * (y - BASE_T) / BODY_H
    return adsk.core.Point3D.create(0.0, cm(y), cm(z))


def sketch_rect_at(sketch, model_point, width, height, radius, normal_offset=0.0):
    """Rounded rectangle centred on a model point, lying on the leaning face.

    Opposite corners are placed in model space and mapped into the sketch, so
    the result does not depend on how Fusion orients the face's sketch axes.
    """
    nz, ny = front_normal()
    up_y, up_z = nz, -ny  # along the face, pointing up and back

    def corner(sign):
        return sketch.modelToSketchSpace(
            adsk.core.Point3D.create(
                model_point.x + sign * cm(width / 2),
                model_point.y + cm(normal_offset * ny + sign * up_y * height / 2),
                model_point.z + cm(normal_offset * nz + sign * up_z * height / 2),
            )
        )

    first, second = corner(-1), corner(1)
    lines = sketch.sketchCurves.sketchLines.addTwoPointRectangle(
        adsk.core.Point3D.create(first.x, first.y, 0),
        adsk.core.Point3D.create(second.x, second.y, 0),
    )
    arcs = sketch.sketchCurves.sketchArcs
    for index in range(4):
        a, b = lines.item(index), lines.item((index + 1) % 4)
        arcs.addFillet(a, a.endSketchPoint.geometry, b, b.startSketchPoint.geometry, cm(radius))
    return smallest_profile(sketch)


def build_body(builder: Builder, comp):
    """Hollow body with a leaning front wall that carries the screen levels."""
    bottom, top = BASE_T, BASE_T + BODY_H
    outer_profile = [
        (bottom, 0.0),
        (top, -FRONT_LEAN),
        (top, -BASE_D),
        (bottom, -BASE_D),
    ]
    body = builder.side_profile(-BASE_W / 2, outer_profile, BASE_W, NEW)
    decline_front_edges(comp, body)

    # Cavity: 4 mm front square to the leaning face; WALL elsewhere.
    nz, _ = front_normal()
    front_thickness = PANEL_T / nz
    side_wall = WALL
    cavity = [
        (bottom, -front_thickness),
        (top - side_wall, -FRONT_LEAN * (top - side_wall - bottom) / BODY_H - front_thickness),
        (top - side_wall, -(BASE_D - WALL)),
        (bottom, -(BASE_D - WALL)),
    ]
    builder.side_profile(-BASE_W / 2 + side_wall, cavity, BASE_W - 2 * side_wall, CUT, body)

    cut_funnel(comp, body)
    return body


def face_point(x: float, y: float):
    """Point on the leaning front face at model x and height y."""
    z = -FRONT_LEAN * (y - BASE_T) / BODY_H
    return adsk.core.Point3D.create(cm(x), cm(y), cm(z))


def cut_from_face(comp, body, profile, depth: float):
    extrudes = comp.features.extrudeFeatures
    ext_input = extrudes.createInput(profile, CUT)
    ext_input.setDistanceExtent(False, adsk.core.ValueInput.createByReal(cm(-depth)))
    ext_input.participantBodies = [body]
    return extrudes.add(ext_input)


def cut_floppy(comp, body):
    """Recessed drive bezel, dark slot and eject pinhole; returns the slot's faces."""
    face = front_faces(body)[0]
    band_center_x = FLOPPY_RIGHT_X - FLOPPY_BAND[0] / 2
    mouth_center_x = FLOPPY_RIGHT_X - FLOPPY_MOUTH[0] / 2
    for center_x, size in ((band_center_x, FLOPPY_BAND), (mouth_center_x, FLOPPY_MOUTH)):
        sketch = comp.sketches.addWithoutEdges(face)
        profile = sketch_rect_at(sketch, face_point(center_x, FLOPPY_Y), size[0], size[1], FLOPPY_R)
        cut_from_face(comp, body, profile, FLOPPY_RECESS_DEPTH)

    slot_right = FLOPPY_RIGHT_X - 1.0
    sketch = comp.sketches.addWithoutEdges(face)
    profile = sketch_rect_at(
        sketch, face_point(slot_right - FLOPPY_SLOT[0] / 2, FLOPPY_Y), FLOPPY_SLOT[0], FLOPPY_SLOT[1], 0.2
    )
    slot = cut_from_face(comp, body, profile, FLOPPY_SLOT_DEPTH)

    sketch = comp.sketches.addWithoutEdges(face)
    center = sketch.modelToSketchSpace(face_point(mouth_center_x + 1.2, FLOPPY_Y - 1.8))
    sketch.sketchCurves.sketchCircles.addByCenterRadius(
        adsk.core.Point3D.create(center.x, center.y, 0), cm(FLOPPY_EJECT_D / 2)
    )
    cut_from_face(comp, body, sketch.profiles.item(0), FLOPPY_SLOT_DEPTH)
    return list(slot.faces)


def cut_funnel(comp, body) -> None:
    """Sloped funnel from the face down to the throat, then the throat to the glass."""
    center = screen_center_point()
    face = front_faces(body)[0]
    face_sketch = comp.sketches.add(face)
    outer = sketch_rect_at(face_sketch, center, FUNNEL_OUTER[0], FUNNEL_OUTER[1], FUNNEL_OUTER_R)

    planes = comp.constructionPlanes
    plane_input = planes.createInput()
    plane_input.setByOffset(face, adsk.core.ValueInput.createByReal(cm(-FUNNEL_DEPTH)))
    throat_plane = planes.add(plane_input)
    inner = sketch_rect_at(
        comp.sketches.add(throat_plane), center, OPENING[0], OPENING[1], OPENING_R, -FUNNEL_DEPTH
    )

    lofts = comp.features.loftFeatures
    loft_input = lofts.createInput(CUT)
    loft_input.loftSections.add(outer)
    loft_input.loftSections.add(inner)
    loft_input.participantBodies = [body]
    lofts.add(loft_input)

    throat = sketch_rect_at(
        comp.sketches.add(throat_plane), center, OPENING[0], OPENING[1], OPENING_R, -FUNNEL_DEPTH
    )
    extrudes = comp.features.extrudeFeatures
    ext_input = extrudes.createInput(throat, CUT)
    ext_input.setDistanceExtent(
        False, adsk.core.ValueInput.createByReal(cm(-(PANEL_T - FUNNEL_DEPTH + 1.0)))
    )
    ext_input.participantBodies = [body]
    extrudes.add(ext_input)

    fillet_edges(comp, funnel_edges(body, 0.0, FUNNEL_OUTER), LIP_FILLET)
    chamfer_edges(comp, funnel_edges(body, -FUNNEL_DEPTH, OPENING), THROAT_CHAMFER)


def funnel_edges(body, depth: float, size):
    """Edges lying on the plane `depth` mm off the face, around the given contour."""
    nz, ny = front_normal()
    up_y, up_z = nz, -ny
    center = screen_center_point()
    edges = adsk.core.ObjectCollection.create()
    for edge in body.edges:
        _, start, end = edge.evaluator.getParameterExtents()
        _, point = edge.evaluator.getPointAtParameter((start + end) / 2)
        dy, dz = (point.y - center.y) / cm(1), (point.z - center.z) / cm(1)
        off_face = dy * ny + dz * nz
        along_face = dy * up_y + dz * up_z
        if abs(off_face - depth) > 0.01:
            continue
        if abs(point.x / cm(1)) <= size[0] / 2 + 0.01 and abs(along_face) <= size[1] / 2 + 0.01:
            edges.add(edge)
    return edges


def fillet_edges(comp, edges, radius: float) -> None:
    if not edges.count:
        raise RuntimeError("no edges found for a {} mm fillet".format(radius))
    fillets = comp.features.filletFeatures
    fillet_input = fillets.createInput()
    fillet_input.addConstantRadiusEdgeSet(edges, adsk.core.ValueInput.createByReal(cm(radius)), True)
    fillets.add(fillet_input)


def chamfer_edges(comp, edges, distance: float) -> None:
    if not edges.count:
        raise RuntimeError("no edges found for a {} mm chamfer".format(distance))
    chamfers = comp.features.chamferFeatures
    chamfer_input = chamfers.createInput2()
    chamfer_input.chamferEdgeSets.addEqualDistanceChamferEdgeSet(
        edges, adsk.core.ValueInput.createByReal(cm(distance)), True
    )
    chamfers.add(chamfer_input)


def build_display_mock(builder: Builder):
    """The OLED module behind the panel, placed so its lit area is centred in the window."""
    glass_center_y = panel_center_y() - ACTIVE_OFFSET_Y
    module_center_y = glass_center_y - GLASS_OFFSET_Y
    glass_front_z = -PANEL_T
    module = builder.panel(
        glass_front_z - GLASS_T - PCB_T,
        -MODULE_W / 2,
        module_center_y - MODULE_H / 2,
        MODULE_W / 2,
        module_center_y + MODULE_H / 2,
        PCB_T,
        NEW,
    )
    builder.panel(
        glass_front_z - GLASS_T,
        -GLASS_W / 2,
        glass_center_y - GLASS_H / 2,
        GLASS_W / 2,
        glass_center_y + GLASS_H / 2,
        GLASS_T,
        JOIN,
        module,
    )
    lean_with_front(builder.comp, module, glass_front_z)
    return module


def lean_with_front(comp, body, glass_front_z: float) -> None:
    """Tilts a body built for an upright front so it sits against the leaning one."""
    nz, ny = front_normal()
    center = screen_center_point()
    pivot = adsk.core.Point3D.create(0.0, cm(panel_center_y()), cm(glass_front_z))
    transform = adsk.core.Matrix3D.create()
    transform.setToRotation(
        -math.atan2(FRONT_LEAN, BODY_H), adsk.core.Vector3D.create(1, 0, 0), pivot
    )
    shift = adsk.core.Matrix3D.create()
    shift.translation = adsk.core.Vector3D.create(
        0.0,
        center.y + cm(glass_front_z * ny) - pivot.y,
        center.z + cm(glass_front_z * nz) - pivot.z,
    )
    transform.transformBy(shift)
    bodies = adsk.core.ObjectCollection.create()
    bodies.add(body)
    moves = comp.features.moveFeatures
    move_input = moves.createInput2(bodies)
    move_input.defineAsFreeMove(transform)
    moves.add(move_input)


def appearance_for(app, design, appearance_id: str):
    appearance = design.appearances.itemById(appearance_id)
    if appearance is None:
        library = app.materialLibraries.itemById(APPEARANCE_LIBRARY_ID)
        appearance = design.appearances.addByCopy(library.appearances.itemById(appearance_id), appearance_id)
    return appearance


def paint(app, design, target, appearance_id: str) -> None:
    """Visual-only appearance on a body or face; not carried into the 3MF exports."""
    target.appearance = appearance_for(app, design, appearance_id)


def export(design: adsk.fusion.Design, body, filename: str) -> None:
    manager = design.exportManager
    options = manager.createC3MFExportOptions(body, "{}/{}".format(EXPORT_DIR, filename))
    manager.execute(options)


def generated_documents(app):
    """Unsaved documents an earlier run of this script built: every run makes a stand_base."""
    found = []
    for document in app.documents:
        if document.isSaved:
            continue
        design = adsk.fusion.Design.cast(document.products.itemByProductType("DesignProductType"))
        if design is not None and design.rootComponent.bRepBodies.itemByName("stand_base"):
            found.append(document)
    return found


def run(context):
    app = adsk.core.Application.get()
    try:
        open(LOG_PATH, "w").close()
        stale = generated_documents(app)
        design = adsk.fusion.Design.cast(app.activeProduct)
        if design is None or app.activeDocument.isSaved or design.rootComponent.bRepBodies.count:
            app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
            design = adsk.fusion.Design.cast(app.activeProduct)
        for document in stale:
            document.close(False)
        comp = design.rootComponent

        builder = Builder(comp)
        base = build_base(builder)
        base.name = "stand_base"
        display = build_display_mock(builder)
        display.name = "mock_display"
        paint(app, design, display, DISPLAY_APPEARANCE_ID)
        body = build_body(builder, comp)
        body.name = "stand_body"
        for face in cut_floppy(comp, body):
            paint(app, design, face, DISPLAY_APPEARANCE_ID)
        comp.isSketchFolderLightBulbOn = False  # keep sketch outlines off the renders
        comp.isConstructionFolderLightBulbOn = False
        app.activeViewport.fit()

        export(design, base, "stand_base.3mf")
        export(design, body, "stand_body.3mf")
        log("plinth {} x {} x {} mm".format(BASE_W, BASE_T, BASE_D))
        log("front decline: sides {} mm on the front x {} mm back, top/bottom {} x {} mm".format(
            FRONT_DECLINE_SIDE[0], FRONT_DECLINE_SIDE[1], FRONT_DECLINE_TOP[0], FRONT_DECLINE_TOP[1]
        ))
        log("body {} x {} x {} mm, {} mm walls, front leaning {} mm ({:.1f} deg)".format(
            BASE_W, BODY_H, BASE_D, WALL, FRONT_LEAN,
            math.degrees(math.atan2(FRONT_LEAN, BODY_H)),
        ))
        log("panel {} x {} x {} mm, opening {} x {} mm".format(
            PANEL_W, PANEL_H, PANEL_T, OPENING[0], OPENING[1]
        ))
        log("funnel {} x {} (r {}) -> {} x {} (r {}) over {} mm, {:.1f} deg, screen centre at y {} mm".format(
            round(FUNNEL_OUTER[0], 2), round(FUNNEL_OUTER[1], 2), FUNNEL_OUTER_R,
            round(OPENING[0], 2), round(OPENING[1], 2), OPENING_R,
            FUNNEL_DEPTH, math.degrees(math.atan2(FUNNEL_DEPTH, FUNNEL_RUN)), round(panel_center_y(), 1),
        ))
        log("lip fillet {} mm, throat chamfer {} mm; glass {} mm behind the throat edge".format(
            LIP_FILLET, THROAT_CHAMFER, PANEL_T - FUNNEL_DEPTH
        ))
    except Exception:
        import traceback

        log("FAILED:\n" + traceback.format_exc())
        raise


if __name__ == "__main__":
    run(None)
