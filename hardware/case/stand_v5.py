"""Build the serviceable JD9853 enclosure prototype in a new Fusion document.

Electronics depths and offsets remain provisional; no production print meshes
are exported until the actual modules and print tolerances have been checked.
"""

import importlib.util
import json
import math
from pathlib import Path

import adsk.core
import adsk.fusion

ROOT = Path(__file__).resolve().parent
OUTPUT = ROOT / "v5"
NAME = "tokometer_stand_v5_assembly"


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def configure(g, s):
    g.BASE_W, g.BASE_D, g.BASE_T = s.CASE_WIDTH, s.CASE_DEPTH, s.BASE_HEIGHT
    g.BODY_H, g.PANEL_H, g.WALL = s.BODY_HEIGHT, s.BODY_HEIGHT, s.WALL
    g.FRONT_LEAN, g.BACK_LEAN = s.FRONT_LEAN, s.REAR_LEAN
    g.SCREEN_FROM_TOP = s.SCREEN_FROM_TOP
    g.OPENING, g.FUNNEL_OUTER = s.DISPLAY_WINDOW, (48.0, 39.8)
    g.OPENING_R, g.FUNNEL_DEPTH = 0.4, 2.5
    g.BOARD_PCB_T, g.USB_ABOVE_PCB = s.ESP_PCB_THICKNESS, s.ESP_USB_ABOVE_PCB
    g.HEADER_T = s.ESP_BOARD_BOTTOM - s.BASE_HEIGHT
    g.USB_OVERMOULD = s.USB_OPENING


def face_box(g, v4, comp, panel, u, v, width, height, start, depth, op, name, target=None):
    plane = g.face_plane(comp, panel, start)
    profile = v4.face_profile(g, comp, plane, g.face_uv_point(u, v),
                              (width, height), 0.15, -start, name + " profile")
    feature = v4.extrude(g, comp, profile, -depth, op, name, target)
    return feature.bodies.item(0) if op == g.NEW else target


def face_hole(g, v4, comp, panel, target, u, v, start, depth, diameter, name):
    sketch = comp.sketches.add(g.face_plane(comp, panel, start))
    sketch.name = name + " profile"
    center = sketch.modelToSketchSpace(g.face_uv_point(u, v, -start))
    sketch.sketchCurves.sketchCircles.addByCenterRadius(center, g.cm(diameter / 2))
    v4.extrude(g, comp, sketch.profiles.item(0), -depth, g.CUT, name, target)


def build_front(g, v3, v4, comp):
    center = g.face_point(0, g.BASE_T + g.BODY_H / 2)
    plane_input = comp.constructionPlanes.createInput()
    points = [g.face_uv_point(-1, 0), g.face_uv_point(1, 0), g.face_uv_point(0, 1)]
    sketch = comp.sketches.add(comp.xYConstructionPlane)
    anchors = [sketch.sketchPoints.add(p) for p in points]
    plane_input.setByThreePoints(*anchors)
    plane = comp.constructionPlanes.add(plane_input)
    plane.name = "Front exterior datum"
    profiles = []
    for depth, size, radius in ((4.0, (67.6, 74.4), 1.3),
                                (1.8, (67.6, 74.4), 1.3),
                                (0.0, (63.8, 70.8), 1.0)):
        inputs = comp.constructionPlanes.createInput()
        inputs.setByOffset(plane, adsk.core.ValueInput.createByReal(g.cm(-depth)))
        section_plane = comp.constructionPlanes.add(inputs)
        profiles.append(v4.face_profile(g, comp, section_plane, center, size,
                                        radius, -depth, "Front perimeter section"))
    panel = v4.extrude(g, comp, profiles[0], 2.2, g.NEW, "Front perimeter return").bodies.item(0)
    inputs = comp.features.loftFeatures.createInput(g.JOIN)
    inputs.loftSections.add(profiles[1])
    inputs.loftSections.add(profiles[2])
    inputs.participantBodies = [panel]
    comp.features.loftFeatures.add(inputs).name = "Macintosh broad front bevel"
    panel.name = "v5_front_frame"
    face = g.front_faces(panel)[0]
    v3.fillet(g, comp, [e for loop in face.loops if loop.isOuter for e in loop.edges],
              0.25, "Front perimeter edge break")
    v4.cut_screen(g, v3, comp, panel, throat_edge_radius=0.08)
    return panel


