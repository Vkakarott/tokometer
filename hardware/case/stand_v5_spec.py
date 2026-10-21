"""Measured footprints and explicitly provisional assembly settings, in mm."""

CASE_WIDTH = 68.0
CASE_DEPTH = 65.0
BODY_HEIGHT = 75.0
BASE_HEIGHT = 9.0
WALL = 2.0
FRONT_LEAN = BODY_HEIGHT * 5.8 / 57.7
REAR_LEAN = BODY_HEIGHT * 2.0 / 57.7
SCREEN_FROM_TOP = 28.0
DISPLAY_WINDOW = (40.0, 31.8)
DISPLAY_PCB = (48.9, 33.0)
ESP_PCB = (16.0, 20.7)

# These are modeling allowances, NOT measurements of the user's modules.
DISPLAY_STACK = 4.0
DISPLAY_PCB_THICKNESS = 1.6
DISPLAY_BOARD_OFFSET = (0.0, 0.0)
ESP_PCB_THICKNESS = 1.6
ESP_BOARD_BOTTOM = 14.0
ESP_BOARD_REAR = -60.0
ESP_USB_ABOVE_PCB = 1.6
USB_OPENING = (13.0, 7.0)
USB_CENTER_HEIGHT = ESP_BOARD_BOTTOM + ESP_PCB_THICKNESS + ESP_USB_ABOVE_PCB

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

MISSING_MEASUREMENTS = (
    "Display visible-area margins relative to the PCB",
    "Display total stack, rear components and connector envelope",
    "ESP32-C3 PCB/component thickness, headers, USB projection and buttons",
    "Actual cable plug envelope and printed fit tolerances",
)


def validate():
    assert DISPLAY_WINDOW[0] < DISPLAY_PCB[0] < CASE_WIDTH - 2 * WALL
    assert DISPLAY_WINDOW[1] < DISPLAY_PCB[1]
    assert GUIDE_WIDTH + 2 * SLIDE_CLEARANCE < 3.0
    assert NUT_SLOT_WIDTH > NUT_ACROSS_FLATS
    assert NUT_SLOT_HEIGHT > NUT_THICKNESS
    assert HEAD_POCKET_DEPTH + BOTTOM_SCREW_LENGTH > NUT_SEAT_HEIGHT + NUT_THICKNESS
    assert USB_CENTER_HEIGHT - USB_OPENING[1] / 2 > BASE_HEIGHT
    assert ESP_BOARD_REAR > -CASE_DEPTH + WALL
    assert all(abs(x) + HEAD_POCKET_DIAMETER / 2 < (CASE_WIDTH - 2) / 2
               for x, _ in SHELL_SCREWS + FRONT_SCREWS)
