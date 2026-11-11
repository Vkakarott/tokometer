"""Layout A specification for the v6 enclosure, in mm.

Every value is tagged MEASURED (user caliper, see ../dimensions.md), ESTIMATED
(read from photos) or ASSUMPTION (a modeling choice, NOT a measurement of the
user's modules). The outer shell, joints and fasteners are inherited from v5.
"""

CASE_WIDTH = 68.0
CASE_DEPTH = 65.0
BODY_HEIGHT = 75.0
BASE_HEIGHT = 9.0
WALL = 2.0
FRONT_LEAN = BODY_HEIGHT * 5.8 / 57.7
REAR_LEAN = BODY_HEIGHT * 2.0 / 57.7
SCREEN_FROM_TOP = 28.0

# MEASURED and CONFIRMED by the user: the opening of the case. It is slightly
# larger than the area that lights up (the lit area is smaller, not measured).
DISPLAY_WINDOW = (38.5, 31.0)
DISPLAY_PCB = (48.87, 33.0)  # MEASURED
DISPLAY_PCB_THICKNESS = 1.1  # MEASURED
FUNNEL_RUN = 8.0
FUNNEL_OUTER = (DISPLAY_WINDOW[0] + FUNNEL_RUN, DISPLAY_WINDOW[1] + FUNNEL_RUN)
ESP_PCB = (16.0, 20.0)  # MEASURED (x width, z length when lying flat)
ESP_PCB_THICKNESS = 1.6  # ASSUMPTION
BATTERY = (42.0, 24.0, 16.0)  # MEASURED envelope: x width, z length, y height

# --- Display: four posts on the front plate (layout A) -------------------
DISPLAY_STACK = 4.0  # ASSUMPTION
DISPLAY_BOARD_OFFSET = (0.0, 0.0)  # ASSUMPTION: PCB centred on the window
# MEASURED by the user: holes of 2.0 mm; 43.0 x 27.8 mm inner edge to inner
# edge on both axes (long side, short side), so 45.0 x 29.8 mm centre to centre.
DISPLAY_HOLE_DIAMETER = 2.0
DISPLAY_HOLE_EDGE_TO_EDGE = (43.0, 27.8)
DISPLAY_HOLE_SPACING = (DISPLAY_HOLE_EDGE_TO_EDGE[0] + DISPLAY_HOLE_DIAMETER,
                        DISPLAY_HOLE_EDGE_TO_EDGE[1] + DISPLAY_HOLE_DIAMETER)
# Screw choice: M1.6 self-tapping (clearance 0.4 mm in the 2.0 mm hole). M2 would
# have no clearance in a 2.0 mm hole, so it is not used.
SCREW_HEAD_DIAMETER = 3.0  # ASSUMPTION, typical M1.6 pan head
POST_DIAMETER = 3.0  # ASSUMPTION: stays inside the PCB edge (1.6 mm short side)
POST_PILOT_DIAMETER = 1.3  # ASSUMPTION: self-tapping M1.6 in printed plastic
POST_PILOT_DEPTH = 2.7
PLATE_DEPTH = 4.0  # front plate thickness inherited from v5

# --- Battery module: USB-C toward the rear (layout A) ---------------------
BATTERY_BOTTOM = 11.0  # ASSUMPTION: rests on 2 mm cradle ribs
BATTERY_REAR_Z = -61.0  # 0.3 mm margin from the inner rear wall at its top
USB_C_BODY = (9.3, 3.2)  # width ESTIMATED (9.3 +- 0.5); height ASSUMPTION
USB_C_PROTRUSION = 1.0  # ESTIMATED (1.0 +- 0.7)
USB_C_ABOVE_BATTERY_BOTTOM = 3.0  # ASSUMPTION
USB_C_X = 0.0  # ASSUMPTION: tail centred on the case
USB_CENTER_HEIGHT = BATTERY_BOTTOM + USB_C_ABOVE_BATTERY_BOTTOM
USB_OPENING = (13.0, 7.0)  # ASSUMPTION: provisional cable envelope, kept from v5
BATTERY_RIB_X = 15.0
BATTERY_RIB_WIDTH = 3.0
# No side stops: the cover's screw tabs (|x| >= 22) sweep the whole length when
# the cover slides off (CAD check, 24 mm3 collision at 20 mm travel), so the base
# cannot hold anything at |x| >= 22. The 1.0 mm gap to the rear tabs locates the
# battery sideways while the cover is on.
BATTERY_STOP_HEIGHT = 8.0
BATTERY_FRONT_STOP = (-36.5, -35.0)  # 0.5 mm clearance to the battery front
BATTERY_FRONT_STOP_HALF_WIDTH = 10.0

