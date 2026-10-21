#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace billiardgl::render {

// Local, actual-geometry shadow projection. No table frustum/profile coupling.
struct SparsePropShadowProjection {
  using Point = std::array<float, 3>;
  using Matrix = std::array<float, 16>;
  bool active = false;
  Matrix viewProjection{};
  static constexpr int mapSize = 1024;
  // DEPTH24 may occupy four bytes per texel on the driver; budget conservatively.
  static constexpr std::size_t residentBytes = mapSize * mapSize * 4u;
};

inline SparsePropShadowProjection buildSparsePropShadowProjection(
    const SparsePropShadowProjection::Point& keyCenterCm,
    const SparsePropShadowProjection::Point& emissionNormal,
    float directKeyScale,
    const std::vector<SparsePropShadowProjection::Point>& casterWorldVerticesCm,
    float floorYCm = 0.0f) {
  using Point = SparsePropShadowProjection::Point;
  SparsePropShadowProjection result;
  if (!(directKeyScale > 0.0f) || !std::isfinite(directKeyScale) ||
      casterWorldVerticesCm.empty()) return result;
  const auto dot = [](const Point& a, const Point& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
  };
  const auto cross = [](const Point& a, const Point& b) -> Point {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
  };
  const auto unit = [&](Point a) {
    const float length = std::sqrt(dot(a,a));
    if (length > 0) for (auto& x : a) x /= length;
    return a;
  };
  for (float x : keyCenterCm) if (!std::isfinite(x)) return result;
  for (float x : emissionNormal) if (!std::isfinite(x)) return result;
  if (dot(emissionNormal,emissionNormal)<1e-8f ||
      !std::isfinite(floorYCm) || keyCenterCm[1]<=floorYCm+1.0f) return result;
  const Point f=unit(emissionNormal);
  const Point up=std::abs(f[1])>0.98f?Point{0,0,1}:Point{0,1,0};
  const Point s=unit(cross(f,up)), u=cross(s,f);
  float left=1e30f,right=-1e30f,bottom=1e30f,top=-1e30f,farDepth=0;
  const auto include = [&](const Point& p) {
    Point relative{p[0]-keyCenterCm[0],p[1]-keyCenterCm[1],p[2]-keyCenterCm[2]};
    const float depth=dot(relative,f);
    if (!(depth>1.0f)) return false;
    const float x=dot(relative,s)/depth,y=dot(relative,u)/depth;
    left=std::min(left,x);right=std::max(right,x);
    bottom=std::min(bottom,y);top=std::max(top,y);farDepth=std::max(farDepth,depth);
    return true;
  };
  for (const Point& p : casterWorldVerticesCm) {
    for (float x : p) if (!std::isfinite(x)) return result;
    // Behind emitting face, below floor, or at light height is unsupported,
    // not a reason to fabricate a projection or clamp a shadow into view.
    if (p[1]<floorYCm-0.01f || p[1]>=keyCenterCm[1]-1.0f || !include(p)) return result;
    const float t=(floorYCm-keyCenterCm[1])/(p[1]-keyCenterCm[1]);
    Point floorHit{keyCenterCm[0]+t*(p[0]-keyCenterCm[0]),floorYCm,
                   keyCenterCm[2]+t*(p[2]-keyCenterCm[2])};
    if (!include(floorHit)) return result;
  }
  // Fixed world-frustum guard, independent of camera and picture brightness.
  const float margin=0.01f;
  left-=margin;right+=margin;bottom-=margin;top+=margin;
  const float nearPlane=1.0f,farPlane=farDepth+10.0f;
  SparsePropShadowProjection::Matrix projection{};
  projection[0]=2.0f/(right-left);projection[5]=2.0f/(top-bottom);
  projection[8]=(right+left)/(right-left);projection[9]=(top+bottom)/(top-bottom);
  projection[10]=-(farPlane+nearPlane)/(farPlane-nearPlane);
  projection[11]=-1;projection[14]=-2*farPlane*nearPlane/(farPlane-nearPlane);
  SparsePropShadowProjection::Matrix view{s[0],u[0],-f[0],0,s[1],u[1],-f[1],0,
    s[2],u[2],-f[2],0,-dot(s,keyCenterCm),-dot(u,keyCenterCm),dot(f,keyCenterCm),1};
  for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k)
    result.viewProjection[c*4+r]+=projection[k*4+r]*view[c*4+k];
  result.active=true;
  return result;
}

// Same fail-lit clip policy as GPU; empty/disabled map must never darken floor.
inline bool sparsePropShadowContains(const SparsePropShadowProjection& projection,
                                     const SparsePropShadowProjection::Point& p) {
  if (!projection.active) return false;
  std::array<float,4> clip{};
  for(int r=0;r<4;++r)clip[r]=projection.viewProjection[r]*p[0]+
    projection.viewProjection[4+r]*p[1]+projection.viewProjection[8+r]*p[2]+
    projection.viewProjection[12+r];
  if (!(clip[3]>0)) return false;
  for(int r=0;r<3;++r)if(!std::isfinite(clip[r]) || std::abs(clip[r])>clip[3])return false;
  return true;
}

} // namespace billiardgl::render
