#!/usr/bin/env python3
import hashlib,json,pathlib,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
STAGE=ROOT/'docs/rules/m4-d5-cue-contact-stage-completion-successor-v1.json'
AUDIT=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v10.json'
V9=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v9.json'
PROMOTED=['WPA-II-12-5-6','WPA-II-12-7','WPA-II-15-1','WPA-II-15-2']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def req(x,m):
 if not x:raise ValueError(m)
def safe(p):
 q=(ROOT/p).resolve();req(ROOT in q.parents,'path');return q
def check_binding(b):req(set(b)=={'path','sha256'},'binding shape');req(sha(safe(b['path']))==b['sha256'],'binding drift')
def validate_stage(d,files=True):
 req(isinstance(d,dict),'stage object');req(d.get('schema')=='m4_d5_cue_contact_stage_completion_successor_v1','stage schema');req(d.get('status')=='FINAL_ROOT_ACCEPTED_D5_ONLY_SUCCESSOR','stage status')
 req([x.get('id') for x in d.get('stage_batches',[])]==['D5-A','D5-B','D5-C','D5-D'],'batch order');req(all(x.get('state')=='CPU_IMPLEMENTED' for x in d['stage_batches']),'batch state')
 rows=d.get('row_assessments',[]);req([x.get('id') for x in rows]==PROMOTED+['WPA-II-17'],'row order');req(all(x['successor_recommendation']=='IMPLEMENTED' and x['remaining_selected_scope_gaps']==[] for x in rows[:4]),'promotions');req(rows[4]['successor_recommendation']=='PARTIAL' and len(rows[4]['remaining_selected_scope_gaps'])==4,'17 boundary')
 dep=d.get('dependency_successor',{});req(dep=={'D5_CUE_CONTACT':'SATISFIED_CPU_IMPLEMENTED','D6_OFF_TABLE_PHYSICS':'NOT_SATISFIED','D56_FRESH_INTEGRATION_EVIDENCE':'NOT_STARTED','M4':'NOT_COMPLETE'},'deps')
 c=d.get('claims',{});req(c.get('d5_stage_completed') is True and c.get('recommended_promotions')==PROMOTED and c.get('audit_successor_nonfinal') is True,'claims');req(all(c.get(k) is False for k in ['d6_completed','wpa_ii_17_complete','production_or_cmake_modified_by_successor','gpu_readiness_or_authorization','m4_complete']),'false claims')
 if files:
  for b in d['bindings'].values():check_binding(b)
  for x in d['stage_batches']:check_binding(x['receipt'])
def validate_audit(d,files=True):
 req(isinstance(d,dict),'audit object');req(d.get('schema')=='phase5_wpa_heyball_as_built_reconciliation_audit_v10' and d.get('schema_version')==10,'audit schema');req(d.get('audit_status')=='FINAL_ROOT_ACCEPTED_D5_ONLY_SUCCESSOR','audit status');req(d.get('summary')=={'total':40,'implemented':19,'partial':7,'missing':7,'procedural_only':7},'summary')
 rows=d.get('matrix',[]);req(len(rows)==40 and len({x['id'] for x in rows})==40,'40 rows');base=json.loads(V9.read_text());bm={x['id']:x for x in base['matrix']};vm={x['id']:x for x in rows};req(set(vm)==set(bm),'ids')
 for i in set(vm)-set(PROMOTED)-{'WPA-II-17'}:req(vm[i]==bm[i],'unchanged '+i)
 for i in PROMOTED:req(vm[i]['status']=='IMPLEMENTED' and vm[i]['product_reachable'] is True and vm[i]['gaps']==[],'promotion '+i)
 req(vm['WPA-II-17']['status']=='PARTIAL' and len(vm['WPA-II-17']['gaps'])==3,'17 partial')
 req(d.get('m4_required_unclosed')==[x for x in base['m4_required_unclosed'] if x not in set(PROMOTED)],'m4 list')
 b=d.get('d5_completion_boundaries',{});req(b.get('rows_promoted')==PROMOTED and b.get('row_retained_partial')=='WPA-II-17','boundary rows');req(all(b.get(k) is False for k in ['d6_off_table_complete','profile_complete','product_exposed','global_rules_ui_exposed','gpu_executed','readiness_or_authorization_created','m4_complete','production_or_cmake_modified']),'boundaries')
 req(d.get('profile_complete') is False and d.get('product_exposed') is False and d.get('complete_profile_product_exposed') is False,'profile')
 if files:
  check_binding(d['frozen_v9_predecessor']);check_binding(d['d5_stage_completion'])
  for i in PROMOTED+['WPA-II-17']:
   for e in vm[i]['evidence']:check_binding({'path':e['path'],'sha256':e['sha256']})
def main():
 try:validate_stage(json.loads(STAGE.read_text()));validate_audit(json.loads(AUDIT.read_text()))
 except Exception as e:print('INVALID_D5_STAGE_OR_AUDIT_V10:',e);return 1
 print('PASS_D5_STAGE_AND_AUDIT_V10_FINAL_ROOT_ACCEPTED');return 0
if __name__=='__main__':sys.exit(main())
