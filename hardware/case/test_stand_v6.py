"""Run without Fusion: python3 -m unittest discover -s hardware/case -p 'test_*.py'."""

import ast
import json
from pathlib import Path
import unittest

import stand_v6_spec as spec

ROOT = Path(__file__).resolve().parent


class LayoutSpecificationTests(unittest.TestCase):
    def test_specification_invariants(self):
        spec.validate()

    def test_measured_footprints_are_not_scaled(self):
        self.assertEqual(spec.DISPLAY_WINDOW, (38.5, 31.0))
        self.assertEqual(spec.DISPLAY_PCB, (48.87, 33.0))
        self.assertEqual(spec.ESP_PCB, (16.0, 20.0))
        self.assertEqual(spec.BATTERY, (42.0, 24.0, 16.0))

    def test_window_is_the_case_opening_not_pending(self):
        self.assertFalse(hasattr(spec, "DISPLAY_WINDOW_PENDING"))
        self.assertIn("CONFIRMED", (ROOT / "stand_v6_spec.py").read_text())

    def test_usb_opening_is_aligned_with_battery_usb_c(self):
        low = spec.USB_CENTER_HEIGHT - spec.USB_OPENING[1] / 2
        high = spec.USB_CENTER_HEIGHT + spec.USB_OPENING[1] / 2
        self.assertGreater(spec.USB_OPENING[0], spec.USB_C_BODY[0])
        self.assertLess(spec.BATTERY_BOTTOM, spec.USB_CENTER_HEIGHT)
        self.assertGreater(low, spec.BASE_HEIGHT)
        self.assertLess(high, spec.battery_top())

    def test_battery_clears_cover_tab_sweep(self):
        # cover tabs start at |x| = 22 and slide the full length on removal
        self.assertLess(spec.BATTERY[0] / 2, 22.0)

    def test_usb_c_tip_stays_inside_the_rear_wall(self):
        tip = spec.BATTERY_REAR_Z - spec.USB_C_PROTRUSION
        self.assertGreaterEqual(tip - spec.rear_wall_inner_z(spec.battery_top()), 0.3 - 1e-9)

    def test_esp_sits_in_front_of_the_battery(self):
        self.assertGreater(spec.ESP_BOARD_REAR, spec.battery_front_z())

    def test_unknown_measurements_remain_explicit(self):
        self.assertEqual(len(spec.MISSING_MEASUREMENTS), 5)
        text = (ROOT / "stand_v6_spec.py").read_text()
        self.assertIn("NOT a measurement", text)
        self.assertIn("ESTIMATED", text)

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
        self.assertEqual(layout["display_window_mm"], list(spec.DISPLAY_WINDOW))
        self.assertTrue(layout["display_window_is_case_opening"])
        self.assertEqual(layout["usb_c_center_height_mm"], spec.USB_CENTER_HEIGHT)
        self.assertEqual(self.report["battery_envelope_measured_mm"], list(spec.BATTERY))

    def test_nothing_is_claimed_as_verified(self):
        self.assertFalse(self.report["physical_fit_verified"])
        self.assertFalse(self.report["print_meshes_exported"])


if __name__ == "__main__":
    unittest.main()
