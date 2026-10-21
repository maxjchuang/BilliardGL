#!/usr/bin/env python3
"""Offline validation for the Focused Practice Hall venue candidate."""

import argparse
import base64
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import zlib

from generate_phase2_venue_gltf import (
    ASSET_NAME, CAMERAS, MANIFEST_NAME, PREVIEW_NAME, SHARED_TEXTURES,
)
from generate_phase2_pbr_package import texture_filename


REQUIRED_ATTRIBUTES = {"POSITION", "NORMAL", "TANGENT", "TEXCOORD_0"}
MATERIAL_NAMES = {
    "MAT_venue_floor", "MAT_venue_wall", "MAT_venue_ceiling",
    "MAT_venue_gunmetal", "MAT_venue_acoustic", "MAT_venue_walnut_trim",
}
ALLOWED_ROLES = {
    "venue_floor", "venue_wall", "venue_ceiling", "venue_acoustic_panel",
    "venue_acoustic_band", "venue_lamp_cable", "venue_lamp_rack", "venue_wall_trim",
}
FORBIDDEN_NAME_TOKENS = ("bench", "chair", "sofa", "wardrobe", "sponsor",
                         "brand", "logo", "billboard", "competitor")


def _sha256(payload):
    return "sha256:" + hashlib.sha256(payload).hexdigest()


def _finite_vector(value, size):
    return isinstance(value, list) and len(value) == size and all(
        isinstance(item, (int, float)) and not isinstance(item, bool) and math.isfinite(item)
        for item in value)


def _close_vector(first, second, tolerance=1e-7):
    return _finite_vector(first, len(second)) and all(
        abs(first[index] - second[index]) <= tolerance for index in range(len(second)))


def _material_has_map(value):
    if isinstance(value, dict):
        return any(key.endswith("Texture") for key in value) or any(
            _material_has_map(child) for child in value.values())
    return isinstance(value, list) and any(_material_has_map(child) for child in value)


def _decode_buffer(document, errors):
    buffers = document.get("buffers", [])
    prefix = "data:application/octet-stream;base64,"
    if len(buffers) != 1 or not str(buffers[0].get("uri", "")).startswith(prefix):
        errors.append("venue geometry must use one embedded buffer")
        return b""
    try:
        payload = base64.b64decode(buffers[0]["uri"][len(prefix):], validate=True)
    except (ValueError, TypeError):
        errors.append("venue embedded buffer is invalid base64")
        return b""
    if buffers[0].get("byteLength") != len(payload):
        errors.append("venue embedded buffer byteLength differs")
    return payload


