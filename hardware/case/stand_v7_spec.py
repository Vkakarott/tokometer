"""Specification for the v7 enclosure, in mm.

v7 starts from the v6 values. The front frame is as in v6 except for a notch
in each front screw tab, where the ESP PCB passes (FRONT_TAB_NOTCH); the rear
cover changes only in its USB opening; the service base keeps only the v5 shell
joints (screw holes, head pockets, locating pins, guide rails): every
electronics support was removed (BASE_SUPPORTS_REMOVED). The battery and USB-C
board values below are kept only because the v6 layout report reads them;
nothing is built from them in v7.

Scope change: the rear cover also changes, but only in the size of its USB
opening, which keeps the v6 centre and shrinks symmetrically. The rest of the
cover stays identical.

Every value is tagged MEASURED (user caliper, see ../dimensions.md), DERIVED
(arithmetic on measured values), STANDARD (typical market value, not measured)
or ASSUMPTION (a modeling choice, NOT a measurement of the user's modules).
The outer shell, joints and fasteners are inherited from v5.

Layout: the display hangs on four posts behind the front opening; the USB-C
board sits low at the rear with its connector facing the rear opening; the
battery is raised above it (it is wider than the clear width between the cover
screw tabs). In v7 an ESP32 devkit lies flat at the rear, its USB-C through the
rear opening, with no cradle (the v6 ESP32-C3 cradle in front was removed).
"""

import math

CASE_WIDTH = 68.0
CASE_DEPTH = 65.0
BODY_HEIGHT = 75.0
BASE_HEIGHT = 9.0
WALL = 2.0
FRONT_LEAN = BODY_HEIGHT * 5.8 / 57.7
REAR_LEAN = BODY_HEIGHT * 2.0 / 57.7
SCREEN_FROM_TOP = 28.0

# --- Display (all MEASURED unless noted) -------------------------------------
DISPLAY_PCB = (51.0, 35.5)
DISPLAY_PCB_THICKNESS = 1.1
DISPLAY_STACK = 3.25  # glass front to highest rear point, with components
# Case opening, centred on the PCB; slightly larger than the lit area.
DISPLAY_WINDOW = (40.0, 33.5)
DISPLAY_WINDOW_MARGINS = ((DISPLAY_PCB[0] - DISPLAY_WINDOW[0]) / 2,
                          (DISPLAY_PCB[1] - DISPLAY_WINDOW[1]) / 2)  # 5.5, 1.0
FUNNEL_RUN = 8.0
FUNNEL_OUTER = (DISPLAY_WINDOW[0] + FUNNEL_RUN, DISPLAY_WINDOW[1] + FUNNEL_RUN)
DISPLAY_BOARD_OFFSET = (0.0, 0.0)  # opening centred on the PCB (user's instruction)
DISPLAY_HOLE_DIAMETER = 2.0
DISPLAY_HOLE_EDGE_TO_EDGE = (45.5, 29.7)  # long side, short side; convention confirmed
DISPLAY_HOLE_SPACING = (DISPLAY_HOLE_EDGE_TO_EDGE[0] + DISPLAY_HOLE_DIAMETER,
                        DISPLAY_HOLE_EDGE_TO_EDGE[1] + DISPLAY_HOLE_DIAMETER)  # 47.5, 31.7
# Screw choice: M1.6 self-tapping (0.4 mm clearance in the 2.0 mm hole). M2 would
# have no clearance in a 2.0 mm hole, so it is not used.
SCREW_HEAD_DIAMETER = 3.0  # ASSUMPTION, typical M1.6 pan head
POST_DIAMETER = 3.0  # ASSUMPTION: stays inside the PCB edge (1.75 / 1.9 mm)
POST_PILOT_DIAMETER = 1.3  # ASSUMPTION: self-tapping M1.6 in printed plastic
POST_PILOT_DEPTH = 2.8  # ASSUMPTION: runs 0.65 mm into the 4 mm front plate
PLATE_DEPTH = 4.0  # front plate thickness inherited from v5
# ASSUMPTION, kept by the user's decision (no measurement needed): the 3.25 mm
# total is taken as glass front to PCB back, with no component behind the PCB.
DISPLAY_PCB_FRONT_DEPTH = PLATE_DEPTH + DISPLAY_STACK - DISPLAY_PCB_THICKNESS

# --- Battery with charging board (MEASURED envelope) -------------------------
BATTERY = (44.6, 25.0, 16.5)  # x width wing tip to wing tip, z, y thickness
COVER_TAB_INNER_X = 22.0  # cover screw tabs, inherited from v5
COVER_TAB_TOP = BASE_HEIGHT + 9.0  # inherited from v5
BATTERY_CLEAR_OVER_TABS = 0.5  # ASSUMPTION
BATTERY_BOTTOM = COVER_TAB_TOP + BATTERY_CLEAR_OVER_TABS
BATTERY_REAR_Z = -61.0  # ASSUMPTION
BATTERY_RIB_X = 15.0
BATTERY_RIB_WIDTH = 3.0
BATTERY_FRONT_STOP = (-35.5, -34.5)  # 0.5 mm clearance to the battery front
BATTERY_FRONT_STOP_HALF_WIDTH = 10.0
BATTERY_FRONT_STOP_TOP = 24.5

