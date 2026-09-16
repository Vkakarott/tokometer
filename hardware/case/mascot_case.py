"""Generates the tokEsp desk case for the Heltec WiFi LoRa 32 V2.

A minimal block mascot: a hollow rectangular body with 2 mm walls, two hollow
side arms and four legs, with the OLED window as its face. Internal fins hold
the board and battery. Every feature is an extrusion along the depth axis or
the width axis, so the body prints face-down without supports.

Run inside Fusion (Scripts and Add-Ins, or the Fusion MCP). All dimensions are
millimetres; Fusion's API works in centimetres, so every value goes through
`cm()`. Coordinates: X to the viewer's right, Y up, Z towards the viewer. The
PCB front face sits on Z = 0 and the OLED active area is centred on X = 0,
Y = 0. The model is moved at the end so the feet stand on Y = 0.
"""

import adsk.core
import adsk.fusion

# --- Board (Heltec WiFi LoRa 32 V2), positions from the Heltec pinout drawing
BOARD_CENTER = (-6.0, -2.0)  # XY of the PCB centre; the OLED sits off-centre
BOARD_L, BOARD_W, PCB_T = 51.0, 25.5, 1.6
OLED_STACK = 6.0  # PCB front face to OLED glass surface (estimate)
PIN_DEPTH = 9.0  # soldered headers, below the PCB back face (estimate)
BUTTON_TOP = 2.5  # tact switch actuator height above the PCB (estimate)
PRG_XY = (-28.5, 6.3)
RST_XY = (-28.5, -10.7)
USB_Y = -0.9
WINDOW = (26.0, 15.0)  # OLED window, centred on the active area

# --- Battery pocket (fits a 102050 LiPo and anything up to 52 x 26 x 11)
BATTERY_L, BATTERY_W, BATTERY_T = 52.0, 26.0, 11.0

# --- Print tolerances (Bambu Lab, PLA, 0.4 nozzle)
CLEARANCE = 0.2
WALL = 2.0

# --- Depth stations along Z
FRONT_INNER_Z = OLED_STACK + 0.3
FRONT_Z = FRONT_INNER_Z + WALL
BOARD_BACK_Z = -PCB_T - PIN_DEPTH - 0.6
BATTERY_BACK_Z = BOARD_BACK_Z - BATTERY_T - 0.5
BACK_INNER_Z = BATTERY_BACK_Z - 1.0  # room for a foam pad
LID_FLANGE_T, LID_LIP_T = 1.4, 1.4
BACK_Z = BACK_INNER_Z - LID_FLANGE_T - LID_LIP_T

# --- Tight side walls. The left wall only has to hide the board; the USB plug
# head sits in an outer recess so its metal shell reaches the receptacle.
CAVITY_LEFT = BOARD_CENTER[0] - BOARD_L / 2 - CLEARANCE - 0.1
SIDE_WALL = WALL
LID_FLANGE = 0.8
USB_SHELL_HOLE = (7.8, 3.4)  # micro-B metal shell plus clearance (Y, Z)
USB_PLUG_RECESS = (11.5, 8.5, 1.0)  # plug overmould pocket (Y, Z, depth)
USB_CENTER_Z = 1.4
SWITCH_Y, SWITCH_Z = -1.0, -9.0  # centre of the SS12D00 lever on the right wall

# --- Mascot silhouette (front view), proportioned on the mascot: the body is
# ~1.16 : 1 and the face sits at ~40 % from the top.
BODY_HALF_W = -CAVITY_LEFT + SIDE_WALL
BODY_HEIGHT = 2 * BODY_HALF_W / 1.16
BODY_TOP = 0.4 * BODY_HEIGHT
BODY_BOTTOM = BODY_TOP - BODY_HEIGHT
ARM_REACH = 0.156 * 2 * BODY_HALF_W
ARM_TOP, ARM_BOTTOM = -7.5, -24.5
LEG_CENTERS_X = (-21.2, -12.4, 12.4, 21.2)
LEG_W, LEG_H = 4.5, 7.3

NEW = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation
CUT = adsk.fusion.FeatureOperations.CutFeatureOperation


def cm(mm: float) -> float:
    return mm / 10.0


