from ..reference_adapter import (
    ReferenceAdaptation,
    ReferenceLimitation,
    _fail,
)


_ENGINE_BALL_DIAMETER_CM = 5.715
_MATERIAL_BY_SERIES = {
    "billiard_alpha1": "billiard",
    "billiard_delta2": "billiard",
    "brass_alpha1": "brass",
    "rubber_delta2": "rubber",
    "rubber_lambda2": "rubber",
    "steel_alpha1": "steel",
    "steel_beta1": "steel",
}


def _admission_limitations(dataset_id):
    return (
        ReferenceLimitation(
            dataset_id,
            "author_data_request_pending",
            "source_table_availability",
            "The article states that data are available on request, but no author request has been sent without user authorization.",
            "Send and archive an authorized author-data request outside the repository, then version any received table separately.",
        ),
        ReferenceLimitation(
            dataset_id,
            "version_record_pdf_audit_pending",
            "source_version_audit",
            "Publisher automation challenge prevented a complete version-of-record PDF audit; official high-resolution figures and article metadata were audited instead.",
            "Lawfully obtain and manually inspect the version-of-record PDF, retaining its digest without redistributing it.",
        ),
    )


def adapt_domenech_2023(package, split, points):
    del split
    dataset_id = package.manifest["dataset_id"]
    materials = package.manifest["apparatus"]["materials"]
    series_by_case = {}
    for point in points:
        material = _MATERIAL_BY_SERIES.get(point.series_id)
        if material is None:
            _fail("UNKNOWN_SOURCE_MATERIAL", f"series {point.series_id} has no material mapping")
        series_by_case.setdefault(point.case_id, set()).add(material)
    mixed = [case_id for case_id, case_materials in series_by_case.items()
             if len(case_materials) != 1]
    if mixed:
        _fail("MIXED_MATERIAL_CASE", f"cases mix source materials: {sorted(mixed)}")

    present_series = sorted({point.series_id for point in points})
    limitations = list(_admission_limitations(dataset_id))
    for series_id in present_series:
        material = _MATERIAL_BY_SERIES[series_id]
        source = materials[material]
        if abs(source["diameter_cm"] - _ENGINE_BALL_DIAMETER_CM) <= 1e-9:
            _fail(
                "UNIMPLEMENTED_EXPRESSIBLE_GEOMETRY",
                f"series {series_id} unexpectedly matches production geometry",
            )
        limitations.append(ReferenceLimitation(
            dataset_id,
            f"{series_id}_source_geometry_not_expressible",
            "source_geometry_not_expressible",
            (
                f"{series_id} used {material} spheres of diameter "
                f"{source['diameter_cm']} cm and mass {source['mass_g']} g on PVC; "
                f"production uses fixed {_ENGINE_BALL_DIAMETER_CM} cm Pool geometry, "
                "cloth contact, and cannot install the source material properties."
            ),
            (
                "Add scenario-level ball geometry, mass, material/contact, and support-surface "
                "parameters without changing global production defaults."
            ),
            tuple(sorted(
                point.point_id for point in points if point.series_id == series_id)),
        ))
    return ReferenceAdaptation((), tuple(limitations))
