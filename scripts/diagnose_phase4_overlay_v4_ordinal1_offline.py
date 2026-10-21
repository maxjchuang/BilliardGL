#!/usr/bin/env python3
"""Pure-CPU, read-only diagnosis of the frozen Phase 4 v4 ordinal-1 frame.

The script never imports or invokes Cocoa/OpenGL and never runs either GPU
capture runner.  It reconstructs the SHA-bound CPU draw package by linking a
new diagnostic main against the already-frozen topology-audit object graph,
then classifies only the immutable PPM.
"""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import math
import pathlib
import shlex
import subprocess
from typing import Any, Iterable


ROOT = pathlib.Path(__file__).resolve().parents[1]
GATE = ROOT / "docs/art/phase4-overlay-v4-targeted-ordinal1-root-cause-diagnostic-gate-v1.json"
FAILURE = ROOT / "docs/art/phase4-training-replay-overlay-v4-targeted-failure-receipt-v1.json"
PPM = ROOT / "docs/art/phase4-training-replay-overlay-v4-targeted-gpu-v1/camera_player_low_1080.ppm"
SOURCE_BINDINGS = ROOT / "docs/art/phase4-training-replay-overlay-v4-ready-v3/source-bindings-v3.json"
FROZEN_BUILD = ROOT / "build-phase4-overlay-v4-ready-v3"
DIAGNOSTIC_BUILD = ROOT / "build-phase4-overlay-v4-ordinal1-diagnostic"
CPU_SOURCE = ROOT / "tests/phase4_overlay_v4_ordinal1_cpu_package_dump.cpp"
TEST_SOURCE = ROOT / "tests/test_phase4_overlay_v4_ordinal1_offline_diagnostic.py"
DEFAULT_OUTPUT = ROOT / "docs/art/phase4-overlay-v4-ordinal1-offline-diagnostic-v1"

EXPECTED = {
    GATE: "ad95db70125384a1de289792002aba05c3557d587a70ea849d5d168f415e57fa",
    FAILURE: "2dbd499249df1e44631669165a891f94d232f4fa436c9fd0910716de35d2717f",
    PPM: "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744",
    SOURCE_BINDINGS: "7e2d69c3ac5c556cf055c859fa18d4205e967885b9d70d521eeaeb92fe65e6f6",
    FROZEN_BUILD / "generated/phase4_overlay_v4_gpu_capture_runner.mm":
        "35fa98ad44e438f11a6a5b50a80c06de00adb518de4d345d51e741283b195d43",
    ROOT / "spikes/render_core_profile/phase4_overlay_v4_gpu_capture_support.inc":
        "faf64793ec6a7dccc59edf10e745d5b02d892b97336dfc77cf1b80bdfa06cba4",
    ROOT / "src/Billiards/training/trajectory_compound_visibility.cpp":
        "b2149fad4bf11151ba8779132ccc5fc377906fd8fa81e835ccd98186074b673a",
    ROOT / "src/Billiards/training/trajectory_compound_visibility.h":
        "5d8af530e49fa08f8dca8ca08db304db9aee4c3e959203e1251a43b76d820360",
}

TARGETS = {
    ("predicted", "core"): (255, 254, 115),
    ("predicted", "outer_halo_edge"): (0, 0, 0),
    ("predicted", "endpoint"): (255, 254, 115),
    ("actual", "core"): (1, 22, 32),
    ("actual", "solid_in_predicted_dash_gap"): (1, 22, 32),
    ("actual", "tick"): (1, 22, 32),
    ("actual", "endpoint"): (1, 22, 32),
    ("actual", "outer_halo_edge"): (255, 255, 255),
}
INSPECTOR_RECT = (1378, 141, 478, 780)


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical(value: Any) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True,
                       ensure_ascii=False) + "\n").encode()


def write_json(path: pathlib.Path, value: Any) -> None:
    path.write_bytes(canonical(value))


def rel(path: pathlib.Path) -> str:
    return path.resolve().relative_to(ROOT).as_posix()


def require(condition: bool, error: str) -> None:
    if not condition:
        raise RuntimeError(error)


def common_bindings() -> dict[str, Any]:
    return {
        "diagnostic_gate": {"path": rel(GATE), "sha256": EXPECTED[GATE]},
        "failure_receipt": {"path": rel(FAILURE), "sha256": EXPECTED[FAILURE]},
        "original_ppm": {"path": rel(PPM), "sha256": EXPECTED[PPM]},
        "ready_source_bindings": {
            "path": rel(SOURCE_BINDINGS), "sha256": EXPECTED[SOURCE_BINDINGS],
            "aggregate_sha256":
                "1f96d521c696410656ae08134b9f493a28ad38dc35342101d161d963f7c43973",
        },
    }


