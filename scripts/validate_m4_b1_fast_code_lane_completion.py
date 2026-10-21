#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
from typing import Any
ROOT=Path(__file__).resolve().parents[1];RECEIPT=ROOT/'docs/rules/m4-b1-fast-code-lane-completion-receipt-v1.json';SUCCESSOR=ROOT/'docs/rules/m4-remaining-gap-dependency-critical-path-register-v2-b1-successor.json'
ROWS=['WPA-II-06-C','WPA-II-06-D','WPA-II-06-E','WPA-II-06-F','WPA-II-08','WPA-II-09'];STAT=['PARTIAL','PARTIAL','PARTIAL','PARTIAL','IMPLEMENTED','IMPLEMENTED'];RECEIPT_COUNTS=[2,1,1,2,2,1]
RD={'bindings':'4621cc6723dfbb6135c3232c0e56d7cc962c326fc17898829e8a4d3fef14b559','rows':'43086d01ba2c9d6d7592eba00d296e20d71a323d4f64926565c4009c7b9bae1f','batch_assessment':'b008504335348a7b1283b2554451d9868b2d2069ae96777aea50534b64595f61','claims':'088b6ede5bd0a80e52e31d126687092de85f9ac3a4b3fc30d03c4432b098cb31'}
SD={'predecessor':'0962befa3bf7dcfcb39d007c71a28edd256c8cd83a16318231d63e615733ab10','new_bindings':'7b32147af7f9e54ec59252e9062801d9c5f7d3caaba83c2ba902307835f49f97','dependency_state_successor':'98f9e10af7d6e685974243c4babea646f05443dcfdfdab1791a55ce38804a3d2','batch_state_successor':'1fe938e481d6b50afa1d27dc94491b568d4bdcff7ff6309dd3afa08bf891cdb1','remaining_critical_path':'30a32f7857b233b8e47268488d20031e427ca1fce99e86ef924ccf97586831d4','claims':'1062021cbc4674f37d0054cebdb69ed112b9258d982878d51b38e26f04332d4c'}
def sha(b):return hashlib.sha256(b).hexdigest()
def dig(v):return sha(json.dumps(v,sort_keys=True,separators=(',',':')).encode())
def path(s):
 if not isinstance(s,str) or Path(s).is_absolute() or '..' in Path(s).parts:return None
 p=(ROOT/s).resolve();return p if p.is_relative_to(ROOT.resolve()) else None
def binding(b):
 if not isinstance(b,dict):return False
 p=path(b.get('path'));return p is not None and p.is_file() and sha(p.read_bytes())==b.get('sha256')
