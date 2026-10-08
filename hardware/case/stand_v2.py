"""Build the refined retro enclosure in a new, empty Fusion design.

The original stand generator and its exports remain the v1 baseline. This
variant reuses its mechanical interfaces and changes exterior styling only.
Run through Fusion's Python API; output goes to hardware/case/v2.
"""

import importlib.util
import json
from pathlib import Path

import adsk.core
import adsk.fusion


ROOT = Path(__file__).resolve().parent
OUTPUT = ROOT / "v2"
NAME = "tokometer_stand_v2"


def load_geometry():
    spec = importlib.util.spec_from_file_location("stand_geometry_v2", ROOT / "stand.py")
    geometry = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(geometry)
    geometry.FRONT_DECLINE_SIDE = (1.6, 1.0)
    geometry.FRONT_DECLINE_TOP = (2.5, 1.0)
    geometry.DECLINE_CORNER_R = 1.8
    geometry.BOX_EDGE_R = 1.4
    geometry.FUNNEL_DEPTH = 1.4
    geometry.FUNNEL_RUN = 3.6
    geometry.FUNNEL_OUTER = tuple(size + 2 * geometry.FUNNEL_RUN for size in geometry.OPENING)
    geometry.FUNNEL_OUTER_R = 1.5
    geometry.LIP_FILLET = 0.4
    geometry.THROAT_CHAMFER = 0.35
    return geometry


def color(design, source, name, rgb):
    appearance = design.appearances.addByCopy(source, name)
    prop = adsk.core.ColorProperty.cast(appearance.appearanceProperties.itemById("opaque_albedo"))
    if prop is None:
        prop = next((adsk.core.ColorProperty.cast(p) for p in appearance.appearanceProperties
                     if adsk.core.ColorProperty.cast(p)), None)
    if prop is None:
        raise RuntimeError("The plastic appearance has no editable color")
    prop.value = adsk.core.Color.create(*rgb, 255)
    return appearance


def recess(g, comp, body, x, y, width, height, depth, radius, name):
    sketch = comp.sketches.addWithoutEdges(g.front_faces(body)[0])
    sketch.name = name + " profile"
    profile = g.sketch_rect_at(sketch, g.face_point(x, y), width, height, radius)
    feature = g.cut_from_face(comp, body, profile, depth)
    feature.name = name
    return feature


def build_base(g, builder):
    profile = [(0, -1.6), (0, -g.BASE_D + 0.4), (g.BASE_T, -g.BASE_D + 0.4),
               (g.BASE_T, 0.6), (g.BASE_T - 1.2, 0.6), (2.0, -1.6)]
    base = builder.side_profile(-g.BASE_W / 2 - 0.4, profile, g.BASE_W + 0.8, g.NEW)
    edges = adsk.core.ObjectCollection.create()
    for edge in base.edges:
        edges.add(edge)
    g.fillet_edges(builder.comp, edges, 0.55)
    g.add_snap_lip(builder, base)
    g.cut_pin_slots(builder, base)
    return base


def front_details(g, comp, body, dark, accent, eject_drop=1.25):
    nz, _ = g.front_normal()
    drive_y = g.panel_center_y() - nz * (g.FUNNEL_OUTER[1] / 2 + 5.0)
    recess(g, comp, body, 2.0, drive_y, 18.0, 3.4, 0.45, 0.65, "Drive bezel")
    slot = recess(g, comp, body, 1.5, drive_y + 0.35, 15.5, 1.0, 1.0, 0.25, "Drive slot")
    for face in slot.faces:
        face.appearance = dark
    eject = recess(g, comp, body, 6.0, drive_y - eject_drop, 4.5, 1.0, 0.6, 0.25, "Eject accent")
    for face in eject.faces:
        face.appearance = accent

    panel_y = g.BASE_T + 9.0
    recess(g, comp, body, 0, panel_y, 42.0, 7.0, 0.3, 0.8, "Lower fascia inset")
    badge = recess(g, comp, body, -16.5, panel_y, 2.5, 3.0, 0.55, 0.5, "Lower fascia badge")
    for face in badge.faces:
        face.appearance = accent
    for index, width in enumerate((10.0, 6.0)):
        detail = recess(g, comp, body, -8.5 + width / 2, panel_y + 0.7 - index * 1.4,
                        width, 0.5, 0.45, 0.15, "Label line {}".format(index + 1))
        for face in detail.faces:
            face.appearance = dark


def side_grilles(g, builder, body):
    rows, columns = 4, 9
    slot_w, slot_h, depth = 3.0, 0.8, 0.55
    pitch_z, pitch_y = 4.4, 1.8
    for side, x in (("right", g.BASE_W / 2 - depth), ("left", -g.BASE_W / 2 - 1.0)):
        sketch = builder.comp.sketches.add(builder._offset_plane(builder.comp.yZConstructionPlane, x))
        sketch.name = side.title() + " grille pattern"
        for row in range(rows):
            for column in range(columns):
                z = g.BASE_D - 5.0 - column * pitch_z
                y = g.BASE_T + 4.0 + row * pitch_y
                g.rounded_rect(sketch, z - slot_w, y, z, y + slot_h, 0.25)
        profiles = adsk.core.ObjectCollection.create()
        for profile in sketch.profiles:
            profiles.add(profile)
        feature_input = builder.comp.features.extrudeFeatures.createInput(profiles, g.CUT)
        feature_input.setDistanceExtent(False, adsk.core.ValueInput.createByReal(g.cm(depth + 1.0)))
        feature_input.participantBodies = [body]
        feature = builder.comp.features.extrudeFeatures.add(feature_input)
        feature.name = side.title() + " recessed grille"


