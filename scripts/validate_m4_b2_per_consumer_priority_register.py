#!/usr/bin/env python3
"""Validate the design-only M4 B2 per-consumer priority register."""
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
from typing import Any
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/m4-b2-per-consumer-priority-register-v1.json'
KEYS={'schema','status','input_bindings','ordering_contract','dependency_definitions','rows','execution_batches_recommended_not_selected','claims'}
DIGESTS={'input_bindings':'4104fd2b028f1b5448a1012b4b76bfdccc319ad3947af6823f33b3318ecc3e7b','ordering_contract':'5f239bb923e8eb359a72fa5799d726215daf0e26cee8de625f6635a4c60d394c','dependency_definitions':'7b1ad75c2858f7e9591d87d3a0daf8b19c47d7fe033b67e95baecea06994a529','rows':'5a0a0ee65837f76cd7adc1578ad32724e19edf87cde0456e07513151a4075012','execution_batches_recommended_not_selected':'86df886e1bb0372b6d58f271be6b68884527be2be76892cdda1f36a09e896b14','claims':'ed85dacd1e9ae97f7d3e454d344aa374f06ca14449347e182e0e23d132ef1575'}
ORDER=['WPA-II-21','WPA-II-19','WPA-II-15-1','WPA-II-15-2','WPA-II-12-5-6','WPA-II-12-7','WPA-II-17','WPA-II-12-3']
SELECTED={'WPA-II-21':'SIGNED_REFEREE_DIRECTIVE_DEFAULT_THREE_EACH','WPA-II-19':'SIGNED_CONFUSION_DECLARATION_ASSIGNMENT_ERROR_ONLY','WPA-II-15-1':'SIGNED_PRE_SHOT_FROZEN_DECLARATION','WPA-II-15-2':'SIGNED_DECLARATION_STRAIGHT_CUSHION_CAPTURE_ONLY','WPA-II-12-5-6':'AUTO_ONLY_COMPLETE_MULTI_INTERVAL_REFEREE_CLOSE_CASES','WPA-II-12-7':'MEASUREMENTS_PLUS_SIGNED_REFEREE_PUSH_DECLARATION','WPA-II-17':'EVIDENCE_BACKED_SUBSET_PLUS_SIGNED_REFEREE_EVENTS','WPA-II-12-3':'SIGNED_TOURNAMENT_PROFILE_DEFAULT_17E_FOUL'}
D5={'WPA-II-15-2','WPA-II-12-5-6','WPA-II-12-7','WPA-II-17'}
D6={'WPA-II-17','WPA-II-12-3'}
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
 if v.get('schema')!='m4_b2_per_consumer_priority_register_v1':e.append('schema')
 if v.get('status')!='DESIGN_PRIORITY_REGISTER_AWAITING_ROOT_CONSUMER_SELECTION_NO_PRODUCTION_MUTATION':e.append('status')
 for k,h in DIGESTS.items():
  if digest(v.get(k))!=h:e.append('contract_digest:'+k)
 inputs=v.get('input_bindings',{});inputs=inputs if isinstance(inputs,dict) else {}
 expected={'root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','policy_referee_input_register':'66e105e968fc49f46cfe7fd117e42b545c6789a87d401f1d657ee263103a31ce','critical_path_b1_successor':'f2cc41f2c00ea5da3eb01cb12c2d1f76acea265eeed688eabc89b0302a98ea53','audit_v7':'d1654e8a82fcfb9c9621b90b36cb4d3eec14b5765025c8c49337b45891d33e15','b2_foundation_receipt':'1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b'}
 if set(inputs)!=set(expected):e.append('inputs')
 for k,h in expected.items():
  b=inputs.get(k,{}) if isinstance(inputs,dict) else {}
  if b.get('sha256')!=h:e.append('input_sha:'+k)
  elif check_files and not binding_ok(b):e.append('input_drift:'+k)
 order=v.get('ordering_contract',{});order=order if isinstance(order,dict) else {}
 if order.get('row_count')!=8 or order.get('stable_priority_order')!=ORDER or order.get('first_recommended_consumer')!='WPA-II-21' or order.get('second_recommended_consumer')!='WPA-II-19':e.append('order')
 rows=v.get('rows');
 if not isinstance(rows,list) or len(rows)!=8:e.append('rows_count');rows=[]
 if [r.get('row_id') for r in rows if isinstance(r,dict)]!=ORDER or [r.get('priority') for r in rows if isinstance(r,dict)]!=list(range(1,9)):e.append('rows_order')
 for r in rows:
  if not isinstance(r,dict):e.append('row_type');continue
  rid=r.get('row_id');d=r.get('dependencies',{});d=d if isinstance(d,dict) else {}
  if r.get('selected_option_id')!=SELECTED.get(rid):e.append('selection:'+str(rid))
  if d.get('requires_D2') is not True or d.get('requires_D5_for_consumer')!=(rid in D5) or d.get('requires_D6_for_consumer')!=(rid in D6):e.append('dependencies:'+str(rid))
  if r.get('consumer_implementation_authorized') is not False:e.append('authorization:'+str(rid))
  if r.get('recommended_next')!=(rid=='WPA-II-21'):e.append('recommended:'+str(rid))
 if rows:
  if rows[0].get('dependencies',{}).get('requires_D8_row_gate') is not True or rows[1].get('dependencies',{}).get('requires_D8_row_gate') is not True:e.append('rack_gate')
  if 'NO_NEW_PHYSICS' not in str(rows[0].get('protocol_only_advancement')) or 'NO_NEW_PHYSICS' not in str(rows[1].get('protocol_only_advancement')):e.append('first_two_protocol')
  if 'BLOCKED_BY_D5' not in str(rows[4].get('m4_value')) or 'BLOCKED_BY_D5' not in str(rows[5].get('m4_value')) or 'D5_D6' not in str(rows[6].get('m4_value')) or 'BLOCKED_BY_D6' not in str(rows[7].get('m4_value')):e.append('blocking_honesty')
 batches=v.get('execution_batches_recommended_not_selected');batches=batches if isinstance(batches,list) else []
 if len(batches)!=6 or [b.get('batch_id') for b in batches if isinstance(b,dict)]!=['B2-C1','B2-C2','B2-C3','B2-C4','B2-C5','B2-C6']:e.append('batches')
 if any(b.get('state') not in ('RECOMMENDED_NOT_ROOT_SELECTED','DEFER_OR_PARTIAL_GATE','WAIT_D5','SPLIT_SUBSET_GATE_AFTER_C1_C2_AND_D5_D6_DECISION','WAIT_D6') for b in batches if isinstance(b,dict)):e.append('batch_state')
 c=v.get('claims',{});c=c if isinstance(c,dict) else {}
 if c.get('design_only') is not True or c.get('row_count')!=8 or c.get('all_rows_root_selected_policy_bound') is not True:e.append('claims_true')
 for k in ('root_selected_next_consumer','consumer_implementation_authorized','specific_consumer_registered','production_or_cmake_modified','audit_row_promoted','b2_complete','m4_complete','global_player_rules_ui_exposed','gpu_readiness_or_authorization'):
  if c.get(k) is not False:e.append('false_claim:'+k)
 return e
def main()->int:
 p=argparse.ArgumentParser();p.add_argument('register',nargs='?',default=str(DEFAULT));p.add_argument('--no-file-checks',action='store_true');a=p.parse_args()
 try:v=json.loads(Path(a.register).read_text())
 except (OSError,json.JSONDecodeError) as ex:print('INVALID:',ex);return 1
 e=validate(v,not a.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_B2_PER_CONSUMER_PRIORITY_REGISTER');return 0
if __name__=='__main__':raise SystemExit(main())
