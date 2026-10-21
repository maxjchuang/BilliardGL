#!/usr/bin/env python3
"""Compile/run the isolated Phase-5 rules UI model and 540-case machine gate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REPORT = ROOT / "docs/evidence/phase5/rules-decision-ui-machine-v1.json"
FONT_CANDIDATES = (
    Path("/System/Library/Fonts/Hiragino Sans GB.ttc"),
    Path("/System/Library/Fonts/PingFang.ttc"),
)
FREETYPE_INCLUDE_CANDIDATES = (
    Path("/opt/homebrew/include/freetype2"),
    Path("/usr/local/include/freetype2"),
)
FREETYPE_LIBRARY_CANDIDATES = (
    Path("/opt/homebrew/lib/libfreetype.dylib"),
    Path("/usr/local/lib/libfreetype.dylib"),
)
READ_ONLY_BINDINGS = {
    "docs/art/phase5-rules-decision-ui-design-gate-v1.json":
        "26567d0547163561ed77e6e41b32753c4cc55efcd693c9ce22faf847d43f5a08",
    "docs/art/phase5-rules-decision-ui-actions-v1.pending.json":
        "7b3de569f57d1a8e589a6d053bf3959f04a5b68bbfb204046bfe8b63428383f1",
    "src/Billiards/rules/group_choice_authority.h":
        "95f6f76bdaddc5627723fe47fd22fa13bc17254a576af63073cba1496dabdf36",
    "src/Billiards/rules/ball_in_hand_authority.h":
        "026fd6196263b1c6dab180fa7f94d29baf277ff56bdff450ca189e7d8ba795d6",
    "src/Billiards/rules/eight_on_break_authority.h":
        "dacb83140b687b285f1dc67bcbb38d571349a60caa01a45664f0bbdcc2981b2e",
    "src/Billiards/rules/break_foul_authority.h":
        "5b4bf09f9950fd61d3d4add4fd70a9304d5c82ab05254f5878c208a6dd92ad9e",
    "src/Billiards/rules/rack_executor.h":
        "d42dc8ae486ad9df8911e4f82cb3d657c0e51e76b35082edb3fd909ee37ef242",
    "assets/modern/hud/focused_practice_hud_shipping_tokens_v1.json":
        "abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860",
    "assets/modern/hud/focused_practice_shell_shipping_tokens_v1.json":
        "c144f1a25d3e7ced14e385bd6f76ca7e0f05b06597dcc8a4bacb4c570fc6bad3",
}
IMPLEMENTATION_BINDINGS = (
    "src/Billiards/rules/rules_decision_ui_model.h",
    "src/Billiards/rules/rules_decision_ui_model.cpp",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.h",
    "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp",
    "src/Billiards/render/ui/rules_decision_product_draw.h",
    "src/Billiards/render/ui/rules_decision_product_draw.cpp",
    "src/Billiards/render/modern/modern_product_session.h",
    "src/Billiards/render/modern/modern_product_session.cpp",
    "src/Billiards/render/ui/product_ui_frame_coordinator.h",
    "src/Billiards/render/ui/product_ui_frame_coordinator.cpp",
    "src/Billiards/render/modern/modern_product_ui_runtime.h",
    "src/Billiards/render/modern/modern_product_ui_runtime.cpp",
    "src/Billiards/render/modern/modern_renderer.h",
    "src/Billiards/render/modern/modern_renderer.cpp",
    "tests/rules_decision_ui_tests.cpp",
    "tests/rules_decision_product_runtime_tests.cpp",
    "tests/rules_decision_session_integration_tests.cpp",
    "spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm",
    "scripts/prepare_phase5_rules_decision_ui_gpu_capture.py",
    "scripts/run_phase5_rules_decision_ui_gpu_capture.py",
)


def first_file(candidates: tuple[Path, ...], label: str) -> Path:
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError(f"{label} not found in declared candidates")


def first_directory(candidates: tuple[Path, ...], label: str) -> Path:
    for candidate in candidates:
        if candidate.is_dir():
            return candidate
    raise FileNotFoundError(f"{label} not found in declared candidates")


def run(report: Path, font: Path | None = None) -> None:
    for relative, expected in READ_ONLY_BINDINGS.items():
        actual = hashlib.sha256((ROOT / relative).read_bytes()).hexdigest()
        if actual != expected:
            raise RuntimeError(
                f"read-only Phase5 binding changed: {relative}: {actual}")
    font = font or first_file(FONT_CANDIDATES, "CJK font")
    freetype_include = first_directory(
        FREETYPE_INCLUDE_CANDIDATES, "FreeType include directory")
    freetype_library = first_file(
        FREETYPE_LIBRARY_CANDIDATES, "FreeType library")
    sources = (
        ROOT / "tests/rules_decision_ui_tests.cpp",
        ROOT / "src/Billiards/rules/rules_decision_ui_model.cpp",
        ROOT / "src/Billiards/render/ui/rules_decision_ui_draw_list.cpp",
        ROOT / "src/Billiards/render/ui/sdf_font_atlas.cpp",
        ROOT / "src/Billiards/render/ui/sdf_generator.cpp",
    )
    with tempfile.TemporaryDirectory(prefix="billiardgl-phase5-rules-ui-") as tmp:
        executable = Path(tmp) / "rules_decision_ui_tests"
        command = [
            "/usr/bin/clang++", "-std=c++17", "-O2", "-Wall", "-Wextra",
            "-Werror", f"-I{ROOT / 'src/Billiards'}",
            f"-I{freetype_include}", *(str(source) for source in sources),
            str(freetype_library), "-o", str(executable),
        ]
        subprocess.run(command, cwd=ROOT, check=True)
        report.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            [str(executable), str(font), str(report)], cwd=ROOT, check=True)
    value = json.loads(report.read_text(encoding="utf-8"))
    value["font_backend"]["font_sha256"] = hashlib.sha256(
        font.read_bytes()).hexdigest()
    value["read_only_bindings"] = READ_ONLY_BINDINGS
    value["implementation_bindings"] = {
        relative: hashlib.sha256((ROOT / relative).read_bytes()).hexdigest()
        for relative in IMPLEMENTATION_BINDINGS
    }
    value["compiler"] = "/usr/bin/clang++ -std=c++17 -O2 -Wall -Wextra -Werror"
    report.write_text(
        json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_REPORT)
    parser.add_argument("--font", type=Path)
    args = parser.parse_args()
    run(args.output.resolve(), args.font.resolve() if args.font else None)
    print(f"Phase5 rules decision UI machine report: {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
