"""Build the layout A enclosure (v6) in a new empty Fusion document.

The outer shell, joints and fasteners are reused from stand_v5.py. The
interior is new: the display is held by four posts on the front plate, the
battery module sits at the rear with its USB-C toward the rear opening, and
the ESP32-C3 Mini lies in front of it. Every estimated or assumed value lives
in stand_v6_spec.py and is repeated in validation.json.
"""

import importlib.util
import json
import math
import time
from pathlib import Path

import adsk.core
import adsk.fusion

ROOT = Path(__file__).resolve().parent
OUTPUT = ROOT / "v6"
NAME = "tokometer_stand_v6_assembly"
# A runner may pre-define OPTIONS (runpy init_globals) to skip slow stages.
OPTIONS = globals().get("OPTIONS", {"opening": True, "views": True, "export": True})
LOG_FILE = OPTIONS.get("log")


def log(message):
    if LOG_FILE:
        with open(LOG_FILE, "a") as handle:
            handle.write(time.strftime("%H:%M:%S ") + message + "\n")


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
    g.OPENING, g.FUNNEL_OUTER = s.DISPLAY_WINDOW, s.FUNNEL_OUTER
    g.OPENING_R, g.FUNNEL_DEPTH = 0.4, 2.5
    g.BOARD_PCB_T, g.USB_ABOVE_PCB = 0.0, 0.0
    g.HEADER_T = s.USB_CENTER_HEIGHT - s.BASE_HEIGHT
    g.USB_OVERMOULD = s.USB_OPENING


def face_round(g, v4, comp, panel, target, uv, start, depth, diameter, op, name):
    """Circle on a plane `start` mm behind the front face; depth > 0 goes inward."""
    sketch = comp.sketches.add(g.face_plane(comp, panel, start))
    sketch.name = name + " profile"
    center = sketch.modelToSketchSpace(g.face_uv_point(uv[0], uv[1], -start))
    sketch.sketchCurves.sketchCircles.addByCenterRadius(center, g.cm(diameter / 2))
    v4.extrude(g, comp, sketch.profiles.item(0), -depth, op, name, target)


def post_positions(s):
    half_x, half_y = s.DISPLAY_HOLE_SPACING[0] / 2, s.DISPLAY_HOLE_SPACING[1] / 2
    ox, oy = s.DISPLAY_BOARD_OFFSET
    return [(ox + sx * half_x, oy + sy * half_y) for sx in (-1, 1) for sy in (-1, 1)]


def display_posts(g, s, v4, comp, front):
    board_front = s.PLATE_DEPTH + s.DISPLAY_STACK - s.DISPLAY_PCB_THICKNESS
    for uv in post_positions(s):
        face_round(g, v4, comp, front, front, uv, s.PLATE_DEPTH,
                   board_front - s.PLATE_DEPTH, s.POST_DIAMETER, g.JOIN, "Display post")
        face_round(g, v4, comp, front, front, uv, board_front, -s.POST_PILOT_DEPTH,
                   s.POST_PILOT_DIAMETER, g.CUT, "Display post pilot hole")
    return board_front


def face_box(g, v4, comp, panel, size, start, depth, name):
    plane = g.face_plane(comp, panel, start)
    profile = v4.face_profile(g, comp, plane, g.face_uv_point(0, 0), size,
                              0.15, -start, name + " profile")
    return v4.extrude(g, comp, profile, -depth, g.NEW, name).bodies.item(0)


def display_references(g, s, v4, comp, front, board_front):
    pcb = face_box(g, v4, comp, front, s.DISPLAY_PCB, board_front,
                   s.DISPLAY_PCB_THICKNESS, "JD9853 measured PCB footprint")
    pcb.name = "reference_jd9853_pcb_depth_unmeasured"
    glass = face_box(g, v4, comp, front, s.DISPLAY_WINDOW, 4.15,
                     board_front - 4.15, "JD9853 provisional screen stack")
    glass.name = "reference_jd9853_screen_depth_unmeasured"
    for uv in post_positions(s):
        face_round(g, v4, comp, front, pcb, uv, board_front, s.DISPLAY_PCB_THICKNESS,
                   s.DISPLAY_HOLE_DIAMETER, g.CUT, "JD9853 PCB mounting hole")
    return pcb, glass


