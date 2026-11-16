"""Build the v7 enclosure in a new empty Fusion document.

v7 redoes the service base and, on the rear cover, only the size of the USB
opening (`configure_rear_opening`: centre frozen at v6, size from the spec).
The front frame and the rest of the cover are built by the same v5/v6
functions as in v6, from the same values, so they stay identical, except for
the notch in the front screw tabs for the ESP PCB (`notch_front_tabs`). The base
keeps only the v5 shell joints: the battery cradle, the USB-C board mount and
stop and the ESP cradle are not built (stand_v7_spec.BASE_SUPPORTS_REMOVED).

The battery and USB-C board reference bodies are not created (user's request,
stand_v7_spec.OMITTED_REFERENCES); what the CAD checks no longer cover is in
stand_v7_spec.UNVERIFIED_WITHOUT_REFERENCES and in validation.json.
"""

import importlib.util
import json
import time
from pathlib import Path

import adsk.core
import adsk.fusion

ROOT = Path(__file__).resolve().parent
OUTPUT = ROOT / "v7"
NAME = "tokometer_stand_v7_assembly"
PART_NAMES = ("v7_front_frame", "v7_rear_cover", "v7_service_base")
# The ESP32 devkit (upside down) is represented by its PCB only (user's request):
# its components and USB-C are not modelled (stand_v7_spec.UNVERIFIED_WITHOUT_REFERENCES).
ESP_PCB_NAME = "reference_esp32_pcb"
ESP_NAMES = (ESP_PCB_NAME,)
# Parts the ESP may overlap without stopping the build. Empty since the front
# tabs were notched (FRONT_TAB_NOTCH): any overlap with the ESP now stops it.
KNOWN_ESP_PARTNERS = ()
# How far a cutter runs past a face into empty space, so no zero-thickness skin remains.
OVERSHOOT = 0.5
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


def configure_rear_opening(g, s):
    """Point the inherited cutter (stand.cut_usb_port) at the v7 opening.

    The cutter is centred at x = 0 and at height g.usb_center_y(), which is
    BASE_T + HEADER_T once v6.configure has zeroed the PCB terms.
    """
    g.USB_OVERMOULD = s.USB_OPENING
    g.USB_OVERMOULD_R = s.USB_OPENING_CORNER_RADIUS
    g.HEADER_T = s.USB_OPENING_CENTER[1] - s.BASE_HEIGHT
    if abs(g.usb_center_y() - s.USB_OPENING_CENTER[1]) > 1e-9 or s.USB_OPENING_CENTER[0]:
        raise RuntimeError("Rear USB opening is not at its v6 centre")


def rear_opening_report(s):
    left, right, bottom, top = s.usb_opening_bounds()
    return {"center_xy_mm": list(s.USB_OPENING_CENTER), "size_mm": list(s.USB_OPENING),
            "v6_size_mm": list(s.USB_OPENING_V6), "x_range_mm": [left, right],
            "y_range_mm": [bottom, top], "corner_radius_mm": s.USB_OPENING_CORNER_RADIUS}


def esp_front_pad(g, s, builder, base):
    """Plain pad under the ESP PCB's front edge (no screw), topped at its underside."""
    x0, x1, z0, z1 = s.esp_front_pad()
    builder.slab(g.BASE_T - 0.1, x0, z0, x1, z1, s.ESP_PCB_BOTTOM - g.BASE_T + 0.1,
                 g.JOIN, base)


def notch_front_tabs(g, s, builder, front):
    """Notch both front screw tabs where the ESP PCB passes (user's option A).

    The cutter overshoots the tab's inner, rear and top faces by OVERSHOOT, where
    the front frame has no material, so no zero-thickness skin is left."""
    x_in, x_out, z_rear, z_front, y_bottom, y_top = s.front_tab_notch()
    for sign in (-1, 1):
        x0, x1 = sorted((sign * (x_in - OVERSHOOT), sign * x_out))
        builder.slab(y_bottom, x0, z_rear - OVERSHOOT, x1, z_front,
                     y_top - y_bottom + OVERSHOOT, g.CUT, front)


