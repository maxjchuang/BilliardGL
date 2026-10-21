#!/usr/bin/env python3
"""Validate the design-only WPA-II-21 stalemate consumer gate."""
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
from typing import Any
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/wpa-ii-21-stalemate-referee-consumer-implementation-design-gate-v1.json'
KEYS={'schema','status','authority_bindings','consumer_identity_and_policy','exact_payload_schemas','rack_and_order_snapshot_contract','state_machine_contract','shot_counter_contract','atomicity_idempotency_and_reset','automation_headless_contract','implementation_allowlist','required_fixtures','source_bindings','test_source_bindings','completion_boundary'}
DIGESTS={'authority_bindings':'2da62efaf53c8af7185092dc7b8474beadac5e49bcd1ffc75af4e0890a2ba8d1','consumer_identity_and_policy':'60793d2fd92a50fd9f01629cd30afda59f282ec599f7d165e9133bf590549b7c','exact_payload_schemas':'b964be47e4beddaf3710788584b000dfe90861790382fc2a40d4524870c64dbc','rack_and_order_snapshot_contract':'646ecca6037696f54f9be32d16c079462e05a315499b2987fba8f08e02399207','state_machine_contract':'3f273820a4e87f5795c02107a5221a7cc43330da4ed25fbe0d7f9a97afb3b2c7','shot_counter_contract':'c9e76f5b27a27d811d747bbe5d40a116c7f27d4b984c72f3dd6f0493861113b8','atomicity_idempotency_and_reset':'66d76d7aa25d491fe432d5c6fd3624d9896f7639c337370899c77c141820c0a4','automation_headless_contract':'052d357d04f04c41aa22b22ab4151b7f542f7c7dc7883f38aabf9bc9b787068a','implementation_allowlist':'5d3792d73e1c1182bca2a4483da1ea7d2e7bcecae55a11bcf6b80fc1be20a432','required_fixtures':'5ab1e3f428df92305e8291191aada537b9e0b032259e965936707d8e9effeb0e','source_bindings':'1ef77245962de024c5607f5942e2fb87d769ac797dc36e88c44cb4f7cfebff73','test_source_bindings':'f62df245dcf7dc60dfe03afff68520f02411e52943d627d03d3c39accf19e50e','completion_boundary':'83b378624c9da335d951189c78f6bae22076aaaad7c420a75684d9318013d8b6'}
PROD=['src/Billiards/rules/stalemate_authority.h','src/Billiards/rules/stalemate_authority.cpp','src/Billiards/rules/referee_authority_protocol.h','src/Billiards/rules/referee_authority_protocol.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp']
TEST=['tests/stalemate_authority_tests.cpp','tests/referee_authority_protocol_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
def sha(b:bytes)->str:return hashlib.sha256(b).hexdigest()
def digest(v:Any)->str:return sha(json.dumps(v,sort_keys=True,separators=(',',':')).encode())
def resolve(raw:Any):
 if not isinstance(raw,str):return None
 p=Path(raw)
 if p.is_absolute() or '..' in p.parts:return None
 q=(ROOT/p).resolve();return q if q.is_relative_to(ROOT.resolve()) else None
def binding_ok(v:Any)->bool:
 if not isinstance(v,dict) or set(v)!={'path','sha256'}:return False
 p=resolve(v.get('path'));return p is not None and p.is_file() and sha(p.read_bytes())==v.get('sha256')
def validate(v:Any,check_files:bool=True)->list[str]:
 if not isinstance(v,dict):return ['root_not_object']
 e=[]
 if set(v)!=KEYS:e.append('top_level_keys')
 if v.get('schema')!='wpa_ii_21_stalemate_referee_consumer_implementation_design_gate_v1':e.append('schema')
 if v.get('status')!='DESIGN_COMPLETE_AWAITING_ROOT_EXACT_ALLOWLIST_APPROVAL_NO_PRODUCTION_MUTATION':e.append('status')
 for k,h in DIGESTS.items():
  if digest(v.get(k))!=h:e.append('contract_digest:'+k)
 a=v.get('authority_bindings',{});a=a if isinstance(a,dict) else {}
 expected={'root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','b2_foundation_receipt':'1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b','b2_priority_register':'8a778fc249a2ba952ff6acca60c5d24cad6ca2135f606bb54e3e0b887d092f55','original_design_gap_gate':'a7156415057caf733e2a6c7317d470ac13e01715b3f9c77b3f960b7d414b4e55'}
 for k,h in expected.items():
  b=a.get(k,{}) if isinstance(a,dict) else {}
  if b.get('sha256')!=h:e.append('authority:'+k)
  elif check_files and not binding_ok({'path':b.get('path'),'sha256':b.get('sha256')}):e.append('authority_drift:'+k)
 if a.get('root_c1_selection_semantic_sha256')!='eee6bee9703d9efd129d92073f29bf39c19c436ad0d9c5a1520fe62e3c79c640':e.append('root_selection')
 c=v.get('consumer_identity_and_policy',{});c=c if isinstance(c,dict) else {}
 if c.get('consumer_id')!='WPA-II-21_STALEMATE_REFEREE_CONSUMER' or c.get('proposal_actor_role')!='REFEREE_ONLY' or c.get('final_actor_role')!='REFEREE_ONLY_SAME_SUBJECT_AS_PROPOSAL' or c.get('player_refusal_roles')!=['PLAYER_0','PLAYER_1']:e.append('identity')
 for k in ('player_refusal_is_final_declaration','specific_other_wpa_consumer_registered','detached_verifier_generic_command_exposed'):
  if c.get(k) is not False:e.append('identity_false:'+k)
 p=v.get('exact_payload_schemas',{});p=p if isinstance(p,dict) else {};proposal=p.get('proposal',{}) if isinstance(p,dict) else {};final=p.get('final',{}) if isinstance(p,dict) else {}
 if proposal.get('default_allowance')!=3 or proposal.get('explicit_alternative_range_inclusive')!=[1,255] or proposal.get('stalemate_attested') is not True:e.append('proposal_payload')
 if final.get('completed_shots_exact_array_length')!=2 or final.get('stalemate_still_present_attested') is not True:e.append('final_payload')
 rack=v.get('rack_and_order_snapshot_contract',{});rack=rack if isinstance(rack,dict) else {};core=rack.get('core_e_binding',{}) if isinstance(rack,dict) else {}
 if rack.get('current_player_may_replace_original_breaker') is not False or rack.get('foundation_id_may_be_submitted_by_consumer_caller') is not False or core.get('direct_product_or_automation_exposure_allowed') is not False:e.append('rack_boundary')
 if check_files:
  for key in ('header','source'):
   if not binding_ok(core.get(key)):e.append('core_e_drift')
 sm=v.get('state_machine_contract',{});sm=sm if isinstance(sm,dict) else {}
 if len(sm.get('states',[]))!=6 or sm.get('partial_commit_allowed') is not False or 'never declares' not in str(sm.get('no_auto_final')):e.append('state_machine')
 shots=v.get('shot_counter_contract',{});shots=shots if isinstance(shots,dict) else {}
 if len(shots.get('count_only',[]))!=2 or len(shots.get('never_count',[]))!=9 or shots.get('counter_snapshot_digest_required_in_final') is not True or shots.get('numerical_threshold_is_referee_authority') is not False:e.append('counter')
 atom=v.get('atomicity_idempotency_and_reset',{});atom=atom if isinstance(atom,dict) else {}
 if 'zero' not in str(atom.get('exact_retry')) or 'all roll back' not in str(atom.get('failed_core_e_plan_or_commit')) or atom.get('turn_change_or_legal_pocket_alone_resets') is not False:e.append('atomicity')
 auto=v.get('automation_headless_contract',{});auto=auto if isinstance(auto,dict) else {}
 if len(auto.get('commands',[]))!=5 or auto.get('headless_only') is not True or auto.get('modern_product_session_or_input_adapter_exposed') is not False or auto.get('global_rules_decision_ui_product_exposed') is not False or auto.get('visible_ui_requires_separate_gate') is not True:e.append('automation')
 allow=v.get('implementation_allowlist',{});allow=allow if isinstance(allow,dict) else {}
 if allow.get('production_files')!=PROD or allow.get('test_files')!=TEST or allow.get('build_files')!=['CMakeLists.txt']:e.append('allowlist')
 for k in ('rack_executor_edit_allowed','chinese_rules_shotfacts_physics_edit_allowed','modern_product_ui_renderer_input_edit_allowed'):
  if allow.get(k) is not False:e.append('forbidden_allowlist:'+k)
 fx=v.get('required_fixtures',{});fx=fx if isinstance(fx,dict) else {}
 if len(fx.get('positive',[]))!=11 or len(fx.get('negative',[]))!=17 or fx.get('headless_cpu_only') is not True:e.append('fixtures')
 src=v.get('source_bindings');tst=v.get('test_source_bindings')
 expected_src=['CMakeLists.txt']+PROD[2:]+['src/Billiards/rules/rack_executor.h','src/Billiards/rules/rack_executor.cpp']
 if not isinstance(src,list) or [x.get('path') for x in src if isinstance(x,dict)]!=expected_src:e.append('sources')
 if not isinstance(tst,list) or [x.get('path') for x in tst if isinstance(x,dict)]!=TEST[1:]:e.append('tests')
 if check_files:
  for rows,label in ((src,'source'),(tst,'test')):
   if isinstance(rows,list):
    for row in rows:
     if not binding_ok(row):e.append(label+'_drift')
 b=v.get('completion_boundary',{});b=b if isinstance(b,dict) else {}
 if b.get('root_revalidation_required_before_tests_first_patch') is not True:e.append('root_gate')
 for k in ('implementation_authorized_by_this_gate','wpa_ii_21_implemented','audit_row_promoted','b2_complete','m4_complete','production_modified','cmake_modified','visible_product_ui_or_global_exposure','gpu_readiness_or_authorization'):
  if b.get(k) is not False:e.append('false_completion:'+k)
 return e
def main()->int:
 p=argparse.ArgumentParser();p.add_argument('gate',nargs='?',default=str(DEFAULT));p.add_argument('--no-file-checks',action='store_true');a=p.parse_args()
 try:v=json.loads(Path(a.gate).read_text())
 except (OSError,json.JSONDecodeError) as ex:print('INVALID:',ex);return 1
 e=validate(v,not a.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_21_STALEMATE_REFEREE_CONSUMER_IMPLEMENTATION_DESIGN_GATE');return 0
if __name__=='__main__':raise SystemExit(main())
