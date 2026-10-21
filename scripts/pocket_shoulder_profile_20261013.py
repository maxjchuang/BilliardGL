"""Variable-width/height cloth shoulder; all dimensions are metres; experimental."""
import math
SHOULDER_SAMPLES=8
C1=0.0035/0.043
C2=0.035/0.043

def profile(width,nose,roof,base):
 radius=min(.003,max(.0002,(roof-nose)*.5),max(.0002,(nose-base)*.5),width*.25)
 if not 0<radius<width or not base<nose-radius<nose+radius<=roof:raise ValueError('invalid local cushion section')
 result=[(width,roof),(width,base)]
 for j in range(13):
  angle=-math.pi/2+math.pi*j/12;result.append((radius*(1-math.cos(angle)),nose+radius*math.sin(angle)))
 start=nose+radius;span=width-radius;ctrl=[(radius,start),(radius+C1*span,start),(radius+C2*span,roof),(width,roof)]
 for i in range(1,SHOULDER_SAMPLES):
  t=i/SHOULDER_SAMPLES;w=((1-t)**3,3*(1-t)**2*t,3*(1-t)*t*t,t**3);result.append(tuple(sum(q[k]*z for q,z in zip(ctrl,w)) for k in (0,1)))
 return result,{'width_m':width,'nose_h_m':nose,'roof_h_m':roof,'base_h_m':base,'nose_radius_m':radius,'shoulder_controls_m':ctrl,'d_monotone_bernstein_differences':[ctrl[i+1][0]-ctrl[i][0] for i in range(3)],'height_monotone_bernstein_differences':[ctrl[i+1][1]-ctrl[i][1] for i in range(3)]}

def upper_seam(width=.008625):
 # nose point on canonical I(t), upper half circle, cubic shoulder, exact back endpoint.
 p,_=profile(width,.025,.0265,.012)
 return p[8:]+[p[0]]