class Builder:
    """Thin helpers over the Fusion API, all taking millimetres."""

    def __init__(self, comp: adsk.fusion.Component) -> None:
        self.comp = comp
        self.features = comp.features

    def plane_at_z(self, z: float) -> adsk.fusion.ConstructionPlane:
        planes = self.comp.constructionPlanes
        plane_input = planes.createInput()
        plane_input.setByOffset(self.comp.xYConstructionPlane, adsk.core.ValueInput.createByReal(cm(z)))
        return planes.add(plane_input)

    def extrude(self, profile, depth: float, op, bodies=None):
        ext = self.features.extrudeFeatures
        ext_input = ext.createInput(profile, op)
        ext_input.setDistanceExtent(False, adsk.core.ValueInput.createByReal(cm(depth)))
        if bodies:
            ext_input.participantBodies = bodies
        return ext.add(ext_input)

    def box(self, x0, x1, y0, y1, z0, z1, op, bodies=None):
        sketch = self.comp.sketches.add(self.plane_at_z(z0))
        corner_a = sketch.modelToSketchSpace(adsk.core.Point3D.create(cm(x0), cm(y0), cm(z0)))
        corner_b = sketch.modelToSketchSpace(adsk.core.Point3D.create(cm(x1), cm(y1), cm(z0)))
        sketch.sketchCurves.sketchLines.addTwoPointRectangle(corner_a, corner_b)
        return self.extrude(sketch.profiles.item(0), z1 - z0, op, bodies)

    def prism_x(self, pts_zy: list, x0: float, x1: float, op, bodies=None):
        planes = self.comp.constructionPlanes
        plane_input = planes.createInput()
        plane_input.setByOffset(self.comp.yZConstructionPlane, adsk.core.ValueInput.createByReal(cm(x0)))
        sketch = self.comp.sketches.add(planes.add(plane_input))
        local = [sketch.modelToSketchSpace(adsk.core.Point3D.create(cm(x0), cm(y), cm(z))) for z, y in pts_zy]
        lines = sketch.sketchCurves.sketchLines
        for i, start in enumerate(local):
            lines.addByTwoPoints(start, local[(i + 1) % len(local)])
        return self.extrude(sketch.profiles.item(0), x1 - x0, op, bodies)

    def cylinder_z(self, x, y, diameter, z0, z1, op, bodies=None):
        sketch = self.comp.sketches.add(self.plane_at_z(z0))
        center = sketch.modelToSketchSpace(adsk.core.Point3D.create(cm(x), cm(y), cm(z0)))
        sketch.sketchCurves.sketchCircles.addByCenterRadius(center, cm(diameter / 2))
        return self.extrude(sketch.profiles.item(0), z1 - z0, op, bodies)


def build_silhouette(b: Builder) -> adsk.fusion.BRepBody:
    body = b.box(-BODY_HALF_W, BODY_HALF_W, BODY_BOTTOM, BODY_TOP, BACK_Z, FRONT_Z, NEW).bodies.item(0)
    body.name = "Body"
    for side in (-1, 1):
        x0, x1 = sorted((side * BODY_HALF_W, side * (BODY_HALF_W + ARM_REACH)))
        b.box(x0, x1, ARM_BOTTOM, ARM_TOP, BACK_Z, FRONT_Z, JOIN, [body])
    for leg_x in LEG_CENTERS_X:
        b.box(leg_x - LEG_W / 2, leg_x + LEG_W / 2, BODY_BOTTOM - LEG_H, BODY_BOTTOM, BACK_Z, FRONT_Z, JOIN, [body])
    return body


def hollow_out(b: Builder, body) -> None:
    b.box(-BODY_HALF_W + WALL, BODY_HALF_W - WALL, BODY_BOTTOM + WALL, BODY_TOP - WALL,
          BACK_INNER_Z, FRONT_INNER_Z, CUT, [body])
    for side in (-1, 1):
        x0, x1 = sorted((side * (BODY_HALF_W - WALL), side * (BODY_HALF_W + ARM_REACH - WALL)))
        b.box(x0, x1, ARM_BOTTOM + WALL, ARM_TOP - WALL, BACK_INNER_Z, FRONT_INNER_Z, CUT, [body])


# Fins across the top and bottom walls. Each one guides the board edge, then
# overlaps its front face by BOARD_LIP so the board stops against it.
GUIDE_FINS_X = (-22.0, -6.0, 10.0)
FIN_T = 1.6
BOARD_LIP = 1.0
BOARD_RIGHT_STOP = BOARD_CENTER[0] + BOARD_L / 2 + CLEARANCE + 0.1


def battery_frame() -> tuple:
    _, by = BOARD_CENTER
    half_bw = BATTERY_W / 2 + 0.3
    return CAVITY_LEFT, CAVITY_LEFT + BATTERY_L + 0.6, by - half_bw, by + half_bw


def fin_profile(sign: int) -> list:
    """(z, y) outline of a top (sign = 1) or bottom (sign = -1) guide fin."""
    _, by = BOARD_CENTER
    board_edge = by + sign * BOARD_W / 2
    battery_edge = by + sign * (BATTERY_W / 2 + 0.3)
    wall = (BODY_TOP if sign > 0 else BODY_BOTTOM) - sign * (WALL - 0.5)
    board_gap = board_edge + sign * (CLEARANCE + 0.1)
    lip = board_edge - sign * BOARD_LIP
    return [(BACK_INNER_Z, wall), (FRONT_INNER_Z, wall), (FRONT_INNER_Z, lip), (0.15, lip),
            (0.15, board_gap), (BOARD_BACK_Z, board_gap), (BOARD_BACK_Z, battery_edge),
            (BACK_INNER_Z, battery_edge)]


