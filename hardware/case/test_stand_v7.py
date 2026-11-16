"""Run without Fusion: python3 -m unittest discover -s hardware/case -p 'test_*.py'."""

import ast
import math
from pathlib import Path
import unittest
from unittest import mock

import stand_v6_spec as v6
import stand_v7_spec as v7

ROOT = Path(__file__).resolve().parent
V7_ONLY = ("PENDING_MEASUREMENTS", "USB_OPENING_V6", "USB_OPENING_CENTER",
           "USB_OPENING_CORNER_RADIUS", "USB_OPENING_WIDTH", "USB_OPENING_HEIGHT",
           "USB_OPENING", "OMITTED_REFERENCES", "UNVERIFIED_WITHOUT_REFERENCES",
           "ESP_PCB", "ESP_PCB_THICKNESS", "ESP_USB_EDGE", "ESP_COMPONENTS_FACE",
           "ESP_COMPONENT_HEIGHT", "ESP_USB_C_BODY", "ESP_USB_C_PROTRUSION",
           "ESP_WALL_GAP", "ESP_X", "ESP_PCB_TOP", "ESP_PCB_BOTTOM", "ESP_FLOOR_GAP",
           "ESP_USB_C_BOTTOM", "ESP_COMPONENT_BOTTOM", "ESP_USB_C_LENGTH",
           "BASE_SUPPORTS_REMOVED", "BASE_FEATURES_KEPT", "BASE_FEATURES_FOR_USER",
           "FRONT_TAB_NOTCH_CLEARANCE", "FRONT_TAB_REAR_Z", "FRONT_TAB_TOP",
           "ESP_HOLE_DIAMETER", "ESP_HOLE_INNER_EDGES_X", "ESP_HOLE_CENTRES_X",
           "ESP_HOLE_EDGE_TO_REAR", "ESP_HOLE_EDGE_TO_FRONT", "ESP_HOLE_CENTRE_FROM_REAR",
           "ESP_HOLE_CENTRE_FROM_FRONT", "ESP_SCREW",
           "ESP_STANDOFF_PILOT_DIAMETER", "ESP_STANDOFF_PILOT_DEPTH",
           "ESP_STANDOFF_DIAMETER", "ESP_STANDOFF_CORNERS",
           "ESP_STANDOFF_MIN_FREE_GAP_X", "ESP_STANDOFF_MIN_WALL",
           "ESP_FRONT_FREE_STRIP", "ESP_FRONT_PAD_WIDTH", "ESP_FRONT_PAD_INSET",
           "ESP_FRONT_PAD_DEPTH", "REAR_SHOULDER_HEIGHT", "REAR_SHOULDER_ANGLE",
           "MISSING_MEASUREMENTS")
# v6 values that v7 drops or replaces (rear cover opening, the old ESP32-C3 Mini)
V6_REPLACED = ("USB_OPENING", "ESP_PCB", "ESP_PCB_THICKNESS", "ESP_TOTAL_HEIGHT",
               "ESP_BOTTOM", "ESP_REAR_Z", "ESP_RIM_CLEARANCE", "ESP_RIM_WALL",
               "ESP_RIM_HEIGHT", "MISSING_MEASUREMENTS")


def public_values(module):
    return {name: value for name, value in vars(module).items()
            if name.isupper() and not name.startswith("_")}


class StartingPointTests(unittest.TestCase):
    def test_specification_invariants(self):
        v7.validate()

    def test_v7_starts_from_the_v6_values(self):
        values = public_values(v7)
        for name in V7_ONLY:
            values.pop(name)
        reference = public_values(v6)
        for name in V6_REPLACED:
            reference.pop(name)
        self.assertEqual(values, reference)

    def test_corrections_wait_for_the_users_values(self):
        pending = v7.PENDING_MEASUREMENTS
        self.assertEqual(len(pending), 8)
        self.assertIn("all MEASURED", pending[0])
        self.assertIn("option A", pending[1])
        self.assertIn("pad", pending[1])
        self.assertIn("MEASURE", pending[1])
        self.assertIn("ESP32 devkit (upside down)", pending[2])
        self.assertIn("NOTE", pending[3])
        for item in ("notched", "option A", "Sacrificed", "screw hole", "nut pocket",
                     "not modelled"):
            self.assertIn(item, pending[3])
        for gone in ("battery ribs", "USB-C board stop"):
            self.assertNotIn(gone, pending[3])
        self.assertIn("Separate USB-C board", pending[4])
        self.assertIn("guide rails", pending[5])
        self.assertIn("locating pins", pending[5])
        self.assertIn("NOTE on v6", pending[6])
        self.assertIn("0.03", pending[6])
        self.assertIn("WARNING", pending[7])
        self.assertIn("9.3", pending[7])
        self.assertIn("9.0", pending[7])


