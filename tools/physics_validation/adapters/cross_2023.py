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
            "The production shot path has no independently evidenced conversion from physical cue speed/mass/offset to user shot power.",
            "Publish an independently validated mechanical mapping and add real vertical tip-offset contact physics.",
        ),
        ReferenceLimitation(
            dataset_id, "cue_contact_regime_telemetry_missing", "cue_contact_regime",
            "Production traces do not expose cue-tip/ball tangential relative velocity, normal and tangential impulse, or contact-state transitions.",
            "Instrument cue contact kinematics and impulses, then classify stick/slip transitions from signed tangential relative velocity without changing production dynamics.",
        ),
    )
    return ReferenceAdaptation((), limitations)