def esp_reference(g, s, builder):
    """ESP32 devkit PCB, lying flat, with its four corner holes; its height comes
    from the (unmodelled) USB-C under it being centred on the rear opening."""
    half, rear, front = s.ESP_PCB[0] / 2, s.esp_rear_z(), s.esp_front_z()
    pcb = builder.slab(s.ESP_PCB_BOTTOM, s.ESP_X - half, rear, s.ESP_X + half, front,
                       s.ESP_PCB_THICKNESS, g.NEW)
    for x, z in s.esp_hole_positions():
        builder.cylinder(s.ESP_PCB_BOTTOM - 0.1, x, z, s.ESP_HOLE_DIAMETER,
                         s.ESP_PCB_THICKNESS + 0.2, g.CUT, pcb)
    pcb.name = ESP_PCB_NAME
    return pcb


def esp_standoffs(g, s, builder, base):
    """Rear standoffs under the PCB holes, topped at the PCB underside, each with a
    pilot hole for an M2.5 self-tapping screw."""
    gap = s.esp_standoff_free_gap_x()
    if gap < s.ESP_STANDOFF_MIN_FREE_GAP_X:
        raise RuntimeError("ESP standoffs only {:.2f} mm apart in x; the components "
                           "under the PCB need {:.1f}".format(gap, s.ESP_STANDOFF_MIN_FREE_GAP_X))
    height = s.ESP_PCB_BOTTOM - g.BASE_T
    for x, z in s.esp_standoff_positions():
        builder.cylinder(g.BASE_T - 0.1, x, z, s.ESP_STANDOFF_DIAMETER, height + 0.1,
                         g.JOIN, base)
        builder.cylinder(s.ESP_PCB_BOTTOM - s.ESP_STANDOFF_PILOT_DEPTH, x, z,
                         s.ESP_STANDOFF_PILOT_DIAMETER, s.ESP_STANDOFF_PILOT_DEPTH + 0.1,
                         g.CUT, base)


def is_known_overlap(first, second):
    return ((first in ESP_NAMES and second in KNOWN_ESP_PARTNERS)
            or (second in ESP_NAMES and first in KNOWN_ESP_PARTNERS))


def overlap_totals(found):
    """Known overlap volume per part, summed over the ESP bodies (mm3)."""
    totals = {}
    for item in found:
        partner = next(name for name in item["bodies"] if name not in ESP_NAMES)
        totals[partner] = round(totals.get(partner, 0.0) + item["volume_mm3"], 4)
    return totals



def check_contacts(design, parts):
    """Interference check that tolerates only KNOWN_OVERLAP; returns what it found."""
    entities = adsk.core.ObjectCollection.create()
    for part in parts:
        entities.add(part)
    inputs = design.createInterferenceInput(entities)
    inputs.areCoincidentFacesIncluded = False
    contacts = design.analyzeInterference(inputs)
    if contacts is None:
        raise RuntimeError("Interference analysis returned no result")
    found = [(c.entityOne.name, c.entityTwo.name, c.interferenceBody.volume * 1000)
             for c in contacts]
    unexpected = [item for item in found if not is_known_overlap(item[0], item[1])]
    if unexpected:
        raise RuntimeError("Unexpected solid overlaps: " + str(unexpected))
    return [{"bodies": [a, b], "volume_mm3": round(v, 4)} for a, b, v in found]


def paint(colors, parts, pcbs, glass, fasteners):
    """v6 colours, without the omitted battery and USB-C board references."""
    front, shell, base = parts
    front.appearance, shell.appearance = colors["Ivory front"], colors["Beige cover"]
    base.appearance = colors["Warm grey base"]
    for pcb in pcbs:
        pcb.appearance = colors["Provisional PCB"]
    glass.appearance = colors["Graphite"]
    for fastener in fasteners:
        fastener.appearance = colors["Fastener reference steel"]


def build_all(app, design, g, s, mods):
    v6, v5, v4, v3, previous = mods
    comp = design.rootComponent
    builder = g.Builder(comp)
    colors, rainbow = v6.make_colors(app, design, previous, g)
    front = v5.build_front(g, v3, v4, comp)
    shell = v5.build_shell(g, v3, v4, builder)
    v5.front_details(g, previous, v4, comp, front, colors["Graphite"], rainbow)
    base = v5.build_base_and_joints(g, s, v3, v4, builder, front, shell, colors["Graphite"])
    notch_front_tabs(g, s, builder, front)
    board_front = v6.display_posts(g, s, v4, comp, front)
    display_pcb, glass = v6.display_references(g, s, v4, comp, front, board_front)
    # no old electronics supports (stand_v7_spec.BASE_SUPPORTS_REMOVED); only the
    # ESP's rear standoffs
    esp_standoffs(g, s, builder, base)
    esp_front_pad(g, s, builder, base)
    esp = esp_reference(g, s, builder)
    fasteners = v5.bottom_fasteners(g, s, builder)
    front.name, shell.name, base.name = PART_NAMES
    paint(colors, (front, shell, base), (display_pcb, esp), glass, fasteners)
    references = (display_pcb, glass, esp) + fasteners
    return (front, shell, base), references, fasteners


