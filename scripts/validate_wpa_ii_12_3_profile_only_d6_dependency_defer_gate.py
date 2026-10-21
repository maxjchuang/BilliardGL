#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];DEFAULT=ROOT/'docs/rules/wpa-ii-12-3-profile-only-d6-dependency-defer-gate-v1.json'
def sha(b):return hashlib.sha256(b).hexdigest()
def bind(r):
 if not isinstance(r,dict):return False
 p=Path(r.get('path',''))
 if p.is_absolute() or '..' in p.parts:return False
 q=ROOT/p;return q.is_file() and sha(q.read_bytes())==r.get('sha256')
def validate(v,check_files=True):
 if not isinstance(v,dict):return ['root']
 e=[]
 if v.get('schema')!='wpa_ii_12_3_profile_only_d6_dependency_defer_gate_v1':e.append('schema')
 if v.get('status')!='FINAL_BLOCKED_DEFER_DESIGN_ONLY_NO_IMPLEMENTABLE_CONSUMER_PROGRESS':e.append('status')
 a=v.get('authority_bindings',{});ex={'root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','b2_foundation_receipt':'1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b','b2_priority_register':'8a778fc249a2ba952ff6acca60c5d24cad6ca2135f606bb54e3e0b887d092f55','off_table_gap_gate':'3fc468502ab09e7ba0122d84b6c20386b0646d1f0db76a3f8ab3e8a1d30a9dbe','audit_v9':'eefd6121b7df2b41e45b66f3c87e2732bbccdb3808d866a89d44e8a324b9cf01'}
 for k,d in ex.items():
  r=a.get(k,{})
  if r.get('sha256')!=d:e.append('authority:'+k)
  elif check_files and not bind(r):e.append('drift:'+k)
 p=a.get('root_selected_policy',{})
 if p.get('row_id')!='WPA-II-12-3' or p.get('selected_option_id')!='SIGNED_TOURNAMENT_PROFILE_DEFAULT_17E_FOUL' or p.get('row_implementation_allowed') is not False:e.append('policy')
 s=v.get('assessment',{})
 for k in ('profile_declaration_alone_is_fact','profile_declaration_alone_is_foul','physical_off_table_observable_now','consumer_without_d6_has_product_or_m4_value','partial_profile_consumer_recommended'):
  if s.get(k) is not False:e.append('false:'+k)
 if s.get('d6_required') is not True:e.append('d6')
 d=v.get('deferred_contract',{})
 if d.get('coordinate_or_injected_fact_shortcut_forbidden') is not True or d.get('eight_ball_sibling_unchanged') is not True or d.get('cue_ball_off_table_sibling_unchanged') is not True:e.append('defer')
 f=v.get('required_future_fixtures',{})
 if len(f.get('positive',[]))!=4 or len(f.get('negative',[]))!=8:e.append('fixtures')
 al=v.get('implementation_allowlist',{})
 if al.get('production_files')!=[] or al.get('test_files')!=[] or al.get('build_files')!=[] or al.get('dormant_future_allowlist_owned_by_d6_gate') is not True:e.append('allowlist')
 c=v.get('completion_boundary',{})
 if c.get('blocked_defer_gate_complete') is not True:e.append('complete')
 for k in ('profile_consumer_implemented','d6_complete','physical_off_table_evidence_complete','object_off_table_classification_implemented','wpa_ii_12_3_promoted','wpa_ii_17_promoted','m4_complete','production_or_cmake_modified','ui_gpu_readiness_or_authorization'):
  if c.get(k) is not False:e.append('claim:'+k)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('path',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');x=ap.parse_args()
 try:v=json.loads(Path(x.path).read_text())
 except Exception as z:print('INVALID:',z);return 1
 e=validate(v,not x.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_12_3_PROFILE_ONLY_D6_DEFER_GATE');return 0
if __name__=='__main__':raise SystemExit(main())