def battery_cradle(g, s, builder, base):
    """Two ribs lift the battery above the cover tabs; a front stop locates it."""
    rib_height = s.BATTERY_BOTTOM - s.BASE_HEIGHT + 0.1
    for sign in (-1, 1):
        x = sign * s.BATTERY_RIB_X
        builder.slab(g.BASE_T - 0.1, x - s.BATTERY_RIB_WIDTH / 2, s.BATTERY_REAR_Z + 1.0,
                     x + s.BATTERY_RIB_WIDTH / 2, s.battery_front_z() - 1.0,
                     rib_height, g.JOIN, base)
    half = s.BATTERY_FRONT_STOP_HALF_WIDTH
    builder.slab(g.BASE_T - 0.1, -half, s.BATTERY_FRONT_STOP[0], half,
                 s.BATTERY_FRONT_STOP[1], s.BATTERY_FRONT_STOP_TOP - g.BASE_T + 0.1,
                 g.JOIN, base)


def battery_reference(g, s, builder):
    half = s.BATTERY[0] / 2
    body = builder.slab(s.BATTERY_BOTTOM, -half, s.BATTERY_REAR_Z, half,
                        s.battery_front_z(), s.BATTERY[2], g.NEW)
    body.name = "reference_battery_envelope"
    return body


def usb_board_mount(g, s, builder, base):
    """Three bosses with locating pins that pass through the board's holes."""
    for x, z in s.usb_hole_positions():
        builder.cylinder(g.BASE_T - 0.1, x, z, s.USB_BOSS_DIAMETER,
                         s.USB_BOARD_BOTTOM - g.BASE_T + 0.1, g.JOIN, base)
        builder.cylinder(s.USB_BOARD_BOTTOM - 0.05, x, z, s.USB_PIN_DIAMETER,
                         s.USB_BOARD[2] + 0.05, g.JOIN, base)


def usb_board_reference(g, s, builder):
    half = s.USB_BOARD[0] / 2
    board = builder.slab(s.USB_BOARD_BOTTOM, -half, s.USB_BOARD_REAR_Z, half,
                         s.usb_board_front_z(), s.USB_BOARD[2], g.NEW)
    board.name = "reference_usb_c_board"
    for x, z in s.usb_hole_positions():
        builder.cylinder(s.USB_BOARD_BOTTOM - 0.1, x, z, s.USB_HOLE_DIAMETER,
                         s.USB_BOARD[2] + 0.2, g.CUT, board)
    width, height = s.USB_C_BODY
    start = s.USB_BOARD_REAR_Z - s.USB_C_PROTRUSION
    builder.slab(s.USB_BOARD_BOTTOM + s.USB_BOARD[2] - 0.01, s.USB_C_X - width / 2, start,
                 s.USB_C_X + width / 2, start + s.USB_C_LENGTH, height + 0.01, g.JOIN, board)
    return board


def esp_cradle(g, s, builder, base):
    """Low rim around the ESP footprint; nothing of the ESP leaves the case."""
    inner = s.ESP_PCB[0] / 2 + s.ESP_RIM_CLEARANCE
    outer = s.esp_rim_half_width()
    rear = s.ESP_REAR_Z - s.ESP_RIM_CLEARANCE
    front = s.esp_front_z() + s.ESP_RIM_CLEARANCE
    builder.slab(g.BASE_T - 0.1, -outer, rear - s.ESP_RIM_WALL, outer,
                 front + s.ESP_RIM_WALL, s.ESP_RIM_HEIGHT + 0.1, g.JOIN, base)
    builder.slab(g.BASE_T, -inner, rear, inner, front, s.ESP_RIM_HEIGHT + 0.2, g.CUT, base)


def esp_reference(g, s, builder):
    half = s.ESP_PCB[0] / 2
    body = builder.slab(s.ESP_BOTTOM, -half, s.ESP_REAR_Z, half, s.esp_front_z(),
                        s.ESP_TOTAL_HEIGHT, g.NEW)
    body.name = "reference_esp32c3_reserved_envelope"
    return body


