from __future__ import annotations
import copy,importlib.util,json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
V=ROOT/'scripts/validate_m4_b2_authenticated_referee_authority_protocol_foundation_implementation.py'
R=ROOT/'docs/rules/m4-b2-authenticated-referee-authority-protocol-foundation-implementation-receipt-v1.json'
S=importlib.util.spec_from_file_location('b2_impl',V);assert S and S.loader
M=importlib.util.module_from_spec(S);sys.modules[S.name]=M;S.loader.exec_module(M)
class ReceiptTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.v=json.loads(R.read_text())
 def reject(self,fn):
  v=copy.deepcopy(self.v);fn(v);self.assertTrue(M.validate(v,False))
 def test_live(self):self.assertEqual([],M.validate(self.v,True))
 def test_schema_status_extra(self):
  self.reject(lambda v:v.__setitem__('schema','v2'));self.reject(lambda v:v.__setitem__('status','M4_COMPLETE'));self.reject(lambda v:v.__setitem__('approved',True))
 def test_authority(self):
  self.reject(lambda v:v['authority_bindings']['root_selected_policy'].__setitem__('sha256','0'*64));self.reject(lambda v:v['authority_bindings']['implementation_design_gate'].__setitem__('sha256','1'*64))
 def test_tests_first(self):
  self.reject(lambda v:v['tests_first_record'].__setitem__('red_observed',False));self.reject(lambda v:v['tests_first_record']['red_failures'].pop());self.reject(lambda v:v['tests_first_record'].__setitem__('green_after_production_patch',False))
 def test_exact_contract(self):
  self.reject(lambda v:v['implemented_contract'].__setitem__('authenticated_exact_request_field_count',16));self.reject(lambda v:v['implemented_contract'].__setitem__('registered_specific_wpa_consumer_count',1));self.reject(lambda v:v['implemented_contract']['operations'].reverse())
 def test_proposal_atomic_retry_reset(self):
  self.reject(lambda v:v['implemented_contract'].__setitem__('proposal_is_final',True));self.reject(lambda v:v['implemented_contract'].__setitem__('atomic_commit_guard_and_rollback',False));self.reject(lambda v:v['implemented_contract'].__setitem__('exact_retry_zero_delta',False));self.reject(lambda v:v['implemented_contract'].__setitem__('reset_changes_generation_and_hash_and_invalidates_proposal_nonce_receipts',False))
 def test_not_physics(self):self.reject(lambda v:v['implemented_contract'].__setitem__('referee_declaration_promoted_to_physics_shotfacts_or_foul',True))
 def test_bindings_and_aggregate(self):
  self.reject(lambda v:v['source_bindings'][0].__setitem__('sha256','0'*64));self.reject(lambda v:v['source_bindings'].reverse());self.reject(lambda v:v['test_bindings'].pop());self.reject(lambda v:v.__setitem__('implementation_aggregate_sha256','0'*64))
 def test_cpu_no_gpu(self):
  self.reject(lambda v:v['cpu_verification'].__setitem__('passed_target_count',10));self.reject(lambda v:v['cpu_verification']['passed_targets'].pop());self.reject(lambda v:v['cpu_verification'].__setitem__('gpu_cocoa_opengl_executed',True))
 def test_unrelated_gap_honest(self):
  self.reject(lambda v:v['known_unrelated_build_gap'].__setitem__('target','fixed'));self.reject(lambda v:v['known_unrelated_build_gap'].__setitem__('modified_outside_allowlist_to_fix',True))
 def test_no_false_completion(self):
  self.reject(lambda v:v['completion_boundary'].__setitem__('foundation_implemented',False));self.reject(lambda v:v['completion_boundary'].__setitem__('next_step_requires_per_consumer_design_gate',False))
  for k in ('specific_wpa_rule_consumer_implemented','b2_complete','audit_row_promoted','global_player_rules_ui_exposed','physics_shotfacts_adjudicator_rack_modified','gpu_ta_readiness_or_authorization'):self.reject(lambda v,k=k:v['completion_boundary'].__setitem__(k,True))
 def test_malformed(self):
  for v in (None,[],1,'x'):self.assertTrue(M.validate(v,False))
  self.reject(lambda v:v.__setitem__('implemented_contract',[]));self.reject(lambda v:v.__setitem__('cpu_verification',None));self.reject(lambda v:v.__setitem__('completion_boundary','done'))
if __name__=='__main__':unittest.main()
