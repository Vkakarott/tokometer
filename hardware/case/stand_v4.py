"""Generate a Macintosh-inspired exterior study, not a hardware-fit revision.

The v3 envelope is retained as a design scale. The larger screen is a visual
placeholder; JD9853 sizing, mounts, ports and assembly tolerances are deferred.
Run in a new empty Fusion design. Previous revisions remain untouched.
"""

import importlib.util
import json
import math
from pathlib import Path

import adsk.core
import adsk.fusion


ROOT = Path(__file__).resolve().parent
OUTPUT = ROOT / "v4"
NAME = "tokometer_stand_v4_design"
FACE_DEPTH = 4.0
FACE_BEVEL_DEPTH = 1.8
SCREEN_OPENING = (35.5, 28.3)
SCREEN_SURROUND = (42.0, 35.0)


def load_revision():
    spec = importlib.util.spec_from_file_location("stand_v3_shared", ROOT / "stand_v3.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def face_profile(g, comp, plane, center, size, radius, offset, name):
    sketch = comp.sketches.add(plane)
    sketch.name = name
    return g.sketch_rect_at(sketch, center, *size, radius, offset)


def extrude(g, comp, profile, distance, operation, name, body=None):
    features = comp.features.extrudeFeatures
    inputs = features.createInput(profile, operation)
    inputs.setDistanceExtent(False, adsk.core.ValueInput.createByReal(g.cm(distance)))
    if body is not None:
        inputs.participantBodies = [body]
    feature = features.add(inputs)
    feature.name = name
    return feature


def build_shell(g, v3, builder):
    bottom, top = g.BASE_T, g.BASE_T + g.BODY_H
    shoulder_y, shoulder_z, top_z, slope, upper_slope = v3.rear_profile(g)
    nz, _ = g.front_normal()
    setback = FACE_DEPTH / nz
    profile = [(bottom, -setback), (top, -g.FRONT_LEAN - setback),
               (top, top_z), (shoulder_y, shoulder_z), (bottom, -g.BASE_D)]
    shell = builder.side_profile(-g.BASE_W / 2, profile, g.BASE_W, g.NEW)
    shell.name = "concept_rear_shell"

    def at_bottom(edge):
        box = edge.boundingBox
        return (abs(box.minPoint.y - g.cm(bottom)) < 1e-6
                and abs(box.maxPoint.y - g.cm(bottom)) < 1e-6)

    v3.fillet(g, builder.comp, [e for e in shell.edges if not at_bottom(e)],
              1.2, "Rear shell controlled corner radii")
    v3.fillet(g, builder.comp, [e for e in shell.edges if at_bottom(e)],
              0.3, "Rear shell lower edge break")
    planes = {"back": g.face_plane(builder.comp, shell, 0),
              "shoulder": g.face_plane(builder.comp, shell, -(FACE_DEPTH - FACE_BEVEL_DEPTH)),
              "front": g.face_plane(builder.comp, shell, -FACE_DEPTH)}

    upper_offset = g.WALL * math.sqrt(1 + upper_slope ** 2)
    inner_shoulder_y = shoulder_y - (upper_offset - g.WALL) / (upper_slope - slope)
    inner_shoulder_z = -g.BASE_D + g.WALL + slope * (inner_shoulder_y - bottom)
    inner_top = top - g.WALL
    inner_top_z = shoulder_z + upper_slope * (inner_top - shoulder_y) + upper_offset
    cavity = [(bottom, 1), (inner_top, 1), (inner_top, inner_top_z),
              (inner_shoulder_y, inner_shoulder_z), (bottom, -g.BASE_D + g.WALL)]
    builder.side_profile(-g.BASE_W / 2 + g.WALL, cavity,
                         g.BASE_W - 2 * g.WALL, g.CUT, shell)
    return shell, planes


def build_front(g, v3, comp, planes):
    center = g.face_point(0, g.BASE_T + g.BODY_H / 2)
    back_size, front_size = (51.6, 57.0), (47.8, 53.6)
    back = face_profile(g, comp, planes["back"], center, back_size, 1.3,
                        -FACE_DEPTH, "Front panel back outline")
    panel = extrude(g, comp, back, FACE_DEPTH - FACE_BEVEL_DEPTH, g.NEW,
                    "Front panel perimeter return").bodies.item(0)
    panel.name = "concept_front_bezel"
    shoulder = face_profile(g, comp, planes["shoulder"], center, back_size, 1.3,
                            -FACE_BEVEL_DEPTH, "Front bevel outer outline")
    front = face_profile(g, comp, planes["front"], center, front_size, 1.0,
                         0, "Front bevel inner outline")
    lofts = comp.features.loftFeatures
    inputs = lofts.createInput(g.JOIN)
    inputs.loftSections.add(shoulder)
    inputs.loftSections.add(front)
    inputs.participantBodies = [panel]
    lofts.add(inputs).name = "Broad flat front perimeter bevel"
    face = g.front_faces(panel)[0]
    edges = [edge for loop in face.loops if loop.isOuter for edge in loop.edges]
    v3.fillet(g, comp, edges, 0.25, "Front bevel subtle edge break")
    cut_screen(g, v3, comp, panel)
    return panel


def cut_screen(g, v3, comp, panel):
    center = g.screen_center_point()
    outer_sketch = comp.sketches.addWithoutEdges(g.front_faces(panel)[0])
    outer = g.sketch_rect_at(outer_sketch, center, *g.FUNNEL_OUTER, 1.2)
    plane = g.face_plane(comp, panel, g.FUNNEL_DEPTH)
    inner = face_profile(g, comp, plane, center, g.OPENING, g.OPENING_R,
                         -g.FUNNEL_DEPTH, "Screen surround inner outline")
    inputs = comp.features.loftFeatures.createInput(g.CUT)
    inputs.loftSections.add(outer)
    inputs.loftSections.add(inner)
    inputs.participantBodies = [panel]
    comp.features.loftFeatures.add(inputs).name = "Deep near-square screen surround"
    throat = face_profile(g, comp, plane, center, g.OPENING, g.OPENING_R,
                          -g.FUNNEL_DEPTH, "Screen appearance opening")
    extrude(g, comp, throat, -(FACE_DEPTH - g.FUNNEL_DEPTH + 1), g.CUT,
            "Screen throat", panel)
    v3.fillet(g, comp, list(g.funnel_edges(panel, 0, g.FUNNEL_OUTER)),
              0.3, "Crisp screen surround edge break")
    v3.fillet(g, comp, list(g.funnel_edges(panel, -g.FUNNEL_DEPTH, g.OPENING)),
              0.2, "Screen throat subtle edge break")


def color_feature(feature, appearance):
    for face in feature.faces:
        face.appearance = appearance


def front_details(g, previous, comp, panel, dark, colors):
    drive_y = 21.8
    previous.recess(g, comp, panel, 8.2, drive_y, 20.6, 2.0, 0.55, 0.2,
                    "Asymmetric floppy drive recess")
    previous.recess(g, comp, panel, 17.4, drive_y + 0.45, 3.0, 3.3, 0.6, 0.15,
                    "Floppy drive stepped right pocket")
    slot = previous.recess(g, comp, panel, 8.0, drive_y + 0.05, 19.6, 0.55,
                           0.85, 0.1, "Fine horizontal floppy slot")
    color_feature(slot, dark)
    latch = previous.recess(g, comp, panel, 16.9, drive_y - 0.5, 1.8, 0.35,
                            0.75, 0.07, "Small drive latch accent")
    color_feature(latch, colors[3])
    previous.recess(g, comp, panel, -17.0, 14.2, 2.8, 3.2, 0.12, 0.3,
                    "Small inset rainbow badge")
    for index, appearance in enumerate(colors):
        stripe = previous.recess(g, comp, panel, -17.0, 15.35 - index * 0.46,
                                 2.25, 0.4, 0.17, 0.05, "Badge stripe {}".format(index + 1))
        color_feature(stripe, appearance)


def side_grilles(g, builder, shell):
    depth = 0.6
    for side, x in (("right", g.BASE_W / 2 - depth), ("left", -g.BASE_W / 2 - 1)):
        sketch = builder.comp.sketches.add(builder._offset_plane(builder.comp.yZConstructionPlane, x))
        sketch.name = side.title() + " continuous ventilation lines"
        for row in range(5):
            y = g.BASE_T + 2.5 + row * 1.5
            g.rounded_rect(sketch, 22.0, y, g.BASE_D - 4.0, y + 0.6, 0.16)
        profiles = adsk.core.ObjectCollection.create()
        for profile in sketch.profiles:
            profiles.add(profile)
        extrude(g, builder.comp, profiles, depth + 1, g.CUT,
                side.title() + " long recessed grille", shell)


def build_base(g, v3, builder, dark):
    profile = [(0, -3.2), (0, -g.BASE_D + 1.2),
               (g.BASE_T, -g.BASE_D + 1.2), (g.BASE_T, -2.4)]
    base = builder.side_profile(-25.0, profile, 50.0, g.NEW)
    base.name = "concept_recessed_foot"
    v3.fillet(g, builder.comp, list(base.edges), 0.8, "Recessed foot soft perimeter")
    plane = builder._offset_plane(builder.comp.xYConstructionPlane, -1.8)
    sketch = builder.comp.sketches.add(plane)
    sketch.name = "Keyboard port visual placeholder"
    g.rounded_rect(sketch, 16.1, 2.4, 19.9, 5.6, 0.4)
    port = extrude(g, builder.comp, g.smallest_profile(sketch), -3.0, g.CUT,
                   "Recessed keyboard port detail - nonfunctional", base)
    color_feature(port, dark)
    return base


def build_screen(g, comp, panel, dark, glass):
    plane = g.face_plane(comp, panel, FACE_DEPTH + 0.05)
    center = g.screen_center_point()
    profile = face_profile(g, comp, plane, center, (37.0, 29.8), 2.2,
                           -FACE_DEPTH - 0.05, "Screen appearance placeholder")
    screen = extrude(g, comp, profile, -0.7, g.NEW, "Screen visual only - not JD9853 geometry").bodies.item(0)
    screen.name = "concept_screen_placeholder"
    screen.appearance = dark
    inside = face_profile(g, comp, plane, center, (33.4, 26.2), 1.7,
                          -FACE_DEPTH - 0.05, "Screen glass appearance region")
    inset = extrude(g, comp, inside, -0.025, g.CUT, "Glass appearance inset", screen)
    color_feature(inset, glass)
    return screen


def capture_views(app):
    view = app.activeViewport
    view.visualStyle = adsk.core.VisualStyles.ShadedVisualStyle
    for name, eye in (("front", (0, 5.1, 16)), ("side", (16, 3.3, -2.7)),
                      ("rear", (-11, 7, -19)), ("preview", (-9, 6.6, 18))):
        camera = view.camera
        camera.isSmoothTransition = False
        camera.cameraType = adsk.core.CameraTypes.OrthographicCameraType
        camera.target = adsk.core.Point3D.create(0, 3.3, -2.7)
        camera.eye = adsk.core.Point3D.create(*eye)
        camera.upVector = adsk.core.Vector3D.create(0, 1, 0)
        camera.isFitView = True
        view.camera = camera
        view.refresh()
        if not view.saveAsImageFile(str(OUTPUT / (name + ".png")), 1200, 1000):
            raise RuntimeError("Could not capture " + name)


def check_contacts(design, parts):
    entities = adsk.core.ObjectCollection.create()
    for part in parts:
        entities.add(part)
    inputs = design.createInterferenceInput(entities)
    inputs.areCoincidentFacesIncluded = False
    contacts = design.analyzeInterference(inputs)
    if contacts is None:
        raise RuntimeError("Interference analysis returned no result")
    if contacts.count:
        raise RuntimeError("Unexpected concept solid overlaps: " + str([
            (contact.entityOne.name, contact.entityTwo.name) for contact in contacts]))


def run(context):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    if (design is None or app.activeDocument.isSaved or design.timeline.count
            or design.rootComponent.bRepBodies.count or design.rootComponent.sketches.count
            or design.rootComponent.occurrences.count):
        raise RuntimeError("Open a new, empty Fusion design before running stand_v4.py")
    design.designType = adsk.fusion.DesignTypes.ParametricDesignType
    app.activeDocument.name = NAME
    v3 = load_revision()
    previous = v3.load_previous()
    g = previous.load_geometry()
    g.OPENING = SCREEN_OPENING
    g.FUNNEL_OUTER = SCREEN_SURROUND
    g.OPENING_R = 1.8
    g.FUNNEL_DEPTH = 2.5
    g.SCREEN_FROM_TOP = 22.0
    comp = design.rootComponent
    builder = g.Builder(comp)
    source = g.appearance_for(app, design, g.DISPLAY_APPEARANCE_ID)
    palette = {name: previous.color(design, source, name, rgb) for name, rgb in (
        ("Front warm ivory", (223, 216, 185)), ("Rear warm beige", (194, 189, 166)),
        ("Recessed foot warm grey", (148, 146, 128)), ("Graphite details", (24, 27, 25)),
        ("Screen muted glass", (57, 69, 61)))}
    badge_colors = [previous.color(design, source, "Badge color {}".format(i + 1), rgb)
                    for i, rgb in enumerate(((69, 131, 62), (232, 195, 61), (224, 136, 47),
                                             (193, 70, 54), (137, 76, 135), (60, 118, 166)))]
    shell, planes = build_shell(g, v3, builder)
    shell.appearance = palette["Rear warm beige"]
    front = build_front(g, v3, comp, planes)
    front.appearance = palette["Front warm ivory"]
    front_details(g, previous, comp, front, palette["Graphite details"], badge_colors)
    side_grilles(g, builder, shell)
    base = build_base(g, v3, builder, palette["Graphite details"])
    base.appearance = palette["Recessed foot warm grey"]
    screen = build_screen(g, comp, front, palette["Graphite details"], palette["Screen muted glass"])
    comp.isSketchFolderLightBulbOn = False
    comp.isConstructionFolderLightBulbOn = False
    parts = (shell, front, base, screen)
    solids = previous.validate(design, parts)
    check_contacts(design, parts)
    OUTPUT.mkdir(exist_ok=True)
    capture_views(app)
    options = design.exportManager.createFusionArchiveExportOptions(str(OUTPUT / (NAME + ".f3d")))
    if not design.exportManager.execute(options):
        raise RuntimeError("Failed to export the Fusion design study")
    report = {"design": NAME, "purpose": "Exterior design study only; not ready for hardware assembly or printing",
              "timeline_features": design.timeline.count, "timeline_issues": [], "parts": solids,
              "unexpected_interferences": [], "screen_opening_at_study_scale_mm": SCREEN_OPENING,
              "screen_surround_at_study_scale_mm": SCREEN_SURROUND,
              "rear_main_angle_degrees": math.degrees(math.atan(g.BACK_LEAN / g.BODY_H)),
              "rear_shoulder_angle_degrees": v3.REAR_SHOULDER_ANGLE,
              "deferred": ["Enclosure enlargement for JD9853", "Display PCB offset and thickness",
                           "Display and ESP32-C3 mounting", "Functional USB port", "Assembly and print tolerances"],
              "grilles": "Decorative recesses, not through vents",
              "keyboard_port": "Nonfunctional visual recess",
              "screen": "Visual placeholder, not the JD9853 module"}
    (OUTPUT / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    run(None)
