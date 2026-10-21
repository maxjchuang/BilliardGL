#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/m4-d6-a-3d-table-departure-physics-foundation-implementation-design-gate-v1.json'
PROD=['src/Billiards/game_state.h','src/Billiards/game_state.cpp','src/Billiards/table_specs.h','src/Billiards/table_specs.cpp','src/Billiards/physics_profile.h','src/Billiards/physics_profile.cpp','src/Billiards/table_departure_geometry.h','src/Billiards/table_departure_geometry.cpp','src/Billiards/pocket_boundary.h','src/Billiards/pocket_boundary.cpp','src/Billiards/physics.h','src/Billiards/physics.cpp']
TEST=['tests/game_state_tests.cpp','tests/table_specs_tests.cpp','tests/physics_profile_tests.cpp','tests/table_departure_geometry_tests.cpp','tests/pocket_boundary_tests.cpp','tests/physics_tests.cpp']
ABSENT=['src/Billiards/table_departure_geometry.h','src/Billiards/table_departure_geometry.cpp','tests/table_departure_geometry_tests.cpp']
LIVE=['CMakeLists.txt']+[p for p in PROD+TEST if p not in ABSENT]
AUTH={'root_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','d5_d6_feasibility':'86d4957620d9e2c9033708ef3dbd5329610cc9cc9c44a134369079cffa375f77','off_table_gap_gate':'3fc468502ab09e7ba0122d84b6c20386b0646d1f0db76a3f8ab3e8a1d30a9dbe','profile_defer_gate':'f217958cf035024150b622421b68d80bdcf5e8dd4f7281bfeecf447fee87043a','d5_stage':'c977989b6d1045218caca8a609dd4df0b39114a81fdc1992d53ea5f79da65d15','audit_v10':'8d2e45aa18f7084b6252592e3ca7a57f5f97747ea6ec978d0d701b65cf60a5ab','measured_table_asset':'fa9e44ea29b1a0561d3e0e725f378e7ba596d0d57cd9e78f8178342b0f70de02'}
def sha(b):return hashlib.sha256(b).hexdigest()
def binding(r):
 p=Path(r.get('path',''))
 if p.is_absolute() or '..' in p.parts:return False
 q=ROOT/p;return q.is_file() and sha(q.read_bytes())==r.get('sha256')
