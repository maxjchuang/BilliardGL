"""Check actual v14 native-cm top seam, section bounds, normal/index identity."""
import json,math,hashlib,struct,collections,sys
from pathlib import Path
import pocket_shoulder_profile_20261013 as shoulder
root=Path(sys.argv[1]);assets=root/'assets';x=json.loads((root/'actual-solid-triangles-cm.json').read_text());nodes={m['node']:m for m in x['solid_meshes_lod0']};jaw=nodes['NODE_pocket_cushion_jaw_assembly_LOD0']['primitives'][0]['positions_world_cm'];rimset={tuple(v) for v in nodes['NODE_pocket_liner_rim_assembly_LOD0']['primitives'][0]['positions_world_cm']};matches=[]
for arm in range(12):
 cap=jaw[(arm+1)*2134-22:(arm+1)*2134];seam=cap[8:]+[cap[0]];missing=[v for v in seam if tuple(v) not in rimset]
 matches.append({'arm':arm,'upper_seam_vertices':len(seam),'actual_native_cm_exact_rim_match':not missing,'missing':missing})
assert all(v['actual_native_cm_exact_rim_match'] for v in matches),matches
sections=[]
for typ,tail in [('corner',.0345*.25),('side',.0365*.25)]:
 for i in range(25):
  q=.5-.5*math.cos(math.pi*i/24);width=.046+q*(tail-.046);nose=.0363+q*(.025-.0363);roof=.045+q*(.0265-.045);base=.004+q*(.012-.004);pr,proof=shoulder.profile(width,nose,roof,base)
  assert all(v>0 for v in proof['d_monotone_bernstein_differences']) and all(v>=0 for v in proof['height_monotone_bernstein_differences'])
  assert max(h for d,h in pr)<=roof and min(h for d,h in pr)>=base
  sections.append({'type':typ,'row':i,**proof})
g=json.loads((assets/'focused_practice_hall_modular_v1.gltf').read_text());buffers=[(assets/v['uri']).read_bytes() for v in g['buffers']]
def acc(i):
 a=g['accessors'][i];v=g['bufferViews'][a['bufferView']];n={'SCALAR':1,'VEC3':3,'VEC4':4}[a['type']];fmt={5126:'f',5123:'H',5125:'I'}[a['componentType']];size=struct.calcsize('<'+fmt*n);offset=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',size);raw=b''.join(buffers[v['buffer']][offset+k*stride:offset+k*stride+size] for k in range(a['count']));return raw,[struct.unpack_from('<'+fmt*n,raw,k*size) for k in range(a['count'])]
witness=[];groups={};normalerr=0.;normalzero=0;doterr=0.
for n in g['nodes']:
 if not n['name'].startswith(('NODE_table_','NODE_pocket_')) or 'mesh' not in n or 'LOD' not in n['name']:continue
 sha=hashlib.sha256()
 for p in g['meshes'][n['mesh']]['primitives']:
  for attribute in ('POSITION','NORMAL','TANGENT'):sha.update(acc(p['attributes'][attribute])[0])
  sha.update(acc(p['indices'])[0]);ns=acc(p['attributes']['NORMAL'])[1];ts=acc(p['attributes']['TANGENT'])[1]
  for normal,tangent in zip(ns,ts):
   ll=math.sqrt(sum(v*v for v in normal));normalerr=max(normalerr,abs(ll-1));normalzero+=ll<1e-6;doterr=max(doterr,abs(sum(a*b for a,b in zip(normal,tangent[:3]))))
 key=n['name'].rsplit('LOD',1)[0];groups.setdefault(key,[]).append(sha.hexdigest());witness.append({'node':n['name'],'actual_f32_position_normal_tangent_index_sha256':sha.hexdigest()})
assert all(len(v)==3 and len(set(v))==1 for v in groups.values())
assert normalzero==0 and normalerr<1e-5 and doterr<1e-5
result={'status':'VARIABLE_SHOULDER_AND_EXACT_C0_UPPER_SEAM_CPU_PASS_NOT_VISUAL_ACCEPTANCE','sections':sections,'seam_matches':matches,'all_3lod_actual_f32_positions_normals_tangents_indices_identical':True,'lod_attributes':witness,'normal_length_max_error':normalerr,'zero_normals':normalzero,'normal_tangent_dot_max':doterr,'caps':'All v13 jaw terminal caps retained. Exact C0 material edge matching is proven; no deletion or full rim/jaw closed solid union/C1 claim.','unchanged_constraints':['I/B dual curves','nominal nose and mouth','cloth/slate','lower lining/deep floor'],'limitations':['actual static CCD new-profile recertification','rim seam directional tangent not C1 because v13 B(t) retained','lower deep shell/floor still visual FAIL','new normal/shoulder geometry requires actual GPU same views']};(root/'shoulder-seam-validity.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:result[k] for k in ('status','all_3lod_actual_f32_positions_normals_tangents_indices_identical','normal_length_max_error','normal_tangent_dot_max')}))