def collect_parts(design, v6):
    bodies = {body.name: body for body in design.rootComponent.bRepBodies}
    parts = tuple(bodies[n] for n in PART_NAMES)
    named = [n for n in bodies if n.startswith("reference_")]
    references = tuple(bodies[n] for n in sorted(named, key=v6.reference_order))
    fasteners = tuple(body for name, body in bodies.items() if "_screw_" in name or "_nut_" in name)
    return parts, references, fasteners


def esp_report(s):
    return {"pcb_mm": [s.ESP_PCB[0], s.ESP_PCB[1], s.ESP_PCB_THICKNESS],
            "lying_flat": True, "upside_down": True, "usb_edge": s.ESP_USB_EDGE,
            "components_face": s.ESP_COMPONENTS_FACE,
            "component_height_mm": s.ESP_COMPONENT_HEIGHT,
            "usb_c_body_mm": list(s.ESP_USB_C_BODY),
            "usb_c_protrusion_mm": s.ESP_USB_C_PROTRUSION,
            "usb_c_centre_xy_mm": [s.ESP_X, (s.ESP_USB_C_BOTTOM + s.ESP_PCB_BOTTOM) / 2],
            "x_range_mm": [s.ESP_X - s.ESP_PCB[0] / 2, s.ESP_X + s.ESP_PCB[0] / 2],
            "pcb_y_range_mm": [s.ESP_PCB_BOTTOM, s.ESP_PCB_TOP],
            "components_y_range_mm": [s.ESP_COMPONENT_BOTTOM, s.ESP_PCB_BOTTOM],
            "z_range_mm": [s.esp_rear_z(), s.esp_front_z()],
            "usb_c_z_range_mm": list(s.esp_usb_c_z()), "usb_c_length_mm": s.ESP_USB_C_LENGTH,
            "bodies": list(ESP_NAMES), "modelled": "PCB only",
            "not_modelled": ["components under the PCB", "USB-C"], "cradle_on_base": False,
            "clearances_mm": {
                "components_to_base_floor": s.ESP_FLOOR_GAP,
                "pcb_underside_to_base_floor": s.ESP_PCB_BOTTOM - s.BASE_HEIGHT,
                "rear_edge_to_inner_wall_at_pcb_top": s.ESP_WALL_GAP,
                "usb_c_tip_recess_from_outer_face":
                    s.esp_usb_tip_z() - s.rear_outer_z(s.USB_OPENING_CENTER[1]),
                "usb_c_in_opening_width_per_side":
                    (s.USB_OPENING[0] - s.ESP_USB_C_BODY[0]) / 2,
                "usb_c_in_opening_height_per_side":
                    (s.USB_OPENING[1] - s.ESP_USB_C_BODY[1]) / 2}}


# v6 ESP names that v6.layout_report reads but the v7 spec no longer has.
V6_ESP_VALUES = ("ESP_REAR_Z", "ESP_BOTTOM", "ESP_TOTAL_HEIGHT", "ESP_RIM_CLEARANCE", "ESP_RIM_WALL",
                 "ESP_RIM_HEIGHT")
V6_ESP_FUNCTIONS = ("esp_rim_half_width",)


class V6EspShim:
    """Lets v6.layout_report run on the v7 spec; only the missing v6 ESP names are
    filled (with 0), and every field built from them is dropped afterwards."""

    def __init__(self, spec):
        self._spec = spec

    def __getattr__(self, name):
        if name in V6_ESP_VALUES:
            return 0.0
        if name in V6_ESP_FUNCTIONS:
            return lambda: 0.0
        return getattr(self._spec, name)


