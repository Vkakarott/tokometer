"""Run without Fusion: python3 -m unittest discover -s hardware/case -p 'test_*.py'."""

import json
from pathlib import Path
import unittest

import stl_v6
import stl_v7
from test_stl_v6 import cube

ROOT = Path(__file__).resolve().parent
STL = ROOT / "stl"


class OrientationTests(unittest.TestCase):
    def test_v7_parts_print_like_their_v6_counterparts(self):
        for name in stl_v7.PARTS:
            v6_name = stl_v7.V6_EQUIVALENT[name]
            self.assertEqual(stl_v7.ORIENTATION[name], stl_v6.ORIENTATION[v6_name])
            point = (1.0, 2.0, 3.0)
            self.assertEqual(stl_v7.rotate_point(name, point),
                             stl_v6.rotate_point(v6_name, point))

    def test_rotations_do_not_mirror(self):
        for name in stl_v7.PARTS:
            turned = [tuple(stl_v7.rotate_point(name, p) for p in t) for t in cube()]
            self.assertAlmostEqual(stl_v6.signed_volume(turned), 8.0, places=6)

    def test_orient_puts_the_part_on_the_bed_and_centres_it(self):
        for name in stl_v7.PARTS:
            low, high = stl_v6.bounds(stl_v7.orient(name, cube(4.0)))
            self.assertAlmostEqual(low[2], 0.0)
            self.assertAlmostEqual(low[0] + high[0], 0.0)
            self.assertAlmostEqual(low[1] + high[1], 0.0)


class PrintableFilesTests(unittest.TestCase):
    def setUp(self):
        self.report = json.loads((STL / stl_v7.REPORT).read_text())

    def test_three_closed_meshes_on_the_bed(self):
        self.assertEqual([p["name"] for p in self.report["parts"]], list(stl_v7.PARTS))
        for part in self.report["parts"]:
            self.assertTrue(part["closed_manifold"], part["name"])
            self.assertEqual(part["bbox_min_mm"][2], 0.0)
            self.assertGreater(part["bed_contact_area_mm2"], 1000.0)
            triangles = stl_v6.read_stl(STL / part["file"])
            self.assertEqual(len(triangles), part["triangles"])
            self.assertTrue(stl_v6.check_mesh(triangles)["closed_manifold"])

    def test_sizes_match_the_model(self):
        sizes = {p["name"]: p["size_mm"] for p in self.report["parts"]}
        # the base is now 15.6 tall: the ESP standoffs and pad reach the PCB underside
        self.assertEqual(sizes["v7_service_base"], [66.0, 61.343, 15.6])
        self.assertEqual(sizes["v7_rear_cover"], [68.0, 60.594, 75.0])
        self.assertEqual(sizes["v7_front_frame"][0], 67.6)
        self.assertEqual(self.report["units"], "mm")

    def test_volumes_match_the_native_validation(self):
        native = {p["name"]: p["volume_mm3"]
                  for p in json.loads((ROOT / "v7" / "validation.json").read_text())["parts"]}
        for part in self.report["parts"]:
            self.assertAlmostEqual(part["volume_mm3"] / native[part["name"]], 1.0, delta=0.002)


if __name__ == "__main__":
    unittest.main()
