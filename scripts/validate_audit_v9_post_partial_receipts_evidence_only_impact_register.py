#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];DEFAULT=ROOT/'docs/rules/audit-v9-post-partial-receipts-evidence-only-impact-register-v1.json';IDS=['WPA-II-12-3','WPA-II-12-5-6','WPA-II-12-7','WPA-II-15-1','WPA-II-15-2','WPA-II-17'];STAT=['PARTIAL','MISSING','MISSING','MISSING','MISSING','PARTIAL']
def sha(b):return hashlib.sha256(b).hexdigest()
def bind(r):
 p=Path(r.get('path',''))
 if p.is_absolute() or '..' in p.parts:return False
 q=ROOT/p;return q.is_file() and sha(q.read_bytes())==r.get('sha256')
def validate(v,files=True):
 if not isinstance(v,dict):return ['root']
 e=[]
 if v.get('schema')!='audit_v9_post_partial_receipts_evidence_only_impact_register_v1' or v.get('status')!='FINAL_READ_ONLY_NO_STATUS_PROMOTION':e.append('header')
 b=v.get('bindings',{});ex={'audit_v9':'eefd6121b7df2b41e45b66f3c87e2732bbccdb3808d866a89d44e8a324b9cf01','d5_d6_feasibility':'86d4957620d9e2c9033708ef3dbd5329610cc9cc9c44a134369079cffa375f77','b2_successor':'663a2a28faf93bd98de89e60498af1c955c55c36033849a0da12af965a478e69'}
 for k,d in ex.items():
  r=b.get(k,{})
  if r.get('sha256')!=d:e.append('binding:'+k)
  elif files and not bind(r):e.append('drift:'+k)
 rows=v.get('rows',[])
 if [r.get('order') for r in rows]!=list(range(6)) or [r.get('id') for r in rows]!=IDS:e.append('rows')
 if [r.get('audit_v9_status') for r in rows]!=STAT or [r.get('post_receipt_status') for r in rows]!=STAT:e.append('status')
 if any(not r.get('evidence_addable') or not r.get('promotion_forbidden_because') for r in rows):e.append('evidence')
 s=v.get('summary_effect',{})
 if s!={'rows_reviewed':6,'status_changes':0,'implemented_count_delta':0,'partial_count_delta':0,'missing_count_delta':0,'audit_v9_bytes_modified':False}:e.append('summary')
 c=v.get('claims',{})
 if c.get('evidence_register_complete') is not True:e.append('complete')
 for k in ('status_promotion_authorized','audit_successor_generated','production_or_cmake_modified','gpu_authorized','m4_complete'):
  if c.get(k) is not False:e.append('claim:'+k)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('path',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');x=ap.parse_args()
 try:v=json.loads(Path(x.path).read_text())
 except Exception as z:print('INVALID:',z);return 1
 e=validate(v,not x.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_AUDIT_V9_POST_PARTIAL_RECEIPTS_EVIDENCE_ONLY_REGISTER');return 0
if __name__=='__main__':raise SystemExit(main())