class RearOpeningTests(unittest.TestCase):
    def test_centre_is_the_v6_one(self):
        self.assertEqual(v7.USB_OPENING_CENTER, (v6.USB_C_X, v6.USB_CENTER_HEIGHT))
        self.assertEqual(v7.USB_OPENING_V6, v6.USB_OPENING)
        self.assertEqual(v7.USB_OPENING_CORNER_RADIUS, 1.5)

    def test_size_is_the_users_value(self):
        self.assertEqual(v7.USB_OPENING, (9.0, 3.5))
        self.assertEqual(v7.usb_opening_bounds(), (-4.5, 4.5, 12.25, 15.75))

    def test_reduction_is_symmetric_about_the_v6_centre(self):
        left, right, bottom, top = v7.usb_opening_bounds()
        old = (-6.5, 6.5, 10.25, 17.75)  # v6 bounds
        self.assertAlmostEqual(left - old[0], old[1] - right)  # 2.0 per side
        self.assertAlmostEqual(bottom - old[2], old[3] - top)  # 2.0 per side

    def test_only_the_plug_metal_has_to_pass(self):
        self.assertGreater(v7.USB_OPENING[0], v7.USB_PLUG_METAL[0])
        self.assertGreater(v7.USB_OPENING[1], v7.USB_PLUG_METAL[1])
        self.assertLess(v7.USB_OPENING[0], v7.USB_PLUG_COVER[0])  # intentional

    def test_receptacle_overlap_is_a_warning_not_a_blocker(self):
        width, height = v7.receptacle_clearance_per_side()
        self.assertAlmostEqual(width, -0.15)
        self.assertAlmostEqual(height, 0.15)
        v7.validate()  # does not block generation

    def test_a_smaller_opening_shrinks_symmetrically_about_the_centre(self):
        # illustrative size only, to exercise the rule; not a design value
        with mock.patch.object(v7, "USB_OPENING", (10.0, 5.0)):
            left, right, bottom, top = v7.usb_opening_bounds()
            v7.validate_rear_opening()
        self.assertEqual((left + right) / 2, v7.USB_OPENING_CENTER[0])
        self.assertEqual((bottom + top) / 2, v7.USB_OPENING_CENTER[1])
        self.assertEqual((right - left, top - bottom), (10.0, 5.0))

    def test_opening_cannot_grow_or_lose_its_corners(self):
        for size in ((13.5, 7.5), (13.0, 8.0), (13.0, 3.0)):
            with mock.patch.object(v7, "USB_OPENING", size):
                with self.assertRaises(AssertionError):
                    v7.validate_rear_opening()