def add_internal_supports(b: Builder, body) -> None:
    for sign in (1, -1):
        for fin_x in GUIDE_FINS_X:
            b.prism_x(fin_profile(sign), fin_x - FIN_T / 2, fin_x + FIN_T / 2, JOIN, [body])
        add_end_stops(b, body, sign)
    add_switch_cradle(b, body)


def add_end_stops(b: Builder, body, sign: int) -> None:
    """Ribs past the right end of the board and battery, clear of the wiring path."""
    wall = (BODY_TOP if sign > 0 else BODY_BOTTOM) - sign * (WALL - 0.5)
    y0, y1 = sorted((wall, 6.5 if sign > 0 else -10.5))
    _, battery_right, _, _ = battery_frame()
    b.box(BOARD_RIGHT_STOP, BOARD_RIGHT_STOP + FIN_T, y0, y1, BOARD_BACK_Z, FRONT_INNER_Z, JOIN, [body])
    b.box(battery_right, battery_right + FIN_T, y0, y1, BACK_INNER_Z, BOARD_BACK_Z, JOIN, [body])


def add_switch_cradle(b: Builder, body) -> None:
    """Two rails hugging the SS12D00 body against the right wall (glue it in)."""
    inner = BODY_HALF_W - WALL
    for rail_y in (SWITCH_Y - 1.8 - 1.6, SWITCH_Y + 1.8):
        b.box(inner - 4.5, inner + 0.5, rail_y, rail_y + 1.6, SWITCH_Z - 4.5, SWITCH_Z + 4.5, JOIN, [body])


LID_RIGHT = 25.2  # opening wide enough for the battery plus its lead


def lid_frame() -> tuple:
    _, _, bottom, _ = battery_frame()
    return CAVITY_LEFT, LID_RIGHT, bottom, BACK_Z + LID_LIP_T


def cut_lid_slot(b: Builder, body) -> None:
    """T-slot on the back face; the lid slides out through the top."""
    left, right, bottom, lip_z = lid_frame()
    b.box(left, right, bottom, BODY_TOP + 5.0, BACK_Z - 1.0, lip_z, CUT, [body])
    b.box(left - LID_FLANGE, right + LID_FLANGE, bottom - LID_FLANGE, BODY_TOP + 5.0,
          lip_z, BACK_INNER_Z, CUT, [body])


def build_lid(b: Builder) -> adsk.fusion.BRepBody:
    left, right, bottom, lip_z = lid_frame()
    gap = CLEARANCE
    flange = b.box(left - LID_FLANGE + gap, right + LID_FLANGE - gap, bottom - LID_FLANGE + gap, BODY_TOP,
                   lip_z + gap, BACK_INNER_Z - gap, NEW)
    lid = flange.bodies.item(0)
    lid.name = "Lid"
    b.box(left + gap, right - gap, bottom + gap, BODY_TOP, BACK_Z, lip_z + gap, JOIN, [lid])
    return lid


def cut_ports(b: Builder, body) -> None:
    bx, _ = BOARD_CENTER
    usb_edge = bx - BOARD_L / 2
    shell_y, shell_z = USB_SHELL_HOLE
    b.box(-BODY_HALF_W - 1.0, usb_edge + 0.5, USB_Y - shell_y / 2, USB_Y + shell_y / 2,
          USB_CENTER_Z - shell_z / 2, USB_CENTER_Z + shell_z / 2, CUT, [body])
    recess_y, recess_z, recess_depth = USB_PLUG_RECESS
    b.box(-BODY_HALF_W - 1.0, -BODY_HALF_W + recess_depth, USB_Y - recess_y / 2, USB_Y + recess_y / 2,
          USB_CENTER_Z - recess_z / 2, USB_CENTER_Z + recess_z / 2, CUT, [body])
    # The receptacle overhangs the PCB edge; this groove lets it pass on insertion.
    b.box(usb_edge - 1.1, usb_edge + 0.5, USB_Y - 4.2, USB_Y + 4.2, BACK_INNER_Z, -2.5, CUT, [body])
    b.box(-WINDOW[0] / 2, WINDOW[0] / 2, -WINDOW[1] / 2, WINDOW[1] / 2, FRONT_INNER_Z - 1.0, FRONT_Z + 1.0, CUT, [body])
    b.cylinder_z(RST_XY[0], RST_XY[1], 1.8, FRONT_INNER_Z - 1.0, FRONT_Z + 1.0, CUT, [body])
    # Slide switch (SS12D00) lever slot on the right wall.
    b.box(BODY_HALF_W - WALL - 0.5, BODY_HALF_W + 5.0, SWITCH_Y - 1.0, SWITCH_Y + 1.0,
          SWITCH_Z - 2.25, SWITCH_Z + 2.25, CUT, [body])


