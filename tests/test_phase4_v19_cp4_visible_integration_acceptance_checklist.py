from __future__ import annotations
import copy,json,tempfile,unittest
from pathlib import Path
from scripts import validate_phase4_v19_cp4_visible_integration_acceptance_checklist as V
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs/art/phase4-v19-cp4-visible-integration-implementation-acceptance-checklist-v1.json'
class T(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(DOC.read_text())
 def reject(s,f):
  v=copy.deepcopy(s.v);f(v)
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'x.json';p.write_text(json.dumps(v));s.assertTrue(V.validate(p))
 def test_pass(s):s.assertEqual(V.validate(DOC),[])
 def test_fail_closed_mutations(s):
  ms=[lambda v:v.update(status='IMPLEMENTED'),lambda v:v['bindings'][3].update(sha256='0'*64),lambda v:v['future_candidate_allowlist_not_authorized'].append('CMakeLists.txt'),lambda v:v['frozen_cp1_cp2_cp3_files_not_editable_in_cp4'].pop(),lambda v:v['explicit_current_frame_request_contract']['accepted_screens_exact'].append('Match'),lambda v:v['explicit_current_frame_request_contract'].update(world_overlay_control_on_required=True),lambda v:v['explicit_current_frame_request_contract'].update(screen_enum_or_product_frame_alone_may_enable=True),lambda v:v['session_layout_package_identity_contract']['exclusion_ids_exact'].remove('rules decision panel or modal'),lambda v:v['render_frame_invocation_and_order_contract'].update(explicit_render_frame_invocation_count_for_accepted_on_request=2),lambda v:v['render_frame_invocation_and_order_contract'].update(atomic_publish_precedes_product_ui_border_and_legend=False),lambda v:v['render_frame_invocation_and_order_contract'].update(world_overlay_draw_calls=1),lambda v:v['visible_atomicity_and_failure_contract'].update(visible_partial_write_or_chrome_without_plot_forbidden=False),lambda v:v['future_cpu_static_gl_mock_acceptance_not_run']['positive_matrix'].update(exact_case_count=47),lambda v:v['future_cpu_static_gl_mock_acceptance_not_run'].update(cpu_static_pass=True),lambda v:v['future_gpu_ta_human_gates_separate_not_run'].update(gpu_authorized=True),lambda v:v['authorization_boundary'].update(cp4_implementation_authorized=True),lambda v:v['authorization_boundary'].update(visible_product_inset_exposed=True)]
  for i,m in enumerate(ms):
   with s.subTest(i=i):s.reject(m)
if __name__=='__main__':unittest.main()