# --- USB-C board (independent; MEASURED unless noted) ------------------------
USB_BOARD = (18.0, 20.0, 1.0)  # x width, z length, thickness
# ASSUMPTION: as far back as the 18 mm board can go (it is wider than the 13 mm
# opening, so its rear corners stop 0.33 mm short of the 2 mm wall's inner face).
# The connector then sits inside the opening tunnel, close to the outer face.
USB_BOARD_REAR_Z = -62.55
USB_BOARD_WALL_GAP_MIN = 0.3
USB_C_X = 0.0  # centred on the board
USB_CENTER_HEIGHT = 14.0  # the height the case is programmed for (inherited)
# The receptacle needs no measurement (user's decision): the opening has ample
# clearance for any cable as long as the connector sits near the case edge.
USB_C_BODY = (9.3, 3.2)  # typical receptacle width and height
USB_C_LENGTH = 7.35  # typical receptacle length
USB_C_PROTRUSION = 1.0  # typical, connector beyond the board edge
USB_BOARD_BOTTOM = USB_CENTER_HEIGHT - USB_C_BODY[1] / 2 - USB_BOARD[2]  # 11.4
USB_HOLE_DIAMETER = 1.0
USB_HOLES_EDGE_TO_EDGE = 15.9  # the two holes on the USB-C line
USB_HOLES_CENTRES = USB_HOLES_EDGE_TO_EDGE + USB_HOLE_DIAMETER  # 16.9
USB_HOLE_ROW_FROM_REAR = 3.5  # CONFIRMED by the user: pair 3.5 mm from the connector edge
USB_MIDDLE_HOLE_FROM_FRONT = 3.4  # MEASURED; CONFIRMED: "bottom" is the edge opposite the connector
USB_BOSS_DIAMETER = 2.0  # ASSUMPTION
USB_PIN_DIAMETER = 0.9  # ASSUMPTION: 0.1 mm clearance in the 1.0 mm holes
# Stop behind the board (the side opposite the connector): the plug's insertion
# thrust pushes the board forward onto it, so the pins only locate. The gap equals
# the pins' play (0.05 mm per side); the stop is far stiffer than the pins, so it
# takes the load. Final gap to be tuned in the printed fit test.
USB_STOP_GAP = 0.05
USB_STOP_THICKNESS = 2.0
USB_STOP_HALF_WIDTH = 10.0
USB_STOP_TOP = 14.0  # 1.6 mm above the board top, 4.5 mm below the battery

# --- Rear opening: generic plug (STANDARD, not measured) ----------------------
USB_PLUG_METAL = (8.4, 2.6)  # STANDARD
USB_PLUG_COVER = (12.0, 6.5)  # STANDARD moulded cover
USB_OPENING_CLEARANCE = 0.5  # per side, chosen by the user
USB_OPENING_V6 = (USB_PLUG_COVER[0] + 2 * USB_OPENING_CLEARANCE,
                  USB_PLUG_COVER[1] + 2 * USB_OPENING_CLEARANCE)  # 13.0 x 7.5

# --- v7 rear cover: USB opening, parametrised by centre and size --------------
# The only change to the rear cover in v7. The centre is frozen at the v6
# position (x, y), independent of USB_CENTER_HEIGHT, so a base change cannot
# move it; the size shrinks symmetrically about it, width and height.
USB_OPENING_CENTER = (0.0, 14.0)  # v6 position, must not move (user's rule)
USB_OPENING_CORNER_RADIUS = 1.5  # inherited from stand.py (USB_OVERMOULD_R)
USB_OPENING_WIDTH = 9.0  # user's value (v7)
USB_OPENING_HEIGHT = 3.5  # user's value (v7)
# Only the plug's metal shell passes (user's intent); the moulded cover stays outside.
USB_OPENING = (USB_OPENING_WIDTH, USB_OPENING_HEIGHT)

