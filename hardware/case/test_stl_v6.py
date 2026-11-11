"""Run without Fusion: python3 -m unittest discover -s hardware/case -p 'test_*.py'."""

import json
from pathlib import Path
import tempfile
import unittest

import stl_v6

ROOT = Path(__file__).resolve().parent
STL = ROOT / "stl"


def cube(size=2.0):
    """Closed, outward-facing cube as 12 triangles."""
    s = size
    v = [(0, 0, 0), (s, 0, 0), (s, s, 0), (0, s, 0), (0, 0, s), (s, 0, s), (s, s, s), (0, s, s)]
    faces = [(0, 3, 2), (0, 2, 1), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4),
             (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
    return [tuple(v[i] for i in f) for f in faces]


class MeshToolTests(unittest.TestCase):
    def test_cube_is_closed_and_has_positive_volume(self):
        info = stl_v6.check_mesh(cube())
        self.assertTrue(info["closed_manifold"])
        self.assertAlmostEqual(info["volume_mm3"], 8.0)

    def test_missing_face_is_reported_open(self):
        info = stl_v6.check_mesh(cube()[:-1])
        self.assertFalse(info["closed_manifold"])
        self.assertGreater(info["open_edges"], 0)

    def test_rotations_do_not_mirror(self):
        for name in stl_v6.PARTS:
            turned = [tuple(stl_v6.rotate_point(name, p) for p in t) for t in cube()]
            self.assertAlmostEqual(stl_v6.signed_volume(turned), 8.0, places=6)

    def test_orient_puts_the_part_on_the_bed_and_centres_it(self):
        for name in stl_v6.PARTS:
            low, high = stl_v6.bounds(stl_v6.orient(name, cube(4.0)))
            self.assertAlmostEqual(low[2], 0.0)
            self.assertAlmostEqual(low[0] + high[0], 0.0)
            self.assertAlmostEqual(low[1] + high[1], 0.0)

    def test_binary_round_trip(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "cube.stl"
            stl_v6.write_stl(path, cube())
            self.assertEqual(len(stl_v6.read_stl(path)), 12)

    def test_degenerate_slivers_are_dropped(self):
        sliver = ((0, 0, 0), (1, 0, 0), (1.00001, 0, 0))
        kept, removed = stl_v6.drop_degenerate(cube() + [sliver])
        self.assertEqual((len(kept), removed), (12, 1))


class PrintableFilesTests(unittest.TestCase):
    def setUp(self):
        self.report = json.loads((STL / "stl_report.json").read_text())

    def test_three_closed_meshes_on_the_bed(self):
        self.assertEqual([p["name"] for p in self.report["parts"]], list(stl_v6.PARTS))
        for part in self.report["parts"]:
            self.assertTrue(part["closed_manifold"], part["name"])
            self.assertEqual(part["bbox_min_mm"][2], 0.0)
            self.assertGreater(part["bed_contact_area_mm2"], 1000.0)
            triangles = stl_v6.read_stl(STL / part["file"])
            self.assertEqual(len(triangles), part["triangles"])
            self.assertTrue(stl_v6.check_mesh(triangles)["closed_manifold"])

    def test_sizes_match_the_model(self):
        sizes = {p["name"]: p["size_mm"] for p in self.report["parts"]}
        self.assertEqual(sizes["v6_service_base"], [66.0, 61.343, 24.5])
        self.assertEqual(sizes["v6_rear_cover"], [68.0, 60.594, 75.0])
        self.assertEqual(sizes["v6_front_frame"][0], 67.6)
        self.assertEqual(self.report["units"], "mm")

    def test_volumes_match_the_native_validation(self):
        native = {p["name"]: p["volume_mm3"]
                  for p in json.loads((ROOT / "v6" / "validation.json").read_text())["parts"]}
        for part in self.report["parts"]:
            self.assertAlmostEqual(part["volume_mm3"] / native[part["name"]], 1.0, delta=0.002)


if __name__ == "__main__":
    unittest.main()
