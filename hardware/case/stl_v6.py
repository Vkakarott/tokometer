"""Export the three printable v6 parts as STL, oriented for FDM, and check them.

Two steps, because Fusion cannot export a rotated copy without editing the design:

1. In Fusion (run with Fusion's Python API): ``export_raw(raw_dir)`` writes each
   part in model coordinates, in mm.
2. Outside Fusion: ``python3 stl_v6.py <raw_dir> <out_dir>`` rotates each part
   into its print orientation, centres it on X/Y with its lowest point at Z = 0
   (the bed), checks that the mesh is closed and consistently oriented, and
   writes ``<out_dir>/<part>.stl`` plus ``<out_dir>/stl_report.json``.

Print orientations: the front frame lies with its front face on the bed, the rear
cover is upside down (top on the bed) and the base sits on its underside.
"""

import json
import math
import struct
import sys
from pathlib import Path

import stand_v6_spec as spec

PARTS = ("v6_front_frame", "v6_rear_cover", "v6_service_base")
ORIENTATION = {
    "v6_front_frame": "front face down",
    "v6_rear_cover": "upside down, top on the bed",
    "v6_service_base": "underside on the bed",
}
WELD_DECIMALS = 4


def export_raw(raw_dir):
    """Run inside Fusion: write each printable body as a binary STL in mm."""
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


def read_stl(path):
    data = Path(path).read_bytes()
    count = struct.unpack_from("<I", data, 80)[0]
    if len(data) != 84 + 50 * count:
        raise ValueError("not a binary STL: " + str(path))
    triangles = []
    for index in range(count):
        values = struct.unpack_from("<12f", data, 84 + 50 * index)
        triangles.append(tuple(values[3 + 3 * k:6 + 3 * k] for k in range(3)))
    return triangles


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def unit_length(vector):
    length = math.sqrt(sum(c * c for c in vector))
    return tuple(c / length for c in vector) if length else vector


def write_stl(path, triangles, header="tokometer v6"):
    chunks = [header.encode("ascii")[:80].ljust(80, b" "), struct.pack("<I", len(triangles))]
    for a, b, c in triangles:
        ab = tuple(b[i] - a[i] for i in range(3))
        ac = tuple(c[i] - a[i] for i in range(3))
        normal = unit_length(cross(ab, ac))
        chunks.append(struct.pack("<12fH", *normal, *a, *b, *c, 0))
    Path(path).write_bytes(b"".join(chunks))


def front_normal():
    """Outward normal (y, z) of the leaning front face."""
    length = math.hypot(spec.FRONT_LEAN, spec.BODY_HEIGHT)
    return spec.FRONT_LEAN / length, spec.BODY_HEIGHT / length


def rotate_point(name, point):
    """Proper rotation (no mirroring) from model axes to print axes (Z up)."""
    x, y, z = point
    if name == "v6_service_base":
        return (x, -z, y)
    if name == "v6_rear_cover":
        return (x, z, -y)
    ny, nz = front_normal()
    return (x, -nz * y + ny * z, -(ny * y + nz * z))


def bounds(triangles):
    points = [p for triangle in triangles for p in triangle]
    low = tuple(min(p[i] for p in points) for i in range(3))
    high = tuple(max(p[i] for p in points) for i in range(3))
    return low, high


def orient(name, triangles):
    """Rotate into print orientation, centre X/Y, put the lowest point at Z = 0."""
    turned = [tuple(rotate_point(name, p) for p in triangle) for triangle in triangles]
    low, high = bounds(turned)
    shift = ((low[0] + high[0]) / 2, (low[1] + high[1]) / 2, low[2])
    return [tuple(tuple(p[i] - shift[i] for i in range(3)) for p in triangle)
            for triangle in turned]


def signed_volume(triangles):
    total = 0.0
    for a, b, c in triangles:
        total += sum(a[i] * cross(b, c)[i] for i in range(3)) / 6.0
    return total


def check_mesh(triangles):
    """Closed and consistently oriented: every directed edge once, and its reverse once."""
    def key(point):
        return tuple(round(c, WELD_DECIMALS) for c in point)

    directed = {}
    degenerate = 0
    for triangle in triangles:
        ids = [key(p) for p in triangle]
        if len(set(ids)) < 3:
            degenerate += 1
            continue
        for first, second in ((0, 1), (1, 2), (2, 0)):
            edge = (ids[first], ids[second])
            directed[edge] = directed.get(edge, 0) + 1
    open_edges = sum(1 for (a, b) in directed if (b, a) not in directed)
    repeated = sum(1 for count in directed.values() if count > 1)
    vertices = {key(p) for triangle in triangles for p in triangle}
    return {"triangles": len(triangles), "vertices": len(vertices),
            "degenerate_triangles": degenerate, "open_edges": open_edges,
            "edges_used_more_than_once": repeated,
            "closed_manifold": open_edges == 0 and repeated == 0 and degenerate == 0,
            "volume_mm3": round(signed_volume(triangles), 1)}


def _weld_key(point):
    return tuple(round(c, WELD_DECIMALS) for c in point)


def drop_degenerate(triangles):
    """Remove zero-area slivers (two corners weld together); returns (kept, removed count).

    Such a sliver adds an edge and its reverse between the same two vertices, so
    dropping it leaves the closed/oriented property of the rest unchanged.
    """
    kept = [t for t in triangles if len({_weld_key(p) for p in t}) == 3]
    return kept, len(triangles) - len(kept)


def bed_contact_area(triangles, tolerance=1e-3):
    """Area (mm2) of faces lying on the bed (Z = 0) and facing down."""
    area = 0.0
    for a, b, c in triangles:
        if max(a[2], b[2], c[2]) > tolerance:
            continue
        normal = cross(tuple(b[i] - a[i] for i in range(3)), tuple(c[i] - a[i] for i in range(3)))
        if normal[2] < 0:
            area += -normal[2] / 2.0
    return area


def process(raw_dir, out_dir):
    Path(out_dir).mkdir(parents=True, exist_ok=True)
    report = {"units": "mm", "origin": "centred on X/Y, lowest point on the bed (Z = 0)", "parts": []}
    for name in PARTS:
        oriented, removed = drop_degenerate(
            orient(name, read_stl(Path(raw_dir) / (name + ".stl"))))
        info = check_mesh(oriented)
        info["removed_degenerate_triangles"] = removed
        info["bed_contact_area_mm2"] = round(bed_contact_area(oriented), 1)
        low, high = bounds(oriented)
        write_stl(Path(out_dir) / (name + ".stl"), oriented)
        info.update({"name": name, "file": name + ".stl", "orientation": ORIENTATION[name],
                     "bbox_min_mm": [round(v, 3) for v in low],
                     "bbox_max_mm": [round(v, 3) for v in high],
                     "size_mm": [round(high[i] - low[i], 3) for i in range(3)]})
        report["parts"].append(info)
    (Path(out_dir) / "stl_report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    print(json.dumps(process(sys.argv[1], sys.argv[2]), indent=2))
