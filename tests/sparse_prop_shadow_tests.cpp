#include "render/modern/sparse_prop_shadow.h"
#include <cassert>
#include <iostream>

int main() {
  using namespace billiardgl::render;
  using Point=SparsePropShadowProjection::Point;
  const Point key{0,300,0}, direction{0,-1,0};
  const std::vector<Point> realBox{{160,0,90},{200,0,90},{160,80,90},{200,80,90},
                                 {160,0,130},{200,0,130},{160,80,130},{200,80,130}};
  assert(!buildSparsePropShadowProjection(key,direction,1,{}).active);
  assert(!buildSparsePropShadowProjection(key,direction,0,realBox).active);
  assert(!buildSparsePropShadowProjection(key,Point{0,1,0},1,realBox).active);
  const auto projection=buildSparsePropShadowProjection(key,direction,1,realBox);
  assert(projection.active);
  for (const auto& p:realBox) {
    assert(sparsePropShadowContains(projection,p));
    const float t=300/(300-p[1]);
    assert(sparsePropShadowContains(projection,Point{t*p[0],0,t*p[2]}));
  }
  assert(!sparsePropShadowContains(projection,Point{-10000,0,0}));
  assert(!sparsePropShadowContains(projection,Point{0,400,0}));
  assert(!sparsePropShadowContains({},Point{160,0,90}));
  const auto shifted=buildSparsePropShadowProjection(Point{20,300,0},direction,1,realBox);
  assert(shifted.active && shifted.viewProjection!=projection.viewProjection);
  assert(SparsePropShadowProjection::residentBytes==4194304);
  // Frozen sparse candidate 4725de6...: real bench/shelf world bounds cm;
  // same actual authored table-key position, not an invented furniture lamp.
  std::vector<Point> candidateVertices;
  for(const auto& box:std::vector<std::array<Point,2>>{
        {Point{50,0,-470},Point{210,85,-430}},
        {Point{-220,0,-480},Point{-140,72,-450}}})
    for(int x=0;x<2;++x)for(int y=0;y<2;++y)for(int z=0;z<2;++z)
      candidateVertices.push_back(Point{box[x][0],box[y][1],box[z][2]});
  const Point actualKey{0,221.483976f,0};
  const auto candidate=buildSparsePropShadowProjection(actualKey,direction,1,candidateVertices);
  assert(candidate.active);
  for(const auto& p:candidateVertices) {
    assert(sparsePropShadowContains(candidate,p));
    const float t=actualKey[1]/(actualKey[1]-p[1]);
    assert(sparsePropShadowContains(candidate,Point{p[0]*t,0,p[2]*t}));
  }
  assert(!buildSparsePropShadowProjection(actualKey,direction,0,candidateVertices).active);
  std::cout << "sparse_prop_shadow: empty/keyoff/behind/outside/floor-rays/moved-key PASS\n";
}