# --- ESP32 devkit (v7): lying flat, upside down, USB-C in the rear opening -----
# The user's real board replaces the v6 ESP32-C3 Mini. It lies flat, long side
# front to back (z), short USB-C edge at the rear wall, but upside down (user's
# correction to the photo): components and the USB-C face DOWN, the PCB rides on
# top of them, above the opening, and the USB-C fills the opening. No cradle or
# support on the base (adjusted later).
ESP_PCB = (28.5, 51.7)  # MEASURED (user): x width (short side), z length (long side)
ESP_PCB_THICKNESS = 1.5  # MEASURED (user)
ESP_USB_EDGE = "rear"  # the short edge with the USB-C, at the rear wall
ESP_COMPONENTS_FACE = "down"  # user's correction (the photo shows them up)
# ASSUMPTION (user: "the chip is about as tall as the USB connector"): one
# envelope of the USB-C body height covers the whole underside of the PCB.
# Components and USB-C are NOT modelled (user's request): only the PCB is a
# reference body; these values still set its height and the arithmetic checks.
ESP_COMPONENT_HEIGHT = USB_C_BODY[1]  # 3.2, typical USB-C body height
ESP_USB_C_BODY = USB_C_BODY  # 9.3 x 3.2, typical (same receptacle as the v6 board)
ESP_USB_C_PROTRUSION = USB_C_PROTRUSION  # 1.0, typical, beyond the PCB edge
ESP_USB_C_LENGTH = USB_C_LENGTH  # 7.35, typical, front to back
ESP_WALL_GAP = 0.3  # v6 clearance reused: rear edge to the inner wall, at the PCB top
ESP_X = USB_OPENING_CENTER[0]  # USB-C centred on the opening
# DERIVED: the USB-C body (under the PCB) is centred on the opening centre.
ESP_USB_C_BOTTOM = USB_OPENING_CENTER[1] - ESP_USB_C_BODY[1] / 2  # 12.4
ESP_PCB_BOTTOM = USB_OPENING_CENTER[1] + ESP_USB_C_BODY[1] / 2  # 15.6
ESP_PCB_TOP = ESP_PCB_BOTTOM + ESP_PCB_THICKNESS  # 17.1
ESP_COMPONENT_BOTTOM = ESP_PCB_BOTTOM - ESP_COMPONENT_HEIGHT  # 12.4
# DERIVED, not chosen: room under the components down to the base floor.
ESP_FLOOR_GAP = ESP_COMPONENT_BOTTOM - BASE_HEIGHT  # 3.4

# --- ESP mounting holes and standoffs on the base -------------------------------
ESP_HOLE_DIAMETER = 3.0  # user: four holes, one per corner
# MEASURED (user, caliper, 2026-11-16): across the short side (x), 20.0 mm between
# the holes' inner edges, so 23.0 mm between centres (x = +-11.5).
ESP_HOLE_INNER_EDGES_X = 20.0
ESP_HOLE_CENTRES_X = ESP_HOLE_INNER_EDGES_X + ESP_HOLE_DIAMETER  # DERIVED: 23.0
# MEASURED (user, caliper, 2026-11-16): along the long side (z), from the hole's
# edge to the PCB's short edge (same convention as the 20.0 in x): 0.9 at the
# rear (USB-C) edge, 0.7 at the front edge. Read as distances to the CENTRE they
# would be impossible (a 1.5 radius hole would break through the edge).
ESP_HOLE_EDGE_TO_REAR = 0.9
ESP_HOLE_EDGE_TO_FRONT = 0.7
ESP_HOLE_CENTRE_FROM_REAR = ESP_HOLE_EDGE_TO_REAR + ESP_HOLE_DIAMETER / 2  # DERIVED: 2.4
ESP_HOLE_CENTRE_FROM_FRONT = ESP_HOLE_EDGE_TO_FRONT + ESP_HOLE_DIAMETER / 2  # DERIVED: 2.2
# ASSUMPTION: M2.5 self-tapping screws from above, through the 3.0 holes into
# the standoffs. The board hangs upside down, so it must be clamped, not only
# located; and the 0.25 mm per side clearance of M2.5 in a 3.0 hole tolerates
# hole-position error better than a 2.8 guide pin (0.1 per side).
ESP_SCREW = "M2.5 self-tapping, about 8 mm long"
ESP_STANDOFF_PILOT_DIAMETER = 2.2  # ASSUMPTION: M2.5 self-tapping in printed plastic
ESP_STANDOFF_PILOT_DEPTH = 7.0  # ASSUMPTION: 8 mm screw minus the 1.5 mm PCB, plus 0.5
# User constraint (MEASURED on the board): at least 18.0 mm free in x between the
# two standoffs' inner edges, or they hit the components under the PCB.
ESP_STANDOFF_MIN_FREE_GAP_X = 18.0
# Minimum plastic wall around the M2.5 pilot (about two 0.45 mm perimeters).
ESP_STANDOFF_MIN_WALL = 1.0
# ASSUMPTION, chosen: 4.2 (was 4.5). It gives 18.8 free in x (0.8 margin over the
# 18.0 above; 4.5 gave only 0.5) and keeps exactly the 1.0 wall around the pilot
# (4.0 would give 19.0 but a 0.9 wall). It stays on the PCB corner (radius 2.1;
# centres 2.75 from the long edges, 2.5 from the short ones) and reaches 4.85 in
# from the long edge, inside the 5.25 the 18.0 gap allows.
ESP_STANDOFF_DIAMETER = 4.2
# Only the rear holes get a standoff: the front holes sit over the front frame's
# screw tabs (x 11.5 to 20.5, z -14.5 to -3.0), a separate part, so a base
# standoff there would overlap it (see PENDING_MEASUREMENTS).
ESP_STANDOFF_CORNERS = ("rear",)
# User's decision (option A): the front holes get no screw and no pilot. The
# front of the PCB only rests on one plain pad on the base, so it does not hang.
# ASSUMPTION, to be measured: on the underside, the module/antenna stops about
# 2.0 mm short of the front edge (photo estimate). The pad sits in that strip,
# centred in x, away from the holes (|x| >= 10.0), the front frame tabs
# (|x| >= 11.5) and the components; it uses 1.4 of the 2.0 mm.
ESP_FRONT_FREE_STRIP = 2.0  # ASSUMPTION (photo)
ESP_FRONT_PAD_WIDTH = 8.0  # ASSUMPTION: x -4 to +4
ESP_FRONT_PAD_INSET = 0.2  # ASSUMPTION: back from the PCB's front edge
ESP_FRONT_PAD_DEPTH = 1.2  # ASSUMPTION: z, about three 0.45 mm perimeters

