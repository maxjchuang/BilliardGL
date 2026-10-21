#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
from typing import Any
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/wpa-ii-21-stalemate-referee-consumer-implementation-receipt-v1.json'
KEYS={'schema','status','authority_bindings','tests_first_record','implemented_contract','automation_headless','fixture_matrix','source_bindings','implementation_aggregate_sha256','test_bindings','test_aggregate_sha256','cpu_verification','completion_boundary'}
PROD=['CMakeLists.txt','src/Billiards/rules/stalemate_authority.h','src/Billiards/rules/stalemate_authority.cpp','src/Billiards/rules/referee_authority_protocol.h','src/Billiards/rules/referee_authority_protocol.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp']
TEST=['tests/stalemate_authority_tests.cpp','tests/referee_authority_protocol_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
COMMANDS=['rules.stalemate_referee_propose','rules.stalemate_player_refuse','rules.stalemate_referee_final','rules.stalemate_referee_cancel','get_stalemate_authority_state']
DIGESTS={'tests_first_record':'f5a2ffdc70052500041d4ee91b7e07dd584fef047d74293e9633950c01ef8eb0','implemented_contract':'3d58c16c8a90c1457a421052a37f26b4bd691c31b82f9305ed28d514b1f1ea13','automation_headless':'c64b032532dc137ea7dc08aea2e76022fd9bb0e50e0def0572358d66d986f792','fixture_matrix':'612bbf1a10042aeea1e205bc6ca4c20f6cf0ef95bba1fbf1c36d731c1d7c014a'}
def sha(b):return hashlib.sha256(b).hexdigest()
def dig(v):return sha(json.dumps(v,sort_keys=True,separators=(',',':')).encode())
def resolve(s):
 if not isinstance(s,str):return None
 p=Path(s)
 if p.is_absolute() or '..' in p.parts:return None
 q=(ROOT/p).resolve();return q if q.is_relative_to(ROOT.resolve()) else None
def bind(row):
 if not isinstance(row,dict) or set(row)!={'path','sha256'}:return False
 p=resolve(row.get('path'));return p is not None and p.is_file() and sha(p.read_bytes())==row.get('sha256')
def validate(v:Any,check_files=True):
 if not isinstance(v,dict):return ['root']
 e=[]
 if set(v)!=KEYS:e.append('keys')
 if v.get('schema')!='wpa_ii_21_stalemate_referee_consumer_implementation_receipt_v1':e.append('schema')
 if v.get('status')!='IMPLEMENTED_CPU_VERIFIED_HEADLESS_AUTHORITY_NO_VISIBLE_UI_NO_AUTO_M4_COMPLETION':e.append('status')
 for k,h in DIGESTS.items():
  if dig(v.get(k))!=h:e.append('digest:'+k)
 a=v.get('authority_bindings',{});a=a if isinstance(a,dict) else {};g=a.get('implementation_design_gate',{});p=a.get('root_selected_policy',{});f=a.get('b2_foundation_receipt',{})
 if g.get('sha256')!='f4820aa0a63fde6857a60208cc1e190f572bb57c6a5bc02a516e5d034b28b1f0' or g.get('root_revalidation')!='PASS_15_OF_15_EXACT_ALLOWLIST_TESTS_FIRST_AUTHORIZED':e.append('gate')
 if p.get('selected_option_id')!='SIGNED_REFEREE_DIRECTIVE_DEFAULT_THREE_EACH' or p.get('row_id')!='WPA-II-21' or f.get('sha256')!='1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b':e.append('authority')
 if check_files:
  for x,label in ((g,'gate'),(p,'policy'),(f,'foundation')):
   if not bind({'path':x.get('path'),'sha256':x.get('sha256')}):e.append(label+'_drift')
 r=v.get('tests_first_record',{});r=r if isinstance(r,dict) else {}
 if r.get('red_observed') is not True or r.get('green_after_minimal_production_patch') is not True or 'did not exist' not in str(r.get('red_failure')) or 'unresolved symbols' not in str(r.get('wide_link_gap_observed_and_closed_without_extra_link_dependency')):e.append('red_green')
 c=v.get('implemented_contract',{});c=c if isinstance(c,dict) else {}
 if c.get('states')!=['INACTIVE','PROPOSAL_OPEN','CONTINUATION_ACTIVE','AWAITING_REFEREE_FINAL','RERACK_COMMITTED','CANCELLED']:e.append('states')
 for k in ('exact_retry_zero_delta','idempotency_nonce_conflict_fail_closed','commit_failure_full_rollback','reset_and_other_official_rerack_invalidate'):
  if c.get(k) is not True:e.append('contract_true:'+k)
 for k in ('direct_core_e_command_exposed','referee_payload_promoted_to_physics_or_shotfacts','visible_or_global_rules_ui_exposed'):
  if c.get(k) is not False:e.append('contract_false:'+k)
 au=v.get('automation_headless',{});au=au if isinstance(au,dict) else {}
 if au.get('capability')!='wpa_ii_21_stalemate_referee_consumer_v1' or au.get('commands')!=COMMANDS or au.get('request_exact_field_count')!=17 or au.get('readback_omits')!=['credential_id','authentication_proof','payload_canonical'] or au.get('headless_only') is not True:e.append('automation')
 fx=v.get('fixture_matrix',{});fx=fx if isinstance(fx,dict) else {}
 if len(fx.get('positive',[]))!=11 or len(fx.get('negative',[]))!=17 or len(set(fx.get('positive',[])+fx.get('negative',[])))!=28:e.append('fixtures')
 s=v.get('source_bindings');t=v.get('test_bindings')
 if not isinstance(s,list) or [x.get('path') for x in s if isinstance(x,dict)]!=PROD or v.get('implementation_aggregate_sha256')!=dig(s):e.append('sources')
 if not isinstance(t,list) or [x.get('path') for x in t if isinstance(x,dict)]!=TEST or v.get('test_aggregate_sha256')!=dig(t):e.append('tests')
 if check_files:
  for rows,label in ((s,'source'),(t,'test')):
   if isinstance(rows,list):
    for x in rows:
     if not bind(x):e.append(label+'_drift')
 cpu=v.get('cpu_verification',{});cpu=cpu if isinstance(cpu,dict) else {};b=cpu.get('required_binaries')
 if not isinstance(b,list) or len(b)!=5 or any(x.get('exit_code')!=0 for x in b) or len(cpu.get('wide_passed_targets',[]))!=7 or cpu.get('all_exit_code')!=0 or cpu.get('gpu_cocoa_opengl_executed') is not False:e.append('cpu')
 elif check_files:
  for x in b:
   if not bind({'path':x.get('path'),'sha256':x.get('sha256')}):e.append('binary_drift')
 cb=v.get('completion_boundary',{});cb=cb if isinstance(cb,dict) else {}
 if cb.get('wpa_ii_21_internal_headless_consumer_implemented') is not True:e.append('implemented')
 for k in ('visible_product_ui_implemented','audit_row_promoted','b2_complete','m4_complete','physics_or_shotfacts_modified','global_rules_ui_exposed','gpu_executed','readiness_or_authorization_created'):
  if cb.get(k) is not False:e.append('false_claim:'+k)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('receipt',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');a=ap.parse_args()
 try:v=json.loads(Path(a.receipt).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not a.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_21_STALEMATE_REFEREE_CONSUMER_IMPLEMENTATION');return 0
if __name__=='__main__':raise SystemExit(main())
