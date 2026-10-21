#!/usr/bin/env python3
"""Independent structural candidate. Not approved, not a default or physics promotion."""
import argparse,base64,hashlib,json,math,shutil,struct
from pathlib import Path
import generate_phase2_modular_gltf as old
import pocket_dual_contour_20261013 as dual
import pocket_shoulder_profile_20261013 as shoulder

N=256
H=old.PLAY_SURFACE_Y_M
R=.028575

def area(poly):
 return sum(a[0]*b[1]-a[1]*b[0] for a,b in zip(poly,poly[1:]+poly[:1]))*.5

def clip(poly,a,b,inside):
 out=[]
 for p,q in zip(poly,poly[1:]+poly[:1]):
  dp=(b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]);dq=(b[0]-a[0])*(q[1]-a[1])-(b[1]-a[1])*(q[0]-a[0])
  if not inside:dp,dq=-dp,-dq
  if dp>=-1e-13:out.append(p)
  if (dp>1e-13 and dq< -1e-13) or (dp< -1e-13 and dq>1e-13):
   t=dp/(dp-dq);out.append((p[0]+t*(q[0]-p[0]),p[1]+t*(q[1]-p[1])))
 return out

def minus(pieces,cut):
 out=[];lo=(min(p[0] for p in cut),min(p[1] for p in cut));hi=(max(p[0] for p in cut),max(p[1] for p in cut))
 for p in pieces:
  if max(v[0] for v in p)<lo[0] or min(v[0] for v in p)>hi[0] or max(v[1] for v in p)<lo[1] or min(v[1] for v in p)>hi[1]:out.append(p);continue
  rem=p
  for a,b in zip(cut,cut[1:]+cut[:1]):
   ext=clip(rem,a,b,False)
   if len(ext)>=3 and abs(area(ext))>1e-13:out.append(ext)
   rem=clip(rem,a,b,True)
   if len(rem)<3 or abs(area(rem))<=1e-13:break
 return out

def plane(poly,y):return old._flat_top_polygon(poly,y)
def rect(x0,z0,x1,z1):return [(x0,z0),(x1,z0),(x1,z1),(x0,z1)]
def surface(base,cuts,y):
 for c in cuts:base=minus(base,c)
 return old.merge_geometries([plane(p,y) for p in base])

def world(p,u,v,y):
 x,z=p['anchor'];tx,tz=p['tangent'];ox,oz=p['outward'];return(x+tx*u+ox*v,H+y,z+tz*u+oz*v)

def ellipse(p,scale=1):
 a=p['half'];c=(p['front']+p['back'])/2;b=(p['back']-p['front'])/2;s=scale/math.cos(math.pi/N)
 return [(a*math.cos(2*math.pi*(i+.5)/N)*s,c+b*math.sin(2*math.pi*(i+.5)/N)*s) for i in range(N)]

def mapped_closed_liner_cut(p,scale=1):
 result=[(world(p,u,v,0)[0],world(p,u,v,0)[2]) for u,v in ellipse(p,scale)]
 if area(result)<0:result.reverse()
 return result

