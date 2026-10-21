from __future__ import annotations
import copy,importlib.util,json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];V=ROOT/'scripts/validate_wpa_ii_21_stalemate_referee_consumer_implementation_design_gate.py';G=ROOT/'docs/rules/wpa-ii-21-stalemate-referee-consumer-implementation-design-gate-v1.json'
S=importlib.util.spec_from_file_location('wpa21_gate',V);assert S and S.loader
M=importlib.util.module_from_spec(S);sys.modules[S.name]=M;S.loader.exec_module(M)
class GateTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.v=json.loads(G.read_text())
 def reject(self,fn):v=copy.deepcopy(self.v);fn(v);self.assertTrue(M.validate(v,False))
 def test_live(self):self.assertEqual([],M.validate(self.v,True))
 def test_schema_status_extra(self):self.reject(lambda v:v.__setitem__('schema','v2'));self.reject(lambda v:v.__setitem__('status','IMPLEMENTED'));self.reject(lambda v:v.__setitem__('approved',True))
 def test_authority_lineage(self):
  for k in ('root_selected_policy','b2_foundation_receipt','b2_priority_register','original_design_gap_gate'):self.reject(lambda v,k=k:v['authority_bindings'][k].__setitem__('sha256','0'*64))
  self.reject(lambda v:v['authority_bindings'].__setitem__('root_c1_selection_semantic_sha256','0'*64))
 def test_roles_and_refusal_not_final(self):self.reject(lambda v:v['consumer_identity_and_policy'].__setitem__('proposal_actor_role','PLAYER'));self.reject(lambda v:v['consumer_identity_and_policy']['player_refusal_roles'].pop());self.reject(lambda v:v['consumer_identity_and_policy'].__setitem__('player_refusal_is_final_declaration',True));self.reject(lambda v:v['consumer_identity_and_policy'].__setitem__('detached_verifier_generic_command_exposed',True))
 def test_directive_payload(self):self.reject(lambda v:v['exact_payload_schemas']['proposal'].__setitem__('default_allowance',4));self.reject(lambda v:v['exact_payload_schemas']['proposal'].__setitem__('explicit_alternative_range_inclusive',[0,999]));self.reject(lambda v:v['exact_payload_schemas']['proposal'].__setitem__('stalemate_attested',False));self.reject(lambda v:v['exact_payload_schemas']['final'].__setitem__('completed_shots_exact_array_length',3))
 def test_original_breaker_and_core_e_boundary(self):self.reject(lambda v:v['rack_and_order_snapshot_contract'].__setitem__('current_player_may_replace_original_breaker',True));self.reject(lambda v:v['rack_and_order_snapshot_contract'].__setitem__('foundation_id_may_be_submitted_by_consumer_caller',True));self.reject(lambda v:v['rack_and_order_snapshot_contract']['core_e_binding'].__setitem__('direct_product_or_automation_exposure_allowed',True))
 def test_no_auto_final(self):self.reject(lambda v:v['state_machine_contract'].__setitem__('no_auto_final','threshold reracks'));self.reject(lambda v:v['state_machine_contract'].__setitem__('partial_commit_allowed',True));self.reject(lambda v:v['shot_counter_contract'].__setitem__('numerical_threshold_is_referee_authority',True))
 def test_counter_exactness(self):self.reject(lambda v:v['shot_counter_contract']['count_only'].pop());self.reject(lambda v:v['shot_counter_contract']['never_count'].pop());self.reject(lambda v:v['shot_counter_contract'].__setitem__('counter_snapshot_digest_required_in_final',False))
 def test_atomic_retry_reset(self):self.reject(lambda v:v['atomicity_idempotency_and_reset'].__setitem__('exact_retry','commit twice'));self.reject(lambda v:v['atomicity_idempotency_and_reset'].__setitem__('failed_core_e_plan_or_commit','partial'));self.reject(lambda v:v['atomicity_idempotency_and_reset'].__setitem__('turn_change_or_legal_pocket_alone_resets',True))
 def test_headless_no_ui(self):self.reject(lambda v:v['automation_headless_contract']['commands'].pop());self.reject(lambda v:v['automation_headless_contract'].__setitem__('headless_only',False));self.reject(lambda v:v['automation_headless_contract'].__setitem__('modern_product_session_or_input_adapter_exposed',True));self.reject(lambda v:v['automation_headless_contract'].__setitem__('global_rules_decision_ui_product_exposed',True))
 def test_allowlist(self):self.reject(lambda v:v['implementation_allowlist']['production_files'].pop());self.reject(lambda v:v['implementation_allowlist']['test_files'].reverse());self.reject(lambda v:v['implementation_allowlist']['build_files'].append('Other'));self.reject(lambda v:v['implementation_allowlist'].__setitem__('rack_executor_edit_allowed',True));self.reject(lambda v:v['implementation_allowlist'].__setitem__('chinese_rules_shotfacts_physics_edit_allowed',True))
 def test_fixtures(self):self.reject(lambda v:v['required_fixtures']['positive'].pop());self.reject(lambda v:v['required_fixtures']['negative'].reverse());self.reject(lambda v:v['required_fixtures'].__setitem__('headless_cpu_only',False))
 def test_bindings(self):self.reject(lambda v:v['source_bindings'][0].__setitem__('sha256','0'*64));self.reject(lambda v:v['source_bindings'].reverse());self.reject(lambda v:v['test_source_bindings'].pop())
 def test_no_false_completion(self):
  self.reject(lambda v:v['completion_boundary'].__setitem__('root_revalidation_required_before_tests_first_patch',False))
  for k in ('implementation_authorized_by_this_gate','wpa_ii_21_implemented','audit_row_promoted','b2_complete','m4_complete','production_modified','cmake_modified','visible_product_ui_or_global_exposure','gpu_readiness_or_authorization'):self.reject(lambda v,k=k:v['completion_boundary'].__setitem__(k,True))
 def test_malformed(self):
  for v in (None,[],1,'x'):self.assertTrue(M.validate(v,False))
  self.reject(lambda v:v.__setitem__('consumer_identity_and_policy',[]));self.reject(lambda v:v.__setitem__('state_machine_contract',None));self.reject(lambda v:v.__setitem__('completion_boundary','done'))
if __name__=='__main__':unittest.main()