class EspBoardTests(unittest.TestCase):
    def test_measured_board_lying_flat_upside_down(self):
        self.assertEqual(v7.ESP_PCB, (28.5, 51.7))  # 28.5 across (x), 51.7 front to back (z)
        self.assertEqual(v7.ESP_PCB_THICKNESS, 1.5)
        self.assertEqual(v7.ESP_USB_EDGE, "rear")
        self.assertEqual(v7.ESP_COMPONENTS_FACE, "down")
        self.assertEqual(v7.ESP_COMPONENT_HEIGHT, v7.USB_C_BODY[1])
        self.assertAlmostEqual(v7.esp_front_z() - v7.esp_rear_z(), 51.7)

    def test_usb_c_under_the_pcb_fills_the_opening(self):
        centre_y = (v7.ESP_USB_C_BOTTOM + v7.ESP_PCB_BOTTOM) / 2
        self.assertEqual((v7.ESP_X, centre_y), v7.USB_OPENING_CENTER)
        self.assertAlmostEqual(v7.ESP_USB_C_BOTTOM, 12.4)
        self.assertAlmostEqual(v7.ESP_PCB_BOTTOM, 15.6)
        self.assertAlmostEqual(v7.ESP_PCB_TOP, 17.1)
        bottom, top = v7.usb_opening_bounds()[2:]
        self.assertTrue(bottom < v7.ESP_USB_C_BOTTOM and v7.ESP_PCB_BOTTOM < top)

    def test_pcb_sits_above_the_opening(self):
        self.assertGreater(v7.ESP_PCB_BOTTOM, v7.USB_OPENING_CENTER[1])
        self.assertLess(v7.usb_opening_bounds()[3] - v7.ESP_PCB_BOTTOM, 0.2)

    def test_components_hang_under_the_pcb_towards_the_floor(self):
        self.assertAlmostEqual(v7.ESP_COMPONENT_BOTTOM, 12.4)
        self.assertAlmostEqual(v7.ESP_FLOOR_GAP, 3.4)

    def test_rear_edge_and_usb_tip(self):
        self.assertAlmostEqual(v7.wall_normal_gap(v7.esp_rear_z(), v7.ESP_PCB_TOP), 0.3)
        self.assertGreater(v7.esp_usb_tip_z(), v7.rear_outer_z(v7.USB_OPENING_CENTER[1]))
        self.assertLess(v7.esp_usb_tip_z(), v7.rear_wall_inner_z(v7.USB_OPENING_CENTER[1]))

    def test_pcb_is_exactly_1_5_thick(self):
        self.assertAlmostEqual(v7.ESP_PCB_TOP - v7.ESP_PCB_BOTTOM, 1.5)

    def test_usb_c_body_runs_under_the_pcb(self):
        tip, back = v7.esp_usb_c_z()
        self.assertAlmostEqual(back - tip, 7.35)
        self.assertAlmostEqual(v7.esp_rear_z() - tip, 1.0)
        self.assertLess(back, v7.esp_front_z())

    def test_standing_values_are_gone(self):
        for name in ("ESP_REAR_Z", "ESP_TOTAL_HEIGHT", "ESP_BOTTOM", "ESP_DEPTH",
                     "esp_envelope_yz", "inner_rear_shoulder_y"):
            self.assertFalse(hasattr(v7, name), name)


class FrontTabNotchTests(unittest.TestCase):
    def test_notch_is_the_one_the_user_chose(self):
        notch = [round(v, 3) for v in v7.front_tab_notch()]
        self.assertEqual(notch, [11.5, 14.55, -14.5, -10.389, 15.3, 18.0])

    def test_notch_clears_the_pcb_by_0_3(self):
        x_in, x_out, z_rear, z_front, y_bottom, y_top = v7.front_tab_notch()
        self.assertAlmostEqual(x_out - v7.ESP_PCB[0] / 2, 0.3)
        self.assertAlmostEqual(z_front - v7.esp_front_z(), 0.3)
        self.assertAlmostEqual(v7.ESP_PCB_BOTTOM - y_bottom, 0.3)
        self.assertEqual(y_top, 18.0)  # open on top: no thin roof

    def test_sacrifices_are_bounded(self):
        x_in, x_out, z_rear, z_front, y_bottom, _ = v7.front_tab_notch()
        screw_x, screw_z = v7.FRONT_SCREWS[1]
        hole_edge = screw_x - v7.SCREW_CLEARANCE / 2  # 14.3
        self.assertLessEqual(x_out - hole_edge, 0.25 + 1e-9)  # sliver into the hole
        self.assertLess(z_front, screw_z)  # never reaches the screw axis
        self.assertGreater(y_bottom, v7.NUT_SEAT_HEIGHT)  # the nut floor is untouched


