"""Layout A specification for the v6 enclosure, in mm.

Every value is tagged MEASURED (user caliper, see ../dimensions.md), DERIVED
(arithmetic on measured values), STANDARD (typical market value, not measured)
or ASSUMPTION (a modeling choice, NOT a measurement of the user's modules).
The outer shell, joints and fasteners are inherited from v5.

Layout: the display hangs on four posts behind the front opening; the USB-C
board sits low at the rear with its connector facing the rear opening; the
battery is raised above it (it is wider than the clear width between the cover
screw tabs); the ESP32-C3 sits in front of them, in its own low cradle.
"""

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

# --- ESP32-C3 Mini (MEASURED) ------------------------------------------------
ESP_PCB = (18.0, 22.7)  # x width, z length when lying flat
ESP_PCB_THICKNESS = 1.0
ESP_TOTAL_HEIGHT = 11.5  # with header and pins; used only to reserve space
ESP_BOTTOM = BASE_HEIGHT  # ASSUMPTION: rests on the base
ESP_REAR_Z = -31.5  # ASSUMPTION
ESP_RIM_CLEARANCE = 0.3  # ASSUMPTION
ESP_RIM_WALL = 1.2  # ASSUMPTION
ESP_RIM_HEIGHT = 6.0  # ASSUMPTION: low retaining rim, nothing leaves the case

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

# --- Rear opening: generic plug (STANDARD, not measured) ----------------------
USB_PLUG_METAL = (8.4, 2.6)  # STANDARD
USB_PLUG_COVER = (12.0, 6.5)  # STANDARD moulded cover
USB_OPENING_CLEARANCE = 0.5  # per side, chosen by the user
USB_OPENING = (USB_PLUG_COVER[0] + 2 * USB_OPENING_CLEARANCE,
               USB_PLUG_COVER[1] + 2 * USB_OPENING_CLEARANCE)  # 13.0 x 7.5

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

MISSING_MEASUREMENTS = (
    "Physical fit test of a printed sample: tolerances, the 0.9 mm USB-C board "
    "pins, the M1.6 display posts, the ESP rim and the plug in the rear opening",
)


def rear_wall_inner_z(y):
    return -(CASE_DEPTH - WALL) + REAR_WALL_SLOPE * (y - BASE_HEIGHT)


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


def usb_connector_top():
    return USB_CENTER_HEIGHT + USB_C_BODY[1] / 2


def esp_front_z():
    return ESP_REAR_Z + ESP_PCB[1]


def esp_rim_half_width():
    return ESP_PCB[0] / 2 + ESP_RIM_CLEARANCE + ESP_RIM_WALL


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
    validate_esp()


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
    assert USB_BOARD_REAR_Z - rear_wall_inner_z(board_top) >= USB_BOARD_WALL_GAP_MIN
    # the connector sits inside the opening tunnel, close to the outer face
    assert usb_tip_z() > rear_outer_z(USB_CENTER_HEIGHT - USB_C_BODY[1] / 2)
    assert usb_tip_z() - rear_outer_z(USB_CENTER_HEIGHT) <= 1.5
    assert (USB_OPENING[0] - USB_C_BODY[0]) / 2 >= 1.5
    assert (USB_OPENING[1] - USB_C_BODY[1]) / 2 >= 1.5
    assert USB_OPENING == (13.0, 7.5)
    assert USB_OPENING[0] > USB_PLUG_COVER[0] and USB_OPENING[1] > USB_PLUG_COVER[1]
    opening_bottom = USB_CENTER_HEIGHT - USB_OPENING[1] / 2
    opening_top = USB_CENTER_HEIGHT + USB_OPENING[1] / 2
    assert opening_bottom > BASE_HEIGHT + 0.25
    assert opening_top < BATTERY_BOTTOM
    assert usb_connector_top() + 1.0 <= BATTERY_BOTTOM
    assert USB_BOARD_BOTTOM > BASE_HEIGHT
    assert USB_PIN_DIAMETER < USB_HOLE_DIAMETER
    assert max(abs(x) for x, _ in usb_hole_positions()) - 0.5 < USB_BOARD[0] / 2 + 1.0


def validate_esp():
    # the 11.5 mm ESP cannot sit under the raised battery, so it stays in front of it
    assert ESP_BOTTOM + ESP_TOTAL_HEIGHT > BATTERY_BOTTOM
    assert ESP_REAR_Z - BATTERY_FRONT_STOP[1] >= 1.5
    assert FRONT_SLAB_INNER_X - esp_rim_half_width() >= 1.0
    assert esp_front_z() + ESP_RIM_CLEARANCE + ESP_RIM_WALL < -4.5
    # the rim's rear wall stays clear of the battery front stop
    assert ESP_REAR_Z - ESP_RIM_CLEARANCE - ESP_RIM_WALL - BATTERY_FRONT_STOP[1] >= 1.0