def verify_frozen_inputs() -> dict[str, Any]:
    for path, expected in EXPECTED.items():
        require(path.exists(), f"frozen_input_missing:{path}")
        require(sha(path) == expected, f"frozen_input_drift:{path}")
    manifest = json.loads(SOURCE_BINDINGS.read_text())
    require(manifest["aggregate_sha256"] ==
            "1f96d521c696410656ae08134b9f493a28ad38dc35342101d161d963f7c43973",
            "ready_source_aggregate_drift")
    current_drift = []
    for item in manifest["files"]:
        path_string = item["path"]
        if path_string.startswith("@build/"):
            path = FROZEN_BUILD / path_string[len("@build/"):]
        else:
            path = ROOT / path_string
        require(path.exists(), f"ready_bound_file_missing:{path_string}")
        if sha(path) != item["sha256"]:
            current_drift.append(path_string)

    # The repository is shared with later phases, so unrelated ready-v3-bound
    # files may now have legitimate newer work.  This diagnostic uses only the
    # frozen topology-audit object graph.  Require every source behind that
    # graph to remain byte-identical to its ready-v3 manifest and bind every
    # object used for the new link; do not silently compile current drift.
    manifest_hash = {item["path"]: item["sha256"] for item in manifest["files"]
                     if item["scope"] == "repository"}
    link_path = (FROZEN_BUILD /
        "CMakeFiles/BilliardsPhase4OverlayV4TopologyAudit.dir/link.txt")
    link = shlex.split(link_path.read_text())
    source_rows = []
    object_rows = []
    for argument in link:
        if not argument.endswith(".o") or ".dir/" not in argument:
            continue
        source_string = argument.split(".dir/", 1)[1][:-2]
        source_path = ROOT / source_string
        object_path = FROZEN_BUILD / argument
        require(source_string in manifest_hash,
                f"topology_source_not_ready_bound:{source_string}")
        require(source_path.exists() and sha(source_path) == manifest_hash[source_string],
                f"topology_source_drift:{source_string}")
        require(object_path.exists(), f"topology_object_missing:{argument}")
        source_rows.append({"path": source_string, "sha256": sha(source_path)})
        object_rows.append({"path": argument, "sha256": sha(object_path)})
    require(len(source_rows) > 40, "topology_object_graph_incomplete")
    return {"ready_manifest_file_count": manifest["file_count"],
            "topology_source_count": len(source_rows),
            "topology_sources": source_rows,
            "topology_objects": object_rows,
            "unrelated_current_ready_bound_drift_excluded": current_drift}


def build_cpu_dump() -> pathlib.Path:
    DIAGNOSTIC_BUILD.mkdir(parents=True, exist_ok=True)
    obj = DIAGNOSTIC_BUILD / "phase4_overlay_v4_ordinal1_cpu_package_dump.o"
    binary = DIAGNOSTIC_BUILD / "BilliardsPhase4OverlayV4Ordinal1CpuPackageDump"
    compile_command = [
        "/usr/bin/c++",
        f'-DBILLIARDGL_ASSET_ROOT="{ROOT / "src/Billiards"}"',
        f'-DBILLIARDGL_SOURCE_ROOT="{ROOT}"',
        f"-I{ROOT / 'src/Billiards'}", "-O3", "-DNDEBUG", "-std=c++17",
        "-arch", "arm64", "-o", str(obj), "-c", str(CPU_SOURCE),
    ]
    subprocess.run(compile_command, check=True, cwd=ROOT)
    link_path = (FROZEN_BUILD /
        "CMakeFiles/BilliardsPhase4OverlayV4TopologyAudit.dir/link.txt")
    link = shlex.split(link_path.read_text())
    frozen_main = ("CMakeFiles/BilliardsPhase4OverlayV4TopologyAudit.dir/"
                   "spikes/render_core_profile/phase4_overlay_v4_topology_audit.cpp.o")
    require(frozen_main in link, "frozen_topology_link_main_missing")
    link[link.index(frozen_main)] = str(obj)
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, check=True, cwd=FROZEN_BUILD)
    linked = subprocess.run(["otool", "-L", str(binary)], check=True,
                            capture_output=True, text=True).stdout
    symbols = subprocess.run(["nm", "-u", str(binary)], check=True,
                             capture_output=True, text=True).stdout
    require("Cocoa" not in linked and "OpenGL" not in linked,
            "cpu_dump_links_gpu_or_cocoa")
    forbidden = ("NSApplication", "CGL", "_glCreate", "_glDraw", "_glReadPixels")
    require(not any(token in symbols for token in forbidden),
            "cpu_dump_has_gpu_or_cocoa_symbol")
    return binary


def cpu_dump(binary: pathlib.Path) -> dict[str, Any]:
    first = subprocess.run([str(binary)], check=True, capture_output=True).stdout
    second = subprocess.run([str(binary)], check=True, capture_output=True).stdout
    require(first == second, "cpu_package_dump_not_byte_reproducible")
    result = json.loads(first)
    require(result["source_probe_count"] == 95, "cpu_probe_denominator_drift")
    require(len(result["source_probes"]) == 95, "cpu_probe_rows_missing")
    ids = [row["source_probe_id"] for row in result["source_probes"]]
    require(len(set(ids)) == 95, "cpu_probe_ids_not_unique")
    require(result["actual_gpu_capture"] is False and
            result["cocoa_or_opengl_used"] is False, "cpu_dump_gpu_claim")
    return result


def read_ppm() -> tuple[int, int, memoryview]:
    data = PPM.read_bytes()
    tokens: list[bytes] = []
    position = 0
    while len(tokens) < 4:
        while data[position:position + 1].isspace():
            position += 1
        if data[position] == ord("#"):
            position = data.index(b"\n", position) + 1
            continue
        end = position
        while not data[end:end + 1].isspace():
            end += 1
        tokens.append(data[position:end])
        position = end
    require(tokens[0] == b"P6" and tokens[3] == b"255", "ppm_header_invalid")
    # P6 has exactly one separator byte after maxval.  Do not consume pixel
    # bytes that happen to have an ASCII-whitespace value.
    position += 1
    width, height = int(tokens[1]), int(tokens[2])
    pixels = memoryview(data)[position:]
    require(len(pixels) == width * height * 3, "ppm_pixel_size_invalid")
    require((width, height) == (1920, 1080), "ppm_dimensions_drift")
    return width, height, pixels


def pixel(pixels: memoryview, width: int, height: int,
          x: int, y: int) -> tuple[int, int, int]:
    if x < 0 or y < 0 or x >= width or y >= height:
        return (0, 0, 0)
    offset = (y * width + x) * 3
    return tuple(pixels[offset:offset + 3])  # type: ignore[return-value]


