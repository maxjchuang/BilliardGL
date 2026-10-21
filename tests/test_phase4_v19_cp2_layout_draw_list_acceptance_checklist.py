from __future__ import annotations
import copy,json,tempfile,unittest
from pathlib import Path
from scripts import validate_phase4_v19_cp2_layout_draw_list_acceptance_checklist as V
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs/art/phase4-v19-cp2-layout-draw-list-implementation-acceptance-checklist-v1.json'
class T(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(DOC.read_text())
 def reject(s,f):
  v=copy.deepcopy(s.v);f(v)
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'x.json';p.write_text(json.dumps(v));s.assertTrue(V.validate(p))
 def test_pass(s):s.assertEqual(V.validate(DOC),[])
 def test_mutations(s):
  ms=[lambda v:v['bindings'][0].update(sha256='0'*64),lambda v:v['future_allowed_files_only'].append('CMakeLists.txt'),lambda v:v['layout_contract'].update(reference_outer_panel_rect_px=[0,0,447,288]),lambda v:v['layout_contract'].update(minimum_outer_panel_px=[335,216]),lambda v:v['layout_contract'].update(default_world_overlay_visible=True),lambda v:v['transparent_cutout_contract'].update(plot_cutout_is_fully_transparent=False),lambda v:v['bilingual_ui_contract']['legend_entries_in_order'].reverse(),lambda v:v['zero_overlap_and_exclusion_contract'].update(panel_vs_each_exclusion_overlap_area_px=1),lambda v:v['future_cpu_static_acceptance_not_run'].update(implementation_started=True),lambda v:v['authorization_boundary'].update(gpu_authorized=True)]
  for i,m in enumerate(ms):
   with s.subTest(i=i):s.reject(m)
if __name__=='__main__':unittest.main()
