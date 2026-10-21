#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/wpa-ii-17-exact-nonphysical-referee-foul-subset-implementation-design-gate-v1.json'
KINDS=['BOTH_FEET_OFF_GROUND_DURING_SHOT','ILLEGAL_BODY_CLOTHING_OR_EQUIPMENT_BALL_TOUCH','ONE_HAND_SHOT','AUXILIARY_EQUIPMENT_PLACED_ON_TABLE_TO_SHOOT','MOBILE_DEVICE_DISTURBANCE_GENERAL_FOUL']
PROD=['src/Billiards/rules/nonphysical_general_foul_authority.h','src/Billiards/rules/nonphysical_general_foul_authority.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp']
TEST=['tests/nonphysical_general_foul_authority_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
LIVE=['CMakeLists.txt','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp']
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
 e=[];keys={'schema','status','authority_bindings','exact_subset','excluded_and_deferred','exact_payload_and_snapshot','atomic_lifecycle','required_fixtures','implementation_allowlist','live_source_bindings','completion_boundary'}
 if set(v)!=keys:e.append('keys')
 if v.get('schema')!='wpa_ii_17_exact_nonphysical_referee_foul_subset_implementation_design_gate_v1':e.append('schema')
 if v.get('status')!='FINAL_DESIGN_ONLY_ROOT_REVALIDATION_REQUIRED_NO_PRODUCTION_CHANGE':e.append('status')
 a=v.get('authority_bindings',{});expected={'root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','b2_foundation_receipt':'1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b','b2_priority_register':'8a778fc249a2ba952ff6acca60c5d24cad6ca2135f606bb54e3e0b887d092f55','audit_v9':'eefd6121b7df2b41e45b66f3c87e2732bbccdb3808d866a89d44e8a324b9cf01','classified_subflow_audit':'0bfdfed54a82be8b82d4ad696bb5ae56fa20dc9c8e3c6e122ef95ce6ea576d50','cue_scratch_receipt':'38d4c067d0f3c77603ef5149ea5160e859f8fc8d02c52d04a116761b8106aef5','off_table_gap_gate':'3fc468502ab09e7ba0122d84b6c20386b0646d1f0db76a3f8ab3e8a1d30a9dbe'}
 for k,d in expected.items():
  row=a.get(k,{}) if isinstance(a,dict) else {}
  if row.get('sha256')!=d:e.append('authority:'+k)
  elif check_files and not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('authority_drift:'+k)
 p=a.get('root_selected_policy',{}) if isinstance(a,dict) else {}
 if p.get('row_id')!='WPA-II-17' or p.get('selected_option_id')!='EVIDENCE_BACKED_SUBSET_PLUS_SIGNED_REFEREE_EVENTS' or p.get('row_implementation_allowed') is not False:e.append('policy')
 s=v.get('exact_subset',{})
 if s.get('signed_referee_event_kinds')!=KINDS or s.get('actor')!='authenticated referee only' or s.get('proposal_is_not_foul') is not True or s.get('final_is_signed_referee_fact_and_parent_commit_request') is not True or s.get('automatic_reclassification_forbidden') is not True:e.append('subset')
 x=v.get('excluded_and_deferred',{})
 if x.get('already_implemented_automatic_subset')!=['CUE_BALL_POCKET_SCRATCH','WRONG_FIRST_CONTACT','NO_CONTACT_OR_NO_RAIL'] or x.get('d5_deferred')!=['DOUBLE_HIT','PUSH_SHOT','OBVIOUS_CONTACT_FOUL'] or x.get('d6_deferred')!=['CUE_BALL_OFF_TABLE','OBJECT_BALL_OFF_TABLE'] or x.get('full_row_closed') is not False:e.append('deferred')
 q=v.get('exact_payload_and_snapshot',{})
 if q.get('declaration_kind')!='WPA_II_17_NONPHYSICAL_GENERAL_FOUL_REFEREE_EVENT' or q.get('operations')!=['PROPOSE','FINAL_DECLARE','CANCEL_PROPOSAL'] or q.get('request_field_count')!=17 or q.get('proposal_fields')!=['event_kind','offending_player','shot_identity'] or q.get('final_fields')!=['event_kind','offending_player','proposal_decision_id','shot_identity'] or q.get('cancel_fields')!=['proposal_decision_id']:e.append('payload')
 atom=v.get('atomic_lifecycle',{})
 for k in ('proposal_is_not_foul',):
  pass
 if atom.get('global_rules_ui_exposed') is not False or atom.get('headless_automation_only') is not True or 'zero duplicate' not in str(atom.get('exact_retry')) or 'byte-equivalent' not in str(atom.get('conflicting_retry')):e.append('atomic')
 fx=v.get('required_fixtures',{});pos=fx.get('positive',[]) if isinstance(fx,dict) else [];neg=fx.get('negative',[]) if isinstance(fx,dict) else []
 if len(pos)!=10 or len(neg)!=20 or len(set(pos+neg))!=30:e.append('fixtures')
 al=v.get('implementation_allowlist',{})
 if al.get('production_files')!=PROD or al.get('test_files')!=TEST or al.get('build_files')!=['CMakeLists.txt'] or al.get('physics_telemetry_shotfacts_rules_or_modern_ui_edit_allowed') is not False:e.append('allowlist')
 live=v.get('live_source_bindings',[])
 if not isinstance(live,list) or [r.get('path') for r in live if isinstance(r,dict)]!=LIVE:e.append('live')
 elif check_files:
  for row in live:
   if not bind(row):e.append('live_drift')
 cb=v.get('completion_boundary',{})
 for k in ('implementation_authorized_by_this_gate','nonphysical_subset_implemented','cue_scratch_modified','d5_complete','d6_complete','off_table_policy_resolved','wpa_ii_17_full_row_implemented','audit_row_promoted','m4_complete','production_or_cmake_modified','visible_or_global_ui_exposed','gpu_readiness_or_authorization'):
  if cb.get(k) is not False:e.append('false_claim:'+k)
 if cb.get('root_revalidation_required') is not True:e.append('root_gate')
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('gate',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');args=ap.parse_args()
 try:v=json.loads(Path(args.gate).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not args.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_17_EXACT_NONPHYSICAL_REFEREE_FOUL_SUBSET_DESIGN_GATE');return 0
if __name__=='__main__':raise SystemExit(main())