# --- Front frame screw tabs: notch for the ESP PCB (user's option A) -----------
# Each front screw tab (x 11.5 to 20.5, y 9 to 18, z -14.5 to -3.0, inherited
# from v5) gets a notch open on its inner face, its rear face and its top, where
# the PCB runs through it, with this clearance to the PCB on every face.
FRONT_TAB_NOTCH_CLEARANCE = 0.3  # user
FRONT_TAB_REAR_Z = -14.5  # inherited from v5 (front tab slab)
FRONT_TAB_TOP = BASE_HEIGHT + 9.0  # 18.0, inherited from v5

SLIDE_CLEARANCE = 0.3
GUIDE_X = 30.0
GUIDE_WIDTH = 1.8
GUIDE_HEIGHT = 2.2
GUIDE_Z = (-52.0, -23.0)
SHELL_SCREWS = tuple((x, z) for x in (-26.0, 26.0) for z in (-14.5, -58.5))
FRONT_SCREWS = ((-16.0, -10.0), (16.0, -10.0))
SCREW_CLEARANCE = 3.4
HEAD_POCKET_DIAMETER = 6.2
HEAD_POCKET_DEPTH = 3.4
NUT_ACROSS_FLATS = 5.5
NUT_THICKNESS = 2.4
NUT_SLOT_WIDTH = 5.8
NUT_SLOT_HEIGHT = 2.8
NUT_SEAT_HEIGHT = 13.0
BOTTOM_SCREW_LENGTH = 16.0
FRONT_SLAB_INNER_X = 11.5  # front screw tabs, inherited from v5

# The inner rear wall leans forward with height (inherited v3 profile).
REAR_WALL_SLOPE = REAR_LEAN / BODY_HEIGHT
# Rear cover upper slope, inherited from stand_v3 (REAR_SHOULDER_HEIGHT/ANGLE);
# needed to place the inner shoulder of the rear wall exactly as stand_v5 does.
REAR_SHOULDER_HEIGHT = 10.0
REAR_SHOULDER_ANGLE = 26.0

MISSING_MEASUREMENTS = (
    "Physical fit test of a printed sample: tolerances, the 0.9 mm USB-C board "
    "pins, the M1.6 display posts, the flat ESP devkit and the plug in "
    "the rear opening",
)

# v7 changes waiting for the user's values: the base corrections from the v6
# print, and the new size of the rear cover's USB opening.
PENDING_MEASUREMENTS = (
    "ESP32 mounting holes: all MEASURED (x: 20.0 between inner edges, centres "
    "+-11.5; z: hole edge 0.9 from the rear edge and 0.7 from the front edge, "
    "read as edge distances, so centres 2.4 and 2.2 from the edges, 47.1 apart). "
    "Confirm with the user that 0.9 and 0.7 are to the hole EDGE (read to the "
    "centre they would be physically impossible)",
    "ESP front: user's decision (option A): no screw and no pilot in the front "
    "holes; the PCB front rests on one pad (x -4 to 4, z -12.12 to -10.92). "
    "MEASURE on the underside: the free strip between the PCB's front edge and the "
    "module/antenna (2.0 assumed from the photo; the pad needs 1.4 plus clearance)",
    "ESP32 devkit (upside down): how it is held (rear standoffs only); the "
    "real height of the components under the PCB (3.2 assumed, the USB-C body "
    "height; 3.4 mm free below them to the floor); how far its USB-C overhangs the "
    "PCB edge (1.0 typical) and its width (9.3 typical); whether header pins point "
    "down (6.6 mm from the PCB underside to the floor)",
    "NOTE, to check in the printed fit test: the front screw tabs are notched for "
    "the ESP PCB (user's option A, x 11.5 to 14.55, z -14.5 to -10.42, y 15.3 to "
    "18.0, mirrored). Sacrificed: the wall between the notch and the 3.4 screw hole "
    "(the notch opens up to 0.19 mm into the hole over z -11.7 to -10.42) and the "
    "roof of the nut pocket over x 11.5 to 14.55 (about a quarter of the nut's "
    "width is unroofed; the nut still bears on the pocket floor at y 13). The "
    "components under the PCB (not modelled) would also reach the front M3 nuts",
    "Separate USB-C board: its base mount (bosses, pins, stop) was removed in v7. "
    "If the board is still used it needs a new mount, and the v6 pin problem "
    "(what failed: position, diameter or height) is still open",
    "Base: keep or remove the v5 guide rails (x +-30) and the two front-frame "
    "locating pins (x +-16, z -5.5)? They are neither electronics nor screw "
    "supports, so they were kept for the user to decide",
    "NOTE on v6 (its files are not changed): v6 uses the old rear-wall formula, "
    "whose inner face is about 0.03 mm too far back, so the v6 clearances to the "
    "rear wall are about 0.03 mm optimistic (its USB-C board edge: 0.33 computed, "
    "about 0.30 real)",
    "WARNING, to be resolved by the user: the USB-C receptacle (now the ESP's; "
    "9.3 wide, typical, not measured) is wider than the 9.0 rear opening; its tip "
    "sits inside the wall, so it overlaps the cover by 0.15 mm per side",
)

