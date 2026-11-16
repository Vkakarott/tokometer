"""Export the three printable v7 parts as STL, oriented for FDM, and check them.

Same two steps as stl_v6.py, whose mesh tools and print orientations are reused
unchanged (the v7 parts have the v6 outer shapes):

1. In Fusion (run with Fusion's Python API): ``export_raw(raw_dir)`` writes each
   v7 part in model coordinates, in mm.
2. Outside Fusion: ``python3 stl_v7.py <raw_dir> <out_dir>`` rotates each part
   into its print orientation, centres it on X/Y with its lowest point at Z = 0
   (the bed), checks that the mesh is closed and consistently oriented, and
   writes ``<out_dir>/<part>.stl`` plus ``<out_dir>/stl_report_v7.json``.
"""

import json
import sys
from pathlib import Path

import stl_v6

PARTS = ("v7_front_frame", "v7_rear_cover", "v7_service_base")
# Each v7 part is printed like its v6 counterpart.
V6_EQUIVALENT = {name: name.replace("v7_", "v6_") for name in PARTS}
ORIENTATION = {name: stl_v6.ORIENTATION[V6_EQUIVALENT[name]] for name in PARTS}
REPORT = "stl_report_v7.json"
HEADER = "tokometer v7"


def export_raw(raw_dir):
    """Run inside Fusion: write each printable v7 body as a binary STL in mm."""
    import adsk.core
    import adsk.fusion

    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    bodies = {body.name: body for body in design.rootComponent.bRepBodies}
    Path(raw_dir).mkdir(parents=True, exist_ok=True)
    manager = design.exportManager
    for name in PARTS:
        options = manager.createSTLExportOptions(bodies[name], str(Path(raw_dir) / (name + ".stl")))
        options.isBinaryFormat = True
        options.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementHigh
        if not manager.execute(options):
            raise RuntimeError("STL export failed for " + name)


def rotate_point(name, point):
    return stl_v6.rotate_point(V6_EQUIVALENT[name], point)


def orient(name, triangles):
    return stl_v6.orient(V6_EQUIVALENT[name], triangles)


def process_part(name, raw_dir, out_dir):
    oriented, removed = stl_v6.drop_degenerate(
        orient(name, stl_v6.read_stl(Path(raw_dir) / (name + ".stl"))))
    info = stl_v6.check_mesh(oriented)
    info["removed_degenerate_triangles"] = removed
    info["bed_contact_area_mm2"] = round(stl_v6.bed_contact_area(oriented), 1)
    low, high = stl_v6.bounds(oriented)
    stl_v6.write_stl(Path(out_dir) / (name + ".stl"), oriented, HEADER)
    info.update({"name": name, "file": name + ".stl", "orientation": ORIENTATION[name],
                 "bbox_min_mm": [round(v, 3) for v in low],
                 "bbox_max_mm": [round(v, 3) for v in high],
                 "size_mm": [round(high[i] - low[i], 3) for i in range(3)]})
    return info


def process(raw_dir, out_dir):
    Path(out_dir).mkdir(parents=True, exist_ok=True)
    report = {"units": "mm", "origin": "centred on X/Y, lowest point on the bed (Z = 0)",
              "parts": [process_part(name, raw_dir, out_dir) for name in PARTS]}
    (Path(out_dir) / REPORT).write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    print(json.dumps(process(sys.argv[1], sys.argv[2]), indent=2))