def open_cut_uv(p):
 front=ellipse(p)[N//2:]
 return front+[(front[-1][0],.25),(front[0][0],.25)]
def mapped_cut(p,scale=1):
 if scale!=1:raise ValueError('slate cut is not a scaled liner aperture')
 result=[(world(p,u,v,0)[0],world(p,u,v,0)[2]) for u,v in open_cut_uv(p)]
 if area(result)<0:result.reverse()
 return result
def slab_ray_end(p,u,start_v):
 values=[]
 for axis,limit in ((0,.681),(1,1.316)):
  a=p['anchor'][axis]+p['tangent'][axis]*u;o=p['outward'][axis]
  if abs(o)>1e-12:
   target=limit if o>0 else -limit;v=(target-a)/o
   if v>=start_v:values.append(v)
 if not values:raise ValueError('open notch side ray misses slab boundary')
 return min(values)
def slate_u_edge(p):
 front=ellipse(p)[N//2:]
 return [(front[0][0],slab_ray_end(p,front[0][0],front[0][1]))]+front+[(front[-1][0],slab_ray_end(p,front[-1][0],front[-1][1]))]
def open_inside_strip(first,second,p):
 g=old._smooth_strip_between_open_rings3d(first,second);c=world(p,0,(p['front']+p['back'])/2,0)
 if sum(n[0]*(v[0]-c[0])+n[2]*(v[2]-c[2]) for v,n in zip(g[0],g[1]))>0:g=reverse(g)
 return g
def native_cm(value):return struct.unpack('<f',struct.pack('<f',struct.unpack('<f',struct.pack('<f',value))[0]*100))[0]
def physical_kind(name):
 if 'table_felt' in name:return 0,'cloth_support'
 if 'pocket_inner' in name:return 1,'cloth_covered_slate'
 if 'table_body' in name:return 2,'cabinet_wood'
 if 'rail_' in name or 'cushion_jaw' in name:return 3,'cushion_rubber'
 if 'liner_rim' in name:return 4,'pocket_leather'
 if 'void_bottom' in name:return 6,'pocket_floor'
 return 5,'pocket_liner'
def physics_solid_binary(meshes,source_sha,geometry_sha):
 ordered=sorted(meshes,key=lambda m:m['node']);count=sum(len(m['triangle_indices'])//3 for m in ordered);data=bytearray(b'PCKTSOL1'+source_sha.encode()+geometry_sha.encode()+struct.pack('<II',len(ordered),count));records=[]
 for m in ordered:
  kind,material=physical_kind(m['node']);name=m['node'].encode();mat=material.encode();records.append({'node':m['node'],'physical_kind_id':kind,'physical_material':material,'vertex_count':len(m['positions_world_cm']),'triangle_count':len(m['triangle_indices'])//3});data.extend(struct.pack('<H',len(name))+name+struct.pack('<H',len(mat))+mat+struct.pack('<B',kind))
 tid=0
 for ni,m in enumerate(ordered):
  ix=m['triangle_indices'];ps=m['positions_world_cm']
  for k in range(0,len(ix),3):
   tri=[v for i in ix[k:k+3] for v in ps[i]];data.extend(struct.pack('<HI9d',ni,tid,*tri));tid+=1
 return bytes(data),records

def reverse(g):
 return(g[0],[tuple(-v for v in n) for n in g[1]],[t[:3]+(-t[3],) for t in g[2]],g[3],[v for k in range(0,len(g[4]),3) for v in (g[4][k],g[4][k+2],g[4][k+1])])
def strip(a,b):
 g=old._smooth_strip_between_closed_rings3d(a,b);cx=sum(p[0] for p in a)/len(a);cz=sum(p[2] for p in a)/len(a)
 if sum(n[0]*(p[0]-cx)+n[2]*(p[2]-cz) for p,n in zip(g[0][:len(a)],g[1][:len(a)]))>0:g=reverse(g)
 return g

def cushion(points,outward,thickness=.046,heights=None,backs=None,bank_trim=False,caps=(True,True)):
 # Rounded cloth-covered nose. Its innermost point remains the canonical nose line.
 rings=[];profiles=[];lengths=[0.0]
 for i in range(1,len(points)):lengths.append(lengths[-1]+math.hypot(points[i][0]-points[i-1][0],points[i][1]-points[i-1][1]))
 for i,(x,z) in enumerate(points):
  before=points[max(0,i-1)];after=points[min(len(points)-1,i+1)];dx,dz=after[0]-before[0],after[1]-before[1];l=math.hypot(dx,dz);ox,oz=-dz/l,dx/l
  if ox*outward[0]+oz*outward[1]<0:ox,oz=-ox,-oz
  width=thickness[i] if isinstance(thickness,list) else thickness;nose,roof,base=heights[i] if heights else (.0363,.045,.004)
  profile,_=shoulder.profile(width,nose,roof,base)
  if backs is not None:
   ox,oz=(backs[i][0]-x)/width,(backs[i][1]-z)/width
  ring=[(x+ox*d,H+h,z+oz*d) for d,h in profile]
  if bank_trim:
   other=points[1-i];ux,uz=other[0]-x,other[1]-z;ll=math.hypot(ux,uz);ring=[(v[0]+ux/ll*.020*d/width,v[1],v[2]+uz/ll*.020*d/width) for v,(d,h) in zip(ring,profile)]
  rings.append(ring);profiles.append(profile)
 quads=[];count=len(rings[0])
 for section,(a,b) in enumerate(zip(rings,rings[1:])):
  center=tuple(sum(v[k] for v in a+b)/(count*2) for k in range(3))
  perimeters=[]
  for profile in (profiles[section],profiles[section+1]):
   distance=[0.0]
   for j in range(1,count):distance.append(distance[-1]+math.hypot(profile[j][0]-profile[j-1][0],profile[j][1]-profile[j-1][1]))
   perimeters.append(distance)
  for j in range(count):
   k=(j+1)%count;corners=(a[j],b[j],b[k],a[k]);g=old._quad_geometry(corners);face=tuple(sum(v[k0] for v in corners)/4 for k0 in range(3))
   if sum(g[1][0][k0]*(face[k0]-center[k0]) for k0 in range(3))<0:g=reverse(g)
   normals=list(g[1])
   uvs=[(lengths[section]/2.82,perimeters[0][j]/2.54),(lengths[section+1]/2.82,perimeters[1][j]/2.54),(lengths[section+1]/2.82,perimeters[1][k]/2.54),(lengths[section]/2.82,perimeters[0][k]/2.54)]
   tangents=[old._orthogonal_tangent(n,t[:3]) for n,t in zip(normals,g[2])]
   quads.append((g[0],normals,tangents,uvs,g[4]))
 # Geometric averaged normals on the nose arc and cubic shoulder of actual bounded surfaces.
 if backs is not None or bank_trim:
  sums={}
  for section in range(len(rings)-1):
   for j in range(2,count):
    vs=[rings[section][j],rings[section+1][j],rings[section+1][(j+1)%count],rings[section][(j+1)%count]];g=old._quad_geometry(vs);center=tuple(sum(v[k] for v in rings[section]+rings[section+1])/(count*2) for k in range(3));face=tuple(sum(v[k] for v in vs)/4 for k in range(3));n=g[1][0]
    if sum(n[k]*(face[k]-center[k]) for k in range(3))<0:n=tuple(-q for q in n)
    for v in vs:
     sums[v]=tuple(a+b for a,b in zip(sums.get(v,(0,0,0)),n))
  for qi,g in enumerate(quads):
   j=qi%count
   if 2<=j<count:
    normals=[old._normalise(sums[v]) for v in g[0]];quads[qi]=(g[0],normals,[old._orthogonal_tangent(n,t[:3]) for n,t in zip(normals,g[2])],g[3],g[4])
 for ri,near in ((0,1),(len(rings)-1,len(rings)-2)):
  if not caps[0 if ri==0 else 1]:continue
  ring=rings[ri];direction=tuple(sum(v[k] for v in ring)/count-sum(v[k] for v in rings[near])/count for k in range(3));normal=old._normalise(direction);indices=old._triangulate_simple_polygon(profiles[ri]);aa,bb,cc=[ring[j] for j in indices[:3]];u=tuple(bb[k]-aa[k] for k in range(3));v=tuple(cc[k]-aa[k] for k in range(3));cross=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
  if sum(cross[k]*normal[k] for k in range(3))<0:indices=[v for k in range(0,len(indices),3) for v in (indices[k],indices[k+2],indices[k+1])]
  tangent=old._orthogonal_tangent(normal,(1,0,0));uvs=[(d/max(v[0] for v in profiles[ri]),(h-min(v[1] for v in profiles[ri]))/max(.001,max(v[1] for v in profiles[ri])-min(v[1] for v in profiles[ri]))) for d,h in profiles[ri]]
  quads.append((list(ring),[normal]*count,[tangent]*count,uvs,indices))
 return old.merge_geometries(quads)

def pockets(contract):
 out=[];delta=.085/(2*math.sqrt(2))
 for pid,name,typ,anchor,inward,tangent,width in old.POCKETS:
  x,z=anchor
  if typ=='corner':x-=math.copysign(delta,x);z-=math.copysign(delta,z)
  cfg=contract[typ];an=contract['corner_mouth_anchors_xz'][pid] if typ=='corner' else contract['side_mouth_anchors_xz'][pid-4]
  out.append(dict(id=pid,type=typ,anchor=(an[0]/100,an[1]/100),tangent=tangent,outward=(-inward[0],-inward[1]),mouth=cfg['nose_mouth_width']/100,front=cfg['cloth_front_v']/100,back=cfg['cloth_back_v']/100,half=cfg['ellipse_halfwidth_u']/100))
 return out

def append_builder(doc,b):
 av=len(doc['accessors']);bv=len(doc['bufferViews']);mv=len(doc['meshes']);bi=len(doc['buffers'])
 for v in b.views:v['buffer']=bi
 for a in b.accessors:a['bufferView']+=bv
 for m in b.meshes:
  for p in m['primitives']:
   p['indices']+=av;p['attributes']={k:v+av for k,v in p['attributes'].items()}
 doc['bufferViews']+=b.views;doc['accessors']+=b.accessors;doc['meshes']+=b.meshes
 doc['buffers'].append({'byteLength':len(b.binary),'uri':'data:application/octet-stream;base64,'+base64.b64encode(b.binary).decode()})
 return mv

def generate(src,out,contract_path):
 global H
 contract=json.loads(contract_path.read_text());H=contract['cloth_y']/100
 if contract.get('pocket_geometry_id')!='edge_open_u_slate_20261013_v1':raise ValueError('v11 requires explicit edge-open support contract')
 source_sha=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
 contract['solid_parameters']={'slate_outer_bounds_xz_cm':[68.1,131.6],'slate_bevel_radius_cm':.35,'slate_thickness_cm':5,'rear_leather_top_h_cm':2.5,'rear_leather_bottom_h_cm':-5,'rear_leather_bottom_ellipse_expansion':1.10,'liner_mid_h_cm':-12,'liner_bottom_h_cm':-22,'pocket_floor_h_cm':-26,'cushion_nose_h_cm':3.63,'cushion_roof_h_cm':4.5,'cushion_base_h_cm':.4,'straight_cushion_width_cm':4.6,'cushion_nose_rounding_radius_max_cm':.3,'cushion_profile_arc_samples':13,'cloth_uv':'continuous bank length/perimeter in metres, not restarted at every quad','rear_leather_rim_outer_expansion':1.25,'rear_leather_rim_roll_steps':14,'wood_aperture_expansion':1.27,'jaw_terminal_base_h_cm':1.2,'jaw_endpoint':'I nose tangent aligned to rear-rim half-sample; fixed v13 B back curve remains C0 seam, height2.5/2.65/1.2cm','wood_outer_half_dimensions_cm':[77.5,141],'wood_cap_h_cm':4.4,'wood_edge_bevel_cm':.4,'support_full_circle_tangent_sample_count':N,'physical_kind_ids':{'cloth_support':0,'cloth_covered_slate':1,'cabinet_wood':2,'cushion_rubber':3,'pocket_leather':4,'pocket_liner':5,'pocket_floor':6},'jaw_topology':'bounded independent inner/back cubic loft; straight-bank back cross-section trimmed upstream2cm; no coincident internal caps','straight_bank_endpoint_back_trim_cm':2.0,'coordinate_quantization':'float32 metre positions then float32*100 cm, identical native importer'}
 contract['generator_binding']={'source_sha256':source_sha,'geometry_helpers_sha256':hashlib.sha256(Path(old.__file__).read_bytes()).hexdigest(),'dual_contour_helper_sha256':hashlib.sha256(Path(dual.__file__).read_bytes()).hexdigest(),'shoulder_helper_sha256':hashlib.sha256(Path(shoulder.__file__).read_bytes()).hexdigest()}
 contract['shoulder_profile_contract']={'schema':'local_variable_width_height_cubic_v1','samples':8,'normalized_control_depth_fractions':[shoulder.C1,shoulder.C2],'height_controls':'nose+local_radius twice, local roof twice; no fixed4.6cm width through tapered jaw','end_seam':'local d/width from upper half nose arc + cubic shoulder, same f32 positions on rear rim endpoint','caps':'existing closed cushion terminal retained; no deletion without union proof','bank':shoulder.profile(.046,.0363,.045,.004)[1],'corner_end':shoulder.profile(.0345*.25,.025,.0265,.012)[1],'side_end':shoulder.profile(.0365*.25,.025,.0265,.012)[1]}
 contract['jaw_dual_contour_parameters']=[dual.prove(p) for p in pockets(contract) if p['id'] in (0,4)]
 canonical=json.dumps(contract,sort_keys=True,separators=(',',':')).encode();contract_sha=hashlib.sha256(canonical).hexdigest()
 out.mkdir(parents=True,exist_ok=True)
 (out/'shared-geometry-contract.canonical.json').write_bytes(canonical)
 for f in src.iterdir():
  if f.is_file() and f.name not in (old.ASSET_NAME,old.MANIFEST_NAME):shutil.copy2(f,out/f.name)
 f=src/old.ASSET_NAME;doc=json.loads(f.read_text());b=old.Builder();mat={m['name']:i for i,m in enumerate(doc['materials'])};ps=pockets(contract);cuts=[mapped_cut(p) for p in ps]
 # Extend cloth over the actual slate under cushions, ending at the wood cap inner boundary.
 clothbase=[rect(-.681,-1.316,.681,1.316)]
 cloth=surface(clothbase,cuts,H)
 # Cabinet cap has no bare V-shaped external chute: cap follows independent wood inner rail margin.
 capbase=[rect(-.771,-1.406,.771,1.406)];capbase=minus(capbase,rect(-.681,-1.316,.681,1.316));bodycap=surface(capbase,[mapped_closed_liner_cut(p,1.27) for p in ps],H+.044)
 walls=[]
 for a,bb in zip(rect(-.775,-1.41,.775,1.41),rect(-.775,-1.41,.775,1.41)[1:]+rect(-.775,-1.41,.775,1.41)[:1]):
  walls.append(old._quad_geometry(((a[0],H-.17,a[1]),(bb[0],H-.17,bb[1]),(bb[0],H+.040,bb[1]),(a[0],H+.040,a[1]))))
 bevel=[];outer=rect(-.775,-1.41,.775,1.41);inner=rect(-.771,-1.406,.771,1.406)
 for i in range(4):
  j=(i+1)%4;g=old._quad_geometry(((inner[i][0],H+.044,inner[i][1]),(inner[j][0],H+.044,inner[j][1]),(outer[j][0],H+.040,outer[j][1]),(outer[i][0],H+.040,outer[i][1])))
  if g[1][0][1]<0:g=reverse(g)
  bevel.append(g)
 body=old.merge_geometries([bodycap,*walls,*bevel])
 jawparts=[];rimparts=[];linerparts=[];deepparts=[];wrapparts=[];bottomparts=[];boundary=[]
 for p in ps:
  uv=ellipse(p);expanded=ellipse(p,1.10);upper=[world(p,u,v,-.05) for u,v in expanded];mid=[world(p,u,v,-.12) for u,v in expanded];lower=[world(p,u,v,-.22) for u,v in expanded]
  linerparts.append(strip(upper,mid));deepparts.append(strip(mid,lower))
  # Cloth/slate wrap turns down from the true support curve; no hidden black plane over intact cloth.
  slate_uv=slate_u_edge(p);edge=[world(p,u,v,0) for u,v in slate_uv];prev=edge
  c=(p['front']+p['back'])/2;ba=(p['back']-p['front'])/2
  for k in range(1,9):
   theta=math.pi*.5*k/8;ring=[]
   for ei,(u,v) in enumerate(slate_uv):
    gu=u/(p['half']**2);gv=(v-c)/(ba**2) if ei not in (0,len(slate_uv)-1) else 0;l=math.hypot(gu,gv);d=.0035*(1-math.cos(theta));ring.append(world(p,u+d*gu/l,v+d*gv/l,-.0035*math.sin(theta)))
   wrapparts.append(open_inside_strip(prev,ring,p));prev=ring
  wrapparts.append(open_inside_strip(prev,[(x,H-.05,z) for x,y,z in prev],p))
  # rear half rim only: front rim remains cloth-covered slate rather than a leather barrier in the roll-in path.
  ids=[i for i,(u,v) in enumerate(uv) if v>=(p['front']+p['back'])/2]
  inner=[world(p,*uv[i],.025) for i in ids];outer=[world(p,uv[i][0]*1.25,(p['front']+p['back'])/2+(uv[i][1]-(p['front']+p['back'])/2)*1.25,.0265) for i in ids]
  # Shared upper seam: exact local terminal section maps across the rear-rim radial span.
  tailwidth=p['half']*.25;seam=shoulder.upper_seam(tailwidth);prev=inner
  for d,h in seam[1:]:
   fraction=d/tailwidth;ring=[(a[0]+(b[0]-a[0])*fraction,H+h,a[2]+(b[2]-a[2])*fraction) for a,b in zip(inner,outer)];rg=old._smooth_strip_between_open_rings3d(prev,ring)
   if sum(n[1] for n in rg[1])<0:rg=reverse(rg)
   rimparts.append(rg);prev=ring
  rear_bottom=[world(p,expanded[i][0],expanded[i][1],-.05) for i in ids]
  rimparts.append(open_inside_strip(inner,rear_bottom,p))
  # Inner nose is unchanged. Back contour is independently bounded, never an offset curve.
  inner_ctrl,back_ctrl=dual.controls(p)
  proof=dual.prove(p)
  for sign in (-1,1):
   pts=[];backs=[]
   for i in range(25):
    t=i/24;u,v=dual.bezier(inner_ctrl,t);bu,bv=dual.bezier(back_ctrl,t)
    w=world(p,sign*u,v,0);bw=world(p,sign*bu,bv,0);pts.append((w[0],w[2]));backs.append((bw[0],bw[2]))
   blend=[.5-.5*math.cos(math.pi*i/24) for i in range(25)];tailwidth=p['half']*.25
   jawparts.append(cushion(pts,(p['tangent'][0]*sign,p['tangent'][1]*sign),[.046+q*(tailwidth-.046) for q in blend],[(.0363+q*(.025-.0363),.045+q*(.0265-.045),.004+q*(.012-.004)) for q in blend],backs=backs,caps=(False,True)))
  bottomparts.append(plane(mapped_closed_liner_cut(p,1.10),H-.26))
  boundary.append({'id':p['id'],'type':p['type'],'anchor_world_cm':[p['anchor'][0]*100,H*100,p['anchor'][1]*100],'mouth_width_cm':p['mouth']*100,'support_boundary_world_cm':[[v*100 for v in w] for w in edge],'jaw_dual_contour_proof':proof,'curve':'edge-open front U tangent-exscribed half-ellipse plus slab-clipped outward rays, experimental','cloth_front_v_cm':p['front']*100,'cloth_back_v_cm':p['back']*100,'cloth_halfwidth_cm':p['half']*100})
 jaws=old.merge_geometries(jawparts);rim=old.merge_geometries(rimparts);liner=old.merge_geometries(linerparts);deep=old.merge_geometries(deepparts);wrap=old.merge_geometries(wrapparts);bottom=old.merge_geometries(bottomparts)
 # Six straight rails end exactly at the actual direction-change points; curved jaw sweep completes each terminal.
 cornergap=.085/math.sqrt(2);sidegap=.043
 railpaths=[([(-.635,-1.27+cornergap),(-.635,-sidegap)],(-1,0)),([(-.635,sidegap),(-.635,1.27-cornergap)],(-1,0)),([(.635,-1.27+cornergap),(.635,-sidegap)],(1,0)),([(.635,sidegap),(.635,1.27-cornergap)],(1,0)),([(-.635+cornergap,-1.27),(.635-cornergap,-1.27)],(0,-1)),([(-.635+cornergap,1.27),(.635-cornergap,1.27)],(0,1))]
 replacements={};triangles={};physics_meshes=[];shared_meshes={}
 for lod in range(3):
  geos={'NODE_table_body_LOD%d'%lod:(body,'MAT_table_walnut'),'NODE_table_felt_LOD%d'%lod:(cloth,'MAT_table_felt_teal'),'NODE_pocket_cushion_jaw_assembly_LOD%d'%lod:(jaws,'MAT_table_felt_teal'),'NODE_pocket_void_assembly_LOD%d'%lod:(deep,'MAT_pocket_liner'),'NODE_pocket_inner_assembly_LOD%d'%lod:(wrap,'MAT_table_felt_teal'),'NODE_pocket_void_bottom_assembly_LOD%d'%lod:(bottom,'MAT_pocket_void_depth'),'NODE_pocket_liner_rim_assembly_LOD%d'%lod:(rim,'MAT_pocket_leather'),'NODE_pocket_liner_sidewall_assembly_LOD%d'%lod:(liner,'MAT_pocket_liner')}
  for i,(pts,oxz) in enumerate(railpaths):geos['NODE_table_rail_%d_LOD%d'%(i,lod)]=(cushion(pts,oxz,bank_trim=True,caps=(False,False)),'MAT_table_felt_teal')
  for name,(g,m) in geos.items():
   if lod==0:
    quantized=[[native_cm(v) for v in p] for p in g[0]]
    physics_meshes.append({'node':name,'positions_world_cm':quantized,'triangle_indices':list(g[4]),'geometry_material':m})
   role=next((n.get('extras',{}).get('asset_role','candidate_geometry') for n in doc['nodes'] if n['name']==name),'candidate_geometry');key=name.rsplit('LOD',1)[0]
   if key not in shared_meshes:shared_meshes[key]=b.mesh('MESH_original_chinese_'+name,g,mat[m],role,0)
   replacements[name]=shared_meshes[key];triangles[name]=len(g[4])//3
 offset=append_builder(doc,b)
 for node in doc['nodes']:
  if node['name'] in replacements:
   node['mesh']=offset+replacements[node['name']];node.pop('translation',None);node.pop('rotation',None);node['extras']={k:v for k,v in node.get('extras',{}).items() if k in ['asset_role','lod']};node['extras'].update({'geometry_contract':'original_chinese_pocket_gray_20261013_v14','candidate_only':True,'production_physics_compatible':False,'shared_geometry_sha256':contract_sha})
  if node['name'].startswith('SOCKET_rail_nose_'):node['translation'][1]=H+.0363
  if node['name'].startswith('SOCKET_pocket_'):
   pid=int(node['name'].split('_')[2]);p=ps[pid];sign=(-1 if node['name'].endswith('neg_t') else 1 if node['name'].endswith('pos_t') else 0);node['translation']=list(world(p,sign*p['mouth']/2,0,0));node['extras']['geometry_contract']='original_chinese_pocket_gray_20261013_v14'
 # The frozen legacy LOD2 omitted its jaws. Add the missing semantic node explicitly.
 existing={n['name'] for n in doc['nodes']}
 for name,mi in replacements.items():
  if name not in existing:
   lod=int(name.rsplit('LOD',1)[1]);idx=len(doc['nodes']);doc['nodes'].append({'name':name,'mesh':offset+mi,'extras':{'lod':lod,'asset_role':'pocket_cushion_jaw_visual_only','geometry_contract':'original_chinese_pocket_gray_20261013_v14','shared_geometry_sha256':contract_sha,'candidate_only':True,'production_physics_compatible':False}})
   next(n for n in doc['nodes'] if n['name']=='NODE_table_LOD'+str(lod))['children'].append(idx)
 old_extras=doc['asset'].get('extras',{});keep_asset=['asset_id','authorship','ball_triangle_winding','external_source_assets','final_high_mesh','forward_axis','handedness','unit','up_axis']
 doc['asset']['generator']='generate_original_chinese_pocket_gray_20261013.py'
 doc['asset']['extras']={k:old_extras[k] for k in keep_asset if k in old_extras}
 marker_indices=[i for i,n in enumerate(doc['nodes']) if n.get('extras',{}).get('alignment_marker')]
 marker_hash=old.marker_binding_sha256(doc,marker_indices)
 doc['asset']['extras'].update({'candidate_only':True,'production_candidate':False,'physics_alignment_validated':False,'geometry_contract':'original_chinese_pocket_gray_20261013_v14','actual_marker_transform_sha256':marker_hash,'shared_geometry_sha256':contract_sha,'runtime_profile_binding':'NOT_BOUND_NO_PLAYABLE_CLAIM','pocket_material_sequence':['MAT_table_felt_teal','MAT_pocket_leather','MAT_pocket_liner','MAT_pocket_void_depth'],'pocket_surface_topology':'experimental_curved_cloth_support_bevel_and_static_solid_channel','factory_sizes_measured':False})
 for node in doc['nodes']:
  if node['name'].startswith('NODE_table_LOD'):
   node['extras'].pop('production_candidate',None);node['extras']['candidate_only']=True
 bin_bytes,node_records=physics_solid_binary(physics_meshes,source_sha,contract_sha);bin_sha=hashlib.sha256(bin_bytes).hexdigest();(out/'contract-generated-physics-solids.bin').write_bytes(bin_bytes)
 physics_payload={'schema':'contract_generated_static_solids_bin_v1','status':'EXPERIMENTAL_OPEN_SLATE_NOT_DEFAULT','geometry_contract_sha256':contract_sha,'shared_geometry_sha256':contract_sha,'geometry_contract_file':'shared-geometry-contract.canonical.json','generator_sha256':source_sha,'coordinate_units':'cm','quantization':'float32 metre then float32*100cm then float64 storage','parameters':contract['solid_parameters'],'binary':{'path':'contract-generated-physics-solids.bin','sha256':bin_sha,'format':'PCKTSOL1','endian':'little','layout':'magic8/sourceShaASCII64(full generator)/geometryShaASCII64(full canonical contract)/nodesU32/trianglesU32; node:nameLenU16+name/materialLenU16+physicalSemantic/featureU8; triangle:nodeU16/globalIDU32/9nativeCmF64'},'solid_meshes':node_records,'limits':['new core support and static collision contract required','rear wall is real leather/liner collision, not fabricated full-ring stone','do not reinterpret old profiles or call initialized fixture a player receipt']}
 physics_bytes=(json.dumps(physics_payload,separators=(',',':'))+'\n').encode();(out/'contract-generated-physics-solids.json').write_bytes(physics_bytes)
 doc.setdefault('extras',{})['contract_generated_physics_mesh_sha256']=hashlib.sha256(physics_bytes).hexdigest()
 doc.setdefault('extras',{})['candidate_status']='EXPERIMENTAL_GRAY_STRUCTURE_NOT_APPROVED_NOT_DEFAULT'
 # Share identical critical geometry across LOD nodes and remove replaced legacy meshes.
 used_meshes=sorted({n['mesh'] for n in doc['nodes'] if 'mesh' in n});remap={old_i:new_i for new_i,old_i in enumerate(used_meshes)};doc['meshes']=[doc['meshes'][i] for i in used_meshes]
 for node in doc['nodes']:
  if 'mesh' in node:node['mesh']=remap[node['mesh']]
 geometry_buffers=[]
 for i,bb in enumerate(doc['buffers']):
  uri=bb['uri'];data=base64.b64decode(uri.split(',',1)[1]) if uri.startswith('data:') else (src/uri).read_bytes();name='original-chinese-gray-buffer-%d.bin'%i;(out/name).write_bytes(data);bb['uri']=name;geometry_buffers.append({'path':name,'bytes':len(data),'sha256':'sha256:'+hashlib.sha256(data).hexdigest()})
 doc['extras']['geometry_buffer_sha256']=geometry_buffers
 payload=(json.dumps(doc,sort_keys=True,separators=(',',':'))+'\n').encode();(out/old.ASSET_NAME).write_bytes(payload)
 receipt={'status':'GENERATED_NOT_VISUALLY_ACCEPTED_NOT_PHYSICS_PROMOTED','source_sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'asset_sha256':hashlib.sha256(payload).hexdigest(),'shared_geometry_sha256':contract_sha,'shared_geometry_source':str(contract_path.resolve()),'contract_generated_physics_mesh_sha256':hashlib.sha256(physics_bytes).hexdigest(),'boundaries':boundary,'lod_boundary_policy':'all three support/jaw/slate boundaries identical; no LOD support decimation','triangle_counts':triangles,'not_closed':['full sphere audit','integrated production physics','visual reviews','bevel law','manufacturer dimensions not measured'],'candidate_geometry_schema':'original_chinese_pocket_gray_20261013_v14'}
 (out/'original-pocket-gray-contract.json').write_text(json.dumps(receipt,indent=2)+'\n')
 inherited=json.loads((src/old.MANIFEST_NAME).read_text())
 keep=['asset_id','authorship','schema_version','ball_atlas_contract','ball_geometry_contract','ball_material_variants','pbr_material_bindings','pbr_texture_sampling_contract','files','budgets','lod_sets']
 manifest={k:inherited[k] for k in keep}
 manifest['status']='EXPERIMENTAL_ORIGINAL_GRAY_NOT_VISUALLY_ACCEPTED_NOT_PHYSICS_PROMOTED'
 manifest['generator']={'path':str(Path(__file__).resolve()),'sha256':'sha256:'+hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'source_asset_sha256':receipt['source_sha256']}
 for key,name in [('gltf',old.ASSET_NAME),('pbr_parameters','focused_practice_hall_pbr_v1.params.json'),('lighting_rig','focused_practice_hall_lighting_rig_v1.json'),('ball_atlas_png',old.ATLAS_NAME)]:
  data=(out/name).read_bytes();manifest['files'][key]['path']=name;manifest['files'][key]['sha256']='sha256:'+hashlib.sha256(data).hexdigest();manifest['files'][key]['bytes']=len(data)
 manifest['files']['geometry_buffers']=geometry_buffers
 manifest['constraints']={'metallic_roughness_fallback_present':True,'normal_convention':'opengl_positive_y','basecolor_srgb_normal_orm_linear':True,'unpromoted_pocket_parameters_present':True,'pocket_leather_no_visual_solid_checked':False,'felt_index_bounds_winding_boundary_validated':False,'clearcoat_extensions_required':False}
 used={n['mesh'] for n in doc['nodes'] if 'mesh' in n};manifest['budgets']['unique_triangles']=sum(doc['accessors'][p['indices']]['count']//3 for i in used for p in doc['meshes'][i]['primitives']);manifest['budgets']['embedded_geometry_buffer_bytes']=0;manifest['budgets']['external_geometry_buffer_bytes']=sum(bb['byteLength'] for bb in doc['buffers'])
 def tree_triangles(index):
  node=doc['nodes'][index];count=sum(doc['accessors'][p['indices']]['count']//3 for p in doc['meshes'][node['mesh']]['primitives']) if 'mesh' in node else 0
  return count+sum(tree_triangles(i) for i in node.get('children',[]))
 for group in manifest['lod_sets'].values():
  for level,index in group['nodes'].items():group['triangles'][level]=tree_triangles(index)
 manifest['alignment_binding']={'status':'EXPERIMENTAL_NOT_APPROVED','geometry_id':receipt['candidate_geometry_schema'],'shared_geometry_sha256':contract_sha,'contract_generated_physics_mesh_sha256':receipt['contract_generated_physics_mesh_sha256'],'runtime_profile_id':'NOT_BOUND_NO_PLAYABLE_CLAIM','surface_y_cm':H*100,'mouth_widths_cm':{'corner':8.5,'side':8.6},'nose_height_ratio':.0363/(R*2),'visual_to_marker_frame':{'x_visual':'x_runtime','y_visual':'y_runtime','z_visual':'z_runtime'},'physical_alignment_validated':False,'actual_marker_transform_sha256':marker_hash}
 manifest['pocket_candidate']={'status':'STATIC_CANDIDATE_PHYSICAL_TRAJECTORY_FAIL','promotion':'NOT_PROMOTED','visual_acceptance':'NOT_RUN','physics_acceptance':'NOT_RUN_NEW_OPEN_SUPPORT_CONTRACT; old closed-stone-wall model failure retained','shape_contract':'edge-open-U-slate-and-rear-leather-liner-v1','size_provenance':'EXPERIMENTAL_NOT_FACTORY_MEASURED','removed_inherited_assertions':['legacy_opening_band_alignment','old_marker_hash','r25_pocket_v10b_design_pass','old_felt_topology','old_no_visual_solid_pass'],'retained_nonpocket_contract':'Unmodified ball/material/texture source files with SHA retained; not inherited pocket acceptance'}
 (out/old.MANIFEST_NAME).write_text(json.dumps(manifest,sort_keys=True,indent=2)+'\n')
 return receipt

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--source-root',type=Path,required=True);ap.add_argument('--output-root',type=Path,required=True);ap.add_argument('--contract',type=Path,required=True);a=ap.parse_args();r=generate(a.source_root,a.output_root,a.contract);print(json.dumps({k:r[k] for k in ['status','asset_sha256','triangle_counts']},indent=2))