# Reference bodies the v7 generator no longer creates (user's request). The
# values above still drive the base features and the arithmetic checks in
# validate(); only the CAD solid checks against these bodies are lost.
OMITTED_REFERENCES = ("reference_battery_envelope", "reference_usb_c_board")
UNVERIFIED_WITHOUT_REFERENCES = (
    "Battery and separate USB-C board: no reference bodies and no supports in v7; "
    "if either is still used, nothing checks its fit",
    "ESP components under the PCB (3.2 assumed, y 12.4 to 15.6) are not modelled: "
    "the CAD no longer checks them against the base floor (3.4 mm gap), the front "
    "frame's screw tabs and the front M3 nuts (y 13 to 15.4, about 2.1 mm3 each "
    "when modelled)",
    "ESP USB-C (9.3 x 3.2 x 7.35 typical) is not modelled: the CAD no longer sees "
    "its 0.15 mm per side overlap with the 9.0 rear opening (WARNING item) nor "
    "checks the cover sliding past it; the PCB height still assumes it is centred "
    "on the opening",
)

# Electronics supports removed from the v7 base (user's request); only the screw
# joints and the v5 guides stay.
BASE_SUPPORTS_REMOVED = (
    "battery ribs (2)", "battery front stop",
    "USB-C board bosses (3) and locating pins (3)", "USB-C board stop",
    "ESP cradle (removed earlier)",
)
BASE_FEATURES_KEPT = (
    "6 M3 screw clearance holes with head pockets (2 front frame, 4 cover)",
    "2 ESP standoffs under the PCB's rear holes, with M2.5 pilot holes (added in v7)",
    "1 plain pad under the ESP PCB's front edge, no screw (added in v7)",
)
BASE_FEATURES_FOR_USER = (
    "2 guide rails for the cover (x +-30, z -52 to -23, 1.8 wide, 2.2 tall)",
    "2 front-frame locating pins (x +-16, z -5.5, 2.4 diameter, 1.9 tall)",
    "decorative keyboard-port recess on the underside (cosmetic, not a support)",
)


def inner_rear_wall_line():
    """(y0, z0, y1, z1): the inner face of the rear wall, as stand_v5.build_shell
    cuts it: from (BASE_T - 1, -BASE_D + WALL) to the inner shoulder.

    v7 fix: the v6 formula anchored the line at y = BASE_HEIGHT instead of
    BASE_HEIGHT - 1, which put the face about 0.03 mm too far back at the ESP's
    height (measured in Fusion: 0.27 mm where 0.30 was computed)."""
    upper = math.tan(math.radians(REAR_SHOULDER_ANGLE))
    shoulder_y = BASE_HEIGHT + BODY_HEIGHT - REAR_SHOULDER_HEIGHT
    offset = WALL * math.sqrt(1 + upper ** 2)
    y1 = shoulder_y - (offset - WALL) / (upper - REAR_WALL_SLOPE)
    z1 = -CASE_DEPTH + WALL + REAR_WALL_SLOPE * (y1 - BASE_HEIGHT)
    return BASE_HEIGHT - 1, -CASE_DEPTH + WALL, y1, z1


def rear_wall_inner_z(y):
    y0, z0, y1, z1 = inner_rear_wall_line()
    return z0 + (y - y0) * (z1 - z0) / (y1 - y0)


def rear_outer_z(y):
    return -CASE_DEPTH + REAR_WALL_SLOPE * (y - BASE_HEIGHT)


def usb_tip_z():
    return USB_BOARD_REAR_Z - USB_C_PROTRUSION


def battery_front_z():
    return BATTERY_REAR_Z + BATTERY[1]


