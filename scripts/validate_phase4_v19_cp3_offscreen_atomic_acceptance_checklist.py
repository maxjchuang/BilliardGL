#!/usr/bin/env python3
from __future__ import annotations
import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/art/phase4-v19-cp3-offscreen-atomic-implementation-acceptance-checklist-v1.json'
BINDS={
'docs/art/phase4-v19-dedicated-trajectory-inset-minimap-product-decision-and-implementation-design-gate-v1.json':'c7d86d9dcdd4891fac9dd9cbfffe3931561b5098db1df5a4aaec9b50e79af4c7',
'docs/art/phase4-v19-cp1-trajectory-inset-foundation-independent-ta-acceptance-addendum-v1.json':'e89e7febf1266efbea751ff0d50ac1d4a6bf358317974f701202d02a5396d02e',
'docs/art/phase4-v19-cp2-layout-draw-list-independent-ta-acceptance-addendum-v1.json':'6e7724df1a977af914b427538728f476d865db54c6cf95a3ae112e9de6172dd6',
'docs/evidence/phase4-v19-cp2-layout-draw-list-implementation-v1/receipt.json':'2524223aa320ff5aecb70a164bfeac471d9fc36f09a67b30a1bb7486333bbbc5'}
ALLOW=['src/Billiards/render/modern/modern_renderer.h','src/Billiards/render/modern/modern_renderer.cpp','src/Billiards/render/modern/trajectory_inset_atomic_transaction.h','src/Billiards/render/modern/trajectory_inset_atomic_transaction.cpp','tests/phase4_v19_trajectory_inset_offscreen_atomic_cpu_tests.cpp']
FAILURES=['format_sample_query','color_allocation','semantic_r32ui_allocation','geometry_upload','framebuffer_attach_complete','color_clear','semantic_clear','table_draw','trajectory_draw','color_resolve','per_sample_semantic_resolve','fixed_palette_tone_encode','generation_frame_serial_check','atomic_composite','state_restore']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def validate(path=DEFAULT):
 e=[]
 try:v=json.loads(path.read_text())
 except Exception as x:return [f'invalid:{x}']
 def q(c,m):
  if not c:e.append(m)
 q(v.get('schema')=='billiardgl.phase4.v19.cp3_offscreen_atomic_implementation_acceptance_checklist.v1','schema')
 q(v.get('status')=='CP3_ACCEPTANCE_CHECKLIST_FROZEN_IMPLEMENTATION_NOT_STARTED_NO_PRODUCTION_CMAKE_GPU_READINESS_AUTH','status')
 q({x.get('path'):x.get('sha256') for x in v.get('bindings',[])}==BINDS,'bindings')
 for p,h in BINDS.items():q((ROOT/p).is_file() and sha(ROOT/p)==h,'drift:'+p)
 q(v.get('future_candidate_allowlist_not_authorized')==ALLOW,'allowlist')
 c=v.get('cmake_strategy',{});q(c.get('current_cmake_must_remain_sha256')=='f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41' and c.get('current_checklist_authorizes_cmake_edit') is False and c.get('adding_target_to_cmake_requires_separate_gate') is True,'cmake')
 i=v.get('injectable_failure_interface_contract',{});q(i.get('required') is True and i.get('failure_points_in_order')==FAILURES and i.get('every_failure_point_must_have_valid_fail_valid_fixture') is True and i.get('may_control_or_observe_main_scene_rendering') is False and i.get('may_accept_camera_scene_color_repair_coordinates_or_prior_pass') is False,'injection')
 r=v.get('independent_resource_contract',{});q(r.get('architecture')=='OFFSCREEN_ATOMIC_INSET_COLOR_AND_SEMANTIC_PLANE' and r.get('sample_counts_exact')==[1,2,4] and len(r.get('required_resources',[]))==6 and len(r.get('forbidden_reuse',[]))==5 and r.get('no_framebuffer_feedback') is True and r.get('no_silent_sample_downgrade') is True,'resources')
 t=v.get('transaction_state_machine_contract',{});q(t.get('ordered_success_states')==['UNALLOCATED','ALLOCATING','READY_CLEARED','DRAWN','COLOR_AND_SEMANTIC_RESOLVED','FIXED_PALETTE_TONED','COMPOSITE_ELIGIBLE','COMPOSITED'] and t.get('on_any_error_state')=='INVALIDATED_NO_COMPOSITE' and t.get('partial_or_stale_present_allowed') is False and 'zero main HDR inset pixels/tokens' in t.get('valid_fail_valid',''),'transaction')
 p=v.get('pass_and_tone_contract',{});q(len(p.get('inset_order',[]))==8 and p.get('composite_rect')=='exact CP2 plotCutoutRect only' and p.get('main_hdr_inset_writes')==0 and p.get('main_world_trajectory_token_writes')==0,'pass')
 g=v.get('gl_error_and_state_restore_contract',{});q(g.get('immediate_error_check_after_every_transition') is True and g.get('restore_required_on_success_and_every_failure') is True and g.get('restore_error_is_transaction_failure') is True and len(g.get('required_state_snapshot_restore',[]))==11,'gl_state')
 n=v.get('instrumentation_parity_contract',{});q(n.get('cp1_source_mapping_inventory_and_five_digests_byte_exact') is True and n.get('cp2_layout_cutout_chrome_and_exclusion_contract_byte_exact') is True and n.get('ui_semantic_probe_denominator_contribution')==0 and 'at most one' in n.get('main_final_ownership',''),'instrumentation')
 f=v.get('future_cpu_static_acceptance_not_run',{});q(f.get('logical_profiles')==['player_standard','elevated_standard','overhead_standard','broadcast_standard','broadcast_cvd'] and f.get('sample_counts')==[1,2,4] and len(f.get('required_positive_fixtures',[]))==6 and len(f.get('required_negative_fixtures',[]))==12 and f.get('production_implementation_started') is False and f.get('cpu_static_pass') is False and f.get('cp3_accepted') is False,'future')
 a=v.get('authorization_boundary',{});q(a.get('cp1_accepted') is True and a.get('cp2_accepted') is True and len(a)==12 and all(x is False for k,x in a.items() if k not in ('cp1_accepted','cp2_accepted')),'authority')
 return e
if __name__=='__main__':
 p=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else DEFAULT;e=validate(p);print(json.dumps({'valid':not e,'errors':e},sort_keys=True));raise SystemExit(bool(e))