def _accessor_values(document, payload, accessor_index, errors, label):
    accessors, views = document.get("accessors", []), document.get("bufferViews", [])
    if not isinstance(accessor_index, int) or not 0 <= accessor_index < len(accessors):
        errors.append(label + " references invalid accessor")
        return []
    accessor = accessors[accessor_index]
    view_index = accessor.get("bufferView")
    if not isinstance(view_index, int) or not 0 <= view_index < len(views):
        errors.append(label + " references invalid bufferView")
        return []
    components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}.get(accessor.get("type"))
    formats = {5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
    format_info = formats.get(accessor.get("componentType"))
    count = accessor.get("count")
    if components is None or format_info is None or not isinstance(count, int) or count <= 0:
        errors.append(label + " accessor format is invalid")
        return []
    code, component_size = format_info
    view = views[view_index]
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    size = count * components * component_size
    if not isinstance(offset, int) or offset < 0 or offset + size > len(payload):
        errors.append(label + " accessor exceeds buffer")
        return []
    values = struct.unpack_from("<%d%s" % (count * components, code), payload, offset)
    if components == 1:
        return list(values)
    return [list(values[index:index + components])
            for index in range(0, len(values), components)]


def _descendant_stats(document, root_index):
    nodes, meshes = document.get("nodes", []), document.get("meshes", [])
    pending, visited, triangles, draws = [root_index], set(), 0, 0
    while pending:
        index = pending.pop()
        if not isinstance(index, int) or not 0 <= index < len(nodes) or index in visited:
            continue
        visited.add(index)
        node = nodes[index]
        pending.extend(node.get("children", []))
        mesh_index = node.get("mesh")
        if isinstance(mesh_index, int) and 0 <= mesh_index < len(meshes):
            triangles += meshes[mesh_index].get("extras", {}).get("triangle_count", 0)
            draws += 1
    return triangles, draws


def _rotate(point, quaternion):
    if quaternion is None:
        return tuple(point)
    x, y, z, w = quaternion
    qvec = (x, y, z)
    cross1 = (qvec[1] * point[2] - qvec[2] * point[1],
              qvec[2] * point[0] - qvec[0] * point[2],
              qvec[0] * point[1] - qvec[1] * point[0])
    cross2 = (qvec[1] * cross1[2] - qvec[2] * cross1[1],
              qvec[2] * cross1[0] - qvec[0] * cross1[2],
              qvec[0] * cross1[1] - qvec[1] * cross1[0])
    return tuple(point[index] + 2 * (w * cross1[index] + cross2[index])
                 for index in range(3))


def _segment_aabb_intersection(origin, target, minimum, maximum):
    direction = [target[index] - origin[index] for index in range(3)]
    enter, exit_value = 0.0, 1.0
    for axis in range(3):
        if abs(direction[axis]) < 1e-10:
            if origin[axis] < minimum[axis] or origin[axis] > maximum[axis]:
                return None
            continue
        first = (minimum[axis] - origin[axis]) / direction[axis]
        second = (maximum[axis] - origin[axis]) / direction[axis]
        if first > second:
            first, second = second, first
        enter, exit_value = max(enter, first), min(exit_value, second)
        if enter > exit_value:
            return None
    return enter


def _read_preview(path, errors):
    try:
        payload = path.read_bytes()
    except OSError as error:
        errors.append("cannot read preview: " + str(error))
        return None, b""
    if payload[:8] != b"\x89PNG\r\n\x1a\n" or len(payload) < 33:
        errors.append("four-camera preview is not PNG")
        return None, payload
    width, height, depth, color_type = struct.unpack(">IIBB", payload[16:26])
    if (width, height, depth, color_type) != (1536, 864, 8, 6):
        errors.append("four-camera preview must be 1536x864 RGBA8")
    compressed = bytearray()
    offset = 8
    while offset + 12 <= len(payload):
        length = struct.unpack_from(">I", payload, offset)[0]
        kind = payload[offset + 4:offset + 8]
        data = payload[offset + 8:offset + 8 + length]
        if kind == b"IDAT":
            compressed.extend(data)
        if kind == b"IEND":
            break
        offset += length + 12
    try:
        raw = zlib.decompress(compressed)
    except zlib.error:
        errors.append("four-camera preview pixels are invalid")
        return None, payload
    stride = width * 4
    pixels = bytearray(width * height * 4)
    for y in range(height):
        if raw[y * (stride + 1)] != 0:
            errors.append("four-camera preview must use deterministic filter 0")
            return None, payload
        pixels[y * stride:(y + 1) * stride] = raw[
            y * (stride + 1) + 1:(y + 1) * (stride + 1)]
    return (width, height, pixels), payload


def _validate_sparse_furniture(document, manifest, payload, errors):
    """Strict candidate-only opt-in; reconstruct the frozen 8300/ground baseline."""
    from generate_phase2_sparse_furniture_candidate import PARTS, ROLE, canonical
    from generate_phase2_modular_gltf import box, merge_geometries
    contract = manifest.get("sparse_furniture_contract", {})
    base = contract.get("baseline", {})
    if contract.get("schema") != "root_approved_sparse_furniture_v1" or \
            contract.get("authorship") != "original_procedural_billiardgl" or \
            contract.get("default_promoted") is not False or contract.get("object_count") != 2 or \
            contract.get("part_count") != 11 or contract.get("material_index") != 5 or \
            contract.get("role") != ROLE or not contract.get("source") or \
            contract.get("budgets") != {"incremental_draws_per_lod":1,"triangles_per_lod_max":1200,"new_materials":0,"new_textures":0}:
        errors.append("sparse furniture explicit role/count/material/budget contract invalid")
    expected_objects = [
        {"role":"original_training_bench","bbox_m":{"min":[.50,0,-4.70],"max":[2.10,.85,-4.30]},"operation_clearance_m":2.83932078},
        {"role":"original_low_storage_shelf","bbox_m":{"min":[-2.20,0,-4.80],"max":[-1.40,.72,-4.50]},"operation_clearance_m":3.03932078}]
    if contract.get("objects") != expected_objects:
        errors.append("sparse furniture exact approved bounds/clearance differ")
    parts = [{"role":role,"part":name,"vertex_range":[i*24,(i+1)*24],"index_range":[i*36,(i+1)*36],
              "bbox_m":{"min":[center[a]-size[a]/2 for a in range(3)],"max":[center[a]+size[a]/2 for a in range(3)]}}
             for i,(role,name,center,size) in enumerate(PARTS)]
    if contract.get("parts") != parts:
        errors.append("sparse furniture authored part-range/bbox contract differs")
    geometry = merge_geometries([box(center,size) for _,_,center,size in PARTS])
    allowed_names = set()
    expected_lods = {}
    for lod in range(3):
        mesh_index, node_index = 28+lod, 93+lod
        expected_lods[str(lod)]={"node":node_index,"mesh":mesh_index,"triangles":132,"draws":1}
        allowed_names.add("NODE_original_bench_and_shelf_LOD%d"%lod)
        try:
            mesh, node = document["meshes"][mesh_index], document["nodes"][node_index]
            primitive=mesh["primitives"][0]
            if primitive.get("material") != 5 or mesh.get("extras",{}).get("furniture_parts") != parts or \
                    mesh.get("extras",{}).get("asset_role") != ROLE or \
                    node != {"name":"NODE_original_bench_and_shelf_LOD%d"%lod,"mesh":mesh_index,
                             "extras":{"asset_role":ROLE,"lod":lod,"original_sparse_furniture":True}}:
                errors.append("sparse furniture node/mesh/material binding differs")
            for semantic, values in zip(("POSITION","NORMAL","TANGENT","TEXCOORD_0"),geometry[:4]):
                actual=_accessor_values(document,payload,primitive["attributes"][semantic],errors,"sparse "+semantic)
                if len(actual)!=len(values) or any(not _close_vector(a,b,2e-6) for a,b in zip(actual,values)):
                    errors.append("sparse furniture actual %s differs from authored parts"%semantic)
            if _accessor_values(document,payload,primitive["indices"],errors,"sparse indices") != geometry[4]:
                errors.append("sparse furniture actual part index ranges differ")
        except (KeyError,IndexError,TypeError):
            errors.append("sparse furniture required merged mesh/node missing")
    if contract.get("lods") != expected_lods:
        errors.append("sparse furniture each LOD must bind exactly one 132-triangle draw")
    if any(base.get(key)!=value for key,value in {"nodes":93,"meshes":28,"accessors":140,"views":140}.items()):
        errors.append("sparse furniture baseline ranges differ")
    try:
        restored=copy.deepcopy(document)
        size=base["buffer_bytes"]
        if not isinstance(size,int) or size<=0 or _sha256(payload[:size])!=base.get("buffer_sha256"):
            raise ValueError("prefix")
        restored["buffers"][0]={"byteLength":size,"uri":"data:application/octet-stream;base64,"+base64.b64encode(payload[:size]).decode()}
        for field,count in (("nodes",93),("meshes",28),("accessors",140),("bufferViews",140)):
            if len(restored[field])!=count+(3 if field in ("nodes","meshes") else 15):
                raise ValueError("range")
            restored[field]=restored[field][:count]
        for lod in range(3):
            root=manifest["lod_set"]["roots"][str(lod)]
            if restored["nodes"][root]["children"][-1]!=93+lod:
                raise ValueError("root")
            restored["nodes"][root]["children"].pop()
        restored["asset"]["extras"]["remote_furniture_in_scope"]=False
        restored["asset"]["extras"].pop("sparse_furniture_contract",None)
        frozen="sha256:5b925e2c1aac0dd56ebc6f1b6e5b6c33d9d93f4ee978ccecfb275178a494090b"
        if _sha256(canonical(restored))!=frozen or base.get("source_gltf_canonical_sha256")!=frozen:
            raise ValueError("frozen")
    except (KeyError,IndexError,TypeError,ValueError):
        errors.append("sparse furniture must preserve exact approved room/material/geometry baseline")
    return allowed_names


def validate(document, manifest, asset_dir, verify_hashes=True):
    errors = []
    asset_dir = Path(asset_dir)
    extras = document.get("asset", {}).get("extras", {})
    sparse = manifest.get("sparse_furniture_contract") is not None
    allowed_roles = ALLOWED_ROLES | ({"venue_sparse_furniture"} if sparse else set())
    metadata = {
        "asset_id": "focused_practice_hall_venue_v1",
        "authorship": "original_procedural_billiardgl",
        "external_source_assets": False,
        "production_candidate": True,
        "final_high_mesh": False,
        "unit": "metre", "up_axis": "+Y", "forward_axis": "+Z",
        "handedness": "right", "remote_furniture_in_scope": sparse,
        "brand_text_present": False,
    }
    if document.get("asset", {}).get("version") != "2.0":
        errors.append("venue must be glTF 2.0")
    for key, expected in metadata.items():
        if extras.get(key) != expected:
            errors.append("asset.extras.%s must be %r" % (key, expected))
    if document.get("extensionsUsed") != ["KHR_texture_transform", "KHR_materials_clearcoat"] or \
            document.get("extensionsRequired"):
        errors.append("venue extensions must be optional texture-transform/clearcoat")

    payload = _decode_buffer(document, errors)
    allowed_furniture_names = _validate_sparse_furniture(document,manifest,payload,errors) if sparse else set()
    if sparse and extras.get("sparse_furniture_contract") != "root_approved_sparse_furniture_v1":
        errors.append("sparse furniture asset opt-in version missing")
    for index, view in enumerate(document.get("bufferViews", [])):
        offset, length = view.get("byteOffset", 0), view.get("byteLength")
        if not isinstance(offset, int) or offset % 4 or not isinstance(length, int) or \
                length <= 0 or offset + length > len(payload):
            errors.append("bufferView[%d] is invalid or unaligned" % index)

    expected_uris = [texture_filename(texture_set, channel)
                     for texture_set, channel in SHARED_TEXTURES]
    images, textures = document.get("images", []), document.get("textures", [])
    if [image.get("uri") for image in images] != expected_uris or len(textures) != 9:
        errors.append("venue must reuse exactly the approved nine PBR texture files")
    if any("/" in str(uri) or "\\" in str(uri) or str(uri).startswith("http")
           for uri in expected_uris):
        errors.append("venue texture URIs must remain local flat filenames")
    for index, image in enumerate(images):
        if image.get("mimeType") != "image/png" or \
                image.get("extras", {}).get("shared_from_package") != \
                "focused_practice_hall_pbr_v1" or \
                image.get("extras", {}).get("original_procedural") is not True:
            errors.append("image[%d] shared-source metadata is invalid" % index)
    if len(document.get("samplers", [])) != 1 or \
            document["samplers"][0].get("wrapS") != 10497:
        errors.append("venue shared textures require one repeat sampler")

    materials = document.get("materials", [])
    material_names = [material.get("name") for material in materials]
    expected_materials = MATERIAL_NAMES
    contract = manifest.get("independent_matte_ground_contract")
    if contract is not None:
        provenance = manifest.get("candidate_provenance", {})
        valid_contract = isinstance(contract, dict) and (
            contract.get("schema") == "root_approved_matte_ground_v1" and
            type(contract.get("material_index")) is int and contract["material_index"] == 0 and
            contract.get("material_name") == "MAT_venue_matte_ground" and
            contract.get("linear_factor") == [0.028, 0.032, 0.036] and
            contract.get("metallic") == 0.0 and contract.get("roughness") == 0.92 and
            contract.get("maps") == "none" and
            isinstance(contract.get("provenance"), str) and bool(contract["provenance"].strip()) and
            provenance.get("default_promoted") is False and
            provenance.get("approved_floor_material") == "MAT_venue_matte_ground")
        floor = materials[0] if materials else {}
        pbr = floor.get("pbrMetallicRoughness", {})
        if not valid_contract or floor.get("name") != "MAT_venue_matte_ground" or \
                not _finite_vector(pbr.get("baseColorFactor"), 4) or \
                pbr.get("baseColorFactor") != [0.028, 0.032, 0.036, 1.0] or \
                type(pbr.get("metallicFactor")) not in (int, float) or pbr["metallicFactor"] != 0.0 or \
                pbr.get("roughnessFactor") != 0.92 or _material_has_map(floor):
            errors.append("independent matte-ground must match explicit approved linear/no-map candidate contract")
        expected_materials = (MATERIAL_NAMES - {"MAT_venue_floor"}) | {"MAT_venue_matte_ground"}
    if len(materials) != 6 or set(material_names) != expected_materials:
        errors.append("venue must contain exactly six approved material slots")
    for index, material in enumerate(materials):
        relative_ev = material.get("extras", {}).get("background_relative_to_table_ev")
        if not isinstance(relative_ev, (int, float)) or not -3.0 <= relative_ev <= -2.0:
            errors.append("material[%d] background level must be 2-3 EV below table" % index)
        pbr = material.get("pbrMetallicRoughness", {})
        if not _finite_vector(pbr.get("baseColorFactor"), 4):
            errors.append("material[%d] baseColorFactor is invalid" % index)
        if material.get("normalTexture") and material.get("extras", {}).get(
                "normal_convention") != "opengl_positive_y":
            errors.append("material[%d] normal convention is invalid" % index)
        if material.get("occlusionTexture") and material.get("extras", {}).get(
                "orm_channels") != "r_ao_g_roughness_b_metallic":
            errors.append("material[%d] ORM convention is invalid" % index)

    meshes = document.get("meshes", [])
    unique_triangles = 0
    mesh_positions = {}
    for mesh_index, mesh in enumerate(meshes):
        primitives = mesh.get("primitives", [])
        if len(primitives) != 1:
            errors.append("mesh[%d] must contain one primitive" % mesh_index)
            continue
        primitive = primitives[0]
        attributes = primitive.get("attributes", {})
        if set(attributes) != REQUIRED_ATTRIBUTES:
            errors.append("mesh[%d] vertex layout is invalid" % mesh_index)
        values = {semantic: _accessor_values(document, payload, attributes.get(semantic), errors,
                                             "mesh[%d] %s" % (mesh_index, semantic))
                  for semantic in REQUIRED_ATTRIBUTES}
        indices = _accessor_values(document, payload, primitive.get("indices"), errors,
                                   "mesh[%d] indices" % mesh_index)
        count = len(values["POSITION"])
        if any(len(values[semantic]) != count for semantic in REQUIRED_ATTRIBUTES):
            errors.append("mesh[%d] vertex counts differ" % mesh_index)
        if len(indices) % 3 or (indices and max(indices) >= count):
            errors.append("mesh[%d] triangle indices are invalid" % mesh_index)
        for normal in values["NORMAL"]:
            if not _finite_vector(normal, 3) or abs(math.sqrt(sum(v * v for v in normal)) - 1) > 2e-5:
                errors.append("mesh[%d] has non-unit normal" % mesh_index)
                break
        for tangent in values["TANGENT"]:
            if not _finite_vector(tangent, 4) or \
                    abs(math.sqrt(sum(v * v for v in tangent[:3])) - 1) > 2e-5 or \
                    tangent[3] not in (-1.0, 1.0):
                errors.append("mesh[%d] has invalid tangent" % mesh_index)
                break
        triangles_count = len(indices) // 3
        if mesh.get("extras", {}).get("triangle_count") != triangles_count:
            errors.append("mesh[%d] triangle metadata differs" % mesh_index)
        if mesh.get("extras", {}).get("asset_role") not in allowed_roles:
            errors.append("mesh[%d] has unapproved venue role" % mesh_index)
        unique_triangles += triangles_count
        mesh_positions[mesh_index] = values["POSITION"]

    nodes = document.get("nodes", [])
    names = [node.get("name") for node in nodes]
    if len(names) != len(set(names)):
        errors.append("venue node names must be unique")
    if any(name not in allowed_furniture_names and any(token in str(name).lower() for token in FORBIDDEN_NAME_TOKENS) for name in names):
        errors.append("venue contains furniture, brand, or competitor node naming")
    for index, node in enumerate(nodes):
        if "scale" in node and node.get("scale") != [1.0, 1.0, 1.0]:
            errors.append("node[%d] has non-unit scale" % index)
        if "rotation" in node:
            rotation = node["rotation"]
            if not _finite_vector(rotation, 4) or \
                    abs(math.sqrt(sum(v * v for v in rotation)) - 1) > 1e-6:
                errors.append("node[%d] rotation is not a unit quaternion" % index)
        for child in node.get("children", []):
            if not isinstance(child, int) or not 0 <= child < len(nodes):
                errors.append("node[%d] has invalid child" % index)

    lod_set = manifest.get("lod_set", {})
    roots = lod_set.get("roots", {})
    actual_triangles, actual_draws = {}, {}
    for lod in ("0", "1", "2"):
        root = roots.get(lod)
        if not isinstance(root, int) or not 0 <= root < len(nodes) or \
                nodes[root].get("extras", {}).get("lod_set") != "venue":
            errors.append("venue LOD%s root is invalid" % lod)
            continue
        actual_triangles[lod], actual_draws[lod] = _descendant_stats(document, root)
    if actual_triangles != lod_set.get("rendered_triangles") or \
            actual_draws != lod_set.get("draw_calls_unbatched"):
        errors.append("venue LOD triangle/draw manifest differs from glTF")
    if set(actual_triangles) == {"0", "1", "2"} and not (
            actual_triangles["0"] > actual_triangles["1"] > actual_triangles["2"] > 0):
        errors.append("venue LOD triangles must strictly decrease")

    by_name = {node.get("name"): (index, node) for index, node in enumerate(nodes)}
    cameras_manifest = manifest.get("cameras", {})
    for name, camera in CAMERAS.items():
        entry = cameras_manifest.get(name, {})
        if any(entry.get(key) != value for key, value in camera.items()):
            errors.append(name + " camera parameters differ from approved composition")
        position = by_name.get("SOCKET_camera_" + name)
        target = by_name.get("SOCKET_camera_" + name + "_target")
        if position is None or target is None or \
                not _close_vector(position[1].get("translation"), camera["eye_m"]) or \
                not _close_vector(target[1].get("translation"), camera["target_m"]):
            errors.append(name + " camera sockets differ")
        elif entry.get("nodes") != {"position": position[0], "target": target[0]}:
            errors.append(name + " manifest camera node mapping differs")
    tactical_margin = (CAMERAS["tactical_overhead"]["orthographic_vertical_span_m"] - 2.54) / \
                      (2 * CAMERAS["tactical_overhead"]["orthographic_vertical_span_m"])
    if not 0.08 <= tactical_margin <= 0.12:
        errors.append("tactical overhead playfield safety margin must be 8-12 percent")
    visibility = manifest.get("visibility_overrides", {})
    if visibility.get("tactical_overhead") != ["hide_tactical_overhead"]:
        errors.append("tactical overhead must hide only its approved obstruction group")

    # Build transformed AABBs and prove venue nodes do not cross protected camera rays.
    world_bounds = []
    for node in nodes:
        mesh_index = node.get("mesh")
        role = node.get("extras", {}).get("asset_role")
        if not isinstance(mesh_index, int) or role not in allowed_roles:
            continue
        translation = node.get("translation", [0.0, 0.0, 0.0])
        transformed = []
        for point in mesh_positions.get(mesh_index, []):
            rotated = _rotate(point, node.get("rotation"))
            transformed.append([rotated[axis] + translation[axis] for axis in range(3)])
        if transformed:
            world_bounds.append((node.get("name"), node.get("extras", {}).get("visibility_group"),
                                 [min(point[axis] for point in transformed) for axis in range(3)],
                                 [max(point[axis] for point in transformed) for axis in range(3)]))
    anchors = [anchor for group in manifest.get("protected_visibility_anchors_m", {}).values()
               for anchor in group]
    if len(anchors) != 8:
        errors.append("table/lamp/cue protected anchors are incomplete")
    for camera_name, camera in CAMERAS.items():
        hidden = set(visibility.get(camera_name, []))
        for anchor in anchors:
            for node_name, group, minimum, maximum in world_bounds:
                if group in hidden:
                    continue
                hit = _segment_aabb_intersection(camera["eye_m"], anchor, minimum, maximum)
                if hit is not None and 0.002 < hit < 0.985:
                    errors.append("%s is occluded by %s from %s" %
                                  (anchor, node_name, camera_name))
                    break

    budgets = manifest.get("budgets", {})
    if budgets.get("unique_triangles_all_lods") != unique_triangles or \
            budgets.get("geometry_buffer_bytes") != len(payload) or \
            budgets.get("materials") != 6 or budgets.get("texture_references") != 9:
        errors.append("venue geometry/material/texture budget differs")
    if unique_triangles > 1500 or actual_triangles.get("0", 100000) > 5000 or \
            actual_triangles.get("1", 100000) > 1500 or actual_triangles.get("2", 100000) > 500 or \
            any(draw > 64 for draw in actual_draws.values()):
        errors.append("venue exceeds triangle or draw-call budget")
    if budgets.get("incremental_texture_gpu_bytes_when_core_pbr_loaded") != 0 or \
            budgets.get("standalone_shared_texture_gpu_bytes_full_mips") != 6291444:
        errors.append("venue shared texture residency budget is invalid")

    constraints = manifest.get("constraints", {})
    expected_constraints = {
        "background_relative_to_table_ev_range": [-3.0, -2.0],
        "four_camera_occlusion_check_required": True,
        "shared_original_pbr_only": True, "new_texture_assets": 0,
        "remote_furniture_present": sparse,
        "brand_or_competitor_content_present": False,
    }
    for key, expected in expected_constraints.items():
        if constraints.get(key) != expected:
            errors.append("venue constraint %s must be %r" % (key, expected))

    preview, preview_bytes = _read_preview(asset_dir / PREVIEW_NAME, errors)
    if preview is not None:
        width, height, pixels = preview
        for panel_y in (0, 432):
            for panel_x in (0, 768):
                teal_pixels = 0
                non_empty = 0
                for y in range(panel_y, panel_y + 432):
                    for x in range(panel_x, panel_x + 768):
                        offset = (y * width + x) * 4
                        color = tuple(pixels[offset:offset + 3])
                        teal_pixels += color == (20, 118, 132)
                        non_empty += color != (12, 14, 16)
                if teal_pixels < 800 or non_empty < 432 * 768 * 0.25:
                    errors.append("each camera preview must show readable table and venue layers")
    if manifest.get("files", {}).get("four_camera_preview", {}).get("runtime") is not False:
        errors.append("four-camera preview must remain review-only")

    if verify_hashes:
        files = manifest.get("files", {})
        gltf_path = asset_dir / ASSET_NAME
        if files.get("gltf", {}).get("sha256") != _sha256(gltf_path.read_bytes()):
            errors.append("venue glTF SHA-256 differs")
        if files.get("four_camera_preview", {}).get("sha256") != _sha256(preview_bytes):
            errors.append("venue preview SHA-256 differs")
        shared = files.get("shared_pbr_textures", {})
        for texture_set, channel in SHARED_TEXTURES:
            path = asset_dir / texture_filename(texture_set, channel)
            if not path.is_file() or shared.get(texture_set, {}).get(channel, {}).get("sha256") != \
                    _sha256(path.read_bytes()):
                errors.append("shared %s %s texture SHA-256 differs" % (texture_set, channel))

    report = {
        "unique_triangles": unique_triangles,
        "lod_triangles": actual_triangles,
        "lod_draws": actual_draws,
        "materials": len(materials), "textures": len(textures),
        "cameras": len(CAMERAS), "geometry_bytes": len(payload),
    }
    return errors, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("asset", nargs="?", type=Path,
                        default=Path("assets/modern/phase2") / ASSET_NAME)
    parser.add_argument("--manifest", type=Path)
    arguments = parser.parse_args()
    manifest_path = arguments.manifest or arguments.asset.with_name(MANIFEST_NAME)
    try:
        document = json.loads(arguments.asset.read_text(encoding="utf-8"))
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print("venue validation failed: " + str(error), file=sys.stderr)
        return 2
    errors, report = validate(document, manifest, arguments.asset.parent)
    if errors:
        for error in errors:
            print("error: " + error, file=sys.stderr)
        return 1
    print("venue glTF valid: unique_triangles=%d LOD0_triangles=%d LOD0_draws=%d "
          "materials=%d textures=%d cameras=%d geometry_bytes=%d" %
          (report["unique_triangles"], report["lod_triangles"]["0"],
           report["lod_draws"]["0"], report["materials"], report["textures"],
           report["cameras"], report["geometry_bytes"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