def validate(receipt:Any,successor:Any,check_files=True):
 e=[]
 if not isinstance(receipt,dict):return ['receipt_root']
 if not isinstance(successor,dict):return ['successor_root']
 if receipt.get('schema')!='m4_b1_fast_code_lane_completion_receipt_v1' or receipt.get('status')!='B1_BATCH_COMPLETE_EVIDENCE_FROZEN_NOT_M4_COMPLETE':e.append('receipt_identity')
 if successor.get('schema')!='m4_remaining_gap_dependency_critical_path_register_v2_b1_successor' or successor.get('status')!='FINAL_B1_EVIDENCE_SUCCESSOR_B2_NOT_AUTHORIZED':e.append('successor_identity')
 for k,h in RD.items():
  if dig(receipt.get(k))!=h:e.append('receipt_digest:'+k)
 for k,h in SD.items():
  if dig(successor.get(k))!=h:e.append('successor_digest:'+k)
 rows=receipt.get('rows')
 if not isinstance(rows,list) or [x.get('row_id') for x in rows if isinstance(x,dict)]!=ROWS:e.append('row_order')
 elif [x.get('audit_status') for x in rows]!=STAT or [len(x.get('receipts',[])) for x in rows]!=RECEIPT_COUNTS or any(x.get('batch_code_complete') is not True for x in rows):e.append('row_state')
 if isinstance(rows,list):
  for x in rows:
   if not x.get('remaining'):e.append('remaining')
   if check_files:
    for b in x.get('receipts',[]):
     if not binding(b):e.append('receipt_binding_drift')
 b=receipt.get('batch_assessment',{});b=b if isinstance(b,dict) else {}
 if b.get('row_order')!=ROWS or b.get('implementation_receipt_count')!=9 or b.get('all_six_authorized_code_lanes_completed') is not True or b.get('batch_completed_is_not_m4_completion') is not True:e.append('batch')
 c=receipt.get('claims',{});c=c if isinstance(c,dict) else {}
 for k in ('b1_batch_completed','all_six_have_frozen_implementation_evidence'):
  if c.get(k) is not True:e.append('receipt_true:'+k)
 for k in ('all_six_audit_rows_implemented','m4_complete','b2_authorized_by_receipt','production_modified','critical_path_original_modified','audit_v7_modified','cmake_modified','gpu_or_readiness_or_authorization'):
  if c.get(k) is not False:e.append('receipt_false:'+k)
 deps=successor.get('dependency_state_successor')
 if not isinstance(deps,list) or [x.get('id') for x in deps]!=[f'D{i}-'+s for i,s in enumerate(['FROZEN-INPUTS','ROOT-POLICY-BUNDLE','REFEREE-AUTHORITY-PROTOCOL','BREAK-CLASSIFICATION','PLACEMENT-LIFECYCLE','CUE-CONTACT-TELEMETRY','OFF-TABLE-PHYSICS','JUMP-3D-PHYSICS','RACK-REFEREE-AUTHORITY','CPU-INTEGRATION','FRESH-SHARED-BUILD','GPU-TA','HUMAN-BETA'])]:e.append('deps')
 else:
  states={x['id']:x['successor_state'] for x in deps}
  if states['D4-PLACEMENT-LIFECYCLE']!='SATISFIED_BY_WPA_II_08_09_AUDIT_V7' or not states['D3-BREAK-CLASSIFICATION'].startswith('PARTIALLY_SATISFIED') or states['D2-REFEREE-AUTHORITY-PROTOCOL']!='WAITING_FOR_B2_ROOT_AUTHORIZATION':e.append('dep_states')
 batches=successor.get('batch_state_successor');
 if not isinstance(batches,list) or len(batches)!=7 or batches[1].get('successor_state')!='BATCH_COMPLETE_SIX_OF_SIX_CODE_EVIDENCE' or batches[2].get('may_start_from_this_successor') is not False:e.append('batches')
 sc=successor.get('claims',{});sc=sc if isinstance(sc,dict) else {}
 for k in ('predecessor_unchanged','b0_complete','b1_batch_complete','d4_satisfied'):
  if sc.get(k) is not True:e.append('successor_true:'+k)
 for k in ('d3_fully_satisfied','b2_complete_or_authorized','b3_complete_or_authorized','m4_complete','production_modified','cmake_modified','gpu_readiness_or_authorization'):
  if sc.get(k) is not False:e.append('successor_false:'+k)
 if check_files:
  for container in (receipt.get('bindings',{}),successor.get('new_bindings',{})):
   for b in container.values():
    if not binding(b):e.append('binding_drift')
  if not binding(successor.get('predecessor')):e.append('predecessor_drift')
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--receipt',type=Path,default=RECEIPT);ap.add_argument('--successor',type=Path,default=SUCCESSOR);ap.add_argument('--no-file-checks',action='store_true');a=ap.parse_args()
 try:r=json.loads(a.receipt.read_text());s=json.loads(a.successor.read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(r,s,not a.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_B1_FAST_CODE_LANE_COMPLETION_AND_CRITICAL_PATH_SUCCESSOR');return 0
if __name__=='__main__':raise SystemExit(main())
