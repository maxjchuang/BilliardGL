#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/wpa-ii-12-5-6-signed-close-case-declaration-lifecycle-implementation-design-gate-v1.json'
PROD=['src/Billiards/rules/double_hit_close_case_declaration.h','src/Billiards/rules/double_hit_close_case_declaration.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp']
TEST=['tests/double_hit_close_case_declaration_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
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
 e=[];keys={'schema','status','authority_bindings','official_and_selected_boundary','declaration_protocol_contract','exact_payload_schemas','pre_shot_and_one_shot_lifecycle','d5_and_classification_blocker','atomicity_idempotency_and_readback','implementation_allowlist','required_fixtures','source_bindings','completion_boundary'}
 if set(v)!=keys:e.append('keys')
 if v.get('schema')!='wpa_ii_12_5_6_signed_close_case_declaration_lifecycle_implementation_design_gate_v1':e.append('schema')
 if v.get('status')!='FINAL_DESIGN_ONLY_ROOT_REVALIDATION_REQUIRED_NO_PRODUCTION_CHANGE':e.append('status')
 a=v.get('authority_bindings',{});a=a if isinstance(a,dict) else {}
 expected={'root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','b2_foundation_receipt':'1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b','b2_priority_register':'8a778fc249a2ba952ff6acca60c5d24cad6ca2135f606bb54e3e0b887d092f55','cue_contact_cluster_gap_gate':'1442e4204c4acb0718ca0b87c858c8222e03ac634f5b5b7def3d9a6bdae64033','audit_v9':'eefd6121b7df2b41e45b66f3c87e2732bbccdb3808d866a89d44e8a324b9cf01'}
 for key,digest in expected.items():
  row=a.get(key,{})
  if not isinstance(row,dict) or row.get('sha256')!=digest:e.append('authority:'+key)
  elif check_files and not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('authority_drift:'+key)
 p=a.get('root_selected_policy',{})
 if p.get('row_id')!='WPA-II-12-5-6' or p.get('selected_option_id')!='AUTO_ONLY_COMPLETE_MULTI_INTERVAL_REFEREE_CLOSE_CASES' or p.get('row_implementation_allowed') is not False:e.append('policy')
 o=v.get('official_and_selected_boundary',{});constants=o.get('selected_machine_constants',{}) if isinstance(o,dict) else {}
 expected_constants={'automatic_foul_requires_complete_full_stroke':True,'automatic_foul_tip_contact_interval_relation':'GREATER_THAN_ONE','canonical_interval_dedupe_required':True,'close_ball_presumption_route':'SIGNED_REFEREE_INPUT','very_close_distance_threshold':None,'light_graze_threshold':None,'incomplete_evidence_result':'NO_AUTOMATIC_CLASSIFICATION'}
 if constants!=expected_constants:e.append('constants')
 c=v.get('declaration_protocol_contract',{})
 if c.get('operations')!=['PROPOSE','FINAL_DECLARE','CANCEL_PROPOSAL'] or c.get('states')!=['INACTIVE','PROPOSAL_OPEN','CLOSE_CASE_ACTIVE','CLOSE_CASE_CONSUMED','CANCELLED','INVALIDATED'] or c.get('proposal_not_attestation') is not True or c.get('final_attests_close_case_only') is not True or c.get('final_does_not_attest_double_hit_or_foul') is not True or c.get('geometry_microtrace_or_single_launch_contact_may_attest') is not False or c.get('request_field_count')!=17:e.append('protocol')
 payload=v.get('exact_payload_schemas',{})
 if payload.get('forbidden_payload_fields')!=['double_hit','foul','tip_interval_count','contact_before_tip_separation','distance_threshold','graze_threshold']:e.append('payload_boundary')
 d=v.get('d5_and_classification_blocker',{})
 for key in ('d5_complete','microtrace_is_full_stroke','declaration_is_physics_fact','declaration_is_double_hit_or_foul_result','audit_row_may_be_promoted_by_partial_lifecycle'):
  if d.get(key) is not False:e.append('d5:'+key)
 if d.get('very_close_distance_threshold') is not None or d.get('light_graze_threshold') is not None or len(d.get('required_before_automatic_branch',[]))!=4:e.append('d5_boundary')
 life=v.get('pre_shot_and_one_shot_lifecycle',{})
 if life.get('later_physics_failure_reactivates') is not False or life.get('second_shot_reuses') is not False or len(life.get('invalidates',[]))!=4:e.append('lifecycle')
 allow=v.get('implementation_allowlist',{})
 if allow.get('production_files')!=PROD or allow.get('test_files')!=TEST or allow.get('build_files')!=['CMakeLists.txt'] or allow.get('physics_telemetry_shotfacts_rules_topology_cue_contact_or_coupled_contact_edit_allowed') is not False or allow.get('modern_product_ui_renderer_or_input_edit_allowed') is not False:e.append('allowlist')
 fx=v.get('required_fixtures',{});pos=fx.get('positive',[]) if isinstance(fx,dict) else [];neg=fx.get('negative',[]) if isinstance(fx,dict) else []
 if len(pos)!=10 or len(neg)!=19 or len(set(pos+neg))!=29:e.append('fixtures')
 sources=v.get('source_bindings')
 expected_sources=['CMakeLists.txt','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp']
 if not isinstance(sources,list) or [x.get('path') for x in sources if isinstance(x,dict)]!=expected_sources:e.append('sources')
 elif check_files:
  for row in sources:
   if not bind(row):e.append('source_drift')
 cb=v.get('completion_boundary',{})
 for key in ('implementation_authorized_by_this_gate','signed_close_case_partial_lifecycle_implemented','d5_complete','wpa_ii_12_5_6_implemented','audit_row_promoted','b2_complete','m4_complete','production_or_cmake_modified','visible_or_global_ui_exposed','gpu_readiness_or_authorization'):
  if cb.get(key) is not False:e.append('false_claim:'+key)
 if cb.get('root_revalidation_required_before_tests_first_patch') is not True:e.append('root_gate')
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('gate',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');args=ap.parse_args()
 try:v=json.loads(Path(args.gate).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not args.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_12_5_6_SIGNED_CLOSE_CASE_DECLARATION_LIFECYCLE_DESIGN_GATE');return 0
if __name__=='__main__':raise SystemExit(main())