def battery_top():
    return BATTERY_BOTTOM + BATTERY[2]


def usb_board_front_z():
    return USB_BOARD_REAR_Z + USB_BOARD[1]


def usb_hole_positions():
    """(x, z) of the three USB-C board holes."""
    row = USB_BOARD_REAR_Z + USB_HOLE_ROW_FROM_REAR
    half = USB_HOLES_CENTRES / 2
    return ((-half, row), (half, row), (0.0, usb_board_front_z() - USB_MIDDLE_HOLE_FROM_FRONT))


def usb_opening_bounds():
    """(left, right, bottom, top) of the rear USB opening, symmetric about its centre."""
    (cx, cy), (width, height) = USB_OPENING_CENTER, USB_OPENING
    return cx - width / 2, cx + width / 2, cy - height / 2, cy + height / 2


def receptacle_clearance_per_side():
    """(width, height) clearance of the board's receptacle in the opening; < 0 overlaps."""
    return tuple((USB_OPENING[i] - USB_C_BODY[i]) / 2 for i in (0, 1))


def usb_connector_top():
    return USB_CENTER_HEIGHT + USB_C_BODY[1] / 2


def usb_stop_z():
    """(rear face, front face) of the stop behind the USB-C board."""
    start = usb_board_front_z() + USB_STOP_GAP
    return start, start + USB_STOP_THICKNESS


def usb_pin_play_per_side():
    return (USB_HOLE_DIAMETER - USB_PIN_DIAMETER) / 2


def wall_normal_gap(z, y):
    """Distance from the point (y, z) to the inner rear wall, perpendicular to the
    wall (what Fusion's minimum distance measures), not along z."""
    y0, z0, y1, z1 = inner_rear_wall_line()
    return (z - rear_wall_inner_z(y)) * math.cos(math.atan((z1 - z0) / (y1 - y0)))


def esp_rear_z():
    """Rear edge: ESP_WALL_GAP from the inner wall, perpendicular to it, at the PCB
    top, the highest point of the ESP (the wall leans forward, so that is its
    closest point)."""
    y0, z0, y1, z1 = inner_rear_wall_line()
    lean = math.atan((z1 - z0) / (y1 - y0))
    return rear_wall_inner_z(ESP_PCB_TOP) + ESP_WALL_GAP / math.cos(lean)


def esp_front_z():
    return esp_rear_z() + ESP_PCB[1]


def esp_usb_c_z():
    """(tip, back) z of the USB-C body under the PCB."""
    return esp_usb_tip_z(), esp_usb_tip_z() + ESP_USB_C_LENGTH


def esp_top():
    return ESP_PCB_TOP


def esp_usb_tip_z():
    return esp_rear_z() - ESP_USB_C_PROTRUSION


def validate():
    inner_width = CASE_WIDTH - 2 * WALL
    assert DISPLAY_WINDOW[0] < DISPLAY_PCB[0] < inner_width
    assert DISPLAY_WINDOW[1] < DISPLAY_PCB[1]
    assert DISPLAY_WINDOW_MARGINS == (5.5, 1.0)
    assert FUNNEL_OUTER[0] < inner_width
    assert GUIDE_WIDTH + 2 * SLIDE_CLEARANCE < 3.0
    assert NUT_SLOT_WIDTH > NUT_ACROSS_FLATS
    assert HEAD_POCKET_DEPTH + BOTTOM_SCREW_LENGTH > NUT_SEAT_HEIGHT + NUT_THICKNESS
    assert all(abs(x) + HEAD_POCKET_DIAMETER / 2 < (CASE_WIDTH - 2) / 2
               for x, _ in SHELL_SCREWS + FRONT_SCREWS)
    validate_display()
    validate_battery()
    validate_usb()
    validate_usb_stop()
    validate_esp()
    validate_rear_opening()
    validate_front_tab_notch()
    validate_esp_standoffs()
    validate_esp_front_pad()


def validate_display():
    assert DISPLAY_HOLE_SPACING == (47.5, 31.7)
    for axis in (0, 1):
        edge = (DISPLAY_PCB[axis] - DISPLAY_HOLE_SPACING[axis]) / 2
        assert POST_DIAMETER / 2 <= edge + 1e-9
        assert SCREW_HEAD_DIAMETER / 2 <= edge + 1e-9
    assert POST_PILOT_DIAMETER < DISPLAY_HOLE_DIAMETER
    # the pilot may enter the plate, but at least 2.5 mm of plate stays solid
    post_length = DISPLAY_PCB_FRONT_DEPTH - PLATE_DEPTH
    assert PLATE_DEPTH - max(0.0, POST_PILOT_DEPTH - post_length) >= 2.5
    assert POST_PILOT_DEPTH >= 2.5  # screw engagement