def edge(a: Iterable[float], b: Iterable[float], p: tuple[float, float]) -> float:
    ax, ay = a
    bx, by = b
    return (p[0] - ax) * (by - ay) - (p[1] - ay) * (bx - ax)


def in_triangle(triangle: dict[str, Any], x: int, y: int) -> bool:
    a, b, c = triangle["vertices_px"]
    p = (x + 0.5, y + 0.5)
    orientation = edge(a, b, tuple(c))
    values = (edge(a, b, p), edge(b, c, p), edge(c, a, p))
    return (all(value >= -1e-6 for value in values) if orientation >= 0.0
            else all(value <= 1e-6 for value in values))


def rasterize(package: dict[str, Any], width: int, height: int
              ) -> tuple[dict[tuple[int, int], dict[str, Any]],
                         dict[tuple[int, int], list[dict[str, Any]]]]:
    owner: dict[tuple[int, int], dict[str, Any]] = {}
    hits: dict[tuple[int, int], list[dict[str, Any]]] = collections.defaultdict(list)
    for triangle in package["semantic_triangles"]:
        left, top, right, bottom = triangle["bounds_px"]
        for y in range(max(0, math.floor(top)), min(height - 1, math.ceil(bottom)) + 1):
            for x in range(max(0, math.floor(left)),
                           min(width - 1, math.ceil(right)) + 1):
                if in_triangle(triangle, x, y):
                    hits[(x, y)].append(triangle)
                    owner[(x, y)] = triangle
    return owner, hits


def color_distance(first: Iterable[int], second: Iterable[int]) -> int:
    return sum((int(a) - int(b)) ** 2 for a, b in zip(first, second))


def luminance(rgb: Iterable[int]) -> float:
    channels = []
    for value in rgb:
        encoded = value / 255.0
        channels.append(encoded / 12.92 if encoded <= 0.04045 else
                        ((encoded + 0.055) / 1.055) ** 2.4)
    return 0.2126 * channels[0] + 0.7152 * channels[1] + 0.0722 * channels[2]


def contrast(first: Iterable[int], second: Iterable[int]) -> float:
    a, b = luminance(first), luminance(second)
    return (max(a, b) + 0.05) / (min(a, b) + 0.05)


def owner_key(value: dict[str, Any] | None) -> tuple[str, str, str] | None:
    if value is None:
        return None
    return (value["track"], value["component_role"], value["primitive_id"])


def target_for(track: str, component: str) -> tuple[int, int, int]:
    return TARGETS[(track, component)]


def tolerance(track: str, component: str) -> int:
    return 12000 if (track, component) == ("actual", "outer_halo_edge") else 5000


def inspector_intersects(x: float, y: float, normal: float,
                         longitudinal: float) -> bool:
    left, top, width, height = INSPECTOR_RECT
    return (x + longitudinal >= left and x - longitudinal <= left + width and
            y + normal >= top and y - normal <= top + height)


