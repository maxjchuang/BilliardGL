"""Actual f32cm mesh topology plus contract polynomial loft validity; no runtime/GPU."""
import json,math,sys,collections
from pathlib import Path
import generate_original_chinese_pocket_gray_20261013 as gen
import pocket_dual_contour_20261013 as dual
root=Path(sys.argv[1]);assets=root/'assets';export=json.loads((root/'actual-solid-triangles-cm.json').read_text());nodes={m['node']:m for m in export['solid_meshes_lod0']};contract=json.loads((assets/'shared-geometry-contract.canonical.json').read_text());gen.H=contract['cloth_y']/100;ps=gen.pockets(contract);jaw=nodes['NODE_pocket_cushion_jaw_assembly_LOD0']['primitives'][0]['positions_world_cm'];assert len(jaw)==12*1455
rows=[]
def det(a,b):return a[0]*b[1]-a[1]*b[0]
def sub(a,b):return(a[0]-b[0],a[1]-b[1])
def local(p,w):
 x,z=w[0]-p['anchor'][0]*100,w[2]-p['anchor'][1]*100
 return(x*p['tangent'][0]+z*p['tangent'][1],x*p['outward'][0]+z*p['outward'][1])
def quantpoint(p,q):return local(p,[gen.native_cm(v) for v in gen.world(p,*q,0)])
def cross_segments(a,b,c,d):
 return det(sub(b,a),sub(c,a))*det(sub(b,a),sub(d,a))<0 and det(sub(d,c),sub(a,c))*det(sub(d,c),sub(b,c))<0
for pi,p in enumerate(ps):
 for si,sign in enumerate((-1,1)):
  vertices=jaw[(pi*2+si)*1455:(pi*2+si+1)*1455];gaps=[]
  for w in vertices:
   uu,v=local(p,w);u=sign*uu;a=p['mouth']*50
   gap=a+.020/.046*v-u if p['type']=='side' else (.020/.046*(u+v-a)-(u-v-a))/math.sqrt(2);gaps.append(gap)
  if min(gaps)<-1e-3:raise ValueError('actual jaw overlaps upstream bank '+str(min(gaps)))
  I,B=dual.controls(p);inner=[quantpoint(p,dual.bezier(I,i/24)) for i in range(25)];back=[quantpoint(p,dual.bezier(B,i/24)) for i in range(25)];jac=[]
  for i in range(24):
   for derivative in (sub(inner[i+1],inner[i]),sub(back[i+1],back[i])):
    for dd in (sub(back[i],inner[i]),sub(back[i+1],inner[i+1])):jac.append(-det(derivative,dd))
  if min(jac)<=0:raise ValueError('actual tessellation fold')
  boundary=inner+back[::-1];edges=list(zip(boundary,boundary[1:]+boundary[:1]));crosses=[]
  for i,(a,b) in enumerate(edges):
   for j,(c,d) in enumerate(edges):
    if j<=i+1 or (i==0 and j==len(edges)-1):continue
    if cross_segments(a,b,c,d):crosses.append([i,j])
  if crosses:raise ValueError('actual patch boundary self intersection')
  rows.append({'pocket':p['id'],'arm_sign':sign,'actual_min_bank_separation_cm':min(gaps),'bank_quantization_tolerance_cm':1e-3,'actual_discrete_loft_signed_jacobian_min_cm2':min(jac),'actual_projected_boundary_proper_crossings':crosses,'continuous_polynomial_proof':dual.prove(p)})
result={'status':'BOUNDED_JAW_SCOPE_VALIDATED_NOT_FULL_POCKET_ACCEPTANCE','scope':'unchanged nominal inner nose; independent bounded back; bank 2cm back-end cut; exact joined actual mesh','arms':rows,'limits':['float32 bank join separation permits <=1e-3cm quantization, exact coordinate edge weld tested separately','solid proof uses positive continuous t/s map Jacobian, simple discrete boundary and simple ordered section profiles; not a full unrelated-node triangle intersection test','lower rear liner/floor unmodified and visual mismatch unresolved','new profile motion and actual GPU still NOT_RUN']};(root/'bounded-jaw-validity.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'arms':len(rows),'min_actual_bank_gap_cm':min(r['actual_min_bank_separation_cm'] for r in rows),'min_actual_discrete_jacobian_cm2':min(r['actual_discrete_loft_signed_jacobian_min_cm2'] for r in rows)}))
