from __future__ import annotations
import copy,json,tempfile,unittest
from pathlib import Path
from scripts import validate_phase4_v19_cp3_independent_ta_acceptance as V
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs/art/phase4-v19-cp3-offscreen-atomic-independent-ta-acceptance-addendum-v1.json'
class T(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(DOC.read_text())
 def reject(s,f):
  v=copy.deepcopy(s.v);f(v)
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'x.json';p.write_text(json.dumps(v));s.assertTrue(V.validate(p))
 def test_pass(s):s.assertEqual(V.validate(DOC),[])
 def test_mutations(s):
  ms=[lambda v:v.update(status='FAIL'),lambda v:v['frozen_bindings'][3].update(sha256='0'*64),lambda v:v['accepted_source_bindings'].update({'CMakeLists.txt':'0'*64}),lambda v:v['independent_reproduction'].update(transaction_red_status='PASS'),lambda v:v['independent_reproduction'].update(independent_modern_compile_only_o2='FAIL'),lambda v:v['accepted_cp3_axes'].update(typed_live_main_resource_alias_and_exact_fbo_attachment_checks=False),lambda v:v['accepted_cp3_axes'].update(fixed_standard_cvd_role_colors_and_color_semantic_current_token_probe_pairing=False),lambda v:v['acceptance_boundary'].update(cp3_accepted=False),lambda v:v['acceptance_boundary'].update(cp4_started=True),lambda v:v['acceptance_boundary'].update(gpu_authorized=True),lambda v:v['acceptance_boundary'].update(visible_product_inset_exposed=True)]
  for i,m in enumerate(ms):
   with s.subTest(i=i):s.reject(m)
if __name__=='__main__':unittest.main()
