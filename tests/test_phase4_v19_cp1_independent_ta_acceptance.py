from __future__ import annotations
import copy,json,tempfile,unittest
from pathlib import Path
from scripts import validate_phase4_v19_cp1_independent_ta_acceptance as V
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs/art/phase4-v19-cp1-trajectory-inset-foundation-independent-ta-acceptance-addendum-v1.json'
class T(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(DOC.read_text())
 def reject(s,f):
  v=copy.deepcopy(s.v);f(v)
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'x.json';p.write_text(json.dumps(v));s.assertTrue(V.validate(p))
 def test_pass(s):s.assertEqual(V.validate(DOC),[])
 def test_mutations(s):
  ms=[lambda v:v.update(status='FAIL'),lambda v:v['frozen_bindings'][0].update(sha256='0'*64),lambda v:v['canonical_layout_inventory'][0].update(probeDenominator=0),lambda v:v['independent_reproduction'].update(receipt_fail_closed_mutation_pass_count=71),lambda v:v['accepted_cp1_axes'].update(camera_independent_geometry_and_exact_96_order=False),lambda v:v['acceptance_boundary'].update(cp1_accepted=False),lambda v:v['acceptance_boundary'].update(cp2_started=True),lambda v:v['acceptance_boundary'].update(gpu_authorized=True)]
  for i,m in enumerate(ms):
   with s.subTest(i=i):s.reject(m)
if __name__=='__main__':unittest.main()
