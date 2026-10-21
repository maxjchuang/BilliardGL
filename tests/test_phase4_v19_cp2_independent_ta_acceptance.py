from __future__ import annotations
import copy,json,tempfile,unittest
from pathlib import Path
from scripts import validate_phase4_v19_cp2_independent_ta_acceptance as V
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs/art/phase4-v19-cp2-layout-draw-list-independent-ta-acceptance-addendum-v1.json'
class T(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(DOC.read_text())
 def reject(s,f):
  v=copy.deepcopy(s.v);f(v)
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'x.json';p.write_text(json.dumps(v));s.assertTrue(V.validate(p))
 def test_pass(s):s.assertEqual(V.validate(DOC),[])
 def test_mutations(s):
  ms=[lambda v:v.update(status='FAIL'),lambda v:v['frozen_bindings'][0].update(sha256='0'*64),lambda v:v['accepted_source_bindings'].update({'CMakeLists.txt':'0'*64}),lambda v:v['independent_reproduction'].update(independent_o0_replay='FAIL'),lambda v:v['accepted_cp2_axes'].update(transparent_cutout_has_zero_opaque_or_ui_coverage=False),lambda v:v['accepted_cp2_axes'].update(cp1_inventory_sources_cmake_and_modern_renderer_byte_exact=False),lambda v:v['acceptance_boundary'].update(cp2_accepted=False),lambda v:v['acceptance_boundary'].update(cp3_started=True),lambda v:v['acceptance_boundary'].update(gpu_authorized=True)]
  for i,m in enumerate(ms):
   with s.subTest(i=i):s.reject(m)
if __name__=='__main__':unittest.main()
