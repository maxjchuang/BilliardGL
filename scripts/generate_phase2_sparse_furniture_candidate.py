#!/usr/bin/env python3
"""Append the approved original sparse furniture to a frozen venue, never regenerate it."""
import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import shutil
from generate_phase2_modular_gltf import Builder, box, merge_geometries
from generate_phase2_venue_gltf import ASSET_NAME, MANIFEST_NAME

ROLE = "venue_sparse_furniture"
PARTS = [
    ("original_training_bench", "seat", (1.30,.42,-4.50), (1.60,.06,.40)),
    ("original_training_bench", "backrest", (1.30,.67,-4.68), (1.60,.36,.04)),
    ("original_training_bench", "left_support", (.54,.195,-4.50), (.08,.39,.40)),
    ("original_training_bench", "right_support", (2.06,.195,-4.50), (.08,.39,.40)),
    ("original_training_bench", "left_back_post", (.54,.62,-4.68), (.08,.46,.04)),
    ("original_training_bench", "right_back_post", (2.06,.62,-4.68), (.08,.46,.04)),
    ("original_low_storage_shelf", "left_side", (-2.18,.36,-4.65), (.04,.72,.30)),
    ("original_low_storage_shelf", "right_side", (-1.42,.36,-4.65), (.04,.72,.30)),
    ("original_low_storage_shelf", "bottom", (-1.80,.02,-4.65), (.80,.04,.30)),
    ("original_low_storage_shelf", "middle", (-1.80,.36,-4.65), (.80,.04,.30)),
    ("original_low_storage_shelf", "top", (-1.80,.70,-4.65), (.80,.04,.30)),
]

def sha(data):
    return "sha256:" + hashlib.sha256(data).hexdigest()

def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()

def append_furniture(source, manifest):
    doc, receipt = copy.deepcopy(source), copy.deepcopy(manifest)
    builder = Builder()
    builder.binary = bytearray(base64.b64decode(doc["buffers"][0]["uri"].split(",",1)[1]))
    for field, attribute in (("bufferViews","views"),("accessors","accessors"),("meshes","meshes"),("nodes","nodes")):
        setattr(builder, attribute, doc[field])
    base = {"buffer_bytes": len(builder.binary), "nodes":len(builder.nodes), "meshes":len(builder.meshes),
            "accessors":len(builder.accessors), "views":len(builder.views),
            "buffer_sha256":sha(builder.binary), "source_gltf_canonical_sha256":sha(canonical(source))}
    parts = []
    for index,(role,name,center,size) in enumerate(PARTS):
        parts.append({"role":role,"part":name,"vertex_range":[index*24,(index+1)*24],
                      "index_range":[index*36,(index+1)*36],
                      "bbox_m":{"min":[center[a]-size[a]/2 for a in range(3)],
                                "max":[center[a]+size[a]/2 for a in range(3)]}})
    contract = {"schema":"root_approved_sparse_furniture_v1", "authorship":"original_procedural_billiardgl",
        "source":"approved camera-venue short design; existing box/merge only", "default_promoted":False,
        "object_count":2,"part_count":len(PARTS),"material_index":5,"role":ROLE,
        "baseline":base,"parts":parts,"lods":{},
        "objects":[{"role":"original_training_bench","bbox_m":{"min":[.50,0,-4.70],"max":[2.10,.85,-4.30]},"operation_clearance_m":2.83932078},
                   {"role":"original_low_storage_shelf","bbox_m":{"min":[-2.20,0,-4.80],"max":[-1.40,.72,-4.50]},"operation_clearance_m":3.03932078}],
        "budgets":{"incremental_draws_per_lod":1,"triangles_per_lod_max":1200,"new_materials":0,"new_textures":0},
        "grounding_status":"pending_real_renderer_shadow_not_visual_complete"}
    geometry = merge_geometries([box(center,size) for _,_,center,size in PARTS])
    for lod in range(3):
        mesh = builder.mesh("MESH_original_bench_and_shelf_LOD%d"%lod, geometry, 5, ROLE, lod)
        builder.meshes[mesh]["extras"]["furniture_parts"] = parts
        node = builder.node("NODE_original_bench_and_shelf_LOD%d"%lod,mesh=mesh,
                            extras={"asset_role":ROLE,"lod":lod,"original_sparse_furniture":True})
        root = receipt["lod_set"]["roots"][str(lod)]
        builder.nodes[root]["children"].append(node)
        contract["lods"][str(lod)]={"node":node,"mesh":mesh,"triangles":132,"draws":1}
        receipt["lod_set"]["rendered_triangles"][str(lod)]+=132
        receipt["lod_set"]["draw_calls_unbatched"][str(lod)]+=1
    doc["buffers"][0]={"byteLength":len(builder.binary),"uri":"data:application/octet-stream;base64,"+base64.b64encode(builder.binary).decode()}
    doc["asset"]["extras"]["remote_furniture_in_scope"]=True
    doc["asset"]["extras"]["sparse_furniture_contract"]="root_approved_sparse_furniture_v1"
    receipt["constraints"]["remote_furniture_present"]=True
    receipt["budgets"]["unique_triangles_all_lods"]+=396
    receipt["budgets"]["geometry_buffer_bytes"]=len(builder.binary)
    receipt["sparse_furniture_contract"]=contract
    return doc,receipt

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_root",type=Path);parser.add_argument("candidate_root",type=Path)
    args=parser.parse_args()
    if args.candidate_root.exists():
        parser.error("candidate root must be new; no overwrite")
    source=json.loads((args.source_root/ASSET_NAME).read_text())
    manifest=json.loads((args.source_root/MANIFEST_NAME).read_text())
    doc,receipt=append_furniture(source,manifest)
    shutil.copytree(args.source_root,args.candidate_root)
    data=(json.dumps(doc,indent=2,sort_keys=True)+"\n").encode()
    (args.candidate_root/ASSET_NAME).write_bytes(data)
    receipt["files"]["gltf"]["sha256"]=sha(data)
    receipt["candidate_provenance"]["sparse_furniture_source_root"]=str(args.source_root.resolve())
    receipt["candidate_provenance"]["sparse_furniture_wrapper_sha256"]=sha(Path(__file__).read_bytes())
    (args.candidate_root/MANIFEST_NAME).write_text(json.dumps(receipt,indent=2,sort_keys=True)+"\n")
    print(json.dumps({"candidate_root":str(args.candidate_root),"venue_sha256":sha(data),"contract":receipt["sparse_furniture_contract"]},indent=2))

if __name__=="__main__":
    main()
