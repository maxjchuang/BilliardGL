import json,math,collections,sys
from pathlib import Path
import generate_original_chinese_pocket_gray_20261013 as gen
import pocket_dual_contour_20261013 as dual
r=Path(sys.argv[1]);prev=Path('/Users/bytedance/Projects/github/BilliardGL-evidence/m4/20261013-original-chinese-gray-v13-freeze');x=json.loads((r/'actual-solid-triangles-cm.json').read_text());old=json.loads((prev/'actual-solid-triangles-cm.json').read_text());nodes={m['node']:m for m in x['solid_meshes_lod0']};oldnodes={m['node']:m for m in old['solid_meshes_lod0']};adj=collections.defaultdict(set);directions=collections.Counter();faces=[]
for m in nodes.values():
 if 'rail_' not in m['node'] and 'cushion_jaw' not in m['node']:continue
 for p in m['primitives']:
  ps=p['positions_world_cm'];ix=p['triangle_indices']
  for k in range(0,len(ix),3):
   vv=[tuple(ps[i]) for i in ix[k:k+3]];faces.append(vv)
   for a,b in zip(vv,vv[1:]+vv[:1]):adj[a].add(b);adj[b].add(a);directions[(a,b)]+=1
bad=sum(directions[(b,a)]!=count for (a,b),count in directions.items());assert bad==0
remaining=set(adj);components=[]
while remaining:
 first=next(iter(remaining));seen={first};queue=[first]
 while queue:
  for v in adj[queue.pop()]:
   if v not in seen:seen.add(v);queue.append(v)
 remaining-=seen;volume=0.
 for a,b,c in faces:
  if a not in seen:continue
  volume+=(a[0]*(b[1]*c[2]-b[2]*c[1])+a[1]*(b[2]*c[0]-b[0]*c[2])+a[2]*(b[0]*c[1]-b[1]*c[0]))/6
 components.append({'vertices':len(seen),'signed_volume_cm3':volume})
assert len(components)==6 and all(c['signed_volume_cm3']>0 for c in components)
contract=json.loads((r/'assets/shared-geometry-contract.canonical.json').read_text());gen.H=contract['cloth_y']/100;newset={tuple(v) for v in nodes['NODE_pocket_cushion_jaw_assembly_LOD0']['primitives'][0]['positions_world_cm']};oldset={tuple(v) for v in oldnodes['NODE_pocket_cushion_jaw_assembly_LOD0']['primitives'][0]['positions_world_cm']};skeleton=[]
for p in gen.pockets(contract):
 I,B=dual.controls(p)
 for sign in (-1,1):
  for i in range(25):
   t=i/24;q=.5-.5*math.cos(math.pi*t);nose=.0363+q*(.025-.0363);u,v=dual.bezier(I,t);point=tuple(gen.native_cm(z) for z in gen.world(p,sign*u,v,nose));assert point in newset and point in oldset;skeleton.append(point)
def rear_wall_triangles(node):
 out=collections.Counter()
 for p in node['primitives']:
  ps=p['positions_world_cm'];ix=p['triangle_indices']
  for i in range(0,len(ix),3):
   tri=tuple(tuple(ps[j]) for j in ix[i:i+3])
   if max(v[1] for v in tri)<=contract['cloth_y']+2.5001:out[tri]+=1
 return out
rearold=rear_wall_triangles(oldnodes['NODE_pocket_liner_rim_assembly_LOD0']);rearnew=rear_wall_triangles(nodes['NODE_pocket_liner_rim_assembly_LOD0']);assert rearnew==rearold
result={'status':'SCOPED_V14_CLOSED_OUTWARD_CUSHION_AND_FIXED_NOSE_LOWER_PASS','directed_inconsistent_edges':bad,'closed_components':components,'actual_fixed_native_cm_nose_points_verified_both_v13_v14':len(skeleton),'rear_wall_native_triangles_unchanged':sum(rearnew.values()),'lower_nodes_actual_bytes_unchanged':{name:nodes[name]['actual_triangle_sha256']==oldnodes[name]['actual_triangle_sha256'] for name in ['NODE_table_body_LOD0','NODE_table_felt_LOD0','NODE_pocket_inner_assembly_LOD0','NODE_pocket_void_assembly_LOD0','NODE_pocket_void_bottom_assembly_LOD0','NODE_pocket_liner_sidewall_assembly_LOD0']},'no_terminal_caps_removed':True,'limits':['does not claim all pocket components closed union','roof/rim contact seam C0 only','new actual motion and GPU not run']};assert all(result['lower_nodes_actual_bytes_unchanged'].values());(r/'closed-and-fixed-geometry.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
