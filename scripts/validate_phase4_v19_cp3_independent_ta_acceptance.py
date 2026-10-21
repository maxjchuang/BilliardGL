#!/usr/bin/env python3
from __future__ import annotations
import hashlib,json,stat,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/art/phase4-v19-cp3-offscreen-atomic-independent-ta-acceptance-addendum-v1.json'
BINDS={
'docs/art/phase4-v19-cp1-trajectory-inset-foundation-independent-ta-acceptance-addendum-v1.json':'e89e7febf1266efbea751ff0d50ac1d4a6bf358317974f701202d02a5396d02e',
'docs/art/phase4-v19-cp2-layout-draw-list-independent-ta-acceptance-addendum-v1.json':'6e7724df1a977af914b427538728f476d865db54c6cf95a3ae112e9de6172dd6',
'docs/art/phase4-v19-cp3-offscreen-atomic-implementation-acceptance-checklist-v1.json':'c232427c702bc25b07f2f836cb7b6f4dc22d872b69dd4de24e77e590deff3a9d',
'docs/evidence/phase4-v19-cp3-offscreen-atomic-implementation-v1/receipt.json':'9848e3f869db91f547e6c2cce1e4f6067538702a6dd04cbb18a585c57007133a',
'docs/evidence/phase4-v19-cp3-offscreen-atomic-implementation-v1/cpu-test.stdout':'fc17521e31e412be2a1e0805ef8b1a12e6e3d48077dae7e3f3f756dd9b83c64b',
'docs/evidence/phase4-v19-cp3-offscreen-atomic-implementation-v1/adapter-red.stdout':'95afd0ce65a38ba3483926a38e20faad8b9cfb1692f869ec78fd8709f0263fa1',
'scripts/validate_phase4_v19_cp3_offscreen_atomic_implementation.py':'a369f96d0c5c3546f007b75ec3094fc25d00c9f152d1fab31ea9cb22175a8583',
'tests/test_phase4_v19_cp3_offscreen_atomic_implementation.py':'70608a4c052be9e02c4f736b2cabe762615ce797714ac4d3d08d0cc9e31a3d05'}
SOURCES={
'src/Billiards/render/modern/modern_renderer.h':'ad611b763397df677606d236e4f33d401676cc2f96042cf0a7e355c64989fe67',
'src/Billiards/render/modern/modern_renderer.cpp':'bfe8565324a020b7f3171d332f7d10bdd9c61f0b0b3b4b62a8b5c10d6f0dd561',
'src/Billiards/render/modern/trajectory_inset_atomic_transaction.h':'69be6cb1e61385e0b70743bc80bb8ab01e9a483b1c79358ab1fc03d578a5ed64',
'src/Billiards/render/modern/trajectory_inset_atomic_transaction.cpp':'183f630052f7676b37f0b66da6f6b2725faf6da930f6dc3cea79c6b160a4c6b2',
'tests/phase4_v19_trajectory_inset_offscreen_atomic_cpu_tests.cpp':'0c75c1da8d67764cb0700cbd6daad099d38445955cfc8945c5c4dc95511255f8'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def validate(path=DEFAULT):
 e=[]
 try:v=json.loads(path.read_text())
 except Exception as x:return [f'invalid:{x}']
 def q(c,m):
  if not c:e.append(m)
 q(v.get('schema')=='billiardgl.phase4.v19.cp3_offscreen_atomic_independent_ta_acceptance_addendum.v1','schema')
 q(v.get('status')=='CP3_INDEPENDENT_TA_ACCEPTED_CPU_STATIC_ONLY_CP4_GPU_READINESS_FALSE','status')
 q({x.get('path'):x.get('sha256') for x in v.get('frozen_bindings',[])}==BINDS,'bindings')
 q(v.get('accepted_source_bindings')==SOURCES,'sources')
 for p,h in (BINDS|SOURCES).items():q((ROOT/p).is_file() and sha(ROOT/p)==h,'drift:'+p)
 r=v.get('independent_reproduction',{})
 q(r.get('post_freeze_receipt_validator')=='PASS_ERRORS_EMPTY' and r.get('post_freeze_mutation_unittest')=='PASS_MINIMUM_40' and r.get('binding_diffcheck')=='PASS_ERRORS_EMPTY' and r.get('independent_standalone_o0')=='PASS' and r.get('independent_standalone_o2')=='PASS' and r.get('independent_modern_compile_only_o0')=='PASS' and r.get('independent_modern_compile_only_o2')=='PASS' and r.get('transaction_red_status')=='RED_UNVERIFIABLE' and r.get('adapter_red_status')=='VERIFIED_MISSING_EXPLICIT_INPUT_API' and r.get('gpu_or_window_context_created') is False and r.get('billiards_executed') is False,'reproduction')
 q(len(v.get('accepted_cp3_axes',{}))==11 and all(v.get('accepted_cp3_axes',{}).values()),'axes')
 b=v.get('acceptance_boundary',{});q(len(b)==13 and all(b.get(k) is True for k in ('cp1_accepted','cp2_accepted','cp3_accepted')) and all(x is False for k,x in b.items() if k not in ('cp1_accepted','cp2_accepted','cp3_accepted')),'boundary')
 frozen=list(BINDS)[3:];q(all(stat.S_IMODE((ROOT/p).stat().st_mode)==0o444 for p in frozen),'freeze')
 return e
if __name__=='__main__':
 p=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else DEFAULT;e=validate(p);print(json.dumps({'valid':not e,'errors':e},sort_keys=True));raise SystemExit(bool(e))