def add_prg_button(b: Builder, body) -> None:
    """A small flush flexure over PRG: slotted on three sides, hinged at the top."""
    px, py = PRG_XY
    half, slot, tab_t = 3.5, 0.6, 1.4
    tab_inner_z = FRONT_Z - tab_t
    x0, x1, y0, y1 = px - half, px + half, py - half, py + half
    b.box(x0 - slot, x1 + slot, y0 - slot, y1, BUTTON_TOP + 0.2, tab_inner_z, CUT, [body])
    b.box(x0 - slot, x0, y0 - slot, y1, tab_inner_z - 0.1, FRONT_Z + 1.0, CUT, [body])
    b.box(x1, x1 + slot, y0 - slot, y1, tab_inner_z - 0.1, FRONT_Z + 1.0, CUT, [body])
    b.box(x0, x1, y0 - slot, y0, tab_inner_z - 0.1, FRONT_Z + 1.0, CUT, [body])
    b.cylinder_z(px, py, 2.4, BUTTON_TOP + 0.4, tab_inner_z + 0.2, JOIN, [body])


def build_references(b: Builder) -> None:
    """Envelopes of the board and battery, for visual fit checks only."""
    bx, by = BOARD_CENTER
    pcb = b.box(bx - BOARD_L / 2, bx + BOARD_L / 2, by - BOARD_W / 2, by + BOARD_W / 2, -PCB_T, 0, NEW)
    ref = pcb.bodies.item(0)
    ref.name = "BoardRef (do not print)"
    b.box(-17.0, 16.0, -11.3, 7.3, 0, OLED_STACK - 1.8, JOIN, [ref])
    b.box(-13.8, 12.8, -11.3, 7.3, OLED_STACK - 1.8, OLED_STACK, JOIN, [ref])
    b.box(bx - BOARD_L / 2 - 0.5, bx - BOARD_L / 2 + 5.0, USB_Y - 3.75, USB_Y + 3.75, 0, 2.8, JOIN, [ref])
    for x, y in (PRG_XY, RST_XY):
        b.box(x - 2.0, x + 2.0, y - 2.0, y + 2.0, 0, BUTTON_TOP, JOIN, [ref])
    for row_y in (by + BOARD_W / 2 - 1.27, by - BOARD_W / 2 + 1.27):
        b.box(-28.9, 16.9, row_y - 1.25, row_y + 1.25, -PCB_T - PIN_DEPTH, -PCB_T, JOIN, [ref])
    battery_z0 = BOARD_BACK_Z - 0.5 - BATTERY_T
    battery_x0 = CAVITY_LEFT + 0.3
    battery = b.box(battery_x0, battery_x0 + BATTERY_L, by - BATTERY_W / 2, by + BATTERY_W / 2,
                    battery_z0, battery_z0 + BATTERY_T, NEW).bodies.item(0)
    battery.name = "BatteryRef (do not print)"


def stand_on_desk(b: Builder, bodies: list) -> None:
    moves = b.features.moveFeatures
    collection = adsk.core.ObjectCollection.create()
    for body in bodies:
        collection.add(body)
    lift = moves.createInput2(collection)
    zero = adsk.core.ValueInput.createByReal(0)
    lift.defineAsTranslateXYZ(zero, adsk.core.ValueInput.createByReal(cm(LEG_H - BODY_BOTTOM)), zero, True)
    moves.add(lift)


def build(design: adsk.fusion.Design) -> None:
    root = design.rootComponent
    b = Builder(root)
    body = build_silhouette(b)
    hollow_out(b, body)
    add_internal_supports(b, body)
    cut_lid_slot(b, body)
    cut_ports(b, body)
    add_prg_button(b, body)
    build_lid(b)
    build_references(b)
    stand_on_desk(b, list(root.bRepBodies))
    root.isSketchFolderLightBulbOn = False
    root.isConstructionFolderLightBulbOn = False


def run(context) -> None:
    app = adsk.core.Application.get()
    app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
    design = adsk.fusion.Design.cast(app.activeProduct)
    design.designType = adsk.fusion.DesignTypes.ParametricDesignType
    build(design)
    for body in design.rootComponent.bRepBodies:
        box = body.boundingBox
        size = [round((box.maxPoint.x - box.minPoint.x) * 10, 1),
                round((box.maxPoint.y - box.minPoint.y) * 10, 1),
                round((box.maxPoint.z - box.minPoint.z) * 10, 1)]
        print(body.name, "size mm", size, "volume cm3", round(body.volume, 2))
