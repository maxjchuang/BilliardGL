#!/usr/bin/env python3
from __future__ import annotations
import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];DEFAULT=ROOT/'docs/art/phase4-v19-cp2-layout-draw-list-implementation-acceptance-checklist-v1.json'
BASE={'docs/art/phase4-v19-dedicated-trajectory-inset-minimap-product-decision-and-implementation-design-gate-v1.json':'c7d86d9dcdd4891fac9dd9cbfffe3931561b5098db1df5a4aaec9b50e79af4c7','docs/art/phase4-v19-landscape-orientation-layout-interpretation-correction-gate-v1.json':'cffc93f29bb24f7f9378dee3c17fffd1da1c139745eec17bdb380cf6bda04d13'}
FILES=['src/Billiards/render/ui/product_shell_layout.h','src/Billiards/render/ui/product_shell_layout.cpp','src/Billiards/render/ui/product_shell_draw_list.h','src/Billiards/render/ui/product_shell_draw_list.cpp','tests/phase4_v19_trajectory_inset_layout_draw_list_cpu_tests.cpp']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def validate(path=DEFAULT):
 e=[]
 try:v=json.loads(path.read_text())
 except Exception as x:return [str(x)]
 def q(c,m):
  if not c:e.append(m)
 q(v.get('schema')=='billiardgl.phase4.v19.cp2_layout_draw_list_implementation_acceptance_checklist.v1','schema')
 q(v.get('status')=='CP2_ACCEPTANCE_CHECKLIST_FROZEN_IMPLEMENTATION_NOT_STARTED_NO_GPU_NO_AUTH','status')
 binds={x.get('path'):x.get('sha256') for x in v.get('bindings',[])}
 for p,h in BASE.items():q(binds.get(p)==h and sha(ROOT/p)==h,'binding:'+p)
 cp1='docs/art/phase4-v19-cp1-trajectory-inset-foundation-independent-ta-acceptance-addendum-v1.json';q(binds.get(cp1)==sha(ROOT/cp1),'cp1_binding')
 q(v.get('future_allowed_files_only')==FILES,'allowlist')
 l=v.get('layout_contract',{});q(l.get('reference_outer_panel_rect_px')==[1440,680,448,288] and l.get('reference_plot_rect_within_panel_px')==[16,48,416,208] and l.get('minimum_outer_panel_px')==[336,216] and l.get('plot_aspect_ratio')==2.0 and l.get('default_inset_visible') is True and l.get('default_world_overlay_visible') is False,'layout')
 c=v.get('transparent_cutout_contract',{});q(len(c)==5 and c.get('plot_cutout_is_fully_transparent') is True and c.get('cutout_writes_no_opaque_panel_fill_text_border_legend_or_toggle_pixels') is True and c.get('trajectory_semantic_members_or_probes_may_enter_ui_draw_list') is False and c.get('ui_label_toggle_or_border_may_enter_inset_semantic_denominator') is False,'cutout')
 b=v.get('bilingual_ui_contract',{});q(b.get('title')=='TRAJECTORY / 轨迹' and b.get('orientation_labels_in_order')==['HEAD / 开球端','FOOT / 底端'] and b.get('legend_entries_in_order')==['ACTUAL / 实际 — paired continuous rails','PREDICTED / 预测 — centerline dashes','END / 终点 — endpoint marker'] and b.get('minimum_legend_text_px_at_reference')==16 and b.get('minimum_toggle_target_px')==[44,44] and b.get('all_labels_excluded_from_trajectory_denominators') is True,'bilingual')
 z=v.get('zero_overlap_and_exclusion_contract',{});q(len(z.get('required_exclusions',[]))==5 and z.get('panel_vs_each_exclusion_overlap_area_px')==0 and z.get('plot_cutout_vs_border_header_legend_toggle_overlap_area_px')==0 and 'remains off' in z.get('modal_behavior','') and 'remains off' in z.get('layout_failure_behavior',''),'overlap')
 f=v.get('future_cpu_static_acceptance_not_run',{});q(len(f.get('required_positive_checks',[]))==8 and len(f.get('required_fail_closed_mutations',[]))==9 and f.get('implementation_started') is False and f.get('cpu_static_pass') is False and f.get('cp2_accepted') is False,'future')
 a=v.get('authorization_boundary',{});q(len(a)==9 and all(x is False for x in a.values()),'authority')
 return e
if __name__=='__main__':
 p=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else DEFAULT;e=validate(p);print(json.dumps({'valid':not e,'errors':e},sort_keys=True));raise SystemExit(bool(e))