def make_colors(app, design, previous, g):
    source = g.appearance_for(app, design, g.DISPLAY_APPEARANCE_ID)
    colors = {name: previous.color(design, source, name, rgb) for name, rgb in (
        ("Ivory front", (223, 216, 185)), ("Beige cover", (194, 189, 166)),
        ("Warm grey base", (148, 146, 128)), ("Graphite", (24, 27, 25)),
        ("Provisional PCB", (30, 107, 77)), ("Mounts", (91, 103, 110)),
        ("Battery board", (60, 130, 90)), ("Fastener reference steel", (130, 137, 145)))}
    rainbow = [previous.color(design, source, "Badge {}".format(i), rgb)
               for i, rgb in enumerate(((69, 131, 62), (232, 195, 61), (224, 136, 47),
                                        (193, 70, 54), (137, 76, 135), (60, 118, 166)))]
    return colors, rainbow


def build_all(app, design, g, s, v5, v4, v3, previous):
    comp = design.rootComponent
    builder = g.Builder(comp)
    colors, rainbow = make_colors(app, design, previous, g)
    front = v5.build_front(g, v3, v4, comp)
    shell = v5.build_shell(g, v3, v4, builder)
    v5.front_details(g, previous, v4, comp, front, colors["Graphite"], rainbow)
    base = v5.build_base_and_joints(g, s, v3, v4, builder, front, shell, colors["Graphite"])
    board_front = display_posts(g, s, v4, comp, front)
    display_pcb, glass = display_references(g, s, v4, comp, front, board_front)
    battery_cradle(g, s, builder, base)
    usb_board_mount(g, s, builder, base)
    esp_cradle(g, s, builder, base)
    battery = battery_reference(g, s, builder)
    usb_board = usb_board_reference(g, s, builder)
    esp = esp_reference(g, s, builder)
    fasteners = v5.bottom_fasteners(g, s, builder)
    front.name, shell.name, base.name = "v6_front_frame", "v6_rear_cover", "v6_service_base"
    paint(colors, front, shell, base, (display_pcb, usb_board, esp), glass, battery, fasteners)
    parts = (front, shell, base)
    references = (display_pcb, glass, battery, usb_board, esp) + fasteners
    return parts, references, fasteners


def paint(colors, front, shell, base, pcbs, glass, battery, fasteners):
    front.appearance, shell.appearance = colors["Ivory front"], colors["Beige cover"]
    base.appearance = colors["Warm grey base"]
    for pcb in pcbs:
        pcb.appearance = colors["Provisional PCB"]
    glass.appearance = colors["Graphite"]
    battery.appearance = colors["Battery board"]
    for fastener in fasteners:
        fastener.appearance = colors["Fastener reference steel"]


def set_visible(bodies, visible):
    for body in bodies:
        body.isLightBulbOn = visible


def capture_views(app, v5, parts, references, fasteners):
    shell = parts[1]
    everything = parts + references
    v5.capture(app, "rear", (-12, 9, -22))
    v5.capture(app, "underside", (-10, -15, -18))
    set_visible(everything, False)
    shell.isLightBulbOn = True
    v5.capture(app, "cover_inside", (-12, -8, 18))
    set_visible(everything, True)
    shell.isLightBulbOn = False
    set_visible([f for f in fasteners if "_cover_" in f.name], False)
    v5.capture(app, "open", (-13, 12, -21))
    v5.capture(app, "layout_top", (0, 110, -20), (0, 9, -33))
    set_visible(everything, True)
    v5.capture(app, "front", (0, 5.5, 20))
    v5.capture(app, "preview", (-13, 9, 21))


