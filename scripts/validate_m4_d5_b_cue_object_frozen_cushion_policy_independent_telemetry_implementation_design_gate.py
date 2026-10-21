#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/m4-d5-b-cue-object-frozen-cushion-policy-independent-telemetry-implementation-design-gate-v1.json'
PROD=['src/Billiards/cue_contact.h','src/Billiards/physics_telemetry.h','src/Billiards/physics_telemetry.cpp','src/Billiards/physics.h','src/Billiards/physics.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/rules/frozen_cushion_declaration.h']
TEST=['tests/cue_contact_tests.cpp','tests/physics_tests.cpp','tests/physics_telemetry_tests.cpp','tests/rules_runtime_integration_tests.cpp']
FORBIDDEN=['src/Billiards/rules/chinese_eight_ball_rules.h','src/Billiards/rules/chinese_eight_ball_rules.cpp','src/Billiards/rules/shot_fact_collector.h','src/Billiards/rules/shot_fact_collector.cpp','src/Billiards/automation_controller.cpp','src/Billiards/hud.cpp','src/Billiards/renderer.cpp','src/Billiards/render/modern/modern_renderer.cpp']
ORDER=['CUE_OBJECT_CONTACT_BEFORE_TIP_SEPARATION','CUE_OBJECT_CONTACT_EQUAL_TIP_SEPARATION','CUE_OBJECT_CONTACT_AFTER_TIP_SEPARATION','NO_CUE_OBJECT_CONTACT','UNORDERED_INCOMPLETE']
POS=['CONTACT_STRICTLY_BEFORE_LAST_TIP_SEPARATION','CONTACT_STRICTLY_AFTER_LAST_TIP_SEPARATION','SIMULTANEOUS_CUE_OBJECT_CONTACTS_CANONICAL_ORDER','SIGNED_DECLARED_OBJECT_DEPARTS_NAMED_CUSHION','DEPARTURE_THEN_SAME_NAMED_CUSHION_RECONTACT','OTHER_DISTINCT_OBJECT_STRAIGHT_CUSHION_ALTERNATIVE','OBJECT_POCKET_CAPTURE_ALTERNATIVE','ALL_EVENTS_COMPLETE_ORIGIN_ATTESTED','EXACT_TICK_AND_FINAL_RETRY_SAME_DIGEST_ZERO_DUPLICATE']
NEG=['EQUAL_CONTACT_AND_TIP_SEPARATION_RECORDED_NOT_CLASSIFIED','MISSING_OR_INCOMPLETE_D5_A_STROKE_UNORDERED','FAILED_OR_GAPPED_PHYSICS_TICK_INCOMPLETE_ROLLBACK','NONFINITE_OR_OUT_OF_RANGE_TOI_REJECTED','MISSING_SOLVER_EVENT_OR_CONTACT_IDENTITY_REJECTED','GEOMETRY_ONLY_FROZEN_RELATION_CANNOT_START_LIFECYCLE','STALE_WRONG_OR_UNCONSUMED_DECLARATION_REJECTED','DECLARED_OBJECT_NEVER_LEAVES','RECONTACT_BEFORE_DEPARTURE_NOT_SATISFY_LIFECYCLE','WRONG_NAMED_CUSHION_RECONTACT_PRESERVED_AS_OTHER_EVENT_ONLY','JAW_CONTACT_NOT_STRAIGHT_CUSHION_ALTERNATIVE','CUE_BALL_POCKET_NOT_OBJECT_POCKET_ALTERNATIVE','POSITION_CORRECTION_WITHOUT_VELOCITY_IMPULSE_NOT_CUSHION_CONTACT','DUPLICATE_EVENT_DEDUPED','CONFLICTING_RETRY_ZERO_DELTA','RESET_RERACK_TURN_OR_STATE_CONFLICT_INVALIDATES','FACTS_DO_NOT_SET_DOUBLE_HIT_PUSH_FROZEN_FOUL_OR_LEGAL','NO_RULES_SHOTFACT_UI_GPU_OR_AUDIT_SIDE_EFFECT']
def sha(b):return hashlib.sha256(b).hexdigest()
def resolve(s):
 if not isinstance(s,str):return None
 p=Path(s)
 if p.is_absolute() or '..' in p.parts:return None
 q=(ROOT/p).resolve();return q if q.is_relative_to(ROOT.resolve()) else None
def bind(row):
 if not isinstance(row,dict) or set(row)!={'path','sha256'}:return False
 p=resolve(row.get('path'));return p is not None and p.is_file() and sha(p.read_bytes())==row.get('sha256')
