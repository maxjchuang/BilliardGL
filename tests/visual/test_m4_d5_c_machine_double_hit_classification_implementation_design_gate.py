from __future__ import annotations
import copy,importlib.util,json,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];SCRIPT=ROOT/'scripts/validate_m4_d5_c_machine_double_hit_classification_implementation_design_gate.py';GATE=ROOT/'docs/rules/m4-d5-c-machine-double-hit-classification-implementation-design-gate-v1.json'
spec=importlib.util.spec_from_file_location('v',SCRIPT);v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
class TestGate(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.base=json.loads(GATE.read_text())
 def bad(s,f):x=copy.deepcopy(s.base);f(x);s.assertTrue(v.validate(x,False))
 def test_live(s):s.assertEqual(v.validate(copy.deepcopy(s.base),True),[])
 def test_cli(s):s.assertEqual(subprocess.run([sys.executable,str(SCRIPT)],cwd=ROOT).returncode,0)
 def test_status(s):s.bad(lambda x:x.__setitem__('status','IMPLEMENTED'))
 def test_policy(s):s.bad(lambda x:x['authority_bindings']['root_selected_d5_c_policy'].__setitem__('sha256','0'*64))
 def test_gap(s):s.bad(lambda x:x['selected_policy_constants'].__setitem__('very_close_surface_gap_cm',0.05))
 def test_graze(s):s.bad(lambda x:x['selected_policy_constants'].__setitem__('light_graze_normal_impulse_ratio',0.05))
 def test_tie(s):s.bad(lambda x:x['selected_policy_constants'].__setitem__('tip_separation_tie_tolerance_seconds',1e-7))
 def test_schema(s):s.bad(lambda x:x['classifier_contract'].__setitem__('schema_id','x'))
 def test_equal(s):s.bad(lambda x:x['classifier_contract'].__setitem__('raw_equal','DOUBLE_HIT_FOUL'))
 def test_signed_override(s):s.bad(lambda x:x['classifier_contract'].__setitem__('signed_review_never_overrides_proven_multiple_intervals',False))
 def test_review_foul(s):s.bad(lambda x:x['rules_consumption_contract'].__setitem__('review_required_does_not_mutate_match_state_or_adjudication',False))
 def test_recompute(s):s.bad(lambda x:x['rules_consumption_contract'].__setitem__('adjudicator_may_not_recompute_interval_count_or_order',False))
 def test_atomic(s):s.bad(lambda x:x['atomicity_and_idempotency'].__setitem__('failed_physics_or_incomplete_d5_rolls_back_classifier_result',False))
 def test_pos(s):s.bad(lambda x:x['required_fixtures']['positive'].reverse())
 def test_neg(s):s.bad(lambda x:x['required_fixtures']['negative'].pop())
 def test_prod(s):s.bad(lambda x:x['implementation_allowlist']['production_files'].pop())
 def test_test(s):s.bad(lambda x:x['implementation_allowlist']['test_files'].pop())
 def test_forbidden(s):s.bad(lambda x:x['implementation_allowlist']['explicitly_forbidden_files'].pop())
 def test_live_binding(s):s.bad(lambda x:x['live_source_bindings'].pop())
 def test_authorize(s):s.bad(lambda x:x['completion_boundary'].__setitem__('implementation_authorized_by_this_gate',True))
 def test_complete(s):s.bad(lambda x:x['completion_boundary'].__setitem__('d5_complete',True))
 def test_audit(s):s.bad(lambda x:x['completion_boundary'].__setitem__('wpa_ii_12_5_6_or_15_1_promoted',True))
 def test_gpu(s):s.bad(lambda x:x['completion_boundary'].__setitem__('gpu_readiness_or_authorization',True))
 def test_json(s):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'x';p.write_text('{');s.assertNotEqual(subprocess.run([sys.executable,str(SCRIPT),str(p)],cwd=ROOT).returncode,0)
if __name__=='__main__':unittest.main()