def layout_report(s):
    return {
        "display_opening_mm": list(s.DISPLAY_WINDOW),
        "display_opening_margins_to_pcb_mm": list(s.DISPLAY_WINDOW_MARGINS),
        "display_pcb_front_depth_mm": s.DISPLAY_PCB_FRONT_DEPTH,
        "display_posts_uv_mm": [list(uv) for uv in post_positions(s)],
        "battery_x_range_mm": [-s.BATTERY[0] / 2, s.BATTERY[0] / 2],
        "battery_z_range_mm": [s.BATTERY_REAR_Z, s.battery_front_z()],
        "battery_y_range_mm": [s.BATTERY_BOTTOM, s.battery_top()],
        "usb_board_z_range_mm": [s.USB_BOARD_REAR_Z, s.usb_board_front_z()],
        "usb_board_y_range_mm": [s.USB_BOARD_BOTTOM, s.USB_BOARD_BOTTOM + s.USB_BOARD[2]],
        "usb_board_holes_xz_mm": [list(p) for p in s.usb_hole_positions()],
        "usb_c_center_height_mm": s.USB_CENTER_HEIGHT,
        "usb_opening_mm": list(s.USB_OPENING),
        "usb_opening_y_range_mm": [s.USB_CENTER_HEIGHT - s.USB_OPENING[1] / 2,
                                   s.USB_CENTER_HEIGHT + s.USB_OPENING[1] / 2],
        "esp_z_range_mm": [s.ESP_REAR_Z, s.esp_front_z()],
        "esp_reserved_y_range_mm": [s.ESP_BOTTOM, s.ESP_BOTTOM + s.ESP_TOTAL_HEIGHT],
        "clearances_mm": {
            "usb_c_tip_recess_from_outer_face":
                s.usb_tip_z() - s.rear_outer_z(s.USB_CENTER_HEIGHT),
            "usb_board_rear_edge_to_inner_wall":
                s.USB_BOARD_REAR_Z - s.rear_wall_inner_z(s.USB_BOARD_BOTTOM + s.USB_BOARD[2]),
            "usb_c_in_opening_width_per_side": (s.USB_OPENING[0] - s.USB_C_BODY[0]) / 2,
            "usb_c_in_opening_height_per_side": (s.USB_OPENING[1] - s.USB_C_BODY[1]) / 2,
            "battery_bottom_over_cover_tabs": s.BATTERY_BOTTOM - s.COVER_TAB_TOP,
            "battery_wing_overhang_past_tab_inner_edge_per_side":
                s.BATTERY[0] / 2 - s.COVER_TAB_INNER_X,
            "usb_connector_top_to_battery_bottom": s.BATTERY_BOTTOM - s.usb_connector_top(),
            "usb_opening_top_to_battery_bottom":
                s.BATTERY_BOTTOM - s.USB_CENTER_HEIGHT - s.USB_OPENING[1] / 2,
            "battery_to_front_stop": s.BATTERY_FRONT_STOP[0] - s.battery_front_z(),
            "esp_rim_rear_to_battery_front_stop":
                s.ESP_REAR_Z - s.ESP_RIM_CLEARANCE - s.ESP_RIM_WALL - s.BATTERY_FRONT_STOP[1],
            "esp_rim_to_front_screw_tabs_per_side": s.FRONT_SLAB_INNER_X - s.esp_rim_half_width(),
            "opening_vs_plug_cover_per_side": (s.USB_OPENING[0] - s.USB_PLUG_COVER[0]) / 2,
            "opening_vs_plug_cover_height_per_side": (s.USB_OPENING[1] - s.USB_PLUG_COVER[1]) / 2,
        },
        "confirmed_by_user": {"usb_board_third_hole_edge": "edge opposite the connector",
                              "usb_board_hole_pair_from_connector_edge_mm": s.USB_HOLE_ROW_FROM_REAR},
        "measured_by_user": {
            "display_pcb_mm": list(s.DISPLAY_PCB), "display_stack_mm": s.DISPLAY_STACK,
            "display_hole_diameter_mm": s.DISPLAY_HOLE_DIAMETER,
            "display_hole_edge_to_edge_mm": list(s.DISPLAY_HOLE_EDGE_TO_EDGE),
            "display_hole_centres_mm": list(s.DISPLAY_HOLE_SPACING),
            "esp_pcb_mm": list(s.ESP_PCB), "esp_total_height_mm": s.ESP_TOTAL_HEIGHT,
            "battery_envelope_mm": list(s.BATTERY), "usb_board_mm": list(s.USB_BOARD),
            "usb_board_hole_diameter_mm": s.USB_HOLE_DIAMETER,
            "usb_board_holes_centres_mm": s.USB_HOLES_CENTRES},
        "standard_not_measured": {"usb_plug_metal_mm": list(s.USB_PLUG_METAL),
                                  "usb_plug_cover_mm": list(s.USB_PLUG_COVER),
                                  "opening_clearance_per_side_mm": s.USB_OPENING_CLEARANCE},
        "assumptions": {"display_screw": "M1.6 self-tapping, head {} mm".format(s.SCREW_HEAD_DIAMETER),
                        "post_diameter_mm": s.POST_DIAMETER,
                        "post_pilot_diameter_mm": s.POST_PILOT_DIAMETER,
                        "post_pilot_depth_mm": s.POST_PILOT_DEPTH,
                        "display_stack_taken_as_glass_front_to_pcb_back": True,
                        "battery_raised_over_cover_tabs_by_mm": s.BATTERY_CLEAR_OVER_TABS,
                        "usb_c_body_mm": list(s.USB_C_BODY), "usb_c_length_mm": s.USB_C_LENGTH,
                        "usb_c_protrusion_mm": s.USB_C_PROTRUSION,
                        "usb_board_pins_boss_diameters_mm": [s.USB_PIN_DIAMETER, s.USB_BOSS_DIAMETER],
                        "esp_rests_on_base_with_rim_mm": [s.ESP_RIM_CLEARANCE, s.ESP_RIM_WALL,
                                                          s.ESP_RIM_HEIGHT]},
    }


