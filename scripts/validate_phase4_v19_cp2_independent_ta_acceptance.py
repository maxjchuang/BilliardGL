#!/usr/bin/env python3
from __future__ import annotations
import hashlib,json,stat,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/art/phase4-v19-cp2-layout-draw-list-independent-ta-acceptance-addendum-v1.json'
BINDS={
'docs/evidence/phase4-v19-cp2-layout-draw-list-implementation-v1/receipt.json':'2524223aa320ff5aecb70a164bfeac471d9fc36f09a67b30a1bb7486333bbbc5',
'docs/evidence/phase4-v19-cp2-layout-draw-list-implementation-v1/cpu-test.stdout':'3907f5f56f6a20787f40b16f9e2e9218bc93e1a08b7c85d953da91927063d7e6',
'scripts/validate_phase4_v19_cp2_layout_draw_list_implementation.py':'55208e8897c12c9091e92061029ea67dc7b878804d9aa88f9fd1e59a3fbfa2f3',
'tests/test_phase4_v19_cp2_layout_draw_list_implementation.py':'b9b133c85e09b601494bc05909ed430180a559615d48b4642c801d3d5d68c245',
'docs/art/phase4-v19-cp1-trajectory-inset-foundation-independent-ta-acceptance-addendum-v1.json':'e89e7febf1266efbea751ff0d50ac1d4a6bf358317974f701202d02a5396d02e'}
SOURCES={
'src/Billiards/render/ui/product_shell_layout.h':'8d70d3355bf40ac78bc212988a4205fa08c2c2caac95f816dfbdf5d6140dc5ec',
'src/Billiards/render/ui/product_shell_layout.cpp':'50d99d388dfa72c81cbec516e6661349b5238fc5aea5b832db9af4e87eb02189',
'src/Billiards/render/ui/product_shell_draw_list.h':'57fbd8ce714274ef4957debe462aa36d91c48e4297486fb9609eec61a061eac2',
'src/Billiards/render/ui/product_shell_draw_list.cpp':'f6b705a100d18cc0218b69e36a74a403fb65987fd9060a047722ca78d391b45c',
'tests/phase4_v19_trajectory_inset_layout_draw_list_cpu_tests.cpp':'810e129aedfef913d4947ebdb0de65944ef0df82029d9bd27e6d26016bac07af'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def validate(path=DEFAULT):
 e=[]
 try:v=json.loads(path.read_text())
 except Exception as x:return [f'invalid:{x}']
 def q(c,m):
  if not c:e.append(m)
 q(v.get('schema')=='billiardgl.phase4.v19.cp2_layout_draw_list_independent_ta_acceptance_addendum.v1','schema')
 q(v.get('status')=='CP2_INDEPENDENT_TA_ACCEPTED_CPU_STATIC_ONLY_CP3_GPU_READINESS_FALSE','status')
 q({x.get('path'):x.get('sha256') for x in v.get('frozen_bindings',[])}==BINDS,'bindings')
 q(v.get('accepted_source_bindings')==SOURCES,'sources')
 for p,h in (BINDS|SOURCES).items():q((ROOT/p).is_file() and sha(ROOT/p)==h,'drift:'+p)
 r=v.get('independent_reproduction',{});q(r.get('receipt_validator_fresh_o2_and_product_shell_replay')=='PASS_ERRORS_EMPTY' and r.get('receipt_fail_closed_mutations_minimum')==27 and r.get('receipt_mutation_unittest')=='PASS' and r.get('independent_o0_replay')=='PASS' and r.get('independent_o2_replay')=='PASS' and r.get('existing_product_shell_replay')=='PASS' and r.get('gpu_or_window_context_created') is False,'reproduction')
 q(len(v.get('accepted_cp2_axes',{}))==9 and all(v.get('accepted_cp2_axes',{}).values()),'axes')
 b=v.get('acceptance_boundary',{});q(b.get('cp1_accepted') is True and b.get('cp2_accepted') is True and len(b)==11 and all(x is False for k,x in b.items() if k not in ('cp1_accepted','cp2_accepted')),'boundary')
 frozen=list(BINDS)[:4];q(all(stat.S_IMODE((ROOT/p).stat().st_mode)==0o444 for p in frozen),'freeze')
 return e
if __name__=='__main__':
 p=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else DEFAULT;e=validate(p);print(json.dumps({'valid':not e,'errors':e},sort_keys=True));raise SystemExit(bool(e))
