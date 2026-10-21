import copy,importlib.util,json,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2];V=ROOT/'scripts/validate_audit_v9_post_partial_receipts_evidence_only_impact_register.py';G=ROOT/'docs/rules/audit-v9-post-partial-receipts-evidence-only-impact-register-v1.json';S=importlib.util.spec_from_file_location('m',V);M=importlib.util.module_from_spec(S);S.loader.exec_module(M)
class T(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(G.read_text())
 def bad(self,f):v=copy.deepcopy(self.v);f(v);self.assertTrue(M.validate(v,False))
 def test_valid(self):self.assertEqual(M.validate(self.v,True),[])
 def test_status(self):self.bad(lambda v:v.update(status='PROMOTED'))
 def test_audit(self):self.bad(lambda v:v['bindings']['audit_v9'].update(sha256='0'*64))
 def test_order(self):self.bad(lambda v:v['rows'][0].update(order=5))
 def test_id(self):self.bad(lambda v:v['rows'][0].update(id='X'))
 def test_pre(self):self.bad(lambda v:v['rows'][0].update(audit_v9_status='IMPLEMENTED'))
 def test_post(self):self.bad(lambda v:v['rows'][0].update(post_receipt_status='IMPLEMENTED'))
 def test_evidence(self):self.bad(lambda v:v['rows'][0]['evidence_addable'].clear())
 def test_gap(self):self.bad(lambda v:v['rows'][0]['promotion_forbidden_because'].clear())
 def test_delta(self):self.bad(lambda v:v['summary_effect'].update(status_changes=1))
 def test_promote(self):self.bad(lambda v:v['claims'].update(status_promotion_authorized=True))
 def test_m4(self):self.bad(lambda v:v['claims'].update(m4_complete=True))
if __name__=='__main__':unittest.main()
