#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];DEFAULT=ROOT/'docs/rules/m4-d5-d6-exact-dependency-implementation-feasibility-register-v1.json'
IDS=['D5-A','D5-B','D5-C','D5-D','D6-A','D6-B','D6-C','D56-E']
def sha(b):return hashlib.sha256(b).hexdigest()
def bind(r):
 p=Path(r.get('path',''))
 if p.is_absolute() or '..' in p.parts:return False
 q=ROOT/p;return q.is_file() and sha(q.read_bytes())==r.get('sha256')
def validate(v,files=True):
 if not isinstance(v,dict):return ['root']
 e=[]
 if v.get('schema')!='m4_d5_d6_exact_dependency_implementation_feasibility_register_v1' or v.get('status')!='FINAL_DESIGN_ONLY_BATCH_FEASIBILITY_NO_IMPLEMENTATION_AUTHORIZATION':e.append('header')
 b=v.get('bindings',{});ex={'root_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','b2_successor':'663a2a28faf93bd98de89e60498af1c955c55c36033849a0da12af965a478e69','cue_contact_gap_gate':'1442e4204c4acb0718ca0b87c858c8222e03ac634f5b5b7def3d9a6bdae64033','off_table_gap_gate':'3fc468502ab09e7ba0122d84b6c20386b0646d1f0db76a3f8ab3e8a1d30a9dbe','c8_defer_gate':'f217958cf035024150b622421b68d80bdcf5e8dd4f7281bfeecf447fee87043a','audit_v9':'eefd6121b7df2b41e45b66f3c87e2732bbccdb3808d866a89d44e8a324b9cf01'}
 for k,d in ex.items():
  r=b.get(k,{})
  if r.get('sha256')!=d:e.append('binding:'+k)
  elif files and not bind(r):e.append('drift:'+k)
 rows=v.get('batches',[])
 if [r.get('order') for r in rows]!=list(range(8)) or [r.get('id') for r in rows]!=IDS:e.append('rows')
 if any(r.get('cpu_first') is not True or r.get('new_root_decision_required') is not True for r in rows):e.append('gates')
 if [r.get('gpu_required') for r in rows] != [False,False,False,False,True,False,False,True]:e.append('gpu')
 x=v.get('execution_constraints',{})
 if x.get('stable_order')!=IDS or x.get('every_batch_requires_independent_implementation_design_gate') is not True or x.get('selection_or_feasibility_is_not_implementation') is not True or x.get('gpu_before_cpu_stable_forbidden') is not True or x.get('physics_or_telemetry_may_not_encode_policy') is not True:e.append('constraints')
 c=v.get('claims',{})
 if c.get('register_complete') is not True:e.append('complete')
 for k in ('production_or_cmake_modified','d5_implemented','d6_implemented','rows_promoted','gpu_authorized','m4_complete'):
  if c.get(k) is not False:e.append('claim:'+k)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('path',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');x=ap.parse_args()
 try:v=json.loads(Path(x.path).read_text())
 except Exception as z:print('INVALID:',z);return 1
 e=validate(v,not x.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_D5_D6_EXACT_DEPENDENCY_FEASIBILITY_REGISTER');return 0
if __name__=='__main__':raise SystemExit(main())
