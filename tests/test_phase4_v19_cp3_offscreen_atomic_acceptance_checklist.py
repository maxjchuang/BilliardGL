from __future__ import annotations
import copy,json,tempfile,unittest
from pathlib import Path
from scripts import validate_phase4_v19_cp3_offscreen_atomic_acceptance_checklist as V
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs/art/phase4-v19-cp3-offscreen-atomic-implementation-acceptance-checklist-v1.json'
class T(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(DOC.read_text())
 def reject(s,f):
  v=copy.deepcopy(s.v);f(v)
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'x.json';p.write_text(json.dumps(v));s.assertTrue(V.validate(p))
 def test_pass(s):s.assertEqual(V.validate(DOC),[])
 def test_mutations(s):
  ms=[lambda v:v.update(status='FAIL'),lambda v:v['bindings'][2].update(sha256='0'*64),lambda v:v['future_candidate_allowlist_not_authorized'].append('CMakeLists.txt'),lambda v:v['cmake_strategy'].update(current_checklist_authorizes_cmake_edit=True),lambda v:v['injectable_failure_interface_contract']['failure_points_in_order'].remove('state_restore'),lambda v:v['injectable_failure_interface_contract'].update(may_accept_camera_scene_color_repair_coordinates_or_prior_pass=True),lambda v:v['independent_resource_contract'].update(sample_counts_exact=[1,4]),lambda v:v['independent_resource_contract']['forbidden_reuse'].remove('main HDR color attachment'),lambda v:v['transaction_state_machine_contract'].update(partial_or_stale_present_allowed=True),lambda v:v['transaction_state_machine_contract']['ordered_success_states'].remove('COMPOSITE_ELIGIBLE'),lambda v:v['pass_and_tone_contract'].update(main_hdr_inset_writes=1),lambda v:v['gl_error_and_state_restore_contract'].update(restore_required_on_success_and_every_failure=False),lambda v:v['instrumentation_parity_contract'].update(ui_semantic_probe_denominator_contribution=1),lambda v:v['future_cpu_static_acceptance_not_run'].update(production_implementation_started=True),lambda v:v['authorization_boundary'].update(production_edit_authorized=True),lambda v:v['authorization_boundary'].update(gpu_authorized=True)]
  for i,m in enumerate(ms):
   with s.subTest(i=i):s.reject(m)
if __name__=='__main__':unittest.main()