def validate(v,check_files=True):
 if not isinstance(v,dict):return ['root']
 e=[];keys={'schema','status','authority_bindings','current_mapping','canonical_time_and_identity_contract','cue_object_order_observation','frozen_cushion_lifecycle_observation','atomicity_dedupe_and_rollback','classification_boundary','required_fixtures','implementation_allowlist','live_source_bindings','completion_boundary'}
 if set(v)!=keys:e.append('keys')
 if v.get('schema')!='m4_d5_b_cue_object_frozen_cushion_policy_independent_telemetry_implementation_design_gate_v1':e.append('schema')
 if v.get('status')!='FINAL_DESIGN_ONLY_ROOT_REVALIDATION_REQUIRED_NO_PRODUCTION_CHANGE':e.append('status')
 a=v.get('authority_bindings',{});expected={'d5_a_receipt':'7a6f7c3c798654d846d6a645451ddfa559a156a32ec869a3d4df4d5a7b2b0bd6','d5_d6_feasibility':'86d4957620d9e2c9033708ef3dbd5329610cc9cc9c44a134369079cffa375f77','root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','cue_contact_gap_gate':'1442e4204c4acb0718ca0b87c858c8222e03ac634f5b5b7def3d9a6bdae64033','frozen_cushion_declaration_receipt':'b21de04b86f4c988c6f32b6e87667e11dbb55dfed60eeaf55c08e4745b64e6af'}
 for key,digest in expected.items():
  row=a.get(key,{}) if isinstance(a,dict) else {}
  if row.get('sha256')!=digest:e.append('authority:'+key)
  elif check_files and not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('authority_drift:'+key)
 if a.get('d5_d6_feasibility',{}).get('batch')!='D5-B':e.append('batch')
 m=v.get('current_mapping',{})
 for key in ('d5_a_complete_stroke_time_is_local_to_cue_observer','physics_contacts_have_whole_tick_toi_solver_event_and_impulse_provenance','physics_tick_order_is_authoritative_only_for_successful_steps','ball_ball_contact_positions_available','straight_rail_impulse_and_pocket_capture_available','pocket_jaw_is_distinguishable_from_straight_rail','signed_frozen_object_and_named_straight_cushion_available'):
  if m.get(key) is not True:e.append('mapping_true:'+key)
 for key in ('cue_object_contact_before_tip_separation_fact_available','declared_object_departure_recontact_lifecycle_available','whole_shot_alternative_attestation_available','policy_or_foul_classification_available'):
  if m.get(key) is not False:e.append('mapping_false:'+key)
 t=v.get('canonical_time_and_identity_contract',{})
 if t.get('required_order_values')!=ORDER or 'stroke-relative seconds' not in str(t.get('time_domain')) or t.get('strict_order_only') is not True or t.get('equal_is_measurement_not_foul') is not True or t.get('tie_tolerance_selected') is not False:e.append('time_order')
 co=v.get('cue_object_order_observation',{})
 if co.get('contact_before_tip_separation_is_raw_order_fact_only') is not True or co.get('does_not_select_very_close_or_light_graze_policy') is not True or co.get('simultaneous_distinct_objects_preserved') is not True:e.append('cue_object')
 fc=v.get('frozen_cushion_lifecycle_observation',{})
 for key in ('same_named_cushion_recontact_preserved_separately','events_are_canonical_sorted_and_exact_deduped','no_departure_means_recontact_requirement_not_observed','alternatives_are_facts_not_legal_result'):
  if fc.get(key) is not True:e.append('frozen_true:'+key)
 for key in ('jaw_counts_as_straight_cushion','cue_ball_pocket_is_not_object_pocket_alternative'):
  if fc.get(key) is not (key=='cue_ball_pocket_is_not_object_pocket_alternative'):e.append('frozen_boundary:'+key)
 if 'geometry alone never starts observation' not in str(fc.get('prerequisite')):e.append('declaration')
 atom=v.get('atomicity_dedupe_and_rollback',{})
 if set(atom.get('dedupe_keys',{}))!={'cue_object','rail','pocket','departure'} or 'incomplete' not in str(atom.get('failed_step')) or 'zero' not in str(atom.get('conflicting_retry')):e.append('atomicity')
 cb=v.get('classification_boundary',{})
 for key in ('double_hit_classified','close_ball_presumption_selected','light_graze_selected','push_classified','frozen_cushion_legal_or_foul_classified','prerequisite_legal_cushion_predicate_selected','jaw_policy_changed','shotfacts_or_rules_consumer_added','audit_row_promoted'):
  if cb.get(key) is not False:e.append('classification:'+key)
 fx=v.get('required_fixtures',{})
 if fx.get('positive')!=POS or fx.get('negative')!=NEG or len(set(POS+NEG))!=27:e.append('fixtures')
 allow=v.get('implementation_allowlist',{})
 if allow.get('production_files')!=PROD or allow.get('test_files')!=TEST or allow.get('build_files')!=['CMakeLists.txt'] or allow.get('explicitly_forbidden_files')!=FORBIDDEN:e.append('allowlist')
 rows=v.get('live_source_bindings',[])
 if not isinstance(rows,list) or len(rows)!=16 or len({x.get('path') for x in rows if isinstance(x,dict)})!=16:e.append('bindings')
 if check_files:
  for row in rows:
   if not bind(row):e.append('source_drift')
 done=v.get('completion_boundary',{})
 if done.get('root_revalidation_required') is not True:e.append('root_gate')
 for key in ('implementation_authorized_by_this_gate','d5_b_implemented','d5_complete','d5_c_or_d5_d_authorized','classification_or_audit_progress','production_or_cmake_modified','rules_shotfacts_ui_modified','gpu_readiness_or_authorization','m4_complete'):
  if done.get(key) is not False:e.append('false_claim:'+key)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('gate',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');args=ap.parse_args()
 try:v=json.loads(Path(args.gate).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not args.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_D5_B_CUE_OBJECT_FROZEN_CUSHION_POLICY_INDEPENDENT_TELEMETRY_DESIGN_GATE');return 0
if __name__=='__main__':raise SystemExit(main())
