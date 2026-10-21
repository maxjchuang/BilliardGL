#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
from typing import Any
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/wpa-ii-15-1-signed-frozen-cue-object-declaration-lifecycle-implementation-design-gate-v1.json'
KEYS={'schema','status','authority_bindings','official_and_selected_boundary','declaration_protocol_contract','exact_payload_schemas','pre_shot_snapshot_and_legality','one_shot_lifecycle','d5_classification_blocker','atomicity_idempotency_and_reset','automation_headless_contract','implementation_allowlist','required_fixtures','source_bindings','test_source_bindings','completion_boundary'}
DIGESTS={'authority_bindings':'f2233a0d359919b0b09ee03678c77e4c5f51a45752b5614c50f6225971a8d5d4','official_and_selected_boundary':'34d14367042a84f8064492bd9adf64625d3d702f0ae5682bd154b975a1ab6924','declaration_protocol_contract':'1474da9ea5f2ed4d792baa4e0309e7bd005503c5880f1a7be32e02abe8002342','exact_payload_schemas':'d78bc978f722eb6dbb5c8cc95ac1e19c7f95b8ced1216aafe2a7d8e343cebdb1','pre_shot_snapshot_and_legality':'1bba636cf5e99d7a0d3a81f832ec14f2fd8091ecda346dd78c0a17a4f27577b8','one_shot_lifecycle':'63194bf31868d7cfb4f56946447e5619ef48c86909f44285aafdb61992deff6b','d5_classification_blocker':'c0210d087477a2fe2a5cb74447a7264500e1c0bc91c3e42460a341c19b516bd1','atomicity_idempotency_and_reset':'4c60993f116184ccb10d6c6a3d7aa745cc6069a224166b326a57a0158c8288ca','automation_headless_contract':'b7a66562e01083a43fed5124dd1f54eeeb5336112236baf546fbd603544f4ae8','implementation_allowlist':'23d71d2b416d8499e1f4e08dc7db76dbd55efb4b2dd63f23f9362276e2b7849d','required_fixtures':'f4dcaf7de0816d929b053334ce33f75251af276d54619bad5297fbf24b2869ef','source_bindings':'2f477c574d07ba37fb9540c74c46520d16d8ca67754a603f5c7bac3a99d825a2','test_source_bindings':'8bc42aba2f4af019090ce912f8668c41b57276d9bf7d4204845e82e1007573dc','completion_boundary':'1b1555da1058fe3f997145b61b62b769ff1a6d6522de7170012f2965a3eda19a'}
PROD=['src/Billiards/rules/frozen_ball_declaration.h','src/Billiards/rules/frozen_ball_declaration.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp']
TEST=['tests/frozen_ball_declaration_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
def sha(b):return hashlib.sha256(b).hexdigest()
def dig(v):return sha(json.dumps(v,sort_keys=True,separators=(',',':')).encode())
def safe(s):
 if not isinstance(s,str) or Path(s).is_absolute() or '..' in Path(s).parts:return None
 p=(ROOT/s).resolve();return p if p.is_relative_to(ROOT.resolve()) else None
def bind(v):
 if not isinstance(v,dict) or set(v)!={'path','sha256'}:return False
 p=safe(v.get('path'));return p is not None and p.is_file() and sha(p.read_bytes())==v.get('sha256')
def validate(v:Any,check_files=True):
 if not isinstance(v,dict):return ['root']
 e=[]
 if set(v)!=KEYS:e.append('keys')
 if v.get('schema')!='wpa_ii_15_1_signed_frozen_cue_object_declaration_lifecycle_implementation_design_gate_v1':e.append('schema')
 if v.get('status')!='DESIGN_COMPLETE_AWAITING_ROOT_EXACT_ALLOWLIST_APPROVAL_PARTIAL_DECLARATION_LIFECYCLE_ONLY_NO_ROW_CLOSURE':e.append('status')
 for k,h in DIGESTS.items():
  if dig(v.get(k))!=h:e.append('digest:'+k)
 a=v.get('authority_bindings',{});a=a if isinstance(a,dict) else {}
 refs={'root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','b2_foundation_receipt':'1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b','b2_priority_register':'8a778fc249a2ba952ff6acca60c5d24cad6ca2135f606bb54e3e0b887d092f55','cue_contact_cluster_gap_gate':'1442e4204c4acb0718ca0b87c858c8222e03ac634f5b5b7def3d9a6bdae64033','audit_v9':'eefd6121b7df2b41e45b66f3c87e2732bbccdb3808d866a89d44e8a324b9cf01'}
 for k,h in refs.items():
  x=a.get(k,{})
  if x.get('sha256')!=h:e.append('authority:'+k)
  elif check_files and not bind({'path':x.get('path'),'sha256':h}):e.append('authority_drift:'+k)
 o=v.get('official_and_selected_boundary',{});o=o if isinstance(o,dict) else {}
 if len(o.get('officially_determined',[]))!=4 or o.get('selected_shipping_scope')!='SIGNED_PRE_SHOT_FROZEN_DECLARATION' or o.get('declaration_authority')!='AUTHENTICATED_REFEREE_ONLY' or o.get('geometry_may_suggest_but_not_declare') is not True or o.get('declaration_may_not_classify_double_hit_push_or_obvious_stroke') is not True:e.append('boundary')
 p=v.get('declaration_protocol_contract',{});p=p if isinstance(p,dict) else {}
 if p.get('proposal_actor_role')!='REFEREE_ONLY' or p.get('final_actor_role')!='SAME_AUTHENTICATED_REFEREE_SUBJECT_AS_PROPOSAL' or p.get('proposal_is_committed_frozen_declaration') is not False or p.get('final_is_committed_frozen_declaration') is not True or p.get('player_or_geometry_declaration_allowed') is not False:e.append('protocol')
 payload=v.get('exact_payload_schemas',{});payload=payload if isinstance(payload,dict) else {}
 if payload.get('proposal',{}).get('exact_keys_in_canonical_order')!=['cue_ball_id','frozen_attested','object_ball_id','relation'] or payload.get('proposal',{}).get('object_ball_id_range_inclusive')!=[1,15] or payload.get('final',{}).get('same_values_as_proposal_required') is not True or payload.get('cancel',{}).get('only_before_final') is not True:e.append('payload')
 s=v.get('pre_shot_snapshot_and_legality',{});s=s if isinstance(s,dict) else {}
 if len(s.get('parent_preconditions',[]))!=8 or len(s.get('snapshot_fields',[]))!=12 or s.get('geometry_distance_or_solver_slop_used_as_declaration') is not False or s.get('frozen_cue_topology_used_as_declaration') is not False or s.get('all_snapshot_fields_enter_parent_authority_hash') is not True:e.append('snapshot')
 life=v.get('one_shot_lifecycle',{});life=life if isinstance(life,dict) else {}
 if len(life.get('states',[]))!=6 or len(life.get('does_not_consume',[]))!=6 or life.get('later_physics_failure_reactivates_declaration') is not False or life.get('second_shot_can_reuse_declaration') is not False or life.get('exact_retry_or_duplicate_accepted_shot_consumes_twice') is not False:e.append('lifecycle')
 d5=v.get('d5_classification_blocker',{});d5=d5 if isinstance(d5,dict) else {}
 for k in ('complete_full_stroke_tip_contact_telemetry_available','obvious_push_machine_predicate_available','obvious_stroke_machine_predicate_available','close_ball_light_graze_presumption_available','declaration_direct_movement_exception_adjudication_allowed','declaration_obvious_push_foul_adjudication_allowed','shotfacts_or_foul_reason_change_allowed','wpa_ii_15_1_row_may_be_promoted_after_lifecycle_only'):
  if d5.get(k) is not False:e.append('d5_false:'+k)
 if d5.get('m4_value')!='PARTIAL_DECLARATION_PRIMITIVE_ONLY_NO_ROW_CLOSURE':e.append('d5_scope')
 atom=v.get('atomicity_idempotency_and_reset',{});atom=atom if isinstance(atom,dict) else {}
 if len(atom.get('prevalidate',[]))!=10 or 'zero' not in str(atom.get('exact_retry')) or 'byte-equivalent' not in str(atom.get('failure_rollback')) or atom.get('partial_commit_allowed') is not False:e.append('atomicity')
 auto=v.get('automation_headless_contract',{});auto=auto if isinstance(auto,dict) else {}
 if len(auto.get('commands',[]))!=4 or auto.get('headless_only') is not True or auto.get('modern_product_session_or_input_adapter_exposed') is not False or auto.get('global_rules_decision_ui_product_exposed') is not False or auto.get('visible_referee_surface_requires_separate_gate') is not True:e.append('automation')
 allow=v.get('implementation_allowlist',{});allow=allow if isinstance(allow,dict) else {}
 if allow.get('production_files')!=PROD or allow.get('test_files')!=TEST or allow.get('build_files')!=['CMakeLists.txt'] or allow.get('protocol_frozen_topology_cue_contact_coupled_contact_physics_telemetry_shotfacts_rules_edit_allowed') is not False or allow.get('modern_product_ui_renderer_input_edit_allowed') is not False:e.append('allowlist')
 fx=v.get('required_fixtures',{});fx=fx if isinstance(fx,dict) else {}
 if len(fx.get('positive',[]))!=10 or len(fx.get('negative',[]))!=17 or fx.get('headless_cpu_only') is not True:e.append('fixtures')
 src=v.get('source_bindings');tst=v.get('test_source_bindings')
 if not isinstance(src,list) or len(src)!=8 or not isinstance(tst,list) or [x.get('path') for x in tst if isinstance(x,dict)]!=['tests/referee_authority_protocol_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']:e.append('bindings')
 if check_files:
  for rows,label in ((src,'source'),(tst,'test')):
   if isinstance(rows,list):
    for row in rows:
     if not bind(row):e.append(label+'_drift')
 c=v.get('completion_boundary',{});c=c if isinstance(c,dict) else {}
 if c.get('root_revalidation_required_before_tests_first_patch') is not True:e.append('root_gate')
 for k in ('implementation_authorized_by_this_gate','declaration_lifecycle_implemented','wpa_ii_15_1_implemented','audit_row_promoted','d5_classification_complete','b2_complete','m4_complete','production_modified','cmake_modified','visible_product_ui_or_global_exposure','gpu_readiness_or_authorization'):
  if c.get(k) is not False:e.append('false_completion:'+k)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('gate',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');a=ap.parse_args()
 try:v=json.loads(Path(a.gate).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not a.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_15_1_SIGNED_FROZEN_DECLARATION_LIFECYCLE_DESIGN_GATE');return 0
if __name__=='__main__':raise SystemExit(main())
