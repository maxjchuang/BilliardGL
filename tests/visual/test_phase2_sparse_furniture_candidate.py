import base64
import copy
import json
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT / "scripts"))
from generate_phase2_sparse_furniture_candidate import append_furniture, canonical
from validate_phase2_venue_gltf import validate


class SparseFurnitureTests(unittest.TestCase):
    def fixture(self):
        root=ROOT / "assets/modern/phase2"
        d=json.loads((root/"focused_practice_hall_venue_v1.gltf").read_text())
        m=json.loads((root/"focused_practice_hall_venue_v1.asset.json").read_text())
        floor=d["materials"][0];floor["name"]="MAT_venue_matte_ground"
        floor.pop("normalTexture",None);floor.pop("occlusionTexture",None)
        floor["pbrMetallicRoughness"]={"baseColorFactor":[.028,.032,.036,1.0],"metallicFactor":0.0,"roughnessFactor":.92}
        floor["extras"]["shared_texture_reuse"]="none_independent_authored_matte_ground"
        m["candidate_provenance"]={"default_promoted":False,"approved_floor_material":"MAT_venue_matte_ground"}
        m["independent_matte_ground_contract"]={"schema":"root_approved_matte_ground_v1","material_index":0,
            "material_name":"MAT_venue_matte_ground","linear_factor":[.028,.032,.036],"metallic":0.0,"roughness":.92,
            "maps":"none","provenance":"root-approved-existing-focused-practice-hall-calibration"}
        return append_furniture(d,m)

    def errors(self,d,m):
        return validate(d,m,ROOT/"assets/modern/phase2",verify_hashes=False)[0]

    def test_exact_candidate_and_repeat_are_valid(self):
        d,m=self.fixture(); other,receipt=self.fixture()
        self.assertEqual(canonical(d),canonical(other));self.assertEqual(canonical(m),canonical(receipt))
        self.assertEqual(self.errors(d,m),[])
        self.assertEqual(m["lod_set"]["draw_calls_unbatched"],{"0":35,"1":35,"2":15})

    def test_legacy_furniture_stays_rejected(self):
        d,m=self.fixture();m.pop("sparse_furniture_contract")
        self.assertTrue(any("furniture" in e or "unapproved venue role" in e for e in self.errors(d,m)))

    def test_wrong_count_and_part_ranges_rejected(self):
        for field,value in (("object_count",3),("part_count",10)):
            d,m=self.fixture();m["sparse_furniture_contract"][field]=value
            self.assertTrue(any("contract invalid" in e for e in self.errors(d,m)))
        d,m=self.fixture();m["sparse_furniture_contract"]["parts"][0]["index_range"][1]=72
        self.assertTrue(any("part-range" in e for e in self.errors(d,m)))

    def test_actual_geometry_cannot_fake_clearance_receipt(self):
        d,m=self.fixture();b=bytearray(base64.b64decode(d["buffers"][0]["uri"].split(",",1)[1]))
        accessor=d["accessors"][d["meshes"][28]["primitives"][0]["attributes"]["POSITION"]]
        offset=d["bufferViews"][accessor["bufferView"]]["byteOffset"]
        struct.pack_into("<f",b,offset+8,-2.0)
        d["buffers"][0]["uri"]="data:application/octet-stream;base64,"+base64.b64encode(b).decode()
        self.assertTrue(any("actual POSITION" in e for e in self.errors(d,m)))

    def test_wrong_role_or_budget_rejected_without_rename_bypass(self):
        d,m=self.fixture();d["nodes"][93]["name"]="NODE_wall_trim_LOD0";d["nodes"][93]["extras"]["asset_role"]="venue_wall_trim"
        self.assertTrue(any("binding differs" in e for e in self.errors(d,m)))
        d,m=self.fixture();m["sparse_furniture_contract"]["budgets"]["incremental_draws_per_lod"]=2
        self.assertTrue(any("contract invalid" in e for e in self.errors(d,m)))

    def test_room_and_floor_cannot_change_under_optin(self):
        d,m=self.fixture();d["materials"][1]["pbrMetallicRoughness"]["baseColorFactor"][0]=.08
        self.assertTrue(any("exact approved room" in e for e in self.errors(d,m)))


if __name__=="__main__":
    unittest.main()
