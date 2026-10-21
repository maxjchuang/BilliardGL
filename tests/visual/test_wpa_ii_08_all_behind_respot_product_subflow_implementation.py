from __future__ import annotations
import copy,json,unittest
from pathlib import Path
from scripts.validate_wpa_ii_08_all_behind_respot_product_subflow_implementation import validate
ROOT=Path(__file__).resolve().parents[2]
RECEIPT=ROOT/'docs/rules/wpa-ii-08-all-behind-respot-product-subflow-implementation-receipt-v1.json'
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(RECEIPT.read_text())
 def reject(c,fn):v=copy.deepcopy(c.v);fn(v);c.assertTrue(validate(v,False))
 def test_live(c):c.assertEqual([],validate(c.v,True))
 def test_identity(c):c.reject(lambda v:v.__setitem__('status','M4_COMPLETE'));c.reject(lambda v:v.__setitem__('extra',1))
 def test_gate(c):c.reject(lambda v:v['authority']['implementation_design_gate'].__setitem__('sha256','0'*64));c.reject(lambda v:v['authority']['implementation_design_gate'].__setitem__('root_authorized_exact_7_plus_3_allowlist',False))
 def test_telemetry(c):c.reject(lambda v:v['authority']['telemetry_receipt'].__setitem__('sha256','0'*64))
 def test_authority(c):c.reject(lambda v:v['implementation_contract'].__setitem__('authority','any actor'));c.reject(lambda v:v['implementation_contract'].__setitem__('foundation_direct_product_access',True))
 def test_candidate(c):c.reject(lambda v:v['implementation_contract'].__setitem__('candidate_predicate','all active'));c.reject(lambda v:v['implementation_contract'].__setitem__('all_behind','on line counts'))
 def test_tie(c):c.reject(lambda v:v['implementation_contract'].__setitem__('tie','random'));c.reject(lambda v:v['implementation_contract'].__setitem__('replacement_child','reuse child'))
 def test_telemetry_successor(c):c.reject(lambda v:v['implementation_contract'].__setitem__('telemetry_successor','sampled endpoint'))
 def test_positive(c):c.reject(lambda v:v['fixture_matrix']['positive'].pop());c.reject(lambda v:v['fixture_matrix']['positive'].reverse())
 def test_negative(c):c.reject(lambda v:v['fixture_matrix']['negative'].pop());c.reject(lambda v:v['fixture_matrix'].__setitem__('telemetry_negative_regression',8))
 def test_sources(c):c.reject(lambda v:v['source_bindings'].pop());c.reject(lambda v:v['source_bindings'][0].__setitem__('sha256','0'*64));c.reject(lambda v:v.__setitem__('implementation_aggregate_sha256','0'*64))
 def test_tests(c):c.reject(lambda v:v['test_bindings'].reverse());c.reject(lambda v:v['test_bindings'][0].__setitem__('path','../bad'))
 def test_cpu(c):c.reject(lambda v:v['cpu_verification']['required'][0].__setitem__('exit_code',1));c.reject(lambda v:v['cpu_verification'].__setitem__('gpu_cocoa_opengl_executed',True));c.reject(lambda v:v['cpu_verification'].__setitem__('collector_cmake_link_gap_unchanged_and_workaround_reused',False))
 def test_cmake(c):c.reject(lambda v:v['cmake_binding'].__setitem__('modified',True));c.reject(lambda v:v['cmake_binding'].__setitem__('sha256','0'*64))
 def test_false_completion(c):
  for k in ('wpa_ii_08_row_auto_closed','m4_complete','audit_modified','foundation_product_exposed','global_rules_ui_exposed','new_production_file','rack_executor_modified','cmake_modified','visual_ui_modified','gpu_executed','readiness_or_authorization_created'):c.reject(lambda v,k=k:v['claims'].__setitem__(k,True))
 def test_positive_claims(c):
  for k in ('all_behind_parent_respot_product_subflow_implemented','telemetry_8_positive_9_negative_regressed','narrow_product_reachable'):c.reject(lambda v,k=k:v['claims'].__setitem__(k,False))
 def test_bad(c):
  for v in (None,[],1,'x'):c.assertTrue(validate(v,False))
  c.reject(lambda v:v.__setitem__('fixture_matrix',None));c.reject(lambda v:v.__setitem__('claims',[]))
if __name__=='__main__':unittest.main()