def write_report(design, s, solids, opening):
    report = {"design": NAME, "status": "Layout A prototype; electronics fit unverified",
              "timeline_features": design.timeline.count, "timeline_issues": [],
              "parts": solids, "unexpected_interferences": [], "opening_test": opening,
              "display_pcb_measured_mm": list(s.DISPLAY_PCB),
              "esp_pcb_measured_mm": list(s.ESP_PCB),
              "battery_envelope_measured_mm": list(s.BATTERY),
              "usb_board_measured_mm": list(s.USB_BOARD),
              "shell_screw_count": len(s.SHELL_SCREWS), "front_screw_count": len(s.FRONT_SCREWS),
              "layout": layout_report(s), "missing_measurements": list(s.MISSING_MEASUREMENTS),
              "print_meshes_exported": False, "physical_fit_verified": False}
    (OUTPUT / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def collect_parts(design):
    bodies = {body.name: body for body in design.rootComponent.bRepBodies}
    parts = tuple(bodies[n] for n in ("v6_front_frame", "v6_rear_cover", "v6_service_base"))
    named = [n for n in bodies if n.startswith("reference_")]
    references = tuple(bodies[n] for n in sorted(named, key=reference_order))
    fasteners = tuple(body for name, body in bodies.items() if "_screw_" in name or "_nut_" in name)
    return parts, references, fasteners


def reference_order(name):
    return (1 if "_screw_" in name or "_nut_" in name else 0, name)


def finalize(app, design, g, s, v5, v4, previous, parts, references, fasteners):
    solids = previous.validate(design, parts + references)
    v4.check_contacts(design, parts + references)
    log("contacts ok")
    opening = check_opening(g, v5, parts, references) if OPTIONS.get("opening") else {}
    log("opening done")
    design.rootComponent.isSketchFolderLightBulbOn = False
    design.rootComponent.isConstructionFolderLightBulbOn = False
    OUTPUT.mkdir(exist_ok=True)
    if OPTIONS.get("views"):
        capture_views(app, v5, parts, references, fasteners)
        log("views done")
    if OPTIONS.get("export"):
        export_native(design)
        log("export done")
    report = write_report(design, s, solids, opening)
    log("report written")
    print(json.dumps(report))


def run(context):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    finalize_only = OPTIONS.get("finalize_only")
    if design is None or (not finalize_only and (
            app.activeDocument.isSaved or design.timeline.count
            or design.rootComponent.bRepBodies.count or design.rootComponent.occurrences.count)):
        raise RuntimeError("Open a new empty Fusion design before generating v6")
    s, v5, v4 = load("stand_v6_spec"), load("stand_v5"), load("stand_v4")
    s.validate()
    v5.OUTPUT = OUTPUT
    v3 = v4.load_revision()
    previous = v3.load_previous()
    g = previous.load_geometry()
    configure(g, s)
    if finalize_only:
        parts, references, fasteners = collect_parts(design)
    else:
        design.designType = adsk.fusion.DesignTypes.ParametricDesignType
        app.activeDocument.name = NAME
        design.rootComponent.attributes.add("TokEsp", "generator", "stand_v6")
        parts, references, fasteners = build_all(app, design, g, s, v5, v4, v3, previous)
        log("built {} timeline items".format(design.timeline.count))
    finalize(app, design, g, s, v5, v4, previous, parts, references, fasteners)


def check_opening(g, v5, parts, references):
    front, shell, base = parts
    fixed = (front, base) + tuple(
        r for r in references if "_cover_" not in r.name)
    return v5.validate_opening(g, shell, fixed)


def export_native(design):
    manager = design.exportManager
    path = str(OUTPUT / (NAME + ".f3d"))
    if not manager.execute(manager.createFusionArchiveExportOptions(path)):
        raise RuntimeError("Failed to export v6 native archive")


if __name__ == "__main__":
    run(None)
