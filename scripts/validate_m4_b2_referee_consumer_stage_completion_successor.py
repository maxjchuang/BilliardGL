#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];DEFAULT=ROOT/'docs/rules/m4-b2-referee-consumer-stage-completion-successor-v1.json'
IDS=['B2-FOUNDATION','WPA-II-21','WPA-II-19','WPA-II-15-1','WPA-II-15-2','WPA-II-12-5-6','WPA-II-12-7','WPA-II-17','WPA-II-12-3']
RECEIPTS=['1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b','0829217fd580d0302ee96d6b4068535f03632420d53f0ae6ca1bb4f2b86644e2','162478839f14fb121738b98f6688f690d1e4683b9132ccd67ee3a3f12e07ab18','97c9e2f279bd398ff62955c31e4ace3132a15e303972d917fe1105d6048518a4','b21de04b86f4c988c6f32b6e87667e11dbb55dfed60eeaf55c08e4745b64e6af','eb0ac7392ca9bfb9f84c768c22cc90bf120da32d8edb8a7d32674b2383fa51f1','136048a8bd5e6f26b5160d111f53d6cc5bc297fc7a2da5e6b74dd4d53e6d74eb','a87ff883cb543317dcaa6b35a1ae6c82e180bf30d067a1a5897f0736c89bb19d']
def sha(b):return hashlib.sha256(b).hexdigest()
def bind(path,digest):
 p=Path(path)
 if p.is_absolute() or '..' in p.parts:return False
 q=ROOT/p;return q.is_file() and sha(q.read_bytes())==digest
def validate(v,check_files=True):
 if not isinstance(v,dict):return ['root']
 e=[]
 if v.get('schema')!='m4_b2_referee_consumer_stage_completion_successor_v1':e.append('schema')
 if v.get('status')!='FINAL_B2_STAGE_EXECUTION_COMPLETE_REMAINING_D5_D6_NOT_M4_COMPLETE':e.append('status')
 p=v.get('predecessor_bindings',{});ex={'root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','priority_register':'8a778fc249a2ba952ff6acca60c5d24cad6ca2135f606bb54e3e0b887d092f55','audit_v9':'eefd6121b7df2b41e45b66f3c87e2732bbccdb3808d866a89d44e8a324b9cf01','c8_defer_gate':'f217958cf035024150b622421b68d80bdcf5e8dd4f7281bfeecf447fee87043a'}
 for k,d in ex.items():
  r=p.get(k,{})
  if r.get('sha256')!=d:e.append('pred:'+k)
  elif check_files and not bind(r.get('path',''),d):e.append('pred_drift:'+k)
 rows=v.get('stage_rows',[])
 if [r.get('order') for r in rows]!=list(range(9)) or [r.get('id') for r in rows]!=IDS:e.append('rows')
 for i,r in enumerate(rows[:-1]):
  if r.get('receipt_sha256')!=RECEIPTS[i]:e.append('receipt')
  elif check_files and not bind(r.get('receipt_path',''),r['receipt_sha256']):e.append('receipt_drift')
 if rows and (rows[-1].get('receipt_path') is not None or rows[-1].get('stage_state')!='BLOCKED_DEFER_D6_NO_PARTIAL_IMPLEMENTATION'):e.append('defer_row')
 d=v.get('dependency_successor',{})
 if d.get('D2_REFEREE_PROTOCOL')!='SATISFIED_FOUNDATION_AND_SEVEN_CONSUMER_SCOPES' or not str(d.get('D5_CUE_CONTACT','')).startswith('NOT_SATISFIED') or not str(d.get('D6_OFF_TABLE_PHYSICS','')).startswith('NOT_SATISFIED') or d.get('M4')!='NOT_COMPLETE':e.append('deps')
 c=v.get('claims',{})
 if c.get('foundation_implemented') is not True or c.get('b2_stage_completed') is not True:e.append('true')
 if c.get('implemented_full_consumers')!=['WPA-II-21','WPA-II-19'] or c.get('implemented_partial_consumers')!=['WPA-II-15-1','WPA-II-15-2','WPA-II-12-5-6','WPA-II-12-7','WPA-II-17'] or c.get('deferred_without_implementation')!=['WPA-II-12-3']:e.append('sets')
 for k in ('all_rows_complete','d5_complete','d6_complete','audit_v9_modified','m4_complete','production_or_cmake_modified_by_successor','ui_gpu_readiness_or_authorization'):
  if c.get(k) is not False:e.append('claim:'+k)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('path',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');x=ap.parse_args()
 try:v=json.loads(Path(x.path).read_text())
 except Exception as z:print('INVALID:',z);return 1
 e=validate(v,not x.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_B2_REFEREE_CONSUMER_STAGE_COMPLETION_SUCCESSOR');return 0
if __name__=='__main__':raise SystemExit(main())