def validate(v,files=True):
 if not isinstance(v,dict):return ['root']
 e=[]
 if v.get('schema')!='m4_d6_a_3d_table_departure_physics_foundation_implementation_design_gate_v1' or v.get('status')!='FINAL_DESIGN_ONLY_ROOT_REVALIDATION_REQUIRED_NO_PRODUCTION_CHANGE':e.append('header')
 a=v.get('authority_bindings',{})
 for k,d in AUTH.items():
  r=a.get(k,{})
  if r.get('sha256')!=d:e.append('authority:'+k)
  elif files and not binding(r):e.append('authority_drift:'+k)
 g=v.get('current_gap',{})
 if g.get('motion_model')!='PLANAR_XZ_SURFACE_ONLY' or g.get('position_y_initialized_but_not_gravity_integrated') is not True or g.get('rail_top_or_outer_landing_collision_geometry') is not False or g.get('physical_departure_or_return_possible') is not False or g.get('d6_a_foundation_currently_implemented') is not False:e.append('gap')
 c=v.get('coordinate_and_profile_contract',{});d=c.get('sealed_default_dimensions',{})
 expected={'ball_radius_cm':2.8575,'cloth_surface_y_cm':86.483976,'cloth_supported_ball_center_y_cm':89.341476,'playfield_half_width_cm':63.5,'playfield_half_length_cm':127.0,'default_cushion_nose_center_y_cm':89.341476,'rail_top_surface_y_cm':92.104021,'rail_top_supported_ball_center_y_cm':94.961521,'outer_rail_half_width_cm':83.4336812635,'outer_rail_half_length_cm':146.0679224206,'outside_landing_surface_y_cm':0.0,'outside_landing_supported_ball_center_y_cm':2.8575,'gravity_y_cm_s2':-980.665}
 if c.get('world_axes')!={'x':'table_width','y':'positive_up','z':'table_length'} or any(d.get(k)!=x for k,x in expected.items()) or 'noseHeightRatio' not in str(d.get('cushion_nose_center_y_formula')):e.append('dimensions')
 p=v.get('physical_domain_contract',{})
 if p.get('domain_enum_order')!=['CLOTH_SUPPORTED','AIRBORNE','RAIL_TOP_SUPPORTED','OUTSIDE_LANDING_SUPPORTED','POCKET_CHANNEL'] or p.get('state_is_physical_not_rules_classification') is not True or 'no canonical' not in str(p.get('rest')):e.append('domains')
 geo=v.get('geometry_and_topology_contract',{})
 if geo.get('analytic_geometry_only') is not True or geo.get('runtime_renderer_mesh_dependency') is not False or 'fail' not in str(geo.get('same_toi_ambiguity')) or 'whole-tick TOI' not in str(geo.get('candidate_order')):e.append('geometry')
 atom=v.get('atomicity_determinism_and_compatibility',{})
 if 'temporary GameState' not in str(atom.get('staging')) or 'exact pre-step bytes' not in str(atom.get('failed_step')) or 'hidden disabled path cannot satisfy' not in str(atom.get('profile_migration')):e.append('atomic')
 b=v.get('policy_and_consumer_boundary',{})
 false_keys=['d6_b_canonical_telemetry_added','d6_c_classifier_added','wpa_ii_12_3_policy_encoded','wpa_ii_17_e_policy_encoded','cue_or_object_ball_foul_decided','ball_in_hand_or_loss_of_rack_awarded','shotfacts_collector_rules_or_adjudicator_modified','product_or_visible_ui_modified']
 for k in false_keys:
  if b.get(k) is not False:e.append('boundary:'+k)
 for k in ('coordinate_outside_is_not_a_fact','pocketed_or_post_pocket_storage_is_not_a_fact','referee_input_or_profile_selection_not_added'):
  if b.get(k) is not True:e.append('boundary:'+k)
 f=v.get('required_fixtures',{});pos=f.get('positive',[]);neg=f.get('negative',[]);gpu=f.get('future_gpu_required_after_cpu_stability',[])
 if len(pos)!=14 or len(neg)!=20 or len(gpu)!=7 or len(set(pos+neg+gpu))!=41:e.append('fixtures')
 al=v.get('implementation_allowlist',{})
 if al.get('production_files')!=PROD or al.get('test_files')!=TEST or al.get('build_files')!=['CMakeLists.txt'] or al.get('required_absent_paths_at_gate')!=ABSENT or len(al.get('forbidden_exact_files',[]))!=8:e.append('allowlist')
 if 'at most one' not in str(al.get('cmake_scope')) or al.get('forbidden_production_prefixes')!=['src/Billiards/rules','src/Billiards/render','src/Billiards/product','src/Billiards/training']:e.append('allowlist_scope')
 live=v.get('live_source_bindings',[])
 if [r.get('path') for r in live]!=LIVE:e.append('live_order')
 elif files:
  for r in live:
   if not binding(r):e.append('live_drift:'+str(r.get('path')))
  for pth in ABSENT:
   if (ROOT/pth).exists():e.append('expected_absent:'+pth)
 inv=v.get('visual_evidence_invalidation_boundary',{})
 if inv.get('historical_receipts_remain_immutable') is not True or inv.get('historical_pixels_remain_historical_only') is not True or len(inv.get('not_inheritable_for_current_build',[]))!=4 or len(inv.get('required_successor_sequence',[]))!=6 or inv.get('gpu_or_ta_executed_by_this_gate') is not False:e.append('evidence')
 x=v.get('completion_boundary',{})
 if x.get('root_revalidation_required') is not True:e.append('root_gate')
 for k in ('implementation_authorized_by_this_gate','production_or_cmake_modified','d6_a_implemented','d6_b_or_d6_c_started','wpa_ii_12_3_or_17_completed','audit_status_or_product_exposure_advanced','gpu_readiness_authorization_or_capture','m4_complete'):
  if x.get(k) is not False:e.append('claim:'+k)
 return e
def main():
 p=argparse.ArgumentParser();p.add_argument('path',nargs='?',default=str(DEFAULT));p.add_argument('--no-file-checks',action='store_true');x=p.parse_args()
 try:v=json.loads(Path(x.path).read_text())
 except Exception as z:print('INVALID:',z);return 1
 e=validate(v,not x.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_D6_A_3D_TABLE_DEPARTURE_PHYSICS_FOUNDATION_DESIGN_GATE');return 0
if __name__=='__main__':raise SystemExit(main())
