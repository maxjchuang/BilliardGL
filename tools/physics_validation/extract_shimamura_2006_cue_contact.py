import csv
import hashlib
import io
import json


DATASET_ID = "shimamura_2006_cue_contact"
DATASET_VERSION = "1.0.0"
DOI = "10.1299/jsmekanto.2006.12.495"
PDF_URL = (
    "https://www.jstage.jst.go.jp/article/jsmekanto/2006.12/0/"
    "2006.12_495/_pdf/-char/en"
)
PDF_SHA256 = "e308884f67a98f58d1067aba1000064a804b7f8c1b017c2371b8d7e06342e07b"

# Reviewed open-circle centers from Figure 5. The values are strain fractions,
# read against the printed 0.01 grid at every published 5e-5 s analysis step.
_EXPERIMENTAL_STRAIN = (
    0.0000, -0.0012, -0.0030, -0.0055, -0.0100,
    -0.0134, -0.0164, -0.0186, -0.0210, -0.0224,
    -0.0228, -0.0226, -0.0219, -0.0210, -0.0198,
    -0.0184, -0.0146, -0.0110, -0.0065, -0.0025, 0.0000,
)


def _csv_bytes(header, rows):
    stream = io.StringIO(newline="")
    writer = csv.writer(stream, lineterminator="\n")
    writer.writerow(header)
    writer.writerows(rows)
    return stream.getvalue().encode("utf-8")


def _json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False)
            + "\n").encode("utf-8")


def _sha256(data):
    return "sha256:" + hashlib.sha256(data).hexdigest()


def extract_shimamura_2006():
    raw_rows = []
    peak = max(abs(value) for value in _EXPERIMENTAL_STRAIN)
    normalized_rows = []
    for index, strain in enumerate(_EXPERIMENTAL_STRAIN):
        point_id = f"shimamura_fig5_{index:03d}"
        time_s = index * 0.00005
        locator = "Shimamura_2006_p496_Fig5_experimental_open_circle"
        raw_rows.append((point_id, f"{time_s:.6f}", f"{strain:.4f}", locator))
        normalized_rows.append((
            DATASET_ID, point_id, "calibration", f"{time_s:.6f}",
            f"{abs(strain) / peak:.9f}", "normalized_strain", "0.000025",
            locator,
        ))

    files = {}
    files["raw_digitized.csv"] = _csv_bytes(
        ("point_id", "time_s", "experimental_strain", "source_locator"),
        raw_rows,
    )
    files["normalized.csv"] = _csv_bytes(
        ("dataset_id", "point_id", "partition", "time_s", "expected",
         "unit", "timing_uncertainty_s", "source_locator"),
        normalized_rows,
    )
    files["scalars.csv"] = _csv_bytes(
        ("quantity", "value", "unit", "uncertainty", "source_locator"),
        (
            ("cue_speed", "3.0", "m/s", "", "Shimamura_2006_p496_section7"),
            ("analysis_time_step", "0.00005", "s", "", "Shimamura_2006_p496_section7"),
            ("contact_duration", "0.001000", "s", "0.000025", "Shimamura_2006_p496_Fig5"),
            ("impact_angle", "0", "degree", "", "Shimamura_2006_p496_section7"),
        ),
    )
    files["source_access_audit.json"] = _json_bytes({
        "audited_on": "2026-07-15",
        "doi": DOI,
        "license_status": "free-access-no-redistribution-grant-recorded",
        "outcome": "FREE_ACCESS_NUMERIC_EXTRACTION_ALLOWED_SOURCE_MEDIA_NOT_REDISTRIBUTED",
        "pdf_sha256": "sha256:" + PDF_SHA256,
        "retrieved_on": "2026-07-15",
        "source_media_committed": False,
        "url": PDF_URL,
    })
    files["extraction.json"] = _json_bytes({
        "axis_calibration": {
            "figure": "Figure 5",
            "time_axis": {"pixel_x_at_0": 198.0, "pixel_x_at_10": 657.0,
                          "scale": "s/10000"},
            "strain_axis": {"pixel_y_at_0": 1913.3,
                            "pixel_y_at_minus_0_01": 1883.3},
        },
        "contact_duration_definition": (
            "last sampled time before/at return to the registered zero-strain band"
        ),
        "dataset_id": DATASET_ID,
        "dataset_version": DATASET_VERSION,
        "digitization_residual_strain": 0.0005,
        "impact": "centered",
        "method": "reviewed pixel-to-printed-axis digitization of experimental open circles",
        "point_count": len(_EXPERIMENTAL_STRAIN),
        "rounding_policy": "time to 1e-6 s; strain to 1e-4 strain fraction",
        "schema_version": 1,
        "script": {"module": "tools.physics_validation.extract_shimamura_2006_cue_contact",
                   "version": "1.0.0"},
        "timing_uncertainty_s": 0.000025,
    })
    manifest_files = []
    for path in sorted(files):
        manifest_files.append({"path": path, "sha256": _sha256(files[path])})
    files["manifest.json"] = _json_bytes({
        "acquisition": {
            "license_status": "free-access-no-redistribution-grant-recorded",
            "pdf_sha256": "sha256:" + PDF_SHA256,
            "retrieved_on": "2026-07-15",
            "source_media_committed": False,
            "url": PDF_URL,
        },
        "dataset_id": DATASET_ID,
        "dataset_version": DATASET_VERSION,
        "evidence": {
            "centered_impact": True,
            "cue_speed_m_s": 3.0,
            "experimental_trace_point_count": len(_EXPERIMENTAL_STRAIN),
            "grade": "B",
            "numerical_step_s": 0.00005,
        },
        "files": manifest_files,
        "schema_version": 1,
        "source": {
            "authors": ["Shinsuke Shimamura", "Shigeru Aoki"],
            "doi": DOI,
            "pages": "495-496",
            "title": "Numerical Analysis of Impact between Cue and Billiard Ball (Effect of Tip Structure)",
            "year": 2006,
        },
    })
    return files


def main():
    import argparse
    from pathlib import Path
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generated = extract_shimamura_2006()
    if args.check:
        for name, data in generated.items():
            if (args.package / name).read_bytes() != data:
                raise SystemExit(f"{name} is not byte-identical")
    else:
        args.package.mkdir(parents=True, exist_ok=True)
        for name, data in generated.items():
            (args.package / name).write_bytes(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