class EspStandoffTests(unittest.TestCase):
    def test_holes_are_the_assumed_corner_positions(self):
        holes = [tuple(round(v, 3) for v in h) for h in v7.esp_hole_positions()]
        self.assertEqual(holes, [(-11.5, -59.989), (11.5, -59.989),
                                 (-11.5, -12.889), (11.5, -12.889)])
        self.assertEqual(v7.ESP_HOLE_DIAMETER, 3.0)

    def test_x_spacing_is_the_measured_one(self):
        self.assertEqual(v7.ESP_HOLE_INNER_EDGES_X, 20.0)
        self.assertEqual(v7.ESP_HOLE_CENTRES_X, 23.0)
        rear_left, rear_right = v7.esp_hole_positions()[:2]
        self.assertAlmostEqual(rear_right[0] - rear_left[0], 23.0)
        self.assertIn("all MEASURED", v7.PENDING_MEASUREMENTS[0])
        self.assertNotIn("assumes", v7.PENDING_MEASUREMENTS[0])

    def test_z_positions_are_the_measured_ones(self):
        self.assertEqual((v7.ESP_HOLE_EDGE_TO_REAR, v7.ESP_HOLE_EDGE_TO_FRONT), (0.9, 0.7))
        self.assertAlmostEqual(v7.ESP_HOLE_CENTRE_FROM_REAR, 2.4)
        self.assertAlmostEqual(v7.ESP_HOLE_CENTRE_FROM_FRONT, 2.2)
        holes = v7.esp_hole_positions()
        self.assertAlmostEqual(holes[0][1] - v7.esp_rear_z(), 2.4)
        self.assertAlmostEqual(v7.esp_front_z() - holes[2][1], 2.2)
        self.assertAlmostEqual(holes[2][1] - holes[0][1], 47.1)

    def test_centre_reading_would_be_impossible(self):
        # 0.9 / 0.7 to the centre would put a 1.5 radius hole through the PCB edge
        self.assertLess(v7.ESP_HOLE_EDGE_TO_REAR, v7.ESP_HOLE_DIAMETER / 2)
        self.assertLess(v7.ESP_HOLE_EDGE_TO_FRONT, v7.ESP_HOLE_DIAMETER / 2)

    def test_only_the_rear_pair_is_built(self):
        self.assertEqual(v7.esp_standoff_positions(), v7.esp_hole_positions()[:2])
        self.assertTrue(v7.esp_front_holes_over_front_tabs())

    def test_standoffs_carry_the_pcb_and_clear_the_wall(self):
        r = v7.ESP_STANDOFF_DIAMETER / 2
        for x, z in v7.esp_standoff_positions():
            gap = v7.wall_normal_gap(z - r, v7.ESP_PCB_BOTTOM)
            self.assertGreaterEqual(gap, 0.3)
            self.assertAlmostEqual(gap, 0.651, places=3)  # Fusion measured 0.6508
        self.assertLessEqual(r, v7.ESP_HOLE_CENTRE_FROM_REAR)
        self.assertLessEqual(r, v7.ESP_PCB[0] / 2 - v7.ESP_HOLE_CENTRES_X / 2)

    def test_free_gap_between_standoffs(self):
        self.assertEqual(v7.ESP_STANDOFF_MIN_FREE_GAP_X, 18.0)
        self.assertEqual(v7.ESP_STANDOFF_DIAMETER, 4.2)
        self.assertAlmostEqual(v7.esp_standoff_free_gap_x(), 18.8)
        self.assertGreaterEqual(v7.esp_standoff_free_gap_x(), 18.0)

    def test_pilot_wall_is_kept(self):
        wall = (v7.ESP_STANDOFF_DIAMETER - v7.ESP_STANDOFF_PILOT_DIAMETER) / 2
        self.assertAlmostEqual(wall, 1.0)

    def test_a_too_narrow_gap_blocks_the_build(self):
        with mock.patch.object(v7, "ESP_STANDOFF_DIAMETER", 5.5):  # gap 17.5
            self.assertLess(v7.esp_standoff_free_gap_x(), 18.0)
            with self.assertRaises(AssertionError):
                v7.validate_esp_standoffs()
        source = (ROOT / "stand_v7.py").read_text()
        self.assertIn("if gap < s.ESP_STANDOFF_MIN_FREE_GAP_X:", source)

    def test_front_pad_position(self):
        pad = [round(v, 3) for v in v7.esp_front_pad()]
        self.assertEqual(pad, [-4.0, 4.0, -12.089, -10.889])
        v7.validate_esp_front_pad()

    def test_front_pad_clears_holes_tabs_and_components(self):
        x0, x1, z0, z1 = v7.esp_front_pad()
        self.assertLessEqual(x1, 10.0 - 0.3)  # front hole edge at |x| 10.0
        self.assertLessEqual(x1, 11.5 - 0.3)  # front frame tab
        self.assertAlmostEqual(v7.esp_front_z() - z0, 1.4)
        self.assertLessEqual(v7.esp_front_z() - z0, v7.ESP_FRONT_FREE_STRIP - 0.3)

    def test_screw_choice(self):
        self.assertIn("M2.5", v7.ESP_SCREW)
        self.assertEqual(v7.ESP_STANDOFF_PILOT_DIAMETER, 2.2)
        self.assertLess(v7.ESP_STANDOFF_PILOT_DEPTH, v7.ESP_PCB_BOTTOM)


