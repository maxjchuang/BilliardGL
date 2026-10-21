#!/usr/bin/env python3
from __future__ import annotations
import hashlib,json,stat,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/art/phase4-v19-cp1-trajectory-inset-foundation-independent-ta-acceptance-addendum-v1.json'
BINDS={
'docs/evidence/phase4-v19-cp1-trajectory-inset-foundation-implementation-v1/receipt.json':'3208c819bd81ddc2143921a3b3ef2136127b275317a6ae0f5549de18188de758',
'docs/evidence/phase4-v19-cp1-trajectory-inset-foundation-implementation-v1/cpu-test.stdout':'ec13f4c1d0638e75ca653ab1b36336239947317acd46cf8d546e2b96121e846b',
'scripts/validate_phase4_v19_cp1_trajectory_inset_foundation.py':'e1c5e57481c3d986ad7463772e9f1c556308e8af153bb95af2cb44585635151b',
'tests/test_phase4_v19_cp1_trajectory_inset_foundation.py':'de93cc48d556b3f7f1d4378c7e4d4257861ac273af5378c96377aec9d5c8c2cf',
'docs/art/phase4-v19-endpoint-event-overlap-ta-acceptance-interpretation-addendum-v1.json':'5d78711e5bd7f6200727db3c60cf8e102fc462aefefc4aa329fff5e52349279c'}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def validate(path=DEFAULT):
 e=[]
 try:
  v=json.loads(path.read_text()); r=json.loads((ROOT/next(iter(BINDS))).read_text())
 except Exception as x:return [f'invalid:{x}']
 def q(c,m):
  if not c:e.append(m)
 q(v.get('schema')=='billiardgl.phase4.v19.cp1_trajectory_inset_foundation_independent_ta_acceptance_addendum.v1','schema')
 q(v.get('status')=='CP1_INDEPENDENT_TA_ACCEPTED_CPU_STATIC_ONLY_CP2_CP3_GPU_READINESS_FALSE','status')
 q({x.get('path'):x.get('sha256') for x in v.get('frozen_bindings',[])}==BINDS,'bindings')
 for p,h in BINDS.items():q((ROOT/p).is_file() and sha(ROOT/p)==h,'drift:'+p)
 q(v.get('canonical_layout_inventory')==r.get('canonical_layout_inventory') and len(v.get('canonical_layout_inventory',[]))==4,'inventory')
 rep=v.get('independent_reproduction',{})
 q(rep.get('receipt_validator_fresh_compile_normal_and_inventory_replay')=='PASS_ERRORS_EMPTY' and rep.get('receipt_fail_closed_mutation_count')==rep.get('receipt_fail_closed_mutation_pass_count')==72 and rep.get('independent_o0_replay')=='PASS_96' and rep.get('independent_o2_replay')=='PASS_96' and rep.get('gpu_or_window_context_created') is False,'reproduction')
 q(all(v.get('accepted_cp1_axes',{}).values()) and len(v.get('accepted_cp1_axes',{}))==8,'axes')
 b=v.get('acceptance_boundary',{});q(b.get('cp1_accepted') is True and len(b)==12 and all(x is False for k,x in b.items() if k!='cp1_accepted'),'boundary')
 frozen=['docs/evidence/phase4-v19-cp1-trajectory-inset-foundation-implementation-v1/receipt.json','docs/evidence/phase4-v19-cp1-trajectory-inset-foundation-implementation-v1/cpu-test.stdout','scripts/validate_phase4_v19_cp1_trajectory_inset_foundation.py','tests/test_phase4_v19_cp1_trajectory_inset_foundation.py']
 q(all(stat.S_IMODE((ROOT/p).stat().st_mode)==0o444 for p in frozen),'freeze')
 return e
if __name__=='__main__':
 p=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else DEFAULT;e=validate(p);print(json.dumps({'valid':not e,'errors':e},sort_keys=True));raise SystemExit(bool(e))
