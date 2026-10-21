from __future__ import annotations
import copy,importlib.util,json,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];SCRIPT=ROOT/'scripts/validate_m4_d5_c_machine_double_hit_classification_policy_root_selected.py';ART=ROOT/'docs/rules/m4-d5-c-machine-double-hit-classification-policy-root-selected-v1.json'
spec=importlib.util.spec_from_file_location('v',SCRIPT);v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
class TestSelected(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.base=json.loads(ART.read_text())
 def bad(s,f):x=copy.deepcopy(s.base);f(x);s.assertTrue(v.validate(x,False))
 def test_live(s):s.assertEqual(v.validate(copy.deepcopy(s.base),True),[])
 def test_cli(s):s.assertEqual(subprocess.run([sys.executable,str(SCRIPT)],cwd=ROOT).returncode,0)
 def test_status(s):s.bad(lambda x:x.__setitem__('status','IMPLEMENTED'))
 def test_candidate(s):s.bad(lambda x:x['input_bindings']['candidate_gate'].__setitem__('sha256','0'*64))
 def test_directive(s):s.bad(lambda x:x['root_selection_directive'].__setitem__('semantic_text','changed'))
 def test_id(s):s.bad(lambda x:x['selected_bundle'].__setitem__('id','D5C-B_CONSERVATIVE_MACHINE_CLOSE_GRAZE'))
 def test_gap(s):s.bad(lambda x:x['selected_bundle']['constants'].__setitem__('very_close_surface_gap_cm',0.05))
 def test_graze(s):s.bad(lambda x:x['selected_bundle']['constants'].__setitem__('light_graze_normal_impulse_ratio',0.05))
 def test_tie(s):s.bad(lambda x:x['selected_bundle']['constants'].__setitem__('tip_separation_tie_tolerance_seconds',1e-7))
 def test_intervals(s):s.bad(lambda x:x['selected_bundle'].__setitem__('automatic_predicate','interval count >= 1'))
 def test_equal(s):s.bad(lambda x:x['selected_bundle'].__setitem__('raw_equal','DOUBLE_HIT_FOUL'))
 def test_review_foul(s):s.bad(lambda x:x['selection_semantics'].__setitem__('review_needed_is_foul',True))
 def test_prod(s):s.bad(lambda x:x['implementation_permissions'].__setitem__('tests_first_implementation_authorized',True))
 def test_rules(s):s.bad(lambda x:x['implementation_permissions'].__setitem__('rules_or_shotfacts_consumer_authorized',True))
 def test_complete(s):s.bad(lambda x:x['claims'].__setitem__('d5_complete',True))
 def test_audit(s):s.bad(lambda x:x['claims'].__setitem__('audit_row_promoted',True))
 def test_gpu(s):s.bad(lambda x:x['claims'].__setitem__('gpu_executed_or_authorized',True))
 def test_json(s):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'x';p.write_text('{');s.assertNotEqual(subprocess.run([sys.executable,str(SCRIPT),str(p)],cwd=ROOT).returncode,0)
if __name__=='__main__':unittest.main()
