#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
from typing import Any
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/wpa-ii-09-coordinate-placement-positive-impulse-lifecycle-implementation-receipt-v1.json'
KEYS={'schema','status','authority','implementation_contract','fixture_matrix','source_bindings','implementation_aggregate_sha256','test_bindings','cpu_verification','cmake_binding','claims'}
D={'authority':'a6dc394acd6704b434ddd7024abc1243bac01d11d2d29a7a400658bc633ebdc2','implementation_contract':'463654349d77dff3f17dc0207e20e596623d6f03bb62ed23b05bde52f42c2bbb','fixture_matrix':'ed4d0778eb26adc8e834bb663708ddb0f27572a1db04f657dd57ac798a931d1e','source_bindings':'53548224ab0cf19ef31adb154bae776e6d44e85da070416424dc0f33d37f6f0f','test_bindings':'14fb7112aa9adf4bdbf7366148bae01cbdc27a34acc0962f6efebc45ccf89db6','cpu_verification':'dd171e237fbc3eec2ea9ab77a14ddd681a07a790319a12d3fcfbe53d22e51794','cmake_binding':'afd6dc405a748b90032257d361c54cb750582c54ea39ff2a33ba41fc19a52579','claims':'4bc88d8cbfa8cd17f8ef88241dbdfd5813c5af3ec022587da0140080ecb4c085'}
PROD=['src/Billiards/rules/ball_in_hand_authority.h','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/render/modern/modern_product_session.h','src/Billiards/render/modern/modern_product_session.cpp']
TEST=['tests/game_runtime_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/rules_decision_session_integration_tests.cpp']
def sha(b):return hashlib.sha256(b).hexdigest()
def dig(v):return sha(json.dumps(v,sort_keys=True,separators=(',',':')).encode())
def resolve(s):
 if not isinstance(s,str):return None
 p=Path(s)
 if p.is_absolute() or '..' in p.parts:return None
 q=(ROOT/p).resolve();return q if q.is_relative_to(ROOT.resolve()) else None
def bind(b):
 if not isinstance(b,dict) or set(b)!={'path','sha256'}:return False
 p=resolve(b.get('path'));return p is not None and p.is_file() and sha(p.read_bytes())==b.get('sha256')
def validate(v:Any,check_files=True):
 if not isinstance(v,dict):return ['root']
 e=[]
 if set(v)!=KEYS:e.append('keys')
 if v.get('schema')!='wpa_ii_09_coordinate_placement_positive_impulse_lifecycle_implementation_receipt_v1':e.append('schema')
 if v.get('status')!='IMPLEMENTED_CPU_VERIFIED_SELECTED_COORDINATE_LIFECYCLE_NO_AUTO_M4_COMPLETION':e.append('status')
 for k,h in D.items():
  if dig(v.get(k))!=h:e.append('digest:'+k)
 a=v.get('authority',{});a=a if isinstance(a,dict) else {};g=a.get('implementation_design_gate',{});g=g if isinstance(g,dict) else {}
 if g.get('sha256')!='412e89c780f6e7f14719a05957f7f774c86c240fddd8b687a3c041ab50cfe93f' or g.get('root_authorized_exact_5_plus_3_allowlist') is not True or a.get('selected_option_id')!='COORDINATE_PLACEMENT_ONLY_POSITIVE_IMPULSE_CONSUMES':e.append('authority')
 c=v.get('implementation_contract',{});c=c if isinstance(c,dict) else {}
 if 'normalImpulseNs > 0.0' not in str(c.get('positive_predicate')) or c.get('physical_cue_part_reposition')!='DISABLED_UNSUPPORTED' or c.get('forward_stroke_classifier')!='DISABLED_UNSUPPORTED' or c.get('global_rules_ui_exposed') is not False or c.get('partial_commit_allowed') is not False:e.append('contract')
 f=v.get('fixture_matrix',{});f=f if isinstance(f,dict) else {}
 if len(f.get('positive',[]))!=7 or len(f.get('negative',[]))!=9 or f.get('wpa_ii_08_telemetry_regression')!='8 positive / 9 negative remain in broad RulesRuntime suite':e.append('fixtures')
 s=v.get('source_bindings');t=v.get('test_bindings')
 if not isinstance(s,list) or [x.get('path') for x in s if isinstance(x,dict)]!=PROD:e.append('sources')
 elif v.get('implementation_aggregate_sha256')!=dig(s):e.append('aggregate')
 if not isinstance(t,list) or [x.get('path') for x in t if isinstance(x,dict)]!=TEST:e.append('tests')
 if check_files:
  for rows,label in ((s,'source'),(t,'test')):
   if isinstance(rows,list):
    for x in rows:
     if not bind(x):e.append(label+'_drift')
  if not bind({'path':g.get('path'),'sha256':g.get('sha256')}):e.append('gate_drift')
 cpu=v.get('cpu_verification',{});cpu=cpu if isinstance(cpu,dict) else {};req=cpu.get('required')
 if not isinstance(req,list) or len(req)!=3 or any(x.get('exit_code')!=0 for x in req) or len(cpu.get('wide',[]))!=6 or any(x.get('exit_code')!=0 for x in cpu.get('wide',[])) or cpu.get('all_exit_code')!=0 or cpu.get('gpu_cocoa_opengl_executed') is not False:e.append('cpu')
 elif check_files:
  for x in req:
   if not bind({'path':x.get('path'),'sha256':x.get('sha256')}):e.append('binary_drift')
 cm=v.get('cmake_binding',{});cm=cm if isinstance(cm,dict) else {}
 if cm.get('modified') is not False:e.append('cmake')
 elif check_files and not bind({'path':cm.get('path'),'sha256':cm.get('sha256')}):e.append('cmake_drift')
 cl=v.get('claims',{});cl=cl if isinstance(cl,dict) else {}
 for k in ('selected_coordinate_lifecycle_implemented','positive_impulse_exactly_once_consumption_implemented','repeated_narrow_product_coordinate_flow_reachable'):
  if cl.get(k) is not True:e.append('true_claim:'+k)
 for k in ('wpa_ii_09_row_auto_closed','m4_complete','audit_modified','physical_cue_part_supported','forward_stroke_classifier_supported','global_rules_ui_exposed','new_production_file','cmake_modified','visual_ui_modified','gpu_executed','readiness_or_authorization_created'):
  if cl.get(k) is not False:e.append('false_claim:'+k)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('receipt',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');a=ap.parse_args()
 try:v=json.loads(Path(a.receipt).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not a.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_09_COORDINATE_PLACEMENT_POSITIVE_IMPULSE_LIFECYCLE_IMPLEMENTATION');return 0
if __name__=='__main__':raise SystemExit(main())
