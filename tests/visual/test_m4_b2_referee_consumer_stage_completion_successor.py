import copy,importlib.util,json,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2];V=ROOT/'scripts/validate_m4_b2_referee_consumer_stage_completion_successor.py';G=ROOT/'docs/rules/m4-b2-referee-consumer-stage-completion-successor-v1.json'
S=importlib.util.spec_from_file_location('g',V);M=importlib.util.module_from_spec(S);S.loader.exec_module(M)
class T(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(G.read_text())
 def bad(self,f):v=copy.deepcopy(self.v);f(v);self.assertTrue(M.validate(v,False))
 def test_valid(self):self.assertEqual(M.validate(self.v,True),[])
 def test_status(self):self.bad(lambda v:v.update(status='M4_COMPLETE'))
 def test_policy(self):self.bad(lambda v:v['predecessor_bindings']['root_selected_policy'].update(sha256='0'*64))
 def test_c8(self):self.bad(lambda v:v['predecessor_bindings']['c8_defer_gate'].update(sha256='0'*64))
 def test_order(self):self.bad(lambda v:v['stage_rows'][1].update(order=8))
 def test_id(self):self.bad(lambda v:v['stage_rows'][1].update(id='X'))
 def test_receipt(self):self.bad(lambda v:v['stage_rows'][1].update(receipt_sha256='0'*64))
 def test_defer_receipt(self):self.bad(lambda v:v['stage_rows'][-1].update(receipt_path='fake'))
 def test_d2(self):self.bad(lambda v:v['dependency_successor'].update(D2_REFEREE_PROTOCOL='OPEN'))
 def test_d5(self):self.bad(lambda v:v['dependency_successor'].update(D5_CUE_CONTACT='SATISFIED'))
 def test_d6(self):self.bad(lambda v:v['dependency_successor'].update(D6_OFF_TABLE_PHYSICS='SATISFIED'))
 def test_full_set(self):self.bad(lambda v:v['claims']['implemented_full_consumers'].pop())
 def test_partial_set(self):self.bad(lambda v:v['claims']['implemented_partial_consumers'].pop())
 def test_defer_set(self):self.bad(lambda v:v['claims']['deferred_without_implementation'].pop())
 def test_all(self):self.bad(lambda v:v['claims'].update(all_rows_complete=True))
 def test_m4(self):self.bad(lambda v:v['claims'].update(m4_complete=True))
if __name__=='__main__':unittest.main()