def validate_battery():
    # wings reach beyond the cover tabs' inner edge, so the battery rides above them
    assert BATTERY[0] / 2 > COVER_TAB_INNER_X
    assert BATTERY_BOTTOM - COVER_TAB_TOP >= 0.4
    assert BATTERY[0] / 2 < CASE_WIDTH / 2 - WALL
    assert BATTERY_FRONT_STOP[0] - battery_front_z() >= 0.5 - 1e-9
    assert rear_wall_inner_z(battery_top()) < BATTERY_REAR_Z


def validate_usb():
    board_top = USB_BOARD_BOTTOM + USB_BOARD[2]
    # v7: the separate USB-C board is not built, so its v6 gap to the wall is not
    # checked here. With the corrected wall it is 0.2998 (v6 computed 0.33); see
    # the NOTE on v6 in PENDING_MEASUREMENTS.
    # the connector sits inside the opening tunnel, close to the outer face
    assert usb_tip_z() > rear_outer_z(USB_CENTER_HEIGHT - USB_C_BODY[1] / 2)
    assert usb_tip_z() - rear_outer_z(USB_CENTER_HEIGHT) <= 1.5
    # v7: the receptacle clearance is a warning in PENDING_MEASUREMENTS, not a check,
    # and only the plug's metal shell has to pass (user's decisions)
    assert USB_OPENING_V6 == (13.0, 7.5)
    assert USB_OPENING[0] > USB_PLUG_METAL[0] and USB_OPENING[1] > USB_PLUG_METAL[1]
    opening_bottom = usb_opening_bounds()[2]
    opening_top = usb_opening_bounds()[3]
    assert opening_bottom > BASE_HEIGHT + 0.25
    assert opening_top < BATTERY_BOTTOM
    assert usb_connector_top() + 1.0 <= BATTERY_BOTTOM
    assert USB_BOARD_BOTTOM > BASE_HEIGHT
    assert USB_PIN_DIAMETER < USB_HOLE_DIAMETER
    assert max(abs(x) for x, _ in usb_hole_positions()) - 0.5 < USB_BOARD[0] / 2 + 1.0


def validate_usb_stop():
    rear, front = usb_stop_z()
    assert 0.0 < USB_STOP_GAP <= 0.1
    assert USB_STOP_THICKNESS >= 1.6
    assert USB_STOP_HALF_WIDTH >= USB_BOARD[0] / 2
    assert USB_STOP_TOP > USB_BOARD_BOTTOM + USB_BOARD[2]
    assert BATTERY_BOTTOM - USB_STOP_TOP >= 4.0
    assert BATTERY_FRONT_STOP[0] - front >= 3.0
    # the third boss (under the board) stays behind the stop
    assert usb_hole_positions()[2][1] + USB_BOSS_DIAMETER / 2 < usb_board_front_z()


def validate_esp():
    # the ESP's USB-C (under the PCB) is centred on the rear opening
    assert (ESP_X, (ESP_USB_C_BOTTOM + ESP_PCB_BOTTOM) / 2) == USB_OPENING_CENTER
    assert abs(ESP_COMPONENT_BOTTOM - ESP_USB_C_BOTTOM) < 1e-9  # same assumed height
    assert ESP_FLOOR_GAP > 0
    assert abs(ESP_X) + ESP_PCB[0] / 2 < CASE_WIDTH / 2 - WALL
    # the PCB clears the leaning wall; the USB-C tip stays inside the wall
    assert wall_normal_gap(esp_rear_z(), esp_top()) >= ESP_WALL_GAP - 1e-9
    assert (rear_outer_z(USB_OPENING_CENTER[1]) < esp_usb_tip_z()
            < rear_wall_inner_z(USB_OPENING_CENTER[1]))
    # the USB-C body stays under the PCB behind its overhang
    assert esp_rear_z() < esp_usb_c_z()[1] < esp_front_z()
    # the USB-C body fits the opening in height (its width is the WARNING item)
    assert ESP_USB_C_BODY[1] < USB_OPENING[1]


def front_tab_notch():
    """(x_inner, x_outer, z_rear, z_front, y_bottom, y_top) of the notch in the +x tab;
    the -x tab gets its mirror image."""
    c = FRONT_TAB_NOTCH_CLEARANCE
    return (FRONT_SLAB_INNER_X, ESP_X + ESP_PCB[0] / 2 + c, FRONT_TAB_REAR_Z,
            esp_front_z() + c, ESP_PCB_BOTTOM - c, FRONT_TAB_TOP)


def validate_front_tab_notch():
    x_in, x_out, z_rear, z_front, y_bottom, y_top = front_tab_notch()
    # the notch reaches past the PCB by the clearance on every face it faces
    assert x_out - (ESP_X + ESP_PCB[0] / 2) >= FRONT_TAB_NOTCH_CLEARANCE - 1e-9
    assert z_front - esp_front_z() >= FRONT_TAB_NOTCH_CLEARANCE - 1e-9
    assert ESP_PCB_BOTTOM - y_bottom >= FRONT_TAB_NOTCH_CLEARANCE - 1e-9
    assert y_top >= ESP_PCB_TOP + FRONT_TAB_NOTCH_CLEARANCE  # open on top
    # it stays inside the tab and leaves the screw axis and most of the nut alone
    screw_x, screw_z = FRONT_SCREWS[1]
    assert x_in < x_out < screw_x - SCREW_CLEARANCE / 2 + 0.3
    assert z_rear < z_front < screw_z
    assert y_bottom > NUT_SEAT_HEIGHT + NUT_THICKNESS - 0.5  # above most of the nut