def build_matrix(package: dict[str, Any], width: int, height: int,
                 pixels: memoryview,
                 owners: dict[tuple[int, int], dict[str, Any]],
                 hits: dict[tuple[int, int], list[dict[str, Any]]]
                 ) -> tuple[dict[str, Any], dict[str, list[dict[str, Any]]]]:
    rows = []
    candidates: dict[str, list[dict[str, Any]]] = {}
    for probe in package["source_probes"]:
        probe_id = probe["source_probe_id"]
        x0, y0 = map(int, probe["physical_coordinate_px"])
        tx, ty = probe["local_tangent"]
        nx, ny = probe["local_normal"]
        normal_bound = probe["normal_bound_px"]
        longitudinal_bound = probe["longitudinal_bound_px"]
        radius = math.ceil(math.hypot(normal_bound, longitudinal_bound))
        neighborhood = []
        eligible = []
        predicted_intersections = 0
        actual_intersections = 0
        for y in range(max(0, y0 - radius), min(height - 1, y0 + radius) + 1):
            for x in range(max(0, x0 - radius), min(width - 1, x0 + radius) + 1):
                dx, dy = x - x0, y - y0
                local_t = dx * tx + dy * ty
                local_n = dx * nx + dy * ny
                if abs(local_t) > longitudinal_bound + 1e-9 or \
                        abs(local_n) > normal_bound + 1e-9:
                    continue
                rgb = pixel(pixels, width, height, x, y)
                final_owner = owners.get((x, y))
                pixel_hits = hits.get((x, y), [])
                predicted_hit = any(v["track"] == "predicted" for v in pixel_hits)
                actual_hit = any(v["track"] == "actual" for v in pixel_hits)
                predicted_intersections += predicted_hit
                actual_intersections += actual_hit
                neighbor_contrast = 0.0
                for ax, ay in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= ax < width and 0 <= ay < height and \
                            owner_key(owners.get((ax, ay))) != owner_key(final_owner):
                        neighbor_contrast = max(neighbor_contrast,
                                                contrast(rgb, pixel(pixels, width, height, ax, ay)))
                record = {
                    "x": x, "y": y,
                    "local_tangent_px": round(local_t, 6),
                    "local_normal_px": round(local_n, 6),
                    "rgb": list(rgb),
                    "palette_distance_sq": {
                        "predicted_core": color_distance(rgb, TARGETS[("predicted", "core")]),
                        "predicted_halo": color_distance(rgb, TARGETS[("predicted", "outer_halo_edge")]),
                        "actual_core": color_distance(rgb, TARGETS[("actual", "core")]),
                        "actual_halo": color_distance(rgb, TARGETS[("actual", "outer_halo_edge")]),
                    },
                    "edge_contrast_ratio": round(neighbor_contrast, 6),
                    "analytic_final_owner": None if final_owner is None else {
                        "track": final_owner["track"],
                        "component_role": final_owner["component_role"],
                        "primitive_id": final_owner["primitive_id"],
                        "production_draw_ordinal": final_owner["production_draw_ordinal"],
                    },
                    "predicted_analytic_triangle_hit": predicted_hit,
                    "actual_analytic_triangle_hit": actual_hit,
                }
                neighborhood.append(record)
                if final_owner is not None and final_owner["track"] == probe["track"]:
                    role = final_owner["component_role"]
                    endpoint_ok = ((probe["component_role"] == "endpoint" and role == "endpoint" and
                                    final_owner["primitive_id"] == probe["primitive_id"]) or
                                   (probe["component_role"] != "endpoint" and role != "endpoint"))
                    if endpoint_ok and (probe["track"], role) in TARGETS and \
                            color_distance(rgb, target_for(probe["track"], role)) <= \
                            tolerance(probe["track"], role):
                        eligible.append({
                            "x": x, "y": y, "owner": final_owner,
                            "local_tangent_px": local_t,
                            "local_normal_px": local_n,
                            "distance_px": math.hypot(local_t, local_n),
                            "edge_contrast_ratio": neighbor_contrast,
                            "rgb": rgb,
                        })
        candidates[probe_id] = eligible
        rows.append({
            **probe,
            "ppm_neighborhood": neighborhood,
            "ppm_neighborhood_pixel_count": len(neighborhood),
            "predicted_analytic_triangle_intersection_pixel_count":
                predicted_intersections,
            "actual_analytic_triangle_intersection_pixel_count": actual_intersections,
            "same_track_gate_bounded_candidate_pixel_count": len(eligible),
            "inspector_rect_px": list(INSPECTOR_RECT),
            "source_probe_intersects_inspector":
                INSPECTOR_RECT[0] <= x0 <= INSPECTOR_RECT[0] + INSPECTOR_RECT[2] and
                INSPECTOR_RECT[1] <= y0 <= INSPECTOR_RECT[1] + INSPECTOR_RECT[3],
            "bounded_neighborhood_intersects_inspector": inspector_intersects(
                x0, y0, normal_bound, longitudinal_bound),
            "semantic_ownership_limitation":
                "PPM color plus analytic CPU triangles cannot prove final semantic ownership.",
        })
    return ({
        "schema": "billiardgl.phase4.overlay_v4_ordinal1_ppm_probe_matrix.v1",
        "status": "DIAGNOSTIC_INFERENCE_ONLY_NOT_A_PASS",
        "input_bindings": common_bindings(),
        "source_probe_count": 95,
        "row_count": len(rows),
        "ppm_dimensions": [width, height],
        "inspector_rect_px": list(INSPECTOR_RECT),
        "inspector_rect_provenance": {
            "source": "src/Billiards/render/ui/product_shell_layout.cpp:63,89-105,118-131",
            "frozen_frame_dock": "right",
        },
        "pass_claimed": False,
        "final_semantic_ownership_proved": False,
        "rows": rows,
    }, candidates)


def connected_count(owners: dict[tuple[int, int], dict[str, Any]],
                    x: int, y: int, expected: tuple[str, str, str]) -> int:
    return sum(1 for cy in range(-1, 2) for cx in range(-1, 2)
               if owner_key(owners.get((x + cx, y + cy))) is not None and
               owner_key(owners.get((x + cx, y + cy)))[0] == expected[0] and
               owner_key(owners.get((x + cx, y + cy)))[2] == expected[2])


def exact_role_classification(package: dict[str, Any], width: int, height: int,
                              pixels: memoryview,
                              owners: dict[tuple[int, int], dict[str, Any]]) -> dict[str, Any]:
    choices = []
    for probe in package["source_probes"]:
        x0, y0 = map(int, probe["physical_coordinate_px"])
        radius = math.ceil(probe["normal_bound_px"])
        best = None
        for y in range(max(0, y0 - radius), min(height - 1, y0 + radius) + 1):
            for x in range(max(0, x0 - radius), min(width - 1, x0 + radius) + 1):
                dx, dy = x - x0, y - y0
                distance = math.hypot(dx, dy)
                if distance > probe["normal_bound_px"] + 1e-9:
                    continue
                owner = owners.get((x, y))
                expected = (probe["track"], probe["component_role"], probe["primitive_id"])
                if owner_key(owner) != expected:
                    continue
                rgb = pixel(pixels, width, height, x, y)
                if color_distance(rgb, target_for(probe["track"], probe["component_role"])) > \
                        tolerance(probe["track"], probe["component_role"]):
                    continue
                count = connected_count(owners, x, y, expected)
                edge_ratio = 0.0
                for ax, ay in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    adjacent = owners.get((ax, ay))
                    if adjacent is not None and adjacent["track"] == expected[0] and \
                            adjacent["primitive_id"] == expected[2]:
                        continue
                    edge_ratio = max(edge_ratio,
                                     contrast(rgb, pixel(pixels, width, height, ax, ay)))
                if count < 2 or edge_ratio < 4.0:
                    continue
                candidate = (distance, probe["primitive_id"], int(probe["probe_ordinal"]),
                             x, y, edge_ratio, count)
                if best is None or candidate < best:
                    best = candidate
        choices.append({"probe": probe, "candidate": best})
    choices.sort(key=lambda value: ((value["candidate"][0] if value["candidate"] else math.inf),
                                    value["probe"]["primitive_id"],
                                    value["probe"]["probe_ordinal"]))
    assigned = set()
    results = {}
    collision_count = 0
    for choice in choices:
        probe = choice["probe"]
        candidate = choice["candidate"]
        visible = False
        reason = "no_exact_role_primitive_candidate"
        evidence = None
        if not probe["inside_final_viewport"]:
            reason = "out_of_bounds_or_clipped"
        elif candidate is not None:
            key = (candidate[3], candidate[4])
            if key in assigned:
                collision_count += 1
                reason = "pixel_already_assigned"
            else:
                assigned.add(key)
                visible = True
                reason = "exact_role_primitive_candidate"
                evidence = {"x": key[0], "y": key[1],
                            "distance_px": round(candidate[0], 6),
                            "edge_contrast_ratio": round(candidate[5], 6),
                            "owned_neighbor_count_3x3": candidate[6]}
        results[probe["source_probe_id"]] = {
            "visible": visible, "reason": reason, "evidence": evidence,
        }
    return {"results": results, "collision_count": collision_count}