def layout_report(s, v6):
    """v6 layout without its old ESP fields, plus the v7 ESP board."""
    layout = v6.layout_report(V6EspShim(s))
    for key in ("esp_z_range_mm", "esp_reserved_y_range_mm"):
        layout.pop(key)
    for key in ("usb_stop_to_esp_rim", "esp_rim_rear_to_battery_front_stop",
                "esp_rim_to_front_screw_tabs_per_side"):
        layout["clearances_mm"].pop(key)
    layout["measured_by_user"].pop("esp_total_height_mm")
    layout["measured_by_user"]["esp_pcb_mm"] = [s.ESP_PCB[0], s.ESP_PCB[1], s.ESP_PCB_THICKNESS]
    layout["assumptions"].pop("esp_rests_on_base_with_rim_mm")
    drop_removed_supports(layout)
    layout["esp32_board"] = esp_report(s)
    return layout


# v6 layout fields about the battery and USB-C board mounts, which v7 does not build.
REMOVED_PREFIXES = ("battery_", "usb_board_", "usb_stop_", "usb_pin_", "usb_c_",
                    "usb_connector_top_to_battery", "usb_opening_top_to_battery")


def drop_removed_supports(layout):
    for section in (layout, layout["clearances_mm"], layout["assumptions"]):
        for key in [k for k in section if k.startswith(REMOVED_PREFIXES)
                    and k != "usb_c_center_height_mm"]:
            section.pop(key)


