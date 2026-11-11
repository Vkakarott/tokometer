"""Run without Fusion: python3 -m unittest discover -s hardware/case -p 'test_*.py'."""

import ast
import json
from pathlib import Path
import unittest

import stand_v6_spec as spec

ROOT = Path(__file__).resolve().parent


class MeasuredValuesTests(unittest.TestCase):
    def test_specification_invariants(self):
        spec.validate()

    def test_display_measurements(self):
        self.assertEqual(spec.DISPLAY_PCB, (51.0, 35.5))
        self.assertEqual(spec.DISPLAY_PCB_THICKNESS, 1.1)
        self.assertEqual(spec.DISPLAY_STACK, 3.25)
        self.assertEqual(spec.DISPLAY_WINDOW, (40.0, 33.5))

    def test_opening_is_centred_on_the_pcb(self):
        self.assertEqual(spec.DISPLAY_WINDOW_MARGINS, (5.5, 1.0))
        self.assertEqual(spec.DISPLAY_BOARD_OFFSET, (0.0, 0.0))

    def test_display_holes_are_edge_to_edge_and_not_flagged(self):
        self.assertEqual(spec.DISPLAY_HOLE_DIAMETER, 2.0)
        self.assertEqual(spec.DISPLAY_HOLE_EDGE_TO_EDGE, (45.5, 29.7))
        self.assertEqual(spec.DISPLAY_HOLE_SPACING, (47.5, 31.7))
        self.assertFalse(hasattr(spec, "DISPLAY_HOLE_CONVENTION_ASSUMED"))
        edges = [(spec.DISPLAY_PCB[i] - spec.DISPLAY_HOLE_SPACING[i]) / 2 for i in (0, 1)]
        self.assertAlmostEqual(edges[0], 1.75)
        self.assertAlmostEqual(edges[1], 1.9)

    def test_posts_and_heads_stay_on_the_pcb(self):
        for axis in (0, 1):
            edge = (spec.DISPLAY_PCB[axis] - spec.DISPLAY_HOLE_SPACING[axis]) / 2
            self.assertLessEqual(spec.POST_DIAMETER / 2, edge + 1e-9)
            self.assertLessEqual(spec.SCREW_HEAD_DIAMETER / 2, edge + 1e-9)
        self.assertLess(spec.POST_PILOT_DIAMETER, spec.DISPLAY_HOLE_DIAMETER)

    def test_other_parts(self):
        self.assertEqual(spec.ESP_PCB, (18.0, 22.7))
        self.assertEqual(spec.ESP_PCB_THICKNESS, 1.0)
        self.assertEqual(spec.ESP_TOTAL_HEIGHT, 11.5)
        self.assertEqual(spec.BATTERY, (44.6, 25.0, 16.5))
        self.assertEqual(spec.USB_BOARD, (18.0, 20.0, 1.0))

    def test_usb_board_holes(self):
        self.assertEqual(spec.USB_HOLE_DIAMETER, 1.0)
        self.assertEqual(spec.USB_HOLES_EDGE_TO_EDGE, 15.9)
        self.assertAlmostEqual(spec.USB_HOLES_CENTRES, 16.9)
        positions = spec.usb_hole_positions()
        self.assertEqual(len(positions), 3)
        self.assertAlmostEqual(positions[1][0] - positions[0][0], 16.9)
        self.assertEqual(positions[2][0], 0.0)
        self.assertAlmostEqual(spec.usb_board_front_z() - positions[2][1], 3.4)