def build_shell(g, v3, v4, builder):
    bottom, top = g.BASE_T + 0.25, g.BASE_T + g.BODY_H
    shoulder_y, shoulder_z, top_z, slope, upper_slope = v3.rear_profile(g)
    nz, _ = g.front_normal()
    setback = 4.25 / nz
    points = [(bottom, -setback - (bottom - g.BASE_T) * g.FRONT_LEAN / g.BODY_H),
              (top, -g.FRONT_LEAN - setback), (top, top_z),
              (shoulder_y, shoulder_z), (bottom, -g.BASE_D + 0.25 * slope)]
    shell = builder.side_profile(-g.BASE_W / 2, points, g.BASE_W, g.NEW)
    shell.name = "v5_rear_cover"
    v3.fillet(g, builder.comp, list(shell.edges), 0.7, "Rear cover continuous edge rounds")
    upper_offset = g.WALL * math.sqrt(1 + upper_slope ** 2)
    inner_shoulder_y = shoulder_y - (upper_offset - g.WALL) / (upper_slope - slope)
    inner_shoulder_z = -g.BASE_D + g.WALL + slope * (inner_shoulder_y - g.BASE_T)
    inner_top = top - g.WALL
    inner_top_z = shoulder_z + upper_slope * (inner_top - shoulder_y) + upper_offset
    cavity = [(g.BASE_T - 1, 2), (inner_top, 2), (inner_top, inner_top_z),
              (inner_shoulder_y, inner_shoulder_z), (g.BASE_T - 1, -g.BASE_D + g.WALL)]
    builder.side_profile(-g.BASE_W / 2 + g.WALL, cavity,
                         g.BASE_W - 2 * g.WALL, g.CUT, shell)
    v4.side_grilles(g, builder, shell)
    v4.cut_rear_usb(g, builder, shell)
    return shell


def bottom_joint(g, s, builder, base, part, x, z, inward_edge, outer_edge):
    x0, x1 = sorted((inward_edge, outer_edge))
    builder.slab(g.BASE_T, x0, z - 4.5, x1, z + 4.5, 9.0, g.JOIN, part)
    builder.cylinder(g.BASE_T - 0.2, x, z, s.SCREW_CLEARANCE, 10.0, g.CUT, part)
    builder.cylinder(-0.1, x, z, s.SCREW_CLEARANCE, g.BASE_T + 0.3, g.CUT, base)
    builder.cylinder(-0.1, x, z, s.HEAD_POCKET_DIAMETER,
                     s.HEAD_POCKET_DEPTH + 0.1, g.CUT, base)


def bottom_fasteners(g, s, builder):
    hardware = []
    for label, positions in (("cover", s.SHELL_SCREWS), ("front", s.FRONT_SCREWS)):
        for index, (x, z) in enumerate(positions, 1):
            seat = s.HEAD_POCKET_DEPTH
            screw = builder.cylinder(seat - 3.0, x, z, 5.5, 3.0, g.NEW)
            screw.name = "reference_{}_screw_{}_M3x16_unthreaded".format(label, index)
            builder.cylinder(seat - 0.01, x, z, 3.0,
                             s.BOTTOM_SCREW_LENGTH + 0.01, g.JOIN, screw)
            sketch = builder.comp.sketches.add(builder._offset_plane(
                builder.comp.xZConstructionPlane, s.NUT_SEAT_HEIGHT))
            sketch.name = "M3 nut across-flats profile"
            radius = s.NUT_ACROSS_FLATS / math.sqrt(3)
            points = [adsk.core.Point3D.create(g.cm(x + radius * math.cos(i * math.pi / 3)),
                                             g.cm(-z + radius * math.sin(i * math.pi / 3)), 0)
                      for i in range(6)]
            for first, second in zip(points, points[1:] + points[:1]):
                sketch.sketchCurves.sketchLines.addByTwoPoints(first, second)
            nut = builder._extrude(sketch.profiles.item(0), s.NUT_THICKNESS, g.NEW)
            nut.name = "reference_{}_nut_{}_M3_unthreaded".format(label, index)
            builder.cylinder(s.NUT_SEAT_HEIGHT - 0.1, x, z, 3.1,
                             s.NUT_THICKNESS + 0.2, g.CUT, nut)
            hardware.extend((screw, nut))
    return tuple(hardware)


