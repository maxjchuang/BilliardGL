from __future__ import annotations
import copy,json,unittest
from pathlib import Path
from scripts.validate_wpa_ii_21_stalemate_referee_consumer_implementation import validate
ROOT=Path(__file__).resolve().parents[2]
R=ROOT/'docs/rules/wpa-ii-21-stalemate-referee-consumer-implementation-receipt-v1.json'
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(R.read_text())
 def reject(c,fn):v=copy.deepcopy(c.v);fn(v);c.assertTrue(validate(v,False))
 def test_live(c):c.assertEqual([],validate(c.v,True))
 def test_identity(c):c.reject(lambda v:v.__setitem__('status','M4_COMPLETE'));c.reject(lambda v:v.__setitem__('extra',True))
 def test_authority(c):c.reject(lambda v:v['authority_bindings']['implementation_design_gate'].__setitem__('sha256','0'*64));c.reject(lambda v:v['authority_bindings']['root_selected_policy'].__setitem__('selected_option_id','AUTO_THREE'))
 def test_red_green(c):c.reject(lambda v:v['tests_first_record'].__setitem__('red_observed',False));c.reject(lambda v:v['tests_first_record'].__setitem__('green_after_minimal_production_patch',False))
 def test_states(c):c.reject(lambda v:v['implemented_contract']['states'].reverse());c.reject(lambda v:v['implemented_contract'].__setitem__('commit_failure_full_rollback',False))
 def test_no_direct_or_physics(c):c.reject(lambda v:v['implemented_contract'].__setitem__('direct_core_e_command_exposed',True));c.reject(lambda v:v['implemented_contract'].__setitem__('referee_payload_promoted_to_physics_or_shotfacts',True))
 def test_automation(c):c.reject(lambda v:v['automation_headless']['commands'].pop());c.reject(lambda v:v['automation_headless'].__setitem__('request_exact_field_count',16));c.reject(lambda v:v['automation_headless']['readback_omits'].pop())
 def test_fixtures(c):c.reject(lambda v:v['fixture_matrix']['positive'].pop());c.reject(lambda v:v['fixture_matrix']['negative'].reverse())
 def test_sources(c):c.reject(lambda v:v['source_bindings'].pop());c.reject(lambda v:v['source_bindings'][0].__setitem__('sha256','0'*64));c.reject(lambda v:v.__setitem__('implementation_aggregate_sha256','0'*64))
 def test_tests(c):c.reject(lambda v:v['test_bindings'].reverse());c.reject(lambda v:v.__setitem__('test_aggregate_sha256','0'*64))
 def test_cpu(c):c.reject(lambda v:v['cpu_verification']['required_binaries'][0].__setitem__('exit_code',1));c.reject(lambda v:v['cpu_verification']['wide_passed_targets'].pop());c.reject(lambda v:v['cpu_verification'].__setitem__('gpu_cocoa_opengl_executed',True))
 def test_no_false_completion(c):
  for k in ('visible_product_ui_implemented','audit_row_promoted','b2_complete','m4_complete','physics_or_shotfacts_modified','global_rules_ui_exposed','gpu_executed','readiness_or_authorization_created'):c.reject(lambda v,k=k:v['completion_boundary'].__setitem__(k,True))
 def test_bad(c):
  for v in (None,[],1,'x'):c.assertTrue(validate(v,False))
  c.reject(lambda v:v.__setitem__('implemented_contract',None));c.reject(lambda v:v.__setitem__('completion_boundary',[]))
if __name__=='__main__':unittest.main()