def esp_hole_positions():
    """(x, z) of the four PCB holes: rear pair (USB-C edge) first, then front pair."""
    x = ESP_HOLE_CENTRES_X / 2
    rear = esp_rear_z() + ESP_HOLE_CENTRE_FROM_REAR
    front = esp_front_z() - ESP_HOLE_CENTRE_FROM_FRONT
    return ((ESP_X - x, rear), (ESP_X + x, rear), (ESP_X - x, front), (ESP_X + x, front))


def esp_standoff_positions():
    """The holes that get a standoff on the base (only the rear pair)."""
    assert ESP_STANDOFF_CORNERS == ("rear",)
    return esp_hole_positions()[:2]


def esp_standoff_free_gap_x():
    """Free distance in x between the inner edges of the two rear standoffs."""
    (left_x, _), (right_x, _) = esp_standoff_positions()
    return (right_x - left_x) - ESP_STANDOFF_DIAMETER


def esp_front_holes_over_front_tabs():
    """True while the front holes' standoff footprint overlaps the front screw tabs."""
    r = ESP_STANDOFF_DIAMETER / 2
    return any(abs(x) + r > FRONT_SLAB_INNER_X and FRONT_TAB_REAR_Z < z + r
               for x, z in esp_hole_positions()[2:])


def esp_front_pad():
    """(x0, x1, z0, z1) of the pad under the PCB's front edge."""
    z1 = esp_front_z() - ESP_FRONT_PAD_INSET
    half = ESP_FRONT_PAD_WIDTH / 2
    return ESP_X - half, ESP_X + half, z1 - ESP_FRONT_PAD_DEPTH, z1


def validate_esp_front_pad():
    x0, x1, z0, z1 = esp_front_pad()
    # inside the free strip at the front edge, with 0.3 to the components
    assert ESP_FRONT_PAD_INSET + ESP_FRONT_PAD_DEPTH <= ESP_FRONT_FREE_STRIP - 0.3
    # clear of the front holes and of the front frame tabs, with 0.3
    hole_edge = ESP_HOLE_CENTRES_X / 2 - ESP_HOLE_DIAMETER / 2
    assert max(abs(x0), abs(x1)) <= hole_edge - 0.3
    assert max(abs(x0), abs(x1)) <= FRONT_SLAB_INNER_X - 0.3
    assert z0 > esp_rear_z() and z1 < esp_front_z()  # under the PCB


def validate_esp_standoffs():
    r = ESP_STANDOFF_DIAMETER / 2
    # the standoff stays under the PCB corner
    assert ESP_PCB[0] / 2 - ESP_HOLE_CENTRES_X / 2 >= r
    assert ESP_HOLE_CENTRE_FROM_REAR >= r  # the built (rear) standoffs
    assert min(ESP_HOLE_EDGE_TO_REAR, ESP_HOLE_EDGE_TO_FRONT) > 0  # holes inside the PCB
    assert ESP_STANDOFF_PILOT_DIAMETER < ESP_HOLE_DIAMETER
    assert (ESP_STANDOFF_DIAMETER - ESP_STANDOFF_PILOT_DIAMETER) / 2 >= ESP_STANDOFF_MIN_WALL - 1e-9
    # free room between the standoffs for the components under the PCB (user)
    assert esp_standoff_free_gap_x() >= ESP_STANDOFF_MIN_FREE_GAP_X
    assert ESP_STANDOFF_PILOT_DEPTH < ESP_PCB_BOTTOM  # stays inside the base
    for x, z in esp_standoff_positions():
        # 0.3 mm from the leaning rear wall at the standoff top (its closest point)
        assert wall_normal_gap(z - r, ESP_PCB_BOTTOM) >= ESP_WALL_GAP
        # clear of the rear opening and of the cover's screw tabs
        assert abs(x) - r > USB_OPENING[0] / 2
        assert abs(x) + r < COVER_TAB_INNER_X
    assert esp_front_holes_over_front_tabs()  # why the front pair is not built


def validate_rear_opening():
    # the centre stays where it was in v6; the cutter (stand.cut_usb_port) is at x = 0
    assert USB_OPENING_CENTER == (0.0, 14.0)
    # it may only shrink, and the rounded corners must still fit
    assert all(0 < USB_OPENING[i] <= USB_OPENING_V6[i] for i in (0, 1))
    assert min(USB_OPENING) > 2 * USB_OPENING_CORNER_RADIUS