def build_base_and_joints(g, s, v3, v4, builder, front, shell, dark):
    profile = [(0, -3.2), (0, -g.BASE_D + 1.2),
               (g.BASE_T, -g.BASE_D + 1.2), (g.BASE_T, -2.4)]
    base = builder.side_profile(-33, profile, 66, g.NEW)
    base.name = "v5_service_base"
    v3.fillet(g, builder.comp, list(base.edges), 0.6, "Base exterior edge break")
    for x, z in s.FRONT_SCREWS:
        sign = math.copysign(1, x)
        builder.slab(g.BASE_T, x - 4.5, -14.5, x + 4.5, -3.0, 9.0, g.JOIN, front)
        bottom_joint(g, s, builder, base, front, x, z, sign * 11.5, sign * 20.5)
        builder.cylinder(g.BASE_T - 0.1, x, -5.5, 2.4, 1.9, g.JOIN, base)
        builder.cylinder(g.BASE_T - 0.1, x, -5.5, 3.0, 2.2, g.CUT, front)
    for x, z in s.SHELL_SCREWS:
        sign = math.copysign(1, x)
        bottom_joint(g, s, builder, base, shell, x, z, sign * 22.0, sign * 32.5)
    for sign in (-1, 1):
        x = sign * s.GUIDE_X
        builder.slab(g.BASE_T - 0.1, x - s.GUIDE_WIDTH / 2, s.GUIDE_Z[0],
                     x + s.GUIDE_WIDTH / 2, s.GUIDE_Z[1], s.GUIDE_HEIGHT + 0.1, g.JOIN, base)
        builder.slab(g.BASE_T + 0.25, sign * 28.0, -g.BASE_D + 2.2,
                     sign * 32.5, -10, 5.0, g.JOIN, shell)
        half = s.GUIDE_WIDTH / 2 + s.SLIDE_CLEARANCE
        builder.slab(g.BASE_T - 0.1, x - half, -g.BASE_D + g.WALL + 0.5,
                     x + half, -6, s.GUIDE_HEIGHT + s.SLIDE_CLEARANCE + 0.1, g.CUT, shell)
        face_box(g, v4, builder.comp, front, sign * 30.7, -1.0, 1.6, 54.0,
                 4.0, 3.0, g.JOIN, "Front cover alignment tongue", front)
    face_box(g, v4, builder.comp, front, 0, 23.5, 60, 1.6,
             4.0, 3.0, g.JOIN, "Top cover alignment tongue", front)
    for part, positions, inner in ((front, s.FRONT_SCREWS, 11.5), (shell, s.SHELL_SCREWS, 22.0)):
        for x, z in positions:
            sign = math.copysign(1, x)
            x0, x1 = sorted((sign * (inner - 0.5), x + sign * 3.5))
            builder.slab(s.NUT_SEAT_HEIGHT, x0, z - s.NUT_SLOT_WIDTH / 2,
                         x1, z + s.NUT_SLOT_WIDTH / 2, s.NUT_SLOT_HEIGHT, g.CUT, part)
    plane = builder._offset_plane(builder.comp.xYConstructionPlane, -1.8)
    sketch = builder.comp.sketches.add(plane)
    g.rounded_rect(sketch, 22.0, 2.5, 26.0, 5.8, 0.4)
    feature = v4.extrude(g, builder.comp, g.smallest_profile(sketch), -3, g.CUT,
                         "Keyboard port decorative recess", base)
    v4.color_feature(feature, dark)
    return base


def front_details(g, previous, v4, comp, front, dark, colors):
    for x, y, w, h, d, r, name in (
            (10.0, 26.5, 26.0, 2.6, 0.55, 0.2, "Floppy drive bezel"),
            (21.6, 27.0, 3.4, 3.8, 0.6, 0.15, "Floppy drive stepped pocket"),
            (9.5, 26.55, 24.5, 0.7, 0.85, 0.1, "Floppy drive dark slot")):
        feature = previous.recess(g, comp, front, x, y, w, h, d, r, name)
        if "dark" in name:
            v4.color_feature(feature, dark)
    feature = previous.recess(g, comp, front, 21, 26, 2.0, 0.4, 0.75, 0.07, "Drive latch accent")
    v4.color_feature(feature, colors[3])
    previous.recess(g, comp, front, -23, 17.4, 3.2, 3.7, 0.12, 0.3, "Rainbow badge border")
    for i, color in enumerate(colors):
        feature = previous.recess(g, comp, front, -23, 18.72 - i * 0.53,
                                   2.6, 0.47, 0.17, 0.05, "Rainbow badge stripe")
        v4.color_feature(feature, color)


