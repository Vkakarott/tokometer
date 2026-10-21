"""Run without Fusion: python3 -m unittest discover -s hardware/case -p 'test_*.py'."""

import ast
import json
import math
from pathlib import Path
import unittest

import stand_v5_spec as spec

ROOT = Path(__file__).resolve().parent


class AssemblySpecificationTests(unittest.TestCase):
    def test_specification_invariants(self):
        spec.validate()

    def test_measured_footprints_are_not_scaled(self):
        self.assertEqual(spec.DISPLAY_WINDOW, (40.0, 31.8))
        self.assertEqual(spec.DISPLAY_PCB, (48.9, 33.0))
        self.assertEqual(spec.ESP_PCB, (16.0, 20.7))

    def test_six_distinct_bottom_fixings(self):
        positions = spec.SHELL_SCREWS + spec.FRONT_SCREWS
        self.assertEqual(len(set(positions)), 6)
        for index, (x, z) in enumerate(positions):
            for other_x, other_z in positions[index + 1:]:
                self.assertGreater(math.hypot(other_x - x, other_z - z), spec.HEAD_POCKET_DIAMETER)

    def test_guide_and_nut_slot_are_separated_vertically(self):
        groove_top = spec.BASE_HEIGHT + spec.GUIDE_HEIGHT + spec.SLIDE_CLEARANCE
        self.assertGreaterEqual(spec.NUT_SEAT_HEIGHT - groove_top, 1.0)
        self.assertGreater(spec.NUT_SLOT_WIDTH, spec.NUT_ACROSS_FLATS)
        self.assertLess(spec.NUT_SLOT_WIDTH, spec.NUT_ACROSS_FLATS * 2 / math.sqrt(3))

    def test_bottom_screw_reaches_full_nut_thickness(self):
        screw_tip = spec.HEAD_POCKET_DEPTH + spec.BOTTOM_SCREW_LENGTH
        self.assertGreaterEqual(screw_tip, spec.NUT_SEAT_HEIGHT + spec.NUT_THICKNESS)
        self.assertGreater(spec.HEAD_POCKET_DEPTH, 3.0)
        self.assertGreater(spec.HEAD_POCKET_DIAMETER, 5.5)

    def test_unknown_measurements_remain_explicit(self):
        self.assertEqual(len(spec.MISSING_MEASUREMENTS), 4)
        self.assertIn("NOT measurements", (ROOT / "stand_v5_spec.py").read_text())

    def test_generator_syntax(self):
        ast.parse((ROOT / "stand_v5.py").read_text())

    def test_native_validation_report(self):
        path = ROOT / "v5" / "validation.json"
        self.assertTrue(path.exists(), "Generate and validate the model in Fusion first")
        report = json.loads(path.read_text())
        self.assertEqual(report["timeline_issues"], [])
        self.assertEqual(report["unexpected_interferences"], [])
        self.assertEqual(report["shell_screw_count"], 4)
        self.assertEqual(report["front_screw_count"], 2)
        self.assertEqual(report["display_window_mm"], list(spec.DISPLAY_WINDOW))
        self.assertEqual(report["opening_test"]["collisions"], [])
        self.assertGreaterEqual(len(report["opening_test"]["sampled_travel_mm"]), 85)
        self.assertFalse(report["physical_fit_verified"])
        self.assertFalse(report["print_meshes_exported"])
        self.assertEqual(len(report["parts"]), 20)


if __name__ == "__main__":
    unittest.main()
