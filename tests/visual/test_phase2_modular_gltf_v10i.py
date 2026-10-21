import base64,copy,json
from pathlib import Path
import sys,tempfile,unittest

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/"scripts"))
from generate_phase2_modular_gltf_v10i import ASSET_NAME,MANIFEST_NAME,DEFAULT_OUTPUT_ROOT,build_document,generate
from validate_phase2_modular_gltf_v10i import validate

class Phase2V10iAssetTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.document=json.loads((DEFAULT_OUTPUT_ROOT/ASSET_NAME).read_text());cls.manifest=json.loads((DEFAULT_OUTPUT_ROOT/MANIFEST_NAME).read_text())
 def errors(self,document=None,manifest=None):return validate(document or copy.deepcopy(self.document),manifest or copy.deepcopy(self.manifest),DEFAULT_OUTPUT_ROOT,verify_hashes=False)[0]
 def test_committed_asset_passes_and_diff_is_narrow(self):
  errors,report=validate(self.document,self.manifest,DEFAULT_OUTPUT_ROOT);self.assertEqual([],errors);self.assertTrue(report["decoded_non_COLOR_accessors_exact"]);self.assertTrue(report["scene_json_exact_after_permitted_restore"])
 def test_generation_byte_deterministic(self):
  with tempfile.TemporaryDirectory() as a,tempfile.TemporaryDirectory() as b:
   generate(Path(a));generate(Path(b))
   for name in (ASSET_NAME,MANIFEST_NAME):self.assertEqual((Path(a)/name).read_bytes(),(Path(b)/name).read_bytes())
 def test_rejects_raw_non_color_change(self):
  d=copy.deepcopy(self.document);prefix="data:application/octet-stream;base64,";payload=bytearray(base64.b64decode(d["buffers"][0]["uri"][len(prefix):]));payload[0]^=1;d["buffers"][0]["uri"]=prefix+base64.b64encode(payload).decode();self.assertTrue(any("outside sidewall COLOR_0" in e or "non-COLOR" in e for e in self.errors(document=d)))
 def test_rejects_material_numeric_workaround(self):
  d=copy.deepcopy(self.document);mesh=next(m for m in d["meshes"] if m.get("extras",{}).get("asset_role")=="pocket_liner_sidewall_visual_only");i=mesh["primitives"][0]["material"];d["materials"][i]["pbrMetallicRoughness"]["baseColorFactor"][2]=.06;self.assertTrue(any("material" in e or "parameter" in e for e in self.errors(document=d)))
 def test_rejects_camera_scene_change(self):
  d=copy.deepcopy(self.document);d["asset"]["generator"]+="_mutated";self.assertTrue(any("glTF JSON" in e for e in self.errors(document=d)))
 def test_rejects_authorization_escalation(self):
  m=copy.deepcopy(self.manifest);m["v10i_offline_contract"]["gpu_capture_authorized"]=True;self.assertTrue(any("authorization boundary" in e for e in self.errors(manifest=m)))
 def test_parameter_domain_fail_closed(self):
  baseline=json.loads((ROOT/"assets/modern/phase2-v10h"/ASSET_NAME).read_text())
  with self.assertRaises(ValueError):build_document(baseline,lower=.1)
  with self.assertRaises(ValueError):build_document(baseline,upper=1.0)

if __name__=="__main__":unittest.main()