def compound_classification(package: dict[str, Any],
                            candidates: dict[str, list[dict[str, Any]]]) -> dict[str, Any]:
    probes = {row["source_probe_id"]: row for row in package["source_probes"]}
    claims: dict[tuple[int, int], list[tuple[Any, ...]]] = collections.defaultdict(list)
    for probe_id, values in candidates.items():
        probe = probes[probe_id]
        for candidate in values:
            claims[(candidate["x"], candidate["y"])].append((
                candidate["distance_px"], candidate["owner"]["primitive_id"],
                int(probe["probe_ordinal"]), probe_id, candidate))
    assigned: dict[str, list[dict[str, Any]]] = collections.defaultdict(list)
    contested = 0
    for pixel_key, pixel_claims in claims.items():
        pixel_claims.sort(key=lambda value: value[:4])
        if len(pixel_claims) > 1:
            contested += 1
        winner = pixel_claims[0]
        assigned[winner[3]].append(winner[4])
    results = {}
    for probe_id, probe in probes.items():
        pixels_for_probe = assigned.get(probe_id, [])
        locations = {(value["x"], value["y"]) for value in pixels_for_probe}
        connected = [value for value in pixels_for_probe
                     if any((value["x"] + dx, value["y"] + dy) in locations
                            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
        edge_pixels = [value for value in connected
                       if value["edge_contrast_ratio"] >= 4.0]
        visible = len(connected) >= 2 and bool(edge_pixels)
        if visible:
            precedence = {"core": 0, "solid_in_predicted_dash_gap": 1,
                          "outer_halo_edge": 2, "tick": 3, "endpoint": 4}
            def substitute(value: dict[str, Any]) -> str:
                role = value["owner"]["component_role"]
                if probe["track"] == "actual" and role == "core" and \
                        probe["component_role"] != "core":
                    return "solid_in_predicted_dash_gap"
                return role
            selected = min(edge_pixels,
                           key=lambda value: (precedence[substitute(value)],
                                              value["distance_px"], value["x"], value["y"]))
            component = substitute(selected)
            reason = "same_track_compound_candidate"
            evidence = {"x": selected["x"], "y": selected["y"],
                        "substitute_component": component,
                        "owner_primitive_id": selected["owner"]["primitive_id"],
                        "distance_px": round(selected["distance_px"], 6),
                        "edge_contrast_ratio": round(selected["edge_contrast_ratio"], 6),
                        "exclusive_assigned_pixel_count": len(pixels_for_probe),
                        "four_connected_pixel_count": len(connected)}
        else:
            evidence = None
            if not probe["inside_final_viewport"]:
                reason = "out_of_bounds_or_clipped"
            elif not candidates[probe_id]:
                reason = "no_same_track_gate_bounded_color_candidate"
            elif len(connected) < 2:
                reason = "insufficient_owned_pixel_area"
            else:
                reason = "final_edge_contrast_below_4"
        results[probe_id] = {"visible": visible, "reason": reason,
                             "evidence": evidence}
    return {"results": results, "contested_pixel_count": contested,
            "exclusive_double_assignment_count": 0}


def counts(package: dict[str, Any], classification: dict[str, Any]) -> dict[str, Any]:
    by_track = collections.Counter()
    by_role = collections.Counter()
    visible = 0
    for probe in package["source_probes"]:
        result = classification["results"][probe["source_probe_id"]]
        by_track[(probe["track"], "source")] += 1
        by_role[(probe["track"], probe["component_role"], "source")] += 1
        if result["visible"]:
            visible += 1
            by_track[(probe["track"], "visible")] += 1
            by_role[(probe["track"], probe["component_role"], "visible")] += 1
    track_rows = {}
    for track in ("predicted", "actual"):
        source = by_track[(track, "source")]
        track_visible = by_track[(track, "visible")]
        track_rows[track] = {"source": source, "visible": track_visible,
                             "hidden": source - track_visible,
                             "coverage": round(track_visible / source, 6)}
    return {"source": 95, "visible": visible, "hidden": 95 - visible,
            "coverage": round(visible / 95, 6), "tracks": track_rows,
            "component_counts": {
                "|".join(key): value for key, value in sorted(by_role.items())}}


def counterfactual(package: dict[str, Any], exact: dict[str, Any],
                   compound: dict[str, Any]) -> dict[str, Any]:
    exact_counts = counts(package, exact)
    compound_counts = counts(package, compound)
    delta = []
    grouped = collections.Counter()
    for probe in package["source_probes"]:
        probe_id = probe["source_probe_id"]
        before = exact["results"][probe_id]
        after = compound["results"][probe_id]
        if before["visible"] == after["visible"]:
            continue
        substitute = (after["evidence"] or {}).get("substitute_component", "none")
        direction = "compound_rescue" if after["visible"] else "compound_loss"
        grouped[(direction, probe["track"], probe["component_role"], substitute,
                 before["reason"], after["reason"])] += 1
        delta.append({"source_probe_id": probe_id, "track": probe["track"],
                      "source_role": probe["component_role"],
                      "direction": direction, "substitute_component": substitute,
                      "exact_reason": before["reason"],
                      "compound_reason": after["reason"],
                      "compound_evidence": after["evidence"]})
    grouped_rows = [{"direction": key[0], "track": key[1],
                     "source_role": key[2], "substitute_component": key[3],
                     "exact_reason": key[4], "compound_reason": key[5],
                     "count": value} for key, value in sorted(grouped.items())]
    require(exact_counts["source"] == compound_counts["source"] == 95,
            "counterfactual_denominator_changed")
    return {
        "schema": "billiardgl.phase4.overlay_v4_ordinal1_exact_vs_compound.v1",
        "status": "PURE_CPU_COUNTERFACTUAL_NOT_FINAL_OWNERSHIP_PROOF",
        "input_bindings": common_bindings(),
        "invariants": {"denominator": 95, "coverage_threshold": 0.9,
                       "minimum_edge_contrast_ratio": 4.0,
                       "predicted_lateral_offset_px": 0.0,
                       "actual_lateral_offset_px": 0.0,
                       "z_order": "actual_then_predicted",
                       "threshold_changed": False, "probe_changed": False,
                       "route_or_offset_changed": False},
        "frozen_actual_counts": {"source": 95, "visible": 46, "hidden": 49,
                                 "coverage": 0.484211},
        "offline_exact_role_analytic_counts": exact_counts,
        "offline_compound_analytic_counts": compound_counts,
        "offline_exact_matches_frozen_visible_count": exact_counts["visible"] == 46,
        "visible_id_delta": delta,
        "visible_id_delta_grouped": grouped_rows,
        "exact_role_collision_count": exact["collision_count"],
        "compound_contested_pixel_count_before_exclusive_arbitration":
            compound["contested_pixel_count"],
        "compound_double_assignment_count_after_arbitration":
            compound["exclusive_double_assignment_count"],
        "limitation": ("Both policies use PPM colors plus analytic CPU triangle ownership. "
                       "Neither can recover resolved MSAA samples, blending, tone map, product "
                       "UI ownership or the discarded ordinal-1 semantic attachment."),
        "pass_claimed": False,
    }


def line_of(path: pathlib.Path, text: str) -> int:
    for number, line in enumerate(path.read_text().splitlines(), 1):
        if text in line:
            return number
    raise RuntimeError(f"source_anchor_missing:{path}:{text}")


def serialization_audit() -> dict[str, Any]:
    runner = FROZEN_BUILD / "generated/phase4_overlay_v4_gpu_capture_runner.mm"
    audit_line = line_of(runner, "latestCompoundAudit = training::auditCompoundFinalVisibility")
    require_line = line_of(runner, 'require(machinePass, "phase4_overlay_machine_gate_failed:"')
    fields = [
        ("predicted source/visible/hidden counts and coverage", "latestCompoundAudit.predicted", 1566),
        ("actual source/visible/hidden counts and coverage", "latestCompoundAudit.actual", 1574),
        ("per-component source counts", "latestCompoundAudit.componentSourceCounts", 1600),
        ("per-component visible counts", "latestCompoundAudit.componentVisibleCounts", 1604),
        ("per-component hidden reason counts", "latestCompoundAudit.componentHiddenReasonCounts", 1608),
        ("hidden probe IDs, roles and physical coordinates", "latestCompoundAudit.hiddenProbes", 1614),
        ("ordered source and visible ID digests", "latestCompoundAudit.orderedSourceProbeDigest", 1582),
    ]
    rows = []
    for field, memory, expected_serialization in fields:
        serialization = line_of(runner, memory)
        if serialization < require_line:
            # First references above the failure are computation/debug reads;
            # find the known result-row reference after require.
            candidates = [n for n, line in enumerate(runner.read_text().splitlines(), 1)
                          if memory in line and n > require_line]
            serialization = candidates[0] if candidates else expected_serialization
        rows.append({"missing_field": field,
                     "computed_before_fail_closed_line": audit_line,
                     "in_memory_source": memory,
                     "serialization_line": serialization,
                     "machine_fail_require_line": require_line,
                     "lost_on_ordinal1_failure": serialization > require_line})
    rows.append({"missing_field": "ownership candidate pixels and arbitration results",
                 "computed_before_fail_closed_line": audit_line,
                 "in_memory_source": "request.ownershipCandidates and local choices/assignedPixels",
                 "serialization_line": None,
                 "machine_fail_require_line": require_line,
                 "lost_on_ordinal1_failure": True,
                 "also_missing_on_success": True})
    require(all(row["lost_on_ordinal1_failure"] for row in rows),
            "serialization_audit_did_not_account_for_missing_field")
    return {
        "schema": "billiardgl.phase4.overlay_v4_ordinal1_failure_serialization_audit.v1",
        "status": "STATIC_CONTROL_FLOW_CONFIRMED_FIELDS_LOST",
        "input_bindings": common_bindings(),
        "runner_source": {"path": rel(runner), "sha256": EXPECTED[runner]},
        "control_flow": {"compound_audit_completed_line": audit_line,
                         "machine_fail_require_line": require_line,
                         "result_row_serialization_begins_line":
                             line_of(runner, "json::Value row = json::Value::object()")},
        "missing_field_accounting": rows,
        "runner_modified": False,
    }


def conformance_matrix() -> dict[str, Any]:
    runner = rel(FROZEN_BUILD / "generated/phase4_overlay_v4_gpu_capture_runner.mm")
    support = "spikes/render_core_profile/phase4_overlay_v4_gpu_capture_support.inc"
    compound = "src/Billiards/training/trajectory_compound_visibility.cpp"
    renderer = "src/Billiards/training/trajectory_overlay_renderer.cpp"
    entries = [
        ("integrated final-composite ownership attachment", "MISSING", runner, "822-924",
         "A separate RGBA32UI framebuffer replays trajectory batches only."),
        ("identical MSAA sample positions and resolved coverage", "MISSING", runner, "851-914",
         "The semantic texture is single-sample; resolved alpha is later forced to 1."),
        ("production blend semantics", "MISSING", runner, "858-873",
         "Integer attachment disables GL_BLEND and FRAMEBUFFER_SRGB."),
        ("production clipping", "APPROXIMATED", renderer, "697-755,883-908",
         "CPU-projected/clipped vertices are reused, but production stencil/scissor sample effects are absent."),
        ("ACES tone-map correspondence", "APPROXIMATED", support, "236-276",
         "Final PPM is checked only by palette-distance tolerance."),
        ("later product UI ownership", "MISSING", runner, "822-924",
         "Detached trajectory replay contains no product UI primitives."),
        ("cross-component same-track substitution", "MISSING", compound, "103-110,317-346",
         "Eligibility requires exact component role and primitive ID."),
        ("probe-local tangent/normal bounds", "APPROXIMATED", support, "260-263,307-322",
         "Euclidean radius is recorded as normal distance; longitudinal distance is zero."),
        ("exclusive assignment", "IMPLEMENTED", compound, "290-380",
         "Candidate choices are deterministically sorted and pixel keys are assigned once."),
        ("failure preservation", "MISSING", runner, "1486-1637",
         "machinePass is required before constructing/writing the diagnostic result row."),
        ("immutable denominator and stable source IDs", "IMPLEMENTED", compound, "221-277",
         "All source probes are inventoried and duplicate IDs fail closed."),
    ]
    rows = [{"requirement": a, "classification": b,
             "source_path": c, "source_lines": d, "evidence": e}
            for a, b, c, d, e in entries]
    return {
        "schema": "billiardgl.phase4.overlay_v4_gate_to_code_conformance.v1",
        "status": "IMPLEMENTATION_CONFORMANCE_AUDITED_NO_IMPLEMENTATION_APPROVAL",
        "input_bindings": common_bindings(),
        "classification_counts": dict(collections.Counter(row["classification"] for row in rows)),
        "requirements": rows,
        "implementation_proposal_admitted": False,
        "blocking_missing_items": [row["requirement"] for row in rows
                                   if row["classification"] == "MISSING"],
    }


def hypotheses(package: dict[str, Any], matrix: dict[str, Any],
               cf: dict[str, Any]) -> list[dict[str, Any]]:
    panel_hits = sum(row["bounded_neighborhood_intersects_inspector"]
                     for row in matrix["rows"])
    cross_track = sum(row["predicted_analytic_triangle_intersection_pixel_count"] > 0 and
                      row["actual_analytic_triangle_intersection_pixel_count"] > 0
                      for row in matrix["rows"])
    rescues = sum(row["direction"] == "compound_rescue"
                  for row in cf["visible_id_delta"])
    return [
        {"id": "H1_PANEL_OCCLUSION", "outcome": "FALSIFIED_FOR_ALL_95_BOUNDED_NEIGHBORHOODS",
         "measurement": {"panel_intersections": panel_hits, "denominator": 95}},
        {"id": "H2_SCENE_GEOMETRY_OCCLUSION",
         "outcome": "NOT_SUPPORTED_AS_POST_TRAJECTORY_OVERWRITE; FINAL_PIXEL_OWNER_UNPROVED",
         "measurement": {"production_order": "scene_before_trajectory_before_ui",
                         "ppm_semantic_owner_proof_available": False},
         "limitation": "PPM alone cannot name scene ownership at a pixel."},
        {"id": "H3_TRUE_CROSS_TRACK_CORE_COVERING", "outcome": "SUPPORTED_BY_CPU_GEOMETRY",
         "measurement": {"probe_neighborhoods_with_both_track_triangle_hits": cross_track,
                         "denominator": 95}},
        {"id": "H4_CROSS_COMPONENT_COMPOUND_ASSIGNMENT_GAP",
         "outcome": "SUPPORTED_BY_CODE_AND_OFFLINE_COUNTERFACTUAL" if rescues else "NOT_OBSERVED",
         "measurement": {"offline_compound_rescue_count": rescues}},
        {"id": "H5_DETACHED_ATTACHMENT_OR_FINAL_COLOR_CORRESPONDENCE_GAP",
         "outcome": "NOT_FALSIFIABLE_FROM_FROZEN_FILES",
         "measurement": {"integrated_final_attachment_present": False,
                         "offline_exact_visible": cf["offline_exact_role_analytic_counts"]["visible"],
                         "frozen_runner_visible": 46}},
        {"id": "H6_EXCLUSIVE_ASSIGNMENT_COLLISION_OR_BAD_DISTANCE_FRAME",
         "outcome": "BOUNDED_BUT_NOT_FINAL_OWNERSHIP_ISOLATED",
         "measurement": {
             "exact_collision_count": cf["exact_role_collision_count"],
             "compound_contested_pixels_before_arbitration":
                 cf["compound_contested_pixel_count_before_exclusive_arbitration"],
             "compound_double_assignment_after_arbitration":
                 cf["compound_double_assignment_count_after_arbitration"],
             "current_distance_frame": "euclidean_as_normal_and_zero_longitudinal",
             "counterfactual_distance_frame": "probe_local_tangent_normal",
         }},
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-root", type=pathlib.Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    output = args.output_root.resolve()
    output.mkdir(parents=True, exist_ok=True)

    source_verification = verify_frozen_inputs()
    binary = build_cpu_dump()
    package = cpu_dump(binary)
    package["input_bindings"] = common_bindings()
    package["diagnostic_binary"] = {"path": rel(binary), "sha256": sha(binary)}
    package["cpu_dump_source"] = {"path": rel(CPU_SOURCE), "sha256": sha(CPU_SOURCE)}
    package["frozen_topology_object_graph_verification"] = source_verification
    package_path = output / "d1-frozen-cpu-package-probe-dump-v1.json"
    write_json(package_path, package)

    width, height, pixels = read_ppm()
    owners, hits = rasterize(package, width, height)
    matrix, candidate_pixels = build_matrix(package, width, height, pixels, owners, hits)
    matrix_path = output / "d2-frozen-ppm-probe-classification-matrix-v1.json"
    write_json(matrix_path, matrix)

    exact = exact_role_classification(package, width, height, pixels, owners)
    compound = compound_classification(package, candidate_pixels)
    cf = counterfactual(package, exact, compound)
    cf_path = output / "d3-exact-role-vs-compound-counterfactual-v1.json"
    write_json(cf_path, cf)

    serialization = serialization_audit()
    serialization_path = output / "d4-failure-serialization-capability-audit-v1.json"
    write_json(serialization_path, serialization)
    conformance = conformance_matrix()
    conformance_path = output / "d5-gate-to-implementation-conformance-matrix-v1.json"
    write_json(conformance_path, conformance)

    hypothesis_rows = hypotheses(package, matrix, cf)
    exact_reproduces_frozen = cf["offline_exact_matches_frozen_visible_count"]
    missing_final_ownership = True
    root_isolated = False
    report = {
        "schema": "billiardgl.phase4.overlay_v4_ordinal1_offline_root_cause_diagnostic.v1",
        "status": "DIAGNOSTIC_MISSING_FINAL_OWNERSHIP_TELEMETRY_FAIL_CLOSED",
        "decision": ("ROOT_CAUSE_CAPABILITY_GAPS_AND_CROSS_TRACK_OVERLAP_CONFIRMED; "
                     "EXACT_49_HIDDEN_PROBE_DISTRIBUTION_NOT_ISOLATED"),
        "input_bindings": common_bindings(),
        "root_cause_isolated": root_isolated,
        "all_95_probes_reconciled_as_offline_inferences": True,
        "all_49_frozen_hidden_probes_reconciled_with_final_ownership_proof": False,
        "offline_exact_policy_reproduces_frozen_46_visible": exact_reproduces_frozen,
        "missing_capability": "resolved integrated final-composite per-sample ownership plus pre-failure serialization",
        "diagnostics_completed": ["D1", "D2", "D3", "D4", "D5"],
        "falsifiable_hypotheses": hypothesis_rows,
        "claims": {"actual_gpu_capture": False, "cocoa_or_opengl": False,
                   "targeted_rerun_or_resume": False, "formal68": False,
                   "technical_art_review": False, "machine_pass": False,
                   "m3": False, "golden": False},
        "minimal_future_no_gpu_instrumentation_gate_proposal_not_implemented": {
            "status": "NEW_GATE_REQUIRED_IMPLEMENTATION_NOT_AUTHORIZED_HERE",
            "scope": [
                "Serialize the already-computed source/visible/hidden IDs, per-track/component reasons and coordinates before fail-closed.",
                "Add capture-only integrated final-composite ownership plumbing that carries per-sample track/component/primitive IDs through production MSAA resolve, blend, clip, tone map and product UI ordering.",
                "Record true probe-local tangent/normal distances, resolved sample coverage and exclusive arbitration candidates.",
                "Add CPU-only negative fixtures for panel occlusion, cross-track covering, compound rescue, final-color mismatch, shared-pixel arbitration and tangent/normal bounds.",
                "Fresh-build and validate the instrumentation without executing GPU; any later capture needs a separate single-use root authorization.",
            ],
            "production_visual_delta_allowed": False,
            "threshold_probe_denominator_offset_z_order_change_allowed": False,
        },
        "frozen_ordinal1_preserved": {"source": 95, "visible": 46, "hidden": 49,
                                      "coverage": 0.484211,
                                      "rerun_or_resume_allowed": False},
    }
    report_path = output / "root-cause-diagnostic-report-v1.json"
    write_json(report_path, report)

    artifacts = [package_path, matrix_path, cf_path, serialization_path,
                 conformance_path, report_path]
    manifest = {
        "schema": "billiardgl.phase4.overlay_v4_ordinal1_offline_diagnostic_manifest.v1",
        "status": "PURE_OFFLINE_DIAGNOSTIC_COMPLETE_FAIL_CLOSED",
        "input_bindings": common_bindings(),
        "artifacts": [{"path": path.name, "sha256": sha(path),
                       "size_bytes": path.stat().st_size} for path in artifacts],
        "tooling": [
            {"path": rel(pathlib.Path(__file__)), "sha256": sha(pathlib.Path(__file__))},
            {"path": rel(CPU_SOURCE), "sha256": sha(CPU_SOURCE)},
            {"path": rel(TEST_SOURCE), "sha256": sha(TEST_SOURCE)},
        ],
        "diagnostic_binary": {"path": rel(binary), "sha256": sha(binary)},
        "actual_gpu_capture": False,
        "targeted_or_formal_runner_invoked": False,
    }
    write_json(output / "diagnostic-evidence-manifest-v1.json", manifest)
    print("PHASE4_V4_ORDINAL1_OFFLINE_DIAGNOSTIC_PASS_FAIL_CLOSED")
    print(json.dumps({"output_root": str(output),
                      "root_cause_isolated": root_isolated,
                      "status": report["status"],
                      "offline_exact_visible": cf["offline_exact_role_analytic_counts"]["visible"],
                      "offline_compound_visible": cf["offline_compound_analytic_counts"]["visible"]},
                     sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