class FitTests(unittest.TestCase):
    def test_battery_is_wider_than_the_clear_width_and_rides_above_the_tabs(self):
        self.assertGreater(spec.BATTERY[0] / 2, spec.COVER_TAB_INNER_X)
        self.assertGreaterEqual(spec.BATTERY_BOTTOM - spec.COVER_TAB_TOP, 0.4)

    def test_esp_stays_in_front_of_the_battery(self):
        self.assertGreater(spec.ESP_BOTTOM + spec.ESP_TOTAL_HEIGHT, spec.BATTERY_BOTTOM)
        self.assertGreaterEqual(spec.ESP_REAR_Z - spec.battery_front_z(), 1.5)

    def test_usb_opening_covers_the_standard_plug(self):
        self.assertEqual(spec.USB_PLUG_COVER, (12.0, 6.5))
        self.assertEqual(spec.USB_OPENING_CLEARANCE, 0.5)
        self.assertEqual(spec.USB_OPENING, (13.0, 7.5))

    def test_usb_opening_sits_between_base_and_battery(self):
        low = spec.USB_CENTER_HEIGHT - spec.USB_OPENING[1] / 2
        high = spec.USB_CENTER_HEIGHT + spec.USB_OPENING[1] / 2
        self.assertGreater(low, spec.BASE_HEIGHT + 0.25)
        self.assertLess(high, spec.BATTERY_BOTTOM)

    def test_usb_connector_aligns_with_the_opening(self):
        self.assertAlmostEqual(spec.USB_BOARD_BOTTOM + spec.USB_BOARD[2]
                               + spec.USB_C_BODY[1] / 2, spec.USB_CENTER_HEIGHT)
        self.assertEqual(spec.USB_C_X, 0.0)

    def test_usb_connector_sits_near_the_outer_face_inside_the_opening(self):
        recess = spec.usb_tip_z() - spec.rear_outer_z(spec.USB_CENTER_HEIGHT)
        self.assertGreater(recess, 0.0)
        self.assertLessEqual(recess, 1.5)
        board_top = spec.USB_BOARD_BOTTOM + spec.USB_BOARD[2]
        gap = spec.USB_BOARD_REAR_Z - spec.rear_wall_inner_z(board_top)
        self.assertGreaterEqual(gap, spec.USB_BOARD_WALL_GAP_MIN)
        self.assertGreaterEqual((spec.USB_OPENING[0] - spec.USB_C_BODY[0]) / 2, 1.5)
        self.assertGreaterEqual((spec.USB_OPENING[1] - spec.USB_C_BODY[1]) / 2, 1.5)

    def test_unknowns_remain_explicit(self):
        self.assertEqual(len(spec.MISSING_MEASUREMENTS), 1)
        self.assertIn("printed sample", spec.MISSING_MEASUREMENTS[0])
        text = (ROOT / "stand_v6_spec.py").read_text()
        self.assertIn("NOT a measurement", text)
        self.assertIn("STANDARD", text)

    def test_generator_syntax(self):
        ast.parse((ROOT / "stand_v6.py").read_text())


class NativeValidationReportTests(unittest.TestCase):
    def setUp(self):
        path = ROOT / "v6" / "validation.json"
        self.assertTrue(path.exists(), "Generate and validate the model in Fusion first")
        self.report = json.loads(path.read_text())

    def test_native_checks_passed(self):
        self.assertEqual(self.report["timeline_issues"], [])
        self.assertEqual(self.report["unexpected_interferences"], [])
        self.assertEqual(self.report["opening_test"]["collisions"], [])
        self.assertGreaterEqual(len(self.report["opening_test"]["sampled_travel_mm"]), 85)
        self.assertEqual(len(self.report["parts"]), 20)

    def test_report_matches_spec(self):
        layout = self.report["layout"]
        self.assertEqual(layout["display_opening_mm"], list(spec.DISPLAY_WINDOW))
        self.assertEqual(layout["usb_opening_mm"], list(spec.USB_OPENING))
        self.assertEqual(layout["usb_c_center_height_mm"], spec.USB_CENTER_HEIGHT)
        self.assertEqual(layout["measured_by_user"]["display_hole_centres_mm"],
                         list(spec.DISPLAY_HOLE_SPACING))
        self.assertEqual(self.report["battery_envelope_measured_mm"], list(spec.BATTERY))
        self.assertEqual(self.report["esp_pcb_measured_mm"], list(spec.ESP_PCB))
        self.assertEqual(self.report["usb_board_measured_mm"], list(spec.USB_BOARD))
        self.assertEqual(self.report["missing_measurements"], list(spec.MISSING_MEASUREMENTS))

    def test_nothing_is_claimed_as_verified(self):
        self.assertFalse(self.report["physical_fit_verified"])
        self.assertFalse(self.report["print_meshes_exported"])


if __name__ == "__main__":
    unittest.main()