# --- ESP32-C3 Mini, beside the battery, toward the front ------------------
ESP_BOARD_BOTTOM = 12.0  # ASSUMPTION
ESP_BOARD_REAR = -33.0  # ASSUMPTION: 2 mm wire gap to the battery front stop
ESP_RETAINER_SCREWS = ((-12.0, -23.0), (12.0, -23.0))

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

# The inner rear wall leans forward with height (inherited v3 profile).
REAR_WALL_SLOPE = REAR_LEAN / BODY_HEIGHT

MISSING_MEASUREMENTS = (
    "Lit area size and its margins to the 38.5 x 31.0 opening (only known smaller)",
    "Display total stack (glass to highest rear point), 8-pin header "
    "position/height/direction and connector envelope",
    "ESP32-C3 thickness, USB-C edge and projection, BOOT/RESET positions",
    "Battery board: USB-C height, offset and protrusion; whether the tail "
    "lies inside the 24 mm; wire pass-through; power switch",
    "Actual cable plug envelope and printed fit tolerances",
)


def rear_wall_inner_z(y):
    return -(CASE_DEPTH - WALL) + REAR_WALL_SLOPE * (y - BASE_HEIGHT)


def battery_front_z():
    return BATTERY_REAR_Z + BATTERY[1]


def battery_top():
    return BATTERY_BOTTOM + BATTERY[2]


def validate():
    assert DISPLAY_WINDOW[0] < DISPLAY_PCB[0] < CASE_WIDTH - 2 * WALL
    assert DISPLAY_WINDOW[1] < DISPLAY_PCB[1]
    assert FUNNEL_OUTER[0] < CASE_WIDTH - 2 * WALL
    assert GUIDE_WIDTH + 2 * SLIDE_CLEARANCE < 3.0
    assert NUT_SLOT_WIDTH > NUT_ACROSS_FLATS
    assert HEAD_POCKET_DEPTH + BOTTOM_SCREW_LENGTH > NUT_SEAT_HEIGHT + NUT_THICKNESS
    # the rear wall at the top of the battery leaves the 0.3 mm margin
    tip = BATTERY_REAR_Z - USB_C_PROTRUSION
    assert tip - rear_wall_inner_z(battery_top()) >= 0.3 - 1e-9
    assert USB_CENTER_HEIGHT - USB_OPENING[1] / 2 > BASE_HEIGHT + 0.25
    assert USB_OPENING[0] > USB_C_BODY[0] and USB_OPENING[1] > USB_C_BODY[1]
    # battery clears the cover screw tabs (|x| >= 22) by at least 1 mm
    assert 22.0 - BATTERY[0] / 2 >= 1.0
    assert BATTERY[0] / 2 < 22.0  # stay inside the cover tab sweep
    assert BATTERY_FRONT_STOP[0] - battery_front_z() >= 0.5
    assert ESP_BOARD_REAR - BATTERY_FRONT_STOP[1] >= 1.5
    # display posts and screw heads sit on the PCB, inside its edge
    assert DISPLAY_HOLE_SPACING == (45.0, 29.8)
    for axis in (0, 1):
        edge = (DISPLAY_PCB[axis] - DISPLAY_HOLE_SPACING[axis]) / 2
        assert POST_DIAMETER / 2 <= edge + 1e-9
        assert SCREW_HEAD_DIAMETER / 2 <= edge + 1e-9
    assert POST_PILOT_DIAMETER < DISPLAY_HOLE_DIAMETER
    assert POST_PILOT_DEPTH < DISPLAY_STACK - DISPLAY_PCB_THICKNESS
    assert all(abs(x) + HEAD_POCKET_DIAMETER / 2 < (CASE_WIDTH - 2) / 2
               for x, _ in SHELL_SCREWS + FRONT_SCREWS)
