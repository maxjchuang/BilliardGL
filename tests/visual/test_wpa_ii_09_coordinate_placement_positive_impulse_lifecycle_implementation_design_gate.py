from __future__ import annotations
import copy,importlib.util,json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
V=ROOT/'scripts/validate_wpa_ii_09_coordinate_placement_positive_impulse_lifecycle_implementation_design_gate.py'
G=ROOT/'docs/rules/wpa-ii-09-coordinate-placement-positive-impulse-lifecycle-implementation-design-gate-v1.json'
S=importlib.util.spec_from_file_location('wpa09_gate',V);assert S and S.loader
M=importlib.util.module_from_spec(S);sys.modules[S.name]=M;S.loader.exec_module(M)
class GateTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.v=json.loads(G.read_text())
 def reject(self,fn):
  v=copy.deepcopy(self.v);fn(v);self.assertTrue(M.validate(v,False))
 def test_live(self):self.assertEqual([],M.validate(self.v,True))
 def test_schema_status_extra(self):
  self.reject(lambda v:v.__setitem__('schema','v2'));self.reject(lambda v:v.__setitem__('status','IMPLEMENTED'));self.reject(lambda v:v.__setitem__('auth',True))
 def test_selection_and_lineage(self):
  self.reject(lambda v:v['authority_bindings']['root_selected_policy'].__setitem__('selected_option_id','VALIDATED_LAUNCH'));self.reject(lambda v:v['authority_bindings']['original_design_gap_gate'].__setitem__('sha256','0'*64));self.reject(lambda v:v['authority_bindings']['current_audit'].__setitem__('must_not_be_promoted_by_gate',False))
 def test_predicate_and_feasibility(self):
  self.reject(lambda v:v['feasibility_finding'].__setitem__('selected_positive_predicate','action.ok'));self.reject(lambda v:v['feasibility_finding'].__setitem__('new_policy_blocker',True));self.reject(lambda v:v['feasibility_finding'].__setitem__('new_solver_or_cue_contact_telemetry_required',True))
 def test_coordinate_replacement_contract(self):
  self.reject(lambda v:v['coordinate_commit_contract'].__setitem__('commit','clear BIH'));self.reject(lambda v:v['coordinate_commit_contract'].__setitem__('partial_commit_allowed',True));self.reject(lambda v:v['coordinate_commit_contract'].__setitem__('exact_retry','restage again'))
 def test_positive_only_contract(self):
  self.reject(lambda v:v['positive_impulse_consumption_contract'].__setitem__('consume_if_and_only_if','validated request'));self.reject(lambda v:v['positive_impulse_consumption_contract'].__setitem__('later_physics_step_failure','restore entitlement'));self.reject(lambda v:v['positive_impulse_consumption_contract'].__setitem__('partial_commit_allowed',True))
 def test_unsupported_stays_disabled(self):
  self.reject(lambda v:v['unsupported_boundary'].__setitem__('physical_cue_part_reposition','ENABLED'));self.reject(lambda v:v['unsupported_boundary'].__setitem__('forward_stroke_classifier','HEURISTIC'))
  for k in ('coordinate_preview_or_confirm_is_tip_touch','new_tip_contact_or_forward_stroke_claim','global_rules_decision_ui_product_exposed','foundation_or_sibling_rules_action_product_exposed'):self.reject(lambda v,k=k:v['unsupported_boundary'].__setitem__(k,True))
 def test_allowlist(self):
  self.reject(lambda v:v['implementation_allowlist']['production_files'].pop());self.reject(lambda v:v['implementation_allowlist']['test_files'].reverse())
  for k in ('new_production_file_allowed','cue_contact_or_physics_edit_allowed','cmake_change_allowed','visual_ui_or_renderer_allowed'):self.reject(lambda v,k=k:v['implementation_allowlist'].__setitem__(k,True))
 def test_fixtures(self):
  self.reject(lambda v:v['required_fixtures']['positive'].pop());self.reject(lambda v:v['required_fixtures']['negative'].reverse());self.reject(lambda v:v['required_fixtures']['regressions'].pop());self.reject(lambda v:v['required_fixtures'].__setitem__('headless_cpu_only',False))
 def test_bindings(self):
  self.reject(lambda v:v['source_bindings'][0].__setitem__('sha256','0'*64));self.reject(lambda v:v['test_source_bindings'].pop());self.reject(lambda v:v['cmake_binding'].__setitem__('modified_in_gate',True))
 def test_no_authorization_or_false_completion(self):
  self.reject(lambda v:v['completion_boundary'].__setitem__('implementation_authorized_by_this_gate',True));self.reject(lambda v:v['completion_boundary'].__setitem__('root_revalidation_required_before_tests_first_patch',False))
  for k in ('implementation_complete','wpa_ii_09_complete','production_modified','cmake_modified','visual_ui_modified','gpu_executed'):self.reject(lambda v,k=k:v['claims'].__setitem__(k,True))
 def test_malformed(self):
  for v in (None,[],1,'x'):self.assertTrue(M.validate(v,False))
  self.reject(lambda v:v.__setitem__('authority_bindings',[]));self.reject(lambda v:v.__setitem__('required_fixtures',None));self.reject(lambda v:v.__setitem__('claims','done'))
if __name__=='__main__':unittest.main()
