from __future__ import annotations
import copy,json,unittest
from pathlib import Path
from scripts.validate_wpa_ii_09_coordinate_placement_positive_impulse_lifecycle_implementation import validate
ROOT=Path(__file__).resolve().parents[2];R=ROOT/'docs/rules/wpa-ii-09-coordinate-placement-positive-impulse-lifecycle-implementation-receipt-v1.json'
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(R.read_text())
 def reject(c,fn):v=copy.deepcopy(c.v);fn(v);c.assertTrue(validate(v,False))
 def test_live(c):c.assertEqual([],validate(c.v,True))
 def test_identity(c):c.reject(lambda v:v.__setitem__('status','M4_COMPLETE'));c.reject(lambda v:v.__setitem__('extra',1))
 def test_authority(c):c.reject(lambda v:v['authority'].__setitem__('selected_option_id','VALIDATED_LAUNCH'));c.reject(lambda v:v['authority']['implementation_design_gate'].__setitem__('root_authorized_exact_5_plus_3_allowlist',False))
 def test_predicate(c):c.reject(lambda v:v['implementation_contract'].__setitem__('positive_predicate','action.ok'));c.reject(lambda v:v['implementation_contract'].__setitem__('partial_commit_allowed',True))
 def test_unsupported(c):c.reject(lambda v:v['implementation_contract'].__setitem__('physical_cue_part_reposition','ENABLED'));c.reject(lambda v:v['implementation_contract'].__setitem__('forward_stroke_classifier','HEURISTIC'));c.reject(lambda v:v['implementation_contract'].__setitem__('global_rules_ui_exposed',True))
 def test_fixtures(c):c.reject(lambda v:v['fixture_matrix']['positive'].pop());c.reject(lambda v:v['fixture_matrix']['negative'].reverse());c.reject(lambda v:v['fixture_matrix'].__setitem__('wpa_ii_08_telemetry_regression','smoke'))
 def test_sources(c):c.reject(lambda v:v['source_bindings'].pop());c.reject(lambda v:v['source_bindings'][0].__setitem__('sha256','0'*64));c.reject(lambda v:v.__setitem__('implementation_aggregate_sha256','0'*64))
 def test_tests(c):c.reject(lambda v:v['test_bindings'].reverse());c.reject(lambda v:v['test_bindings'][0].__setitem__('path','../bad'))
 def test_cpu(c):c.reject(lambda v:v['cpu_verification']['required'][0].__setitem__('exit_code',1));c.reject(lambda v:v['cpu_verification']['wide'].pop());c.reject(lambda v:v['cpu_verification'].__setitem__('gpu_cocoa_opengl_executed',True))
 def test_cmake(c):c.reject(lambda v:v['cmake_binding'].__setitem__('modified',True));c.reject(lambda v:v['cmake_binding'].__setitem__('sha256','0'*64))
 def test_true_claims(c):
  for k in ('selected_coordinate_lifecycle_implemented','positive_impulse_exactly_once_consumption_implemented','repeated_narrow_product_coordinate_flow_reachable'):c.reject(lambda v,k=k:v['claims'].__setitem__(k,False))
 def test_false_completion(c):
  for k in ('wpa_ii_09_row_auto_closed','m4_complete','audit_modified','physical_cue_part_supported','forward_stroke_classifier_supported','global_rules_ui_exposed','new_production_file','cmake_modified','visual_ui_modified','gpu_executed','readiness_or_authorization_created'):c.reject(lambda v,k=k:v['claims'].__setitem__(k,True))
 def test_bad(c):
  for v in (None,[],1,'x'):c.assertTrue(validate(v,False))
  c.reject(lambda v:v.__setitem__('implementation_contract',None));c.reject(lambda v:v.__setitem__('claims',[]))
if __name__=='__main__':unittest.main()
