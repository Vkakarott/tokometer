"""Generate the two-slope rear shell with continuous molded edge finishes.

Run in a new empty Fusion design. The v1/v2 models and exports are preserved;
v3 shares their mounting geometry and exports to its own directory.
"""

import importlib.util
import json
import math
from collections import Counter
from pathlib import Path
import xml.etree.ElementTree as ET
from zipfile import ZipFile

import adsk.core
import adsk.fusion


ROOT = Path(__file__).resolve().parent
OUTPUT = ROOT / "v3"
NAME = "tokometer_stand_v3"
REAR_SHOULDER_HEIGHT = 10.0
REAR_SHOULDER_ANGLE = 26.0
SHELL_EDGE_RADIUS = 2.0
LOWER_EDGE_RADIUS = 0.35
BASE_EDGE_RADIUS = 1.2


def load_previous():
    spec = importlib.util.spec_from_file_location("stand_v2_shared", ROOT / "stand_v2.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def rear_profile(g):
    bottom, top = g.BASE_T, g.BASE_T + g.BODY_H
    shoulder_y = top - REAR_SHOULDER_HEIGHT
    slope = g.BACK_LEAN / g.BODY_H
    shoulder_z = -g.BASE_D + slope * (shoulder_y - bottom)
    upper_slope = math.tan(math.radians(REAR_SHOULDER_ANGLE))
    top_z = shoulder_z + upper_slope * REAR_SHOULDER_HEIGHT
    return shoulder_y, shoulder_z, top_z, slope, upper_slope


def fillet(g, comp, edges, radius, name):
    selection = adsk.core.ObjectCollection.create()
    for edge in edges:
        selection.add(edge)
    if not selection.count:
        raise RuntimeError("No edges found for " + name)
    features = comp.features.filletFeatures
    inputs = features.createInput()
    inputs.edgeSetInputs.addConstantRadiusEdgeSet(
        selection, adsk.core.ValueInput.createByReal(g.cm(radius)), False
    )
    feature = features.add(inputs)
    feature.name = name
    return feature


def round_shell(g, comp, body):
    def at_bottom(edge):
        box = edge.boundingBox
        return abs(box.minPoint.y - g.cm(g.BASE_T)) < 1e-6 and abs(box.maxPoint.y - g.cm(g.BASE_T)) < 1e-6

    fillet(g, comp, [edge for edge in body.edges if not at_bottom(edge)],
           SHELL_EDGE_RADIUS, "Continuous exterior corner blends")
    fillet(g, comp, [edge for edge in body.edges if at_bottom(edge)],
           LOWER_EDGE_RADIUS, "Lower shell edge break")


def cut_screen(g, comp, body):
    center = g.screen_center_point()
    face = g.front_faces(body)[0]
    sketch = comp.sketches.addWithoutEdges(face)
    sketch.name = "Screen outer rim"
    outer = g.sketch_rect_at(sketch, center, *g.FUNNEL_OUTER, 2.0)
    plane = g.face_plane(comp, body, g.FUNNEL_DEPTH)
    inner_sketch = comp.sketches.add(plane)
    inner = g.sketch_rect_at(inner_sketch, center, *g.OPENING, g.OPENING_R, -g.FUNNEL_DEPTH)
    lofts = comp.features.loftFeatures
    inputs = lofts.createInput(g.CUT)
    inputs.loftSections.add(outer)
    inputs.loftSections.add(inner)
    inputs.participantBodies = [body]
    lofts.add(inputs).name = "Recessed screen surround"
    throat_sketch = comp.sketches.add(plane)
    throat = g.sketch_rect_at(throat_sketch, center, *g.OPENING, g.OPENING_R, -g.FUNNEL_DEPTH)
    g.cut_from_face(comp, body, throat, g.PANEL_T - g.FUNNEL_DEPTH + 1.0).name = "OLED aperture"
    fillet(g, comp, list(g.funnel_edges(body, 0.0, g.FUNNEL_OUTER)), 0.7, "Soft screen entry lip")
    fillet(g, comp, list(g.funnel_edges(body, -g.FUNNEL_DEPTH, g.OPENING)), 0.5, "Rounded screen throat")


def build_body(g, builder, comp):
    bottom, top = g.BASE_T, g.BASE_T + g.BODY_H
    shoulder_y, shoulder_z, top_z, slope, upper_slope = rear_profile(g)
    profile = [(bottom, 0), (top, -g.FRONT_LEAN), (top, top_z),
               (shoulder_y, shoulder_z), (bottom, -g.BASE_D)]
    body = builder.side_profile(-g.BASE_W / 2, profile, g.BASE_W, g.NEW)
    round_shell(g, comp, body)

    # Offset the upper rear plane by its normal thickness, keeping the lower
    # cavity unchanged where the ESP32, cable and snap lip fit.
    upper_offset = g.WALL * math.sqrt(1 + upper_slope ** 2)
    inner_shoulder_y = shoulder_y - (upper_offset - g.WALL) / (upper_slope - slope)
    inner_shoulder_z = -g.BASE_D + g.WALL + slope * (inner_shoulder_y - bottom)
    inner_top = top - g.WALL
    inner_top_z = shoulder_z + upper_slope * (inner_top - shoulder_y) + upper_offset
    nz, _ = g.front_normal()
    front_t = g.FRONT_WALL / nz
    cavity = [(bottom, -front_t),
              (inner_top, -g.FRONT_LEAN * (inner_top - bottom) / g.BODY_H - front_t),
              (inner_top, inner_top_z), (inner_shoulder_y, inner_shoulder_z),
              (bottom, -g.BASE_D + g.WALL)]
    builder.side_profile(-g.BASE_W / 2 + g.WALL, cavity, g.BASE_W - 2 * g.WALL, g.CUT, body)
    g.add_display_pad(comp, body)
    cut_screen(g, comp, body)
    g.add_display_rails(comp, body)
    g.cut_usb_port(builder, body)
    g.cut_snap_grooves(builder, body)
    return body


def build_base(g, builder):
    profile = [(0, -1.6), (0, -g.BASE_D + 0.4),
               (g.BASE_T, -g.BASE_D + 0.4), (g.BASE_T, 0.4)]
    base = builder.side_profile(-g.BASE_W / 2 - 0.4, profile, g.BASE_W + 0.8, g.NEW)
    fillet(g, builder.comp, list(base.edges), BASE_EDGE_RADIUS, "Molded base perimeter")
    g.add_snap_lip(builder, base)
    g.cut_pin_slots(builder, base)
    return base


def soften_details(g, comp, body):
    nz, ny = g.front_normal()
    drive_y = g.panel_center_y() - nz * (g.FUNNEL_OUTER[1] / 2 + 5.0)
    for x, y, width, height, name in ((2, drive_y, 18, 3.4, "Drive bezel edge"),
                                    (0, g.BASE_T + 9, 42, 7, "Lower fascia edge")):
        center = g.face_point(x, y)
        edges = []
        for edge in body.edges:
            _, start, end = edge.evaluator.getParameterExtents()
            _, point = edge.evaluator.getPointAtParameter((start + end) / 2)
            dy, dz = (point.y - center.y) * 10, (point.z - center.z) * 10
            off = dy * ny + dz * nz
            along = dy * nz - dz * ny
            if abs(off) < 0.01 and abs(point.x * 10 - x) <= width / 2 + 0.01 and abs(along) <= height / 2 + 0.01:
                edges.append(edge)
        fillet(g, comp, edges, 0.15, name)


def capture_views(app):
    view = app.activeViewport
    view.visualStyle = adsk.core.VisualStyles.ShadedVisualStyle
    for name, eye in (("side", (16, 3.3, -2.7)),
                      ("rear", (-11, 8, -19)),
                      ("preview", (-11, 8, 15))):
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


def validate_mesh(path):
    meshes = []
    with ZipFile(path) as archive:
        for entry in archive.namelist():
            if not entry.endswith(".model"):
                continue
            root = ET.fromstring(archive.read(entry))
            for mesh in root.findall(".//{*}mesh"):
                vertices = list(mesh.find("{*}vertices"))
                triangles = list(mesh.find("{*}triangles"))
                edges, directed = Counter(), Counter()
                for triangle in triangles:
                    indices = [int(triangle.attrib[key]) for key in ("v1", "v2", "v3")]
                    if len(set(indices)) != 3 or not all(0 <= i < len(vertices) for i in indices):
                        raise RuntimeError("Invalid mesh triangle: " + path.name)
                    for a, b in zip(indices, indices[1:] + indices[:1]):
                        edges[tuple(sorted((a, b)))] += 1
                        directed[(a, b)] += 1
                if not triangles or any(count != 2 for count in edges.values()):
                    raise RuntimeError("Mesh is not a closed edge manifold: " + path.name)
                if any(directed[(a, b)] != directed[(b, a)] for a, b in edges):
                    raise RuntimeError("Inconsistent mesh winding: " + path.name)
                meshes.append({"file": path.name, "vertices": len(vertices),
                               "triangles": len(triangles), "closed_edges": True,
                               "consistent_winding": True})
    if len(meshes) != 1:
        raise RuntimeError("Expected one printable mesh: " + path.name)
    return meshes[0]


def run(context):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    if (design is None or app.activeDocument.isSaved or design.timeline.count
            or design.rootComponent.bRepBodies.count or design.rootComponent.sketches.count
            or design.rootComponent.occurrences.count):
        raise RuntimeError("Open a new, empty Fusion design before running stand_v3.py")
    design.designType = adsk.fusion.DesignTypes.ParametricDesignType
    app.activeDocument.name = NAME
    previous = load_previous()
    g = previous.load_geometry()
    comp = design.rootComponent
    builder = g.Builder(comp)
    body = build_body(g, builder, comp)
    body.name = "stand_v3_body"
    base = build_base(g, builder)
    base.name = "stand_v3_base"
    display = g.build_display_mock(builder)
    display.name = "mock_display"
    esp = g.build_esp_mock(builder)
    esp.name = "mock_esp"
    source = g.appearance_for(app, design, g.DISPLAY_APPEARANCE_ID)
    body.appearance = previous.color(design, source, "Warm ivory shell", (224, 219, 201))
    base.appearance = previous.color(design, source, "Warm grey foot", (179, 179, 167))
    dark = previous.color(design, source, "Graphite details", (25, 31, 32))
    accent = previous.color(design, source, "Terracotta accent", (181, 65, 45))
    display.appearance = dark
    previous.front_details(g, comp, body, dark, accent, eject_drop=1.0)
    soften_details(g, comp, body)
    previous.side_grilles(g, builder, body)
    esp.isLightBulbOn = False
    comp.isSketchFolderLightBulbOn = False
    comp.isConstructionFolderLightBulbOn = False
    parts = (body, base, display, esp)
    result = previous.validate(design, parts)
    contacts = previous.check_assembly(g, design, parts, body_name=body.name)
    OUTPUT.mkdir(exist_ok=True)
    capture_views(app)
    manager = design.exportManager
    meshes = []
    for part in (body, base):
        path = OUTPUT / (part.name + ".3mf")
        options = manager.createC3MFExportOptions(part, str(path))
        options.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementHigh
        if not manager.execute(options):
            raise RuntimeError("Failed to export " + part.name)
        meshes.append(validate_mesh(path))
    if not manager.execute(manager.createFusionArchiveExportOptions(str(OUTPUT / (NAME + ".f3d")))):
        raise RuntimeError("Failed to export Fusion archive")
    report = {"design": NAME, "timeline_features": design.timeline.count, "timeline_issues": [],
              "parts": result, "expected_contacts": contacts, "unexpected_interferences": [],
              "rear_main_angle_degrees": math.degrees(math.atan(g.BACK_LEAN / g.BODY_H)),
              "front_angle_degrees": math.degrees(math.atan(g.FRONT_LEAN / g.BODY_H)),
              "rear_shoulder_angle_degrees": REAR_SHOULDER_ANGLE,
              "rear_shoulder_height_mm": REAR_SHOULDER_HEIGHT,
              "exterior_radius_mm": SHELL_EDGE_RADIUS,
              "base_radius_mm": BASE_EDGE_RADIUS, "meshes": meshes,
              "mechanical_interfaces": "Original OLED rails, ESP32 location, USB opening, pin slots and snap interface",
              "grilles": "Decorative recesses, 0.55 mm deep; not through vents"}
    (OUTPUT / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    run(None)
