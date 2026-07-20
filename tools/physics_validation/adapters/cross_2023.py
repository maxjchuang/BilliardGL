from ..reference_adapter import ReferenceAdaptation, ReferenceLimitation, _fail


def adapt_cross_2023(package, split, points):
    del split
    if points:
        _fail(
            "UNREVIEWED_NUMERIC_ADMISSION",
            "Cross numeric points require an audited lawful full text before adaptation",
        )
    dataset_id = package.manifest["dataset_id"]
    limitations = (
        ReferenceLimitation(
            dataset_id, "full_text_not_acquired", "source_full_text",
            "SAGE, OpenAlex, OpenAIRE, Semantic Scholar, and repository searches expose metadata/abstract only; no lawful full text was acquired.",
            "Acquire and audit the version of record or an authorized author manuscript, recording its digest and license.",
        ),
        ReferenceLimitation(
            dataset_id, "experimental_markers_not_admitted", "complete_numeric_dataset",
            "No experimental marker/table value is admitted from an abstract, snippet, or the distinct Cross 2008 paper.",
            "Digitize every experimental 2023/2025 marker twice after full-text acquisition and distinguish it from model curves.",
        ),
        ReferenceLimitation(
            dataset_id, "cue_speed_to_power_mapping_missing", "cue_input_mapping",
            "The versioned production power scale is a compatibility mapping, not an independently evidenced conversion from user shot power to physical cue speed.",
            "Publish and validate a mechanical or synchronized player-input mapping from user control to cue speed.",
        ),
    )
    return ReferenceAdaptation((), limitations)
