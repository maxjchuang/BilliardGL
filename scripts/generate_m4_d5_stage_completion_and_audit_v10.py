#!/usr/bin/env python3
import copy, hashlib, json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
V9=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v9.json'
STAGE=ROOT/'docs/rules/m4-d5-cue-contact-stage-completion-successor-v1.json'
V10=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v10.json'

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def binding(path): return {'path':str(path.relative_to(ROOT)),'sha256':sha(path)}

RECEIPTS={
 'D5-A':ROOT/'docs/rules/m4-d5-a-stroke-observation-canonical-tip-interval-foundation-implementation-receipt-v1.json',
 'D5-B':ROOT/'docs/rules/m4-d5-b-cue-object-frozen-cushion-policy-independent-telemetry-implementation-receipt-v1.json',
 'D5-C':ROOT/'docs/rules/m4-d5-c-machine-double-hit-classification-implementation-receipt-v1.json',
 'D5-D':ROOT/'docs/rules/m4-d5-d-push-frozen-classification-implementation-receipt-v1.json'}
LIFECYCLES={
 'WPA-II-12-5-6':ROOT/'docs/rules/wpa-ii-12-5-6-signed-close-case-declaration-lifecycle-implementation-receipt-v1.json',
 'WPA-II-12-7':ROOT/'docs/rules/wpa-ii-12-7-signed-push-review-declaration-lifecycle-implementation-receipt-v1.json',
 'WPA-II-15-1':ROOT/'docs/rules/wpa-ii-15-1-signed-frozen-cue-object-declaration-lifecycle-implementation-receipt-v1.json',
 'WPA-II-15-2':ROOT/'docs/rules/wpa-ii-15-2-signed-frozen-object-straight-cushion-declaration-lifecycle-implementation-receipt-v1.json',
 'WPA-II-17':ROOT/'docs/rules/wpa-ii-17-exact-nonphysical-referee-foul-subset-implementation-receipt-v1.json'}
POLICY=ROOT/'docs/rules/m4-wpa-policy-bundle-root-selected-v1.json'

def write(path,value): path.write_text(json.dumps(value,indent=2)+'\n')

def stage():
 return {
  'schema':'m4_d5_cue_contact_stage_completion_successor_v1',
  'status':'FINAL_ROOT_ACCEPTED_D5_ONLY_SUCCESSOR',
  'bindings':{'audit_v9':binding(V9),'root_selected_policy':binding(POLICY),
              **{k.lower().replace('-','_')+'_receipt':binding(v) for k,v in RECEIPTS.items()}},
  'stage_batches':[{'order':i,'id':k,'receipt':binding(RECEIPTS[k]),'state':'CPU_IMPLEMENTED'} for i,k in enumerate(RECEIPTS)],
  'row_assessments':[
   {'order':0,'id':'WPA-II-12-5-6','audit_v9_status':'MISSING','successor_recommendation':'IMPLEMENTED','product_reachable':True,'basis':['complete canonical interval count','exact cue-object/tip order','automatic multi-interval classifier','signed close-case review fallback'],'remaining_selected_scope_gaps':[]},
   {'order':1,'id':'WPA-II-12-7','audit_v9_status':'MISSING','successor_recommendation':'IMPLEMENTED','product_reachable':True,'basis':['complete report-only duration and path','same-shot signed referee push final','immutable push foul classification','ordinary foul ball-in-hand'],'remaining_selected_scope_gaps':[]},
   {'order':2,'id':'WPA-II-15-1','audit_v9_status':'MISSING','successor_recommendation':'IMPLEMENTED','product_reachable':True,'basis':['signed pre-shot frozen cue/object declaration','exact D5 contact order','multi-interval double-hit remains foul','signed obvious push remains foul and is never suppressed by frozen declaration'],'remaining_selected_scope_gaps':[]},
   {'order':3,'id':'WPA-II-15-2','audit_v9_status':'MISSING','successor_recommendation':'IMPLEMENTED','product_reachable':True,'basis':['signed frozen object and named straight cushion','departure then exact named recontact','post-first-legal-contact straight impulse or complete object pocket','jaw and correction-only contacts excluded'],'remaining_selected_scope_gaps':[]},
   {'order':4,'id':'WPA-II-17','audit_v9_status':'PARTIAL','successor_recommendation':'PARTIAL','product_reachable':True,'basis':['D5 double-hit and push ordinary-foul BIH subset now implemented','existing cue scratch and signed nonphysical subset remain implemented'],'remaining_selected_scope_gaps':['D6 cue/object off-table physical telemetry and classification','WPA-II-12.3 versus 17(e) off-table policy application','mobile repeat loss-of-rack lifecycle','full general-foul inventory completion']}
  ],
  'dependency_successor':{'D5_CUE_CONTACT':'SATISFIED_CPU_IMPLEMENTED','D6_OFF_TABLE_PHYSICS':'NOT_SATISFIED','D56_FRESH_INTEGRATION_EVIDENCE':'NOT_STARTED','M4':'NOT_COMPLETE'},
  'claims':{'d5_stage_completed':True,'d6_completed':False,'recommended_promotions':['WPA-II-12-5-6','WPA-II-12-7','WPA-II-15-1','WPA-II-15-2'],'wpa_ii_17_complete':False,'audit_successor_nonfinal':True,'production_or_cmake_modified_by_successor':False,'gpu_readiness_or_authorization':False,'m4_complete':False}}

