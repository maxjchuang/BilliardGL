#!/usr/bin/env python3
from __future__ import annotations
import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/art/phase4-v19-cp4-visible-integration-implementation-acceptance-checklist-v1.json'
BINDS={
'docs/art/phase4-v19-dedicated-trajectory-inset-minimap-product-decision-and-implementation-design-gate-v1.json':'c7d86d9dcdd4891fac9dd9cbfffe3931561b5098db1df5a4aaec9b50e79af4c7',
'docs/art/phase4-v19-cp1-trajectory-inset-foundation-independent-ta-acceptance-addendum-v1.json':'e89e7febf1266efbea751ff0d50ac1d4a6bf358317974f701202d02a5396d02e',
'docs/art/phase4-v19-cp2-layout-draw-list-independent-ta-acceptance-addendum-v1.json':'6e7724df1a977af914b427538728f476d865db54c6cf95a3ae112e9de6172dd6',
'docs/art/phase4-v19-cp3-offscreen-atomic-independent-ta-acceptance-addendum-v1.json':'46dd31775ce501969a8079171d8ff106c8de4bf9698b04ba0c09cfed21f6007d',
'docs/evidence/phase4-v19-cp3-offscreen-atomic-implementation-v1/receipt.json':'9848e3f869db91f547e6c2cce1e4f6067538702a6dd04cbb18a585c57007133a'}
ALLOW=['src/Billiards/render/modern/modern_renderer.h','src/Billiards/render/modern/modern_renderer.cpp','src/Billiards/render/modern/modern_product_session.h','src/Billiards/render/modern/modern_product_session.cpp','src/Billiards/render/ui/product_ui_frame_coordinator.h','src/Billiards/render/ui/product_ui_frame_coordinator.cpp','src/Billiards/render/modern/trajectory_inset_visible_integration.h','src/Billiards/render/modern/trajectory_inset_visible_integration.cpp','tests/phase4_v19_trajectory_inset_visible_integration_cpu_tests.cpp']
FROZEN=['src/Billiards/training/trajectory_inset_model.h','src/Billiards/training/trajectory_inset_model.cpp','src/Billiards/training/trajectory_inset_renderer.h','src/Billiards/training/trajectory_inset_renderer.cpp','src/Billiards/render/ui/product_shell_layout.h','src/Billiards/render/ui/product_shell_layout.cpp','src/Billiards/render/ui/product_shell_draw_list.h','src/Billiards/render/ui/product_shell_draw_list.cpp','src/Billiards/render/modern/trajectory_inset_atomic_transaction.h','src/Billiards/render/modern/trajectory_inset_atomic_transaction.cpp']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def validate(path=DEFAULT):
 e=[]
 try:v=json.loads(path.read_text())
 except Exception as x:return [f'invalid:{x}']
 def q(c,m):
  if not c:e.append(m)
 q(v.get('schema')=='billiardgl.phase4.v19.cp4_visible_integration_implementation_acceptance_checklist.v1','schema')
 q(v.get('status')=='CP4_DESIGN_CHECKLIST_FROZEN_IMPLEMENTATION_NOT_STARTED_NO_PRODUCTION_CMAKE_GPU_READINESS_AUTH','status')
 q({x.get('path'):x.get('sha256') for x in v.get('bindings',[])}==BINDS,'bindings')
 for p,h in BINDS.items():q((ROOT/p).is_file() and sha(ROOT/p)==h,'drift:'+p)
 q(v.get('future_candidate_allowlist_not_authorized')==ALLOW,'allowlist')
 q(v.get('frozen_cp1_cp2_cp3_files_not_editable_in_cp4')==FROZEN,'frozen_files')
 r=v.get('explicit_current_frame_request_contract',{})
 q(r.get('accepted_screens_exact')==['TrainingSession','ReplayPlayback'] and r.get('world_overlay_control_on_required') is False and r.get('world_overlay_visibility_required') is False and r.get('screen_enum_or_product_frame_alone_may_enable') is False and r.get('user_toggle_off_wins') is True and r.get('stale_or_mixed_serials_fail_before_any_visible_write') is True and r.get('request_must_be_consumed_at_most_once_for_exact_frame') is True,'request')
 i=v.get('session_layout_package_identity_contract',{})
 q(i.get('exclusion_ids_exact')==['player status HUD','shot power and tip controls','rules decision panel or modal','caption and accessibility live-region band','safe-area edges and product footer'] and i.get('any_missing_duplicate_foreign_reordered_or_moved_exclusion_fails_closed') is True and i.get('no_stale_package_layout_preparation_or_ui_draw_list_reuse') is True,'identity')
 o=v.get('render_frame_invocation_and_order_contract',{})
 q(o.get('explicit_render_frame_invocation_count_for_accepted_on_request')==1 and o.get('absent_off_rejected_or_invalid_request_invocation_count')==0 and o.get('atomic_publish_precedes_product_ui_border_and_legend') is True and o.get('plot_pixels_may_not_be_double_tone_mapped') is True and o.get('panel_footer_or_surrounding_ui_may_not_refill_cutout') is True and o.get('main_world_trajectory_token_writes')==0 and o.get('world_overlay_draw_calls')==0 and len(o.get('ordered_visible_sequence',[]))==7,'order')
 a=v.get('visible_atomicity_and_failure_contract',{})
 q(a.get('publish_must_be_prevalidated_state_neutral_and_single_write') is True and a.get('visible_partial_write_or_chrome_without_plot_forbidden') is True and a.get('resize_context_loss_screen_change_modal_or_toggle_change_invalidates') is True and a.get('gl_error_query_upload_resolve_publish_or_ui_submit_fails_closed') is True,'atomicity')
 c=v.get('future_cpu_static_gl_mock_acceptance_not_run',{});m=c.get('positive_matrix',{})
 q(m=={'screens':['TrainingSession','ReplayPlayback'],'layout_profiles':['1920x1080_ui100','1920x1080_ui135','1280x720_ui100','1280x720_ui135'],'palettes':['Standard','CVD'],'sample_counts':[1,2,4],'exact_case_count':48},'matrix')
 q(len(c.get('required_static_checks',[]))==5 and len(c.get('required_gl_mock_checks',[]))==5 and len(c.get('required_negative_fixtures',[]))==12 and all(c.get(k) is False for k in ('cp4_implementation_started','cpu_static_pass','gl_mock_pass','cp4_accepted')),'cpu_boundary')
 g=v.get('future_gpu_ta_human_gates_separate_not_run',{})
 q(g.get('minimum_future_visual_cases')==['player_standard','elevated_standard','overhead_standard','broadcast_standard','broadcast_cvd'] and len(g.get('ta_axes',[]))==8 and all(g.get(k) is False for k in ('gpu_authorized','readiness_authorized','capture_authorized','machine_pass_claimed','ta_visual_pass_claimed','human_comprehension_pass_claimed')),'future_gates')
 b=v.get('authorization_boundary',{});truth={'cp1_accepted','cp2_accepted','cp3_accepted','cp4_design_frozen'}
 q(len(b)==15 and all(b.get(k) is (k in truth) for k in b),'boundary')
 return e
if __name__=='__main__':
 p=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else DEFAULT;e=validate(p);print(json.dumps({'valid':not e,'errors':e},sort_keys=True));raise SystemExit(bool(e))
