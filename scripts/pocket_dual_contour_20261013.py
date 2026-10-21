"""Bounded jaw loft: CPU polynomial proofs and explicit independent back contour."""
import math

def bezier(c,t):return tuple(sum(c[i][k]*((1-t)**3,3*(1-t)**2*t,3*(1-t)*t*t,t**3)[i] for i in range(4)) for k in range(2))
def controls(p):
 a=p['mouth']/2;hh=p['half'];c=(p['front']+p['back'])/2;b=(p['back']-p['front'])/2;tail=c+b*math.tan(math.pi/256);slope=hh/b*math.tan(math.pi/256)
 inner=[(a,0),(a-.014,.014) if p['type']=='corner' else (a-.006,0),(hh+.010*slope,tail-.010),(hh,tail)]
 start=(a+(.046+.020)/math.sqrt(2),(.046-.020)/math.sqrt(2)) if p['type']=='corner' else (a+.020,.046)
 end=(hh*1.25,c+(tail-c)*1.25)
 if p['type']=='side':back=[start,(a+.017,.046),(hh+.0175,.034),end]
 else:back=[start,(start[0]+.65*(end[0]-start[0]),start[1]+.35*(end[1]-start[1])),(end[0]+.25*(start[0]-end[0]),end[1]-.015),end]
 return inner,back

def power(c,k):
 a,b,d,e=[p[k] for p in c];return [a,3*(b-a),3*(a-2*b+d),-a+3*b-3*d+e]
def sub(a,b):return [(a[i] if i<len(a) else 0)-(b[i] if i<len(b) else 0) for i in range(max(len(a),len(b)))]
def mul(a,b):
 out=[0.]*(len(a)+len(b)-1)
 for i,x in enumerate(a):
  for j,y in enumerate(b):out[i+j]+=x*y
 return out

def bernstein(p):
 n=len(p)-1;return [sum(p[i]*math.comb(k,i)/math.comb(n,i) for i in range(k+1)) for k in range(n+1)]
def certify(coeff,threshold=1e-12,depth=0):
 if min(coeff)>threshold:return {'minimum_bound':min(coeff),'leaves':1,'max_depth':depth}
 if depth>=24:raise ValueError('signed polynomial proof failed '+str(coeff))
 rows=[coeff]
 while len(rows[-1])>1:rows.append([(a+b)/2 for a,b in zip(rows[-1],rows[-1][1:])])
 l=certify([r[0] for r in rows],threshold,depth+1);r=certify([v[-1] for v in rows[::-1]],threshold,depth+1)
 return {'minimum_bound':min(l['minimum_bound'],r['minimum_bound']),'leaves':l['leaves']+r['leaves'],'max_depth':max(l['max_depth'],r['max_depth'])}

def prove(p):
 I,B=controls(p);ix,iy=power(I,0),power(I,1);bx,by=power(B,0),power(B,1);dx,dy=sub(bx,ix),sub(by,iy);proof=[]
 for x,y in [(ix,iy),(bx,by)]:
  xp=[i*v for i,v in enumerate(x)][1:];yp=[i*v for i,v in enumerate(y)][1:];j=sub(mul(yp,dx),mul(xp,dy));proof.append(certify(bernstein(j)))
 # Linear bank separating plane applied to each cubic control is a Bernstein certificate.
 def bank_gap(q):
  u,v=q;a=p['mouth']/2
  return a+.020/.046*v-u if p['type']=='side' else (.020/.046*(u+v-a)-(u-v-a))/math.sqrt(2)
 gaps=[[bank_gap(q) for q in curve] for curve in (I,B)]
 if min(v for row in gaps for v in row)<-1e-12:raise ValueError('upstream bank overlap')
 if min(B[i][0]-I[j][0] for i in range(4) for j in range(4))<=0:raise ValueError('contour separation not certified')
 if p['type']=='side' and any(curve[i+1][0]>=curve[i][0] for curve in (I,B) for i in range(3)):raise ValueError('nonmonotone side contour')
 if p['type']=='corner' and any(curve[i+1][1]<=curve[i][1] for curve in (I,B) for i in range(3)):raise ValueError('nonmonotone corner contour')
 return {'type':p['type'],'inner_controls_m':I,'back_controls_m':B,'signed_jacobian_endpoint_polynomial_proofs':proof,'signed_jacobian_unit':'m2','bank_gap_bernstein_controls_m':gaps,'bank_halfspace':'bank upstream >= 0.020/0.046 * outward depth; jaw remains opposite halfspace, shared t=0 only','boundary_simple_certificate':'side strictly decreasing u, corner strictly increasing v; disjoint inner/back u ranges; endpoint joining segments checked in actual polygon audit','back_contour_separation_min_m':min(B[i][0]-I[j][0] for i in range(4) for j in range(4))}