def validate(design, parts):
    unhealthy = [{"name": item.name, "message": item.errorOrWarningMessage}
                 for item in design.timeline
                 if item.healthState != adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState]
    if unhealthy:
        raise RuntimeError("Unhealthy timeline: " + json.dumps(unhealthy))
    for body in parts:
        if not body.isSolid or body.volume <= 0:
            raise RuntimeError("Invalid solid: " + body.name)
    return [{"name": body.name, "volume_mm3": round(body.volume * 1000, 3),
             "size_mm": [round((getattr(body.boundingBox.maxPoint, axis) -
                                getattr(body.boundingBox.minPoint, axis)) * 10, 3)
                         for axis in ("x", "y", "z")]}
            for body in parts]


def check_assembly(g, design, parts, body_name="stand_v2_body"):
    entities = adsk.core.ObjectCollection.create()
    for body in parts:
        entities.add(body)
    inputs = design.createInterferenceInput(entities)
    inputs.areCoincidentFacesIncluded = False
    results = design.analyzeInterference(inputs)
    if results is None:
        raise RuntimeError("Interference analysis returned no result")
    center = g.screen_center_point()
    nz, ny = g.front_normal()
    bottom, _ = g.module_v_span()
    nub_y = (bottom + g.MODULE_H * 0.25, bottom + g.MODULE_H * 0.75)
    nub_x = g.MODULE_W / 2 - g.RAIL_LIP / 2
    nub_depth = g.PANEL_T + g.GLASS_T + g.PCB_T + g.RAIL_GAP_CLEAR - g.RAIL_NUB
    contacts = []
    for result in results:
        names = {result.entityOne.name, result.entityTwo.name}
        if names != {body_name, "mock_display"}:
            raise RuntimeError("Unexpected interference: " + str(names))
        for vertex in result.interferenceBody.vertices:
            point = vertex.geometry
            x = point.x * 10
            dy, dz = (point.y - center.y) * 10, (point.z - center.z) * 10
            v, depth = dy * nz - dz * ny, -(dy * ny + dz * nz)
            if not (abs(abs(x) - nub_x) <= g.RAIL_LIP * 0.4 + 0.02
                    and any(abs(v - y) <= 1.52 for y in nub_y)
                    and nub_depth - 0.02 <= depth <= nub_depth + g.RAIL_NUB + 0.02):
                raise RuntimeError("OLED contact extends beyond the original retention nubs")
        contacts.append({"parts": sorted(names), "kind": "Original OLED press-fit nubs",
                         "volume_mm3": round(result.interferenceBody.volume * 1000, 4)})
    return contacts


def show_model(app):
    camera = app.activeViewport.camera
    camera.isSmoothTransition = False
    camera.cameraType = adsk.core.CameraTypes.OrthographicCameraType
    camera.target = adsk.core.Point3D.create(0, 3.3, -2.7)
    camera.eye = adsk.core.Point3D.create(11, 8.0, 15)
    camera.upVector = adsk.core.Vector3D.create(0, 1, 0)
    camera.isFitView = True
    app.activeViewport.camera = camera
    app.activeViewport.refresh()


def export_model(design, body, base):
    OUTPUT.mkdir(exist_ok=True)
    manager = design.exportManager
    for part in (body, base):
        options = manager.createC3MFExportOptions(part, str(OUTPUT / (part.name + ".3mf")))
        options.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementHigh
        if not manager.execute(options):
            raise RuntimeError("Failed to export " + part.name)
    if not manager.execute(manager.createFusionArchiveExportOptions(str(OUTPUT / (NAME + ".f3d")))):
        raise RuntimeError("Failed to export Fusion archive")


def run(context):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    if (design is None or app.activeDocument.isSaved or design.timeline.count
            or design.rootComponent.bRepBodies.count or design.rootComponent.sketches.count
            or design.rootComponent.occurrences.count):
        raise RuntimeError("Open a new, empty Fusion design before running stand_v2.py")
    design.designType = adsk.fusion.DesignTypes.ParametricDesignType
    app.activeDocument.name = NAME
    comp = design.rootComponent
    g = load_geometry()
    builder = g.Builder(comp)
    body = g.build_body(builder, comp)
    body.name = "stand_v2_body"
    base = build_base(g, builder)
    base.name = "stand_v2_base"
    display = g.build_display_mock(builder)
    display.name = "mock_display"
    esp = g.build_esp_mock(builder)
    esp.name = "mock_esp"

    source = g.appearance_for(app, design, g.DISPLAY_APPEARANCE_ID)
    ivory = color(design, source, "Warm ivory shell", (224, 219, 201))
    foot = color(design, source, "Warm grey foot", (179, 179, 167))
    dark = color(design, source, "Graphite details", (25, 31, 32))
    accent = color(design, source, "Terracotta accent", (181, 65, 45))
    body.appearance = ivory
    base.appearance = foot
    display.appearance = dark
    front_details(g, comp, body, dark, accent)
    side_grilles(g, builder, body)
    esp.isLightBulbOn = False
    comp.isSketchFolderLightBulbOn = False
    comp.isConstructionFolderLightBulbOn = False
    result = validate(design, (body, base, display, esp))
    contacts = check_assembly(g, design, (body, base, display, esp))
    show_model(app)
    export_model(design, body, base)
    if not app.activeViewport.saveAsImageFile(str(OUTPUT / "preview.png"), 1200, 1000):
        raise RuntimeError("Failed to save the viewport preview")
    report = {"design": NAME, "timeline_features": design.timeline.count,
              "timeline_issues": [], "parts": result,
              "expected_contacts": contacts, "unexpected_interferences": [],
              "mechanical_interfaces": "Inherited from stand.py; unchanged",
              "grilles": "Decorative recesses, 0.55 mm deep; not through vents"}
    (OUTPUT / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    run(None)