def display_mount(g, s, v4, comp, front):
    board_front = 4.0 + s.DISPLAY_STACK - s.DISPLAY_PCB_THICKNESS
    board_back = 4.0 + s.DISPLAY_STACK
    retainer_start = board_back + 0.2
    for sign in (-1, 1):
        for v in (-9.0, 9.0):
            face_box(g, v4, comp, front, sign * 23.4, v, 2.0, 7.0, 4.0,
                     board_front - 4.0, g.JOIN, "Provisional PCB edge support", front)
        for v in (-12.0, 12.0):
            u = sign * 28.0
            face_box(g, v4, comp, front, u, v, 7.0, 7.0, 4.0,
                     retainer_start - 4.0, g.JOIN, "Display retainer screw pedestal", front)
            face_box(g, v4, comp, front, sign * 27.3, v, 8.4, s.NUT_SLOT_WIDTH,
                     4.8, s.NUT_SLOT_HEIGHT, g.CUT, "Display nut insertion channel", front)
            face_hole(g, v4, comp, front, front, u, v, 4.0,
                      retainer_start - 4 + 0.1, s.SCREW_CLEARANCE, "Display screw clearance")
    frame = face_box(g, v4, comp, front, -23.4, 0, 2.0, 40,
                     retainer_start, 2.4, g.NEW, "Display retainer left rail")
    frame.name = "v5_display_retainer_provisional"
    for v in (-18.8, 18.8):
        face_box(g, v4, comp, front, 0, v, 48.8, 2.4, retainer_start, 2.4,
                 g.JOIN, "Display retainer crossbar", frame)
    face_box(g, v4, comp, front, 23.4, 0, 2.0, 40, retainer_start,
             2.4, g.JOIN, "Display retainer right rail", frame)
    for sign in (-1, 1):
        for v in (-12.0, 12.0):
            face_box(g, v4, comp, front, sign * 27, v, 9, 7, retainer_start,
                     2.4, g.JOIN, "Display retainer screw ear", frame)
            face_hole(g, v4, comp, front, frame, sign * 28, v, retainer_start - 0.1,
                      2.6, s.SCREW_CLEARANCE, "Display retainer through hole")
    pcb = face_box(g, v4, comp, front, *s.DISPLAY_BOARD_OFFSET, *s.DISPLAY_PCB,
                   board_front, s.DISPLAY_PCB_THICKNESS, g.NEW, "JD9853 measured PCB footprint")
    pcb.name = "reference_jd9853_pcb_depth_unmeasured"
    glass = face_box(g, v4, comp, front, 0, 0, *s.DISPLAY_WINDOW, 4.15,
                     board_front - 4.15, g.NEW, "JD9853 provisional screen stack")
    glass.name = "reference_jd9853_screen_depth_unmeasured"
    return frame, pcb, glass