class RearWallProfileTests(unittest.TestCase):
    """rear_wall_inner_z must follow the cavity that stand_v5.build_shell cuts."""

    def v5_inner_line(self):
        # The cavity polygon in stand_v5.build_shell, recomputed from its own formulas.
        source = (ROOT / "stand_v5.py").read_text()
        self.assertIn("(g.BASE_T - 1, -g.BASE_D + g.WALL)]", source)
        self.assertIn("inner_shoulder_y = shoulder_y - (upper_offset - g.WALL) / "
                      "(upper_slope - slope)", source)
        self.assertIn("inner_shoulder_z = -g.BASE_D + g.WALL + slope * "
                      "(inner_shoulder_y - g.BASE_T)", source)
        stand_v3 = (ROOT / "stand_v3.py").read_text()
        self.assertIn("REAR_SHOULDER_HEIGHT = 10.0", stand_v3)
        self.assertIn("REAR_SHOULDER_ANGLE = 26.0", stand_v3)
        base_t, body_h, base_d, wall = 9.0, 75.0, 65.0, 2.0
        slope = v6.REAR_LEAN / body_h
        upper = math.tan(math.radians(26.0))
        shoulder_y = base_t + body_h - 10.0
        y1 = shoulder_y - (wall * math.sqrt(1 + upper ** 2) - wall) / (upper - slope)
        z1 = -base_d + wall + slope * (y1 - base_t)
        return (base_t - 1, -base_d + wall), (y1, z1)

    def test_formula_matches_the_v5_profile(self):
        (y0, z0), (y1, z1) = self.v5_inner_line()
        for y in (8.0, 12.4, 15.6, 17.1, 35.0, y1):
            expected = z0 + (y - y0) * (z1 - z0) / (y1 - y0)
            self.assertAlmostEqual(v7.rear_wall_inner_z(y), expected, places=9)

    def test_old_v6_formula_was_about_0_03_too_far_back(self):
        for y in (12.4, 15.6, 17.1):
            self.assertAlmostEqual(v7.rear_wall_inner_z(y) - v6.rear_wall_inner_z(y),
                                   0.03, delta=0.004)

    def test_matches_the_gap_measured_in_fusion(self):
        # Fusion, 2026-11-16: 0.27 mm from the old PCB position (rear edge at
        # -62.4192) to the cover; the corrected formula predicts the same.
        self.assertAlmostEqual(-62.4192 - v7.rear_wall_inner_z(v7.ESP_PCB_TOP), 0.27, places=2)

    def test_pcb_now_keeps_0_3_from_the_wall(self):
        # perpendicular to the leaning wall, as Fusion measures it
        self.assertAlmostEqual(v7.wall_normal_gap(v7.esp_rear_z(), v7.ESP_PCB_TOP), 0.3)
        self.assertGreater(v7.esp_rear_z() - v7.rear_wall_inner_z(v7.ESP_PCB_TOP), 0.3)