def write_report(design, s, v6, solids, opening, known):
    report = {"design": NAME, "timeline_features": design.timeline.count,
              "timeline_issues": [], "parts": solids, "unexpected_interferences": [],
              "opening_test": opening,
              "display_pcb_measured_mm": list(s.DISPLAY_PCB),
              "esp_pcb_measured_mm": [s.ESP_PCB[0], s.ESP_PCB[1], s.ESP_PCB_THICKNESS],
              "usb_board_measured_mm": list(s.USB_BOARD),
              "shell_screw_count": len(s.SHELL_SCREWS), "front_screw_count": len(s.FRONT_SCREWS),
              "layout": layout_report(s, v6), "missing_measurements": list(s.MISSING_MEASUREMENTS),
              "print_meshes_exported": False, "physical_fit_verified": False}
    report["known_interferences"] = known
    report["known_interference_totals_mm3"] = overlap_totals(known)
    x_in, x_out, z_rear, z_front, y_bottom, y_top = s.front_tab_notch()
    report["front_tab_notch"] = {"x_range_mm": [x_in, x_out], "mirrored_in_x": True,
                                 "z_range_mm": [z_rear, z_front],
                                 "y_range_mm": [y_bottom, y_top],
                                 "clearance_to_pcb_mm": s.FRONT_TAB_NOTCH_CLEARANCE,
                                 "open_faces": ["inner", "rear", "top"]}
    report["esp_standoffs"] = {
        "hole_positions_xz_mm": [[round(x, 3), round(z, 3)] for x, z in s.esp_hole_positions()],
        "built_at_xz_mm": [[round(x, 3), round(z, 3)] for x, z in s.esp_standoff_positions()],
        "front_holes_without_standoff": True,
        "hole_diameter_mm": s.ESP_HOLE_DIAMETER,
        "hole_centres_x_mm_measured": s.ESP_HOLE_CENTRES_X,
        "hole_inner_edges_x_mm_measured": s.ESP_HOLE_INNER_EDGES_X,
        "hole_edge_to_rear_edge_mm_measured": s.ESP_HOLE_EDGE_TO_REAR,
        "hole_edge_to_front_edge_mm_measured": s.ESP_HOLE_EDGE_TO_FRONT,
        "hole_centres_z_from_edges_mm": [s.ESP_HOLE_CENTRE_FROM_REAR,
                                         s.ESP_HOLE_CENTRE_FROM_FRONT],
        "standoff_diameter_mm": s.ESP_STANDOFF_DIAMETER,
        "free_gap_x_mm": s.esp_standoff_free_gap_x(),
        "min_free_gap_x_mm": s.ESP_STANDOFF_MIN_FREE_GAP_X,
        "pilot_wall_mm": (s.ESP_STANDOFF_DIAMETER - s.ESP_STANDOFF_PILOT_DIAMETER) / 2,
        "standoff_y_range_mm": [s.BASE_HEIGHT, s.ESP_PCB_BOTTOM],
        "screw": s.ESP_SCREW, "pilot_diameter_mm": s.ESP_STANDOFF_PILOT_DIAMETER,
        "pilot_depth_mm": s.ESP_STANDOFF_PILOT_DEPTH,
        "standoff_to_rear_wall_at_top_mm":
            s.esp_standoff_positions()[0][1] - s.ESP_STANDOFF_DIAMETER / 2
            - s.rear_wall_inner_z(s.ESP_PCB_BOTTOM)}
    x0, x1, z0, z1 = s.esp_front_pad()
    report["esp_front_pad"] = {"x_range_mm": [x0, x1], "z_range_mm": [z0, z1],
                               "y_range_mm": [s.BASE_HEIGHT, s.ESP_PCB_BOTTOM],
                               "front_free_strip_mm_assumed": s.ESP_FRONT_FREE_STRIP,
                               "screw": None}
    report["base"] = {"supports_removed": list(s.BASE_SUPPORTS_REMOVED),
                      "kept": list(s.BASE_FEATURES_KEPT),
                      "kept_for_user_to_decide": list(s.BASE_FEATURES_FOR_USER)}
    report["omitted_references"] = list(s.OMITTED_REFERENCES)
    report["unverified_without_references"] = list(s.UNVERIFIED_WITHOUT_REFERENCES)
    report["design"] = NAME
    report["status"] = ("v7: rear cover USB opening resized (v6 centre kept); ESP32 "
                        "devkit lying flat upside down, USB-C in the rear opening, ESP cradle removed; other base "
                        "corrections pending; front frame as in v6 plus the ESP notch in its screw tabs")
    report["pending_measurements"] = list(s.PENDING_MEASUREMENTS)
    report["rear_cover_usb_opening"] = rear_opening_report(s)
    report["rear_cover_usb_opening"]["receptacle_clearance_per_side_mm"] = list(
        s.receptacle_clearance_per_side())
    (OUTPUT / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def finalize(app, design, g, s, mods, parts, references, fasteners):
    v6, v5, _, _, previous = mods
    solids = previous.validate(design, parts + references)
    known = check_contacts(design, parts + references)
    log("contacts ok, known: {}".format(known))
    opening = v6.check_opening(g, v5, parts, references) if OPTIONS.get("opening") else {}
    log("opening done")
    design.rootComponent.isSketchFolderLightBulbOn = False
    design.rootComponent.isConstructionFolderLightBulbOn = False
    OUTPUT.mkdir(exist_ok=True)
    if OPTIONS.get("views"):
        v6.capture_views(app, v5, parts, references, fasteners)
        log("views done")
    if OPTIONS.get("export"):
        v6.export_native(design)
        log("export done")
    report = write_report(design, s, v6, solids, opening, known)
    log("report written")
    print(json.dumps(report))


def load_modules():
    """Load v6..v3 helpers and point their outputs at v7/."""
    v6, v5, v4 = load("stand_v6"), load("stand_v5"), load("stand_v4")
    v6.OUTPUT, v6.NAME, v5.OUTPUT = OUTPUT, NAME, OUTPUT
    v3 = v4.load_revision()
    return v6, v5, v4, v3, v3.load_previous()


def require_empty(app, design, finalize_only):
    if design is None or (not finalize_only and (
            app.activeDocument.isSaved or design.timeline.count
            or design.rootComponent.bRepBodies.count or design.rootComponent.occurrences.count)):
        raise RuntimeError("Open a new empty Fusion design before generating v7")


def run(context):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    finalize_only = OPTIONS.get("finalize_only")
    require_empty(app, design, finalize_only)
    s = load("stand_v7_spec")
    s.validate()
    mods = load_modules()
    g = mods[4].load_geometry()
    mods[0].configure(g, s)
    configure_rear_opening(g, s)
    if finalize_only:
        parts, references, fasteners = collect_parts(design, mods[0])
    else:
        design.designType = adsk.fusion.DesignTypes.ParametricDesignType
        app.activeDocument.name = NAME
        design.rootComponent.attributes.add("TokEsp", "generator", "stand_v7")
        parts, references, fasteners = build_all(app, design, g, s, mods)
        log("built {} timeline items".format(design.timeline.count))
    finalize(app, design, g, s, mods, parts, references, fasteners)


if __name__ == "__main__":
    run(None)