def esp_mount(g, s, builder, base):
    rear, front = s.ESP_BOARD_REAR, s.ESP_BOARD_REAR + s.ESP_PCB[1]
    for x in (-7.4, 7.4):
        builder.slab(g.BASE_T - 0.1, x - 0.6, rear + 2.0, x + 0.6, front - 2.0,
                     s.ESP_BOARD_BOTTOM - g.BASE_T + 0.1, g.JOIN, base)
    clamp_y = s.ESP_BOARD_BOTTOM + s.ESP_PCB_THICKNESS + 0.2
    for x in (-12.0, 12.0):
        builder.slab(g.BASE_T - 0.1, x - 3.8, -52.3, x + 3.8, -44.7,
                     clamp_y - g.BASE_T + 0.1, g.JOIN, base)
        builder.cylinder(g.BASE_T - 4, x, -48.5, s.SCREW_CLEARANCE,
                         clamp_y - g.BASE_T + 4.1, g.CUT, base)
        sign = math.copysign(1, x)
        x0, x1 = sorted((sign * 8, sign * 18))
        builder.slab(11.0, x0, -48.5 - s.NUT_SLOT_WIDTH / 2,
                     x1, -48.5 + s.NUT_SLOT_WIDTH / 2, s.NUT_SLOT_HEIGHT, g.CUT, base)
    clamp = builder.slab(clamp_y, -8.2, rear + 2, -6.6, front + 3.0, 2.4, g.NEW)
    clamp.name = "v5_esp_retainer_provisional"
    builder.slab(clamp_y, -8.2, front + 1.0, 8.2, front + 3.0, 2.4, g.JOIN, clamp)
    builder.slab(clamp_y, 6.6, rear + 2, 8.2, front + 3.0, 2.4, g.JOIN, clamp)
    for x in (-12.0, 12.0):
        sign = math.copysign(1, x)
        x0, x1 = sorted((sign * 6.6, sign * 15.8))
        builder.slab(clamp_y, x0, -52.3, x1, -44.7, 2.4, g.JOIN, clamp)
        builder.cylinder(clamp_y - 0.1, x, -48.5, s.SCREW_CLEARANCE, 2.6, g.CUT, clamp)
    pcb = builder.slab(s.ESP_BOARD_BOTTOM, -s.ESP_PCB[0] / 2, rear,
                       s.ESP_PCB[0] / 2, front, s.ESP_PCB_THICKNESS, g.NEW)
    pcb.name = "reference_esp32c3_pcb_depth_unmeasured"
    return clamp, pcb


def collision_volume(mgr, first, second):
    body = mgr.copy(first)
    if not mgr.booleanOperation(body, mgr.copy(second), adsk.fusion.BooleanTypes.IntersectionBooleanType):
        raise RuntimeError("Temporary intersection failed")
    return body.volume * 1000 if body.isValid else 0.0


def validate_opening(g, shell, fixed):
    mgr = adsk.fusion.TemporaryBRepManager.get()
    distances = (0.0, 0.1, 0.25, 0.5, 0.75) + tuple(range(1, 81))
    for distance in distances:
        moved = mgr.copy(shell)
        matrix = adsk.core.Matrix3D.create()
        matrix.translation = adsk.core.Vector3D.create(0, 0, -g.cm(distance))
        mgr.transform(moved, matrix)
        for other in fixed:
            volume = collision_volume(mgr, moved, other)
            if volume > 0.001:
                raise RuntimeError("Cover collision at {} mm with {}: {} mm3".format(distance, other.name, volume))
    return {"direction": "rearward (-Z), unplug USB and remove four underside screws first",
            "sampled_travel_mm": list(distances), "collisions": [],
            "scope": "CAD solids only; excludes unmeasured wiring, headers and connectors"}


def capture(app, name, eye, target=(0, 4.2, -3.25)):
    view = app.activeViewport
    view.visualStyle = adsk.core.VisualStyles.ShadedVisualStyle
    camera = view.camera
    camera.isSmoothTransition = False
    camera.cameraType = adsk.core.CameraTypes.OrthographicCameraType
    camera.target = adsk.core.Point3D.create(*target)
    camera.eye = adsk.core.Point3D.create(*eye)
    camera.upVector = adsk.core.Vector3D.create(0, 1, 0)
    camera.isFitView = True
    view.camera = camera
    view.refresh()
    if not view.saveAsImageFile(str(OUTPUT / (name + ".png")), 1400, 1100):
        raise RuntimeError("Could not save view " + name)


