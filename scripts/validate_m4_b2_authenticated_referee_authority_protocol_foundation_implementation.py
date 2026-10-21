#!/usr/bin/env python3
"""Validate the M4 B2 referee-authority foundation receipt."""
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path
from typing import Any

ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/m4-b2-authenticated-referee-authority-protocol-foundation-implementation-receipt-v1.json'
KEYS={'schema','status','authority_bindings','tests_first_record','implemented_contract','source_bindings','test_bindings','implementation_aggregate_sha256','cpu_verification','known_unrelated_build_gap','completion_boundary'}
DIGESTS={
'authority_bindings':'85873b49d5fe734d2b9645d20b11c2e9853ad5715e41d675eaa00bafe6721072','tests_first_record':'6a19de14fca203edc16f6e8020070b369e14516470dca429bfa2c66e7f54fd76','implemented_contract':'a22692529a215d9fb1d3ecc1c1a79f874c25975c1cfcad866a7661c989e6dd4b','source_bindings':'4afb1fe50dc9842476e15c375f31f3b9f7c11bd354ff07b4d3fc167e417f7f7a','test_bindings':'f62df245dcf7dc60dfe03afff68520f02411e52943d627d03d3c39accf19e50e','implementation_aggregate_sha256':'9508ab032c5344597820815e472020410db180f41e7c52a195d4713a746062d9','cpu_verification':'c538d12a19bdef00c51f1a0f9837426c7849995d54699fddb0292fb471d2f4fe','known_unrelated_build_gap':'abcc71f7e724e26fb214242fd8832c89f0d07b9d55194a667f561529b574fd5f','completion_boundary':'12bdc3a1b5ebaf4117d1fc2a7db33984fcfe29fa7791ec2c307384b394dd5cb8'}
SOURCE=['CMakeLists.txt','src/Billiards/rules/referee_authority_protocol.h','src/Billiards/rules/referee_authority_protocol.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp']
TEST=['tests/referee_authority_protocol_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
AGG='7bfa14ee830c0366530c206c552ee44c040a7ced7d66977f424778b803e06d51'
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
def aggregate(rows:list[dict])->str:
 return sha('\n'.join(f"{r['path']}:{r['sha256']}" for r in rows).encode())

def validate(v:Any,check_files:bool=True)->list[str]:
 if not isinstance(v,dict):return ['root_not_object']
 e=[]
 if set(v)!=KEYS:e.append('top_level_keys')
 if v.get('schema')!='m4_b2_authenticated_referee_authority_protocol_foundation_implementation_receipt_v1':e.append('schema')
 if v.get('status')!='IMPLEMENTED_CPU_VERIFIED_FOUNDATION_ONLY_NO_RULE_CONSUMER_NO_UI':e.append('status')
 for k,h in DIGESTS.items():
  if digest(v.get(k))!=h:e.append('contract_digest:'+k)
 a=v.get('authority_bindings',{});a=a if isinstance(a,dict) else {}
 refs=[a.get('root_selected_policy',{}),a.get('implementation_design_gate',{})]
 if refs[0].get('sha256')!='fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f' or refs[1].get('sha256')!='0d006f9571cb3ae439e5be6d6f618f06afafdd2167bec777cabbc31eb33d19c8':e.append('authority')
 if check_files:
  for ref in refs:
   if not binding_ok({'path':ref.get('path'),'sha256':ref.get('sha256')}):e.append('authority_drift')
 red=v.get('tests_first_record',{});red=red if isinstance(red,dict) else {}
 if red.get('red_observed') is not True or len(red.get('red_failures',[]))!=3 or red.get('green_after_production_patch') is not True:e.append('tests_first')
 c=v.get('implemented_contract',{});c=c if isinstance(c,dict) else {}
 if c.get('authenticated_exact_request_field_count')!=17 or c.get('registered_specific_wpa_consumer_count')!=0 or c.get('operations')!=['PROPOSE','FINAL_DECLARE','CANCEL_PROPOSAL']:e.append('contract')
 for k in ('proposal_is_final','referee_declaration_promoted_to_physics_shotfacts_or_foul'):
  if c.get(k) is not False:e.append('contract_false:'+k)
 for k in ('final_requires_live_exact_proposal_revalidation','atomic_commit_guard_and_rollback','exact_retry_zero_delta','nonce_and_idempotency_conflict_fail_closed','reset_changes_generation_and_hash_and_invalidates_proposal_nonce_receipts'):
  if c.get(k) is not True:e.append('contract_true:'+k)
 src=v.get('source_bindings');tst=v.get('test_bindings')
 if not isinstance(src,list) or [x.get('path') for x in src if isinstance(x,dict)]!=SOURCE:e.append('sources')
 if not isinstance(tst,list) or [x.get('path') for x in tst if isinstance(x,dict)]!=TEST:e.append('tests')
 if isinstance(src,list) and isinstance(tst,list):
  if aggregate(src+tst)!=AGG or v.get('implementation_aggregate_sha256')!=AGG:e.append('aggregate')
 if check_files:
  for rows,label in ((src,'source'),(tst,'test')):
   if isinstance(rows,list):
    for row in rows:
     if not binding_ok(row):e.append(label+'_drift')
 cpu=v.get('cpu_verification',{});cpu=cpu if isinstance(cpu,dict) else {}
 if cpu.get('passed_target_count')!=11 or len(cpu.get('passed_targets',[]))!=11 or cpu.get('gpu_cocoa_opengl_executed') is not False:e.append('cpu')
 gap=v.get('known_unrelated_build_gap',{});gap=gap if isinstance(gap,dict) else {}
 if gap.get('target')!='BilliardsShotFactCollectorTests' or gap.get('modified_outside_allowlist_to_fix') is not False:e.append('unrelated_gap')
 b=v.get('completion_boundary',{});b=b if isinstance(b,dict) else {}
 if b.get('foundation_implemented') is not True or b.get('next_step_requires_per_consumer_design_gate') is not True:e.append('boundary_true')
 for k in ('specific_wpa_rule_consumer_implemented','b2_complete','audit_row_promoted','global_player_rules_ui_exposed','physics_shotfacts_adjudicator_rack_modified','gpu_ta_readiness_or_authorization'):
  if b.get(k) is not False:e.append('false_completion:'+k)
 return e
def main()->int:
 ap=argparse.ArgumentParser();ap.add_argument('receipt',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');x=ap.parse_args()
 try:v=json.loads(Path(x.receipt).read_text())
 except (OSError,json.JSONDecodeError) as ex:print('INVALID:',ex);return 1
 e=validate(v,not x.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_B2_AUTHENTICATED_REFEREE_AUTHORITY_PROTOCOL_FOUNDATION_IMPLEMENTATION');return 0
if __name__=='__main__':raise SystemExit(main())