class GeneratorTests(unittest.TestCase):
    def setUp(self):
        self.tree = ast.parse((ROOT / "stand_v7.py").read_text())

    def defined(self):
        return {node.name for node in self.tree.body if isinstance(node, ast.FunctionDef)}

    def test_front_frame_and_cover_are_not_redefined(self):
        # they come from the v5/v6 builders, so they stay identical to v6
        for name in ("build_front", "build_shell", "display_posts", "display_references"):
            self.assertNotIn(name, self.defined())

    def test_rear_opening_is_configured_from_the_spec(self):
        self.assertIn("configure_rear_opening", self.defined())

    def test_battery_and_usb_board_references_are_not_built(self):
        self.assertEqual(v7.OMITTED_REFERENCES,
                         ("reference_battery_envelope", "reference_usb_c_board"))
        source = (ROOT / "stand_v7.py").read_text()
        self.assertNotIn("battery_reference(", source)
        self.assertNotIn("usb_board_reference(", source)
        self.assertNotIn("v6.esp_reference(", source)
        self.assertIn("KNOWN_ESP_PARTNERS", source)

    def test_esp_is_the_pcb_only(self):
        source = (ROOT / "stand_v7.py").read_text()
        self.assertIn('ESP_NAMES = (ESP_PCB_NAME,)', source)
        for gone in ("reference_esp32_components_under_pcb", "reference_esp32_usb_c",
                     "reference_esp32_devkit_flat"):
            self.assertNotIn(gone, source)

    def test_no_esp_overlap_is_accepted_any_more(self):
        source = (ROOT / "stand_v7.py").read_text()
        self.assertIn("KNOWN_ESP_PARTNERS = ()", source)

    def test_front_tabs_are_notched_in_the_generator(self):
        source = (ROOT / "stand_v7.py").read_text()
        self.assertIn("notch_front_tabs", self.defined())
        self.assertIn("notch_front_tabs(g, s, builder, front)", source)

    def test_what_is_no_longer_verified_is_recorded(self):
        text = " ".join(v7.UNVERIFIED_WITHOUT_REFERENCES)
        self.assertEqual(len(v7.UNVERIFIED_WITHOUT_REFERENCES), 3)
        for word in ("Battery", "USB-C board", "no supports", "ESP components",
                     "front M3 nuts", "ESP USB-C", "0.15 mm"):
            self.assertIn(word, text)

    def test_base_has_no_electronics_supports(self):
        source = (ROOT / "stand_v7.py").read_text()
        for call in ("battery_cradle(", "usb_board_mount(", "usb_board_stop(",
                     "build_base_interior("):
            self.assertNotIn(call, source)
        self.assertIn("v5.build_base_and_joints(", source)  # screw joints and v5 guides stay
        self.assertNotIn('"v7_service_base",', source.split("KNOWN_ESP_PARTNERS")[1].split(")")[0])

    def test_front_pad_is_built_without_a_screw(self):
        source = (ROOT / "stand_v7.py").read_text()
        self.assertIn("esp_front_pad(g, s, builder, base)", source)
        body = source.split("def esp_front_pad(")[1].split("\ndef ")[0]
        self.assertNotIn("PILOT", body)

    def test_esp_standoffs_are_built_on_the_base(self):
        source = (ROOT / "stand_v7.py").read_text()
        self.assertIn("esp_standoffs", self.defined())
        self.assertIn("esp_standoffs(g, s, builder, base)", source)

    def test_base_lists_are_recorded(self):
        self.assertEqual(len(v7.BASE_SUPPORTS_REMOVED), 5)
        text = " ".join(v7.BASE_FEATURES_FOR_USER)
        for item in ("guide rails", "locating pins", "keyboard-port"):
            self.assertIn(item, text)
        self.assertIn("screw clearance holes", v7.BASE_FEATURES_KEPT[0])

    def test_esp_cradle_is_removed(self):
        source = (ROOT / "stand_v7.py").read_text()
        self.assertNotIn("esp_cradle(", source)


if __name__ == "__main__":
    unittest.main()