def run(context):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    if (design is None or app.activeDocument.isSaved or design.timeline.count
            or design.rootComponent.bRepBodies.count or design.rootComponent.occurrences.count):
        raise RuntimeError("Open a new empty Fusion design before generating v5")
    s, v4 = load("stand_v5_spec"), load("stand_v4")
    s.validate()
    v3 = v4.load_revision()
    previous = v3.load_previous()
    g = previous.load_geometry()
    configure(g, s)
    design.designType = adsk.fusion.DesignTypes.ParametricDesignType
    app.activeDocument.name = NAME
    comp = design.rootComponent
    comp.attributes.add("TokEsp", "generator", "stand_v5")
    builder = g.Builder(comp)
    source = g.appearance_for(app, design, g.DISPLAY_APPEARANCE_ID)
    colors = {name: previous.color(design, source, name, rgb) for name, rgb in (
        ("Ivory front", (223, 216, 185)), ("Beige cover", (194, 189, 166)),
        ("Warm grey base", (148, 146, 128)), ("Graphite", (24, 27, 25)),
        ("Provisional PCB", (30, 107, 77)), ("Mounts", (91, 103, 110)))}
    rainbow = [previous.color(design, source, "Badge {}".format(i), rgb) for i, rgb in enumerate(
        ((69, 131, 62), (232, 195, 61), (224, 136, 47), (193, 70, 54), (137, 76, 135), (60, 118, 166)))]
    front = build_front(g, v3, v4, comp)
    shell = build_shell(g, v3, v4, builder)
    front_details(g, previous, v4, comp, front, colors["Graphite"], rainbow)
    base = build_base_and_joints(g, s, v3, v4, builder, front, shell, colors["Graphite"])
    display_retainer, display_pcb, glass = display_mount(g, s, v4, comp, front)
    esp_retainer, esp_pcb = esp_mount(g, s, builder, base)
    fasteners = bottom_fasteners(g, s, builder)
    metal = previous.color(design, source, "Fastener reference steel", (130, 137, 145))
    for fastener in fasteners:
        fastener.appearance = metal
    front.appearance, shell.appearance = colors["Ivory front"], colors["Beige cover"]
    base.appearance = colors["Warm grey base"]
    display_retainer.appearance = esp_retainer.appearance = colors["Mounts"]
    display_pcb.appearance = esp_pcb.appearance = colors["Provisional PCB"]
    glass.appearance = colors["Graphite"]
    parts = (front, shell, base, display_retainer, esp_retainer)
    references = (display_pcb, glass, esp_pcb) + fasteners
    solids = previous.validate(design, parts + references)
    v4.check_contacts(design, parts + references)
    opening = validate_opening(g, shell, (front, base, display_retainer, esp_retainer,
                                          display_pcb, glass, esp_pcb)
                               + tuple(body for body in fasteners if "_front_" in body.name))
    comp.isSketchFolderLightBulbOn = False
    comp.isConstructionFolderLightBulbOn = False
    OUTPUT.mkdir(exist_ok=True)
    capture(app, "rear", (-12, 9, -22))
    capture(app, "underside", (-10, -15, -18))
    for body in parts + references:
        body.isLightBulbOn = body == shell
    capture(app, "cover_inside", (-12, -8, 18))
    for body in parts + references:
        body.isLightBulbOn = True
    shell.isLightBulbOn = False
    for body in fasteners:
        if "_cover_" in body.name:
            body.isLightBulbOn = False
    capture(app, "open", (-13, 12, -21))
    shell.isLightBulbOn = True
    for body in fasteners:
        body.isLightBulbOn = True
    capture(app, "front", (0, 5.5, 20))
    capture(app, "preview", (-13, 9, 21))
    archive = OUTPUT / (NAME + ".f3d")
    manager = design.exportManager
    if not manager.execute(manager.createFusionArchiveExportOptions(str(archive))):
        raise RuntimeError("Failed to export v5 native archive")
    report = {"design": NAME, "status": "Serviceable assembly prototype; electronics fit unverified",
              "timeline_features": design.timeline.count, "timeline_issues": [],
              "parts": solids, "unexpected_interferences": [], "opening_test": opening,
              "display_window_mm": s.DISPLAY_WINDOW, "display_pcb_measured_mm": s.DISPLAY_PCB,
              "esp_pcb_measured_mm": s.ESP_PCB, "shell_screw_count": len(s.SHELL_SCREWS),
              "front_screw_count": len(s.FRONT_SCREWS), "bottom_screw_length_mm": s.BOTTOM_SCREW_LENGTH,
              "slide_clearance_per_side_mm": s.SLIDE_CLEARANCE,
              "usb_opening_mm": s.USB_OPENING, "usb_center_height_mm": s.USB_CENTER_HEIGHT,
              "provisional": {"display_stack_mm": s.DISPLAY_STACK,
                              "display_pcb_offset_mm": s.DISPLAY_BOARD_OFFSET,
                              "pcb_thickness_mm": s.DISPLAY_PCB_THICKNESS,
                              "esp_board_bottom_mm": s.ESP_BOARD_BOTTOM,
                              "esp_usb_height_above_pcb_mm": s.ESP_USB_ABOVE_PCB},
              "missing_measurements": s.MISSING_MEASUREMENTS,
              "print_meshes_exported": False, "physical_fit_verified": False}
    (OUTPUT / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    run(None)
