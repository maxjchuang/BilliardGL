from __future__ import annotations
import copy,json,unittest
from pathlib import Path
from scripts.validate_wpa_ii_15_1_signed_frozen_cue_object_declaration_lifecycle_implementation_design_gate import validate
ROOT=Path(__file__).resolve().parents[2];P=ROOT/'docs/rules/wpa-ii-15-1-signed-frozen-cue-object-declaration-lifecycle-implementation-design-gate-v1.json'
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(P.read_text())
 def reject(c,fn):v=copy.deepcopy(c.v);fn(v);c.assertTrue(validate(v,False))
 def test_live(c):c.assertEqual([],validate(c.v,True))
 def test_identity(c):c.reject(lambda v:v.__setitem__('status','IMPLEMENTED'));c.reject(lambda v:v.__setitem__('extra',True))
 def test_authority(c):
  for k in ('root_selected_policy','b2_foundation_receipt','b2_priority_register','cue_contact_cluster_gap_gate','audit_v9'):c.reject(lambda v,k=k:v['authority_bindings'][k].__setitem__('sha256','0'*64))
 def test_official_selected_boundary(c):c.reject(lambda v:v['official_and_selected_boundary']['officially_determined'].pop());c.reject(lambda v:v['official_and_selected_boundary'].__setitem__('declaration_authority','PLAYER'));c.reject(lambda v:v['official_and_selected_boundary'].__setitem__('geometry_may_suggest_but_not_declare',False))
 def test_protocol_roles(c):c.reject(lambda v:v['declaration_protocol_contract'].__setitem__('proposal_actor_role','PLAYER'));c.reject(lambda v:v['declaration_protocol_contract'].__setitem__('proposal_is_committed_frozen_declaration',True));c.reject(lambda v:v['declaration_protocol_contract'].__setitem__('player_or_geometry_declaration_allowed',True))
 def test_payload(c):c.reject(lambda v:v['exact_payload_schemas']['proposal']['exact_keys_in_canonical_order'].reverse());c.reject(lambda v:v['exact_payload_schemas']['proposal'].__setitem__('object_ball_id_range_inclusive',[0,15]));c.reject(lambda v:v['exact_payload_schemas']['final'].__setitem__('same_values_as_proposal_required',False))
 def test_snapshot_no_geometry(c):c.reject(lambda v:v['pre_shot_snapshot_and_legality']['parent_preconditions'].pop());c.reject(lambda v:v['pre_shot_snapshot_and_legality']['snapshot_fields'].pop());c.reject(lambda v:v['pre_shot_snapshot_and_legality'].__setitem__('geometry_distance_or_solver_slop_used_as_declaration',True));c.reject(lambda v:v['pre_shot_snapshot_and_legality'].__setitem__('frozen_cue_topology_used_as_declaration',True))
 def test_one_shot(c):c.reject(lambda v:v['one_shot_lifecycle']['states'].pop());c.reject(lambda v:v['one_shot_lifecycle']['does_not_consume'].pop());c.reject(lambda v:v['one_shot_lifecycle'].__setitem__('later_physics_failure_reactivates_declaration',True));c.reject(lambda v:v['one_shot_lifecycle'].__setitem__('second_shot_can_reuse_declaration',True))
 def test_d5_stays_blocked(c):
  for k in ('complete_full_stroke_tip_contact_telemetry_available','obvious_push_machine_predicate_available','obvious_stroke_machine_predicate_available','close_ball_light_graze_presumption_available','declaration_direct_movement_exception_adjudication_allowed','declaration_obvious_push_foul_adjudication_allowed','shotfacts_or_foul_reason_change_allowed','wpa_ii_15_1_row_may_be_promoted_after_lifecycle_only'):c.reject(lambda v,k=k:v['d5_classification_blocker'].__setitem__(k,True))
 def test_atomicity(c):c.reject(lambda v:v['atomicity_idempotency_and_reset']['prevalidate'].pop());c.reject(lambda v:v['atomicity_idempotency_and_reset'].__setitem__('exact_retry','commit twice'));c.reject(lambda v:v['atomicity_idempotency_and_reset'].__setitem__('partial_commit_allowed',True))
 def test_headless_no_ui(c):c.reject(lambda v:v['automation_headless_contract']['commands'].pop());c.reject(lambda v:v['automation_headless_contract'].__setitem__('headless_only',False));c.reject(lambda v:v['automation_headless_contract'].__setitem__('modern_product_session_or_input_adapter_exposed',True));c.reject(lambda v:v['automation_headless_contract'].__setitem__('global_rules_decision_ui_product_exposed',True))
 def test_allowlist(c):c.reject(lambda v:v['implementation_allowlist']['production_files'].pop());c.reject(lambda v:v['implementation_allowlist']['test_files'].reverse());c.reject(lambda v:v['implementation_allowlist']['build_files'].append('Other'));c.reject(lambda v:v['implementation_allowlist'].__setitem__('protocol_frozen_topology_cue_contact_coupled_contact_physics_telemetry_shotfacts_rules_edit_allowed',True))
 def test_fixtures(c):c.reject(lambda v:v['required_fixtures']['positive'].pop());c.reject(lambda v:v['required_fixtures']['negative'].reverse());c.reject(lambda v:v['required_fixtures'].__setitem__('headless_cpu_only',False))
 def test_bindings(c):c.reject(lambda v:v['source_bindings'][0].__setitem__('sha256','0'*64));c.reject(lambda v:v['source_bindings'].reverse());c.reject(lambda v:v['test_source_bindings'].pop())
 def test_no_false_completion(c):
  for k in ('implementation_authorized_by_this_gate','declaration_lifecycle_implemented','wpa_ii_15_1_implemented','audit_row_promoted','d5_classification_complete','b2_complete','m4_complete','production_modified','cmake_modified','visible_product_ui_or_global_exposure','gpu_readiness_or_authorization'):c.reject(lambda v,k=k:v['completion_boundary'].__setitem__(k,True))
  c.reject(lambda v:v['completion_boundary'].__setitem__('root_revalidation_required_before_tests_first_patch',False))
 def test_bad(c):
  for v in (None,[],1,'x'):c.assertTrue(validate(v,False))
  c.reject(lambda v:v.__setitem__('d5_classification_blocker',None));c.reject(lambda v:v.__setitem__('completion_boundary',[]))
if __name__=='__main__':unittest.main()