def evidence(path, finding):
 b=binding(path); b.update({'kind':'implementation_receipt','finding':finding}); return b

def audit(stage_binding):
 d=copy.deepcopy(json.loads(V9.read_text()))
 d['schema']='phase5_wpa_heyball_as_built_reconciliation_audit_v10'
 d['schema_version']=10
 d['audit_id']='phase5_wpa_heyball_2025_08_16_as_built_reconciliation_v10'
 d['audit_scope']='d5_cue_contact_stage_completion_four_row_successor'
 d['audit_status']='FINAL_ROOT_ACCEPTED_D5_ONLY_SUCCESSOR'
 d['frozen_v9_predecessor']=binding(V9)
 d['d5_stage_completion']=stage_binding
 d['summary']={'total':40,'implemented':19,'partial':7,'missing':7,'procedural_only':7}
 remove={'WPA-II-12-5-6','WPA-II-12-7','WPA-II-15-1','WPA-II-15-2'}
 d['m4_required_unclosed']=[x for x in d['m4_required_unclosed'] if x not in remove]
 mapping={x['id']:x for x in d['matrix']}
 row_specs={
 'WPA-II-12-5-6':('Complete canonical cue-tip intervals and exact cue-object ordering feed the root-selected automatic multi-interval double-hit classifier; close cases remain signed-referee review, and the immutable foul result grants ordinary ball in hand.',['D5-A','D5-B','D5-C']),
 'WPA-II-12-7':('Complete report-only duration/path measurements plus an exact same-shot signed referee final produce the immutable push-foul result; no duration or distance threshold exists.',['D5-A','D5-D']),
 'WPA-II-15-1':('The signed pre-shot frozen cue/object declaration is bound to D5 telemetry. Frozen status never suppresses a multi-interval double-hit or signed obvious-push foul.',['D5-A','D5-B','D5-C','D5-D']),
 'WPA-II-15-2':('The signed frozen object/named-straight-cushion declaration is enforced by complete departure/recontact and alternative straight-impulse/object-pocket facts; jaw and correction-only contacts are excluded.',['D5-B','D5-D'])}
 for row_id,(assessment,batches) in row_specs.items():
  row=mapping[row_id]; row['status']='IMPLEMENTED'; row['product_reachable']=True; row['policy_decision_required']=False
  row['core_increments']=batches; row['assessment']=assessment; row['gaps']=[]
  row['evidence']=[evidence(RECEIPTS[b],f'{b} CPU implementation receipt') for b in batches]+[evidence(LIFECYCLES[row_id],'Authenticated signed lifecycle receipt')]
 row=mapping['WPA-II-17']; row['status']='PARTIAL'; row['product_reachable']=True; row['policy_decision_required']=True
 row['assessment']='Cue scratch, wrong-first/no-contact/no-rail, D5 double-hit/push, and the exact signed nonphysical subset grant ordinary ball in hand. Full §17 remains open for D6 off-table physics/classification, the 12.3/17(e) policy application, mobile-repeat loss-of-rack, and the remaining inventory.'
 row['gaps']=['D6 cue/object off-table physical telemetry and classification are absent.','WPA-II-12.3 versus 17(e) off-table application remains unresolved.','Mobile repeat loss-of-rack and the remaining full general-foul inventory are incomplete.']
 row['evidence']=[evidence(RECEIPTS['D5-C'],'Double-hit classification and BIH'),evidence(RECEIPTS['D5-D'],'Push classification and BIH'),evidence(LIFECYCLES['WPA-II-17'],'Exact signed nonphysical foul subset')]
 d['d5_completion_boundaries']={'rows_promoted':['WPA-II-12-5-6','WPA-II-12-7','WPA-II-15-1','WPA-II-15-2'],'row_retained_partial':'WPA-II-17','d6_off_table_complete':False,'profile_complete':False,'product_exposed':False,'global_rules_ui_exposed':False,'gpu_executed':False,'readiness_or_authorization_created':False,'m4_complete':False,'production_or_cmake_modified':False}
 return d

def main():
 s=stage(); write(STAGE,s); write(V10,audit(binding(STAGE)))
if __name__=='__main__': main()
