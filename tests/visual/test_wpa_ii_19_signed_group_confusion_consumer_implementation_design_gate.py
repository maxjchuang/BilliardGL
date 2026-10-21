from __future__ import annotations
import copy, importlib.util, json, sys, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
V=ROOT/"scripts/validate_wpa_ii_19_signed_group_confusion_consumer_implementation_design_gate.py"
G=ROOT/"docs/rules/wpa-ii-19-signed-group-confusion-consumer-implementation-design-gate-v1.json"
S=importlib.util.spec_from_file_location("wpa19_gate",V); assert S and S.loader
M=importlib.util.module_from_spec(S); sys.modules[S.name]=M; S.loader.exec_module(M)
class GateTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls): cls.v=json.loads(G.read_text())
 def reject(self,fn): v=copy.deepcopy(self.v); fn(v); self.assertTrue(M.validate(v,False))
 def test_live(self): self.assertEqual([],M.validate(self.v,True))
 def test_schema_status_and_extra(self): self.reject(lambda v:v.__setitem__("schema","v2")); self.reject(lambda v:v.__setitem__("status","IMPLEMENTED")); self.reject(lambda v:v.__setitem__("approved",True))
 def test_authority_lineage(self):
  for k in ("root_selected_policy","b2_foundation_receipt","b2_priority_register","original_design_gap_gate","wpa_ii_21_d8_predecessor_receipt","audit_v8"): self.reject(lambda v,k=k:v["authority_bindings"][k].__setitem__("sha256","0"*64))
  self.reject(lambda v:v["authority_bindings"].__setitem__("root_c2_selection_semantic_sha256","0"*64))
 def test_classification_and_actors(self): self.reject(lambda v:v["consumer_identity_and_selected_policy"].__setitem__("classification","WRONG_FIRST")); self.reject(lambda v:v["consumer_identity_and_selected_policy"]["accepted_proposal_actor_roles"].pop()); self.reject(lambda v:v["consumer_identity_and_selected_policy"].__setitem__("final_actor_role","ANY"))
 def test_selected_deadline_breaker_order(self): self.reject(lambda v:v["consumer_identity_and_selected_policy"].__setitem__("deadline","SHOT_COMPLETE")); self.reject(lambda v:v["consumer_identity_and_selected_policy"].__setitem__("rerack_breaker","CURRENT")); self.reject(lambda v:v["consumer_identity_and_selected_policy"].__setitem__("shooting_order","NEW")); self.reject(lambda v:v["consumer_identity_and_selected_policy"].__setitem__("ordinary_wrong_first_contact_alone_opens_or_commits_section_19",True))
 def test_window_boundary(self): self.reject(lambda v:v["window_and_snapshot_contract"]["window_reference"].pop()); self.reject(lambda v:v["window_and_snapshot_contract"]["does_not_close_window"].pop()); self.reject(lambda v:v["window_and_snapshot_contract"].__setitem__("accepted_launch_later_physics_failure_reopens_previous_window",True)); self.reject(lambda v:v["window_and_snapshot_contract"].__setitem__("automatic_classification_from_first_contact_or_group_assignment",True))
 def test_exact_payload_and_no_cancel(self): self.reject(lambda v:v["exact_payload_schemas"]["proposal"]["exact_keys_in_canonical_order"].reverse()); self.reject(lambda v:v["exact_payload_schemas"]["proposal"].__setitem__("ordinary_wrong_first_contact_only_payload_allowed",True)); self.reject(lambda v:v["exact_payload_schemas"]["final"].__setitem__("same_authenticated_actor_required",False)); self.reject(lambda v:v["exact_payload_schemas"]["cancel"].__setitem__("supported",True))
 def test_stop_final_state_machine(self): self.reject(lambda v:v["state_machine_and_stop_contract"]["states"].pop()); self.reject(lambda v:v["state_machine_and_stop_contract"].__setitem__("proposal","rerack immediately")); self.reject(lambda v:v["state_machine_and_stop_contract"].__setitem__("partial_commit_allowed",True))
 def test_bih_core_e_and_order(self): self.reject(lambda v:v["rack_bih_and_order_contract"]["core_e_binding"].__setitem__("direct_caller_exposure_allowed",True)); self.reject(lambda v:v["rack_bih_and_order_contract"].__setitem__("rerack_uses_current_player",True)); self.reject(lambda v:v["rack_bih_and_order_contract"].__setitem__("foundation_action_is_consumer_action",True)); self.reject(lambda v:v["rack_bih_and_order_contract"].__setitem__("numbered_seed_order_is_normative",True))
 def test_atomic_retry_rollback(self): self.reject(lambda v:v["atomicity_idempotency_and_reset"]["prevalidate"].pop()); self.reject(lambda v:v["atomicity_idempotency_and_reset"].__setitem__("exact_retry","commit twice")); self.reject(lambda v:v["atomicity_idempotency_and_reset"].__setitem__("failed_core_e_plan_or_parent_commit","partial")); self.reject(lambda v:v["atomicity_idempotency_and_reset"].__setitem__("plain_turn_change_or_legal_pocket_resets_window",True))
 def test_headless_no_ui(self): self.reject(lambda v:v["automation_headless_contract"]["commands"].pop()); self.reject(lambda v:v["automation_headless_contract"].__setitem__("headless_only",False)); self.reject(lambda v:v["automation_headless_contract"].__setitem__("modern_product_session_or_input_adapter_exposed",True)); self.reject(lambda v:v["automation_headless_contract"].__setitem__("global_rules_decision_ui_product_exposed",True))
 def test_allowlist(self): self.reject(lambda v:v["implementation_allowlist"]["production_files"].pop()); self.reject(lambda v:v["implementation_allowlist"]["test_files"].reverse()); self.reject(lambda v:v["implementation_allowlist"]["build_files"].append("Other")); self.reject(lambda v:v["implementation_allowlist"].__setitem__("referee_protocol_rack_executor_chinese_rules_shotfacts_physics_edit_allowed",True)); self.reject(lambda v:v["implementation_allowlist"].__setitem__("modern_product_ui_renderer_input_edit_allowed",True))
 def test_fixtures(self): self.reject(lambda v:v["required_fixtures"]["positive"].pop()); self.reject(lambda v:v["required_fixtures"]["negative"].reverse()); self.reject(lambda v:v["required_fixtures"].__setitem__("headless_cpu_only",False))
 def test_bindings(self): self.reject(lambda v:v["source_bindings"][0].__setitem__("sha256","0"*64)); self.reject(lambda v:v["source_bindings"].reverse()); self.reject(lambda v:v["test_source_bindings"].pop())
 def test_no_false_completion(self):
  self.reject(lambda v:v["completion_boundary"].__setitem__("root_revalidation_required_before_tests_first_patch",False))
  for k in ("implementation_authorized_by_this_gate","wpa_ii_19_implemented","audit_row_promoted","b2_complete","m4_complete","production_modified","cmake_modified","visible_product_ui_or_global_exposure","gpu_readiness_or_authorization"): self.reject(lambda v,k=k:v["completion_boundary"].__setitem__(k,True))
 def test_malformed(self):
  for v in (None,[],1,"x"): self.assertTrue(M.validate(v,False))
  self.reject(lambda v:v.__setitem__("window_and_snapshot_contract",[])); self.reject(lambda v:v.__setitem__("required_fixtures",None)); self.reject(lambda v:v.__setitem__("completion_boundary","done"))
if __name__=="__main__": unittest.main()
