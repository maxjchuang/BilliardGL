#import <Cocoa/Cocoa.h>
#include <mach-o/dyld.h>
#include <CommonCrypto/CommonDigest.h>
#include <OpenGL/gl3.h>

#include "presentation_snapshot.h"
#include "table_specs.h"
#include "render/modern/camera_projection.h"
#include "render/modern/modern_renderer.h"
#include "render/gltf/gltf_importer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string phase2AssetRoot()
{
    if(const char* overrideRoot=std::getenv("BILLIARDGL_MODERN_PHASE2_ROOT"))
        return std::string(overrideRoot)+"/";
    return std::string(BILLIARDGL_SOURCE_ROOT)+"/assets/modern/phase2/";
}

struct PpmImage { int width=0,height=0;std::vector<unsigned char> pixels; };

class SeamlessCubemapTestStateGuard {
public:
    SeamlessCubemapTestStateGuard()
        : enabled_(glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) == GL_TRUE)
    {
        if (const char* value =
                std::getenv("BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC")) {
            hadEnvironmentValue_ = true;
            environmentValue_ = value;
        }
    }

    ~SeamlessCubemapTestStateGuard()
    {
        if (hadEnvironmentValue_)
            setenv("BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC",
                   environmentValue_.c_str(), 1);
        else
            unsetenv("BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC");
        if (enabled_)
            glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        else
            glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }

private:
    bool enabled_ = false;
    bool hadEnvironmentValue_ = false;
    std::string environmentValue_;
};

PpmImage readPpm(const std::filesystem::path& path)
{
    std::ifstream input(path,std::ios::binary);std::string magic;int maximum=0;PpmImage image;
    input>>magic>>image.width>>image.height>>maximum;input.get();
    if(magic!="P6"||image.width<=0||image.height<=0||maximum!=255)throw std::runtime_error("invalid PPM: "+path.string());
    image.pixels.resize(static_cast<std::size_t>(image.width*image.height*3));
    input.read(reinterpret_cast<char*>(image.pixels.data()),static_cast<std::streamsize>(image.pixels.size()));
    if(!input)throw std::runtime_error("truncated PPM: "+path.string());
    return image;
}

billiardgl::PresentationSnapshot breakSnapshot()
{
    billiardgl::PresentationSnapshot snapshot;
    snapshot.camera.eye = {0.0f, 132.0f, 125.0f};
    snapshot.camera.target = {0.0f, 86.5f, -25.0f};
    snapshot.camera.fieldOfViewDegrees = 30.0f;
    snapshot.camera.viewportWidth = 1920;
    snapshot.camera.viewportHeight = 1080;
    snapshot.hud.viewportWidth = 1920;
    snapshot.hud.viewportHeight = 1080;
    snapshot.hud.shotPower = 105.0f;
    snapshot.cue.visible = false;
    snapshot.cue.referencePosition = {0.0f, 89.34f, 72.0f};
    snapshot.cue.direction = {0.0f, 0.0f, -1.0f};
    snapshot.balls[0].identity = 0;
    snapshot.balls[0].position = {0.0f, 89.34f, 45.0f};
    snapshot.balls[0].visible = snapshot.balls[0].castsShadow = true;
    int index = 1;
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column <= row; ++column) {
            auto& ball = snapshot.balls[static_cast<std::size_t>(index)];
            ball.identity = index;
            ball.position = {(column - row * 0.5f) * 5.9f, 89.34f, -45.0f - row * 5.15f};
            ball.rotationAxis = {0.0f, 1.0f, 0.0f};
            ball.rotationAngleDegrees = static_cast<float>(index * 17);
            ball.visible = ball.castsShadow = true;
            ++index;
        }
    }
    return snapshot;
}

billiardgl::PresentationSnapshot aimSnapshot()
{
    auto snapshot = breakSnapshot();
    snapshot.hud.aimMode = true;
    snapshot.hud.shotPower = 72.0f;
    snapshot.cue.showGuideLine = true;
    snapshot.cue.visible = true;
    snapshot.cue.referencePosition=snapshot.balls[0].position;
    snapshot.cue.modelOriginBackoffCm=9.5f;
    snapshot.cue.guideStart={0.0f,0.0f,-7.0f};
    // Aim at the authored right-hand side pocket from the cue-ball position.
    // The camera sits behind the same shot vector, so this remains an honest
    // ShotControl direction instead of a screen-space composition cheat.
    snapshot.cue.direction = {0.816f, 0.0f, -0.578f};
    snapshot.cue.guideEnd={63.5f,0.0f,0.0f};
    for(int index=1;index<16;++index){
        snapshot.balls[static_cast<std::size_t>(index)].position.x+=75.0f;
        snapshot.balls[static_cast<std::size_t>(index)].position.z+=30.0f;
    }
    // A modest lateral offset reveals the shaft beside the projected ball;
    // the look target stays near the cue ball so the real side pocket lands in
    // the frozen upper target-pocket region.
    snapshot.camera.eye = {-32.0f, 104.0f, 75.0f};
    snapshot.camera.target = {27.0f, 80.0f, 22.5f};
    snapshot.camera.fieldOfViewDegrees = 25.0f;
    return snapshot;
}

billiardgl::PresentationSnapshot atlasCloseupSnapshot(float rotationOffset)
{
    auto snapshot = breakSnapshot();
    snapshot.hud.aimMode = false;
    snapshot.cue.visible = false;
    snapshot.cue.direction = {0.0f, 0.0f, -1.0f};
    for(auto& ball:snapshot.balls)ball.visible=false;
    const int identities[4]={1,8,11,15};
    for (int slot = 0; slot < 4; ++slot) {
        const int identity=identities[slot];
        auto& ball = snapshot.balls[static_cast<std::size_t>(identity)];
        ball.identity = identity;
        ball.position = {-9.0f+static_cast<float>(slot)*6.0f,89.34f,0.0f};
        ball.rotationAxis={0.0f,1.0f,0.0f};
        ball.rotationAngleDegrees = rotationOffset;
        ball.visible = true;
        ball.castsShadow = true;
    }
    snapshot.camera.eye={0.0f,108.0f,55.0f};
    snapshot.camera.target={0.0f,89.34f,0.0f};
    snapshot.camera.fieldOfViewDegrees=25.0f;
    return snapshot;
}

billiardgl::PresentationSnapshot heroBallSnapshot()
{
    auto snapshot=breakSnapshot();
    snapshot.hud.aimMode=false;
    snapshot.cue.visible=false;
    for(auto& ball:snapshot.balls)ball.visible=false;
    const int identities[4]={1,8,11,15};
    const float positions[4][2]={{-2.7f,1.0f},{2.7f,1.0f},{-5.8f,-6.5f},{5.8f,-6.5f}};
    for(int slot=0;slot<4;++slot){
        auto& hero=snapshot.balls[static_cast<std::size_t>(identities[slot])];
        hero.identity=identities[slot];
        hero.position={positions[slot][0],89.34f,positions[slot][1]};
        hero.rotationAxis={0.0f,1.0f,0.0f};
        hero.rotationAngleDegrees=0.0f;
        hero.visible=hero.castsShadow=true;
    }
    snapshot.camera.eye={0.0f,112.0f,48.0f};
    snapshot.camera.target={0.0f,89.0f,0.0f};
    snapshot.camera.fieldOfViewDegrees=25.0f;
    return snapshot;
}

billiardgl::PresentationSnapshot pocketCloseupSnapshot()
{
    auto snapshot=breakSnapshot();
    snapshot.cue.visible=false;
    for(auto& ball:snapshot.balls)ball.visible=false;
    const int pocketBalls[6]={1,3,5,8,11,15};
    const float pocketPositions[6][2]={{43.0f,91.0f},{34.0f,89.0f},{25.0f,87.0f},{40.0f,82.0f},{31.0f,80.0f},{22.0f,78.0f}};
    for(int slot=0;slot<6;++slot){
        auto& ball=snapshot.balls[static_cast<std::size_t>(pocketBalls[slot])];
        ball.identity=pocketBalls[slot];ball.position={pocketPositions[slot][0],89.34f,pocketPositions[slot][1]};
        ball.visible=ball.castsShadow=true;
    }
    // Validation-only equivalent of the public-camera pocket composition used
    // by capture_product_acceptance.py.  The previous 30-degree close-up cut
    // the outer half of the near cavity at the bottom edge, so it could not
    // substantiate jaw/cavity alignment.  This wider, backed-off view changes
    // no product camera defaults and keeps the neighbouring balls as scale.
    snapshot.camera.eye={101.91410f,179.36365f,168.97122f};
    snapshot.camera.target={54.0f,86.48398f,110.0f};
    snapshot.camera.fieldOfViewDegrees=48.0f;
    return snapshot;
}

// Additional TA views only. The frozen B4x pocketCloseupSnapshot remains
// untouched and is still selected when the review ID is absent.
billiardgl::PresentationSnapshot pocketReviewSnapshot(int pocketId)
{
    auto snapshot=pocketCloseupSnapshot();
    const float signX=(pocketId==0||pocketId==2||pocketId==4)?-1.0f:1.0f;
    const float signZ=pocketId<2?-1.0f:1.0f;
    if(pocketId<4){
        snapshot.camera.eye.x*=signX;snapshot.camera.eye.z*=signZ;
        snapshot.camera.target.x*=signX;snapshot.camera.target.z*=signZ;
        for(auto& ball:snapshot.balls){ball.position.x*=signX;ball.position.z*=signZ;}
    }else{
        // Steeper supplemental view reveals the side-pocket floor behind the
        // near wall; the original shallower view remains in its old receipt.
        snapshot.camera.eye={100.0f*signX,190.0f,25.0f};
        snapshot.camera.target={58.0f*signX,82.0f,0.0f};
        const int identities[6]={1,3,5,8,11,15};
        for(int slot=0;slot<6;++slot)
            snapshot.balls[identities[slot]].position={35.0f*signX,89.34f,-25.0f+9.0f*slot};
    }
    return snapshot;
}

void writeRuntimeCamera(std::ostream& output,const billiardgl::PresentationSnapshot& snapshot)
{
    const auto& camera=snapshot.camera;
    output<<"{\"eye_cm\":["<<camera.eye.x<<","<<camera.eye.y<<","<<camera.eye.z
          <<"],\"target_cm\":["<<camera.target.x<<","<<camera.target.y<<","<<camera.target.z
          <<"],\"vertical_fov_degrees\":"<<camera.fieldOfViewDegrees
          <<",\"viewport_pixels\":["<<camera.viewportWidth<<","<<camera.viewportHeight<<"]}";
}

billiardgl::PresentationSnapshot tacticalOverheadSnapshot()
{
    auto snapshot=breakSnapshot();
    snapshot.cue.visible=false;
    for(int index=1;index<16;++index){
        const int slot=index-1,row=slot/5,column=slot%5;
        snapshot.balls[static_cast<std::size_t>(index)].position={
            -30.0f+static_cast<float>(column)*15.0f,89.34f,-52.0f+static_cast<float>(row)*42.0f};
    }
    snapshot.camera.eye={0.0f,345.0f,0.0f};
    snapshot.camera.target={0.0f,86.48f,0.0f};
    snapshot.camera.fieldOfViewDegrees=42.0f;
    if(const char* full=std::getenv("BILLIARDGL_TACTICAL_FULL_TABLE_REVIEW");
       full&&std::string(full)=="1"){
        snapshot.camera.eye={0.0f,500.0f,0.0f};
        snapshot.camera.target={0.0f,billiardgl::kTableModelPlaySurfaceHeightCm,0.0f};
    }
    snapshot.cue.visible=true;
    snapshot.cue.showGuideLine=true;
    snapshot.cue.referencePosition=snapshot.balls[0].position;
    snapshot.cue.direction={0.816f,0.0f,-0.578f};
    snapshot.cue.guideEnd={63.5f,0.0f,0.0f};
    return snapshot;
}

billiardgl::PresentationSnapshot postShotObserveSnapshot()
{
    auto snapshot=breakSnapshot();
    snapshot.cue.visible=false;
    snapshot.balls[0].position={0.0f,89.34f,5.0f};
    for(int index=1;index<16;++index){
        auto& ball=snapshot.balls[static_cast<std::size_t>(index)];
        ball.position.x += static_cast<float>((index%5)-2)*7.5f;
        ball.position.z -= static_cast<float>((index%3)+1)*8.0f;
    }
    snapshot.camera.eye={-130.0f,165.0f,135.0f};
    snapshot.camera.target={0.0f,86.48f,-10.0f};
    snapshot.camera.fieldOfViewDegrees=25.0f;
    return snapshot;
}

std::uint64_t checksum(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing capture: " + path.string());
    std::uint64_t hash = 1469598103934665603ull;
    char value = 0;
    while (input.get(value)) {
        hash ^= static_cast<unsigned char>(value);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::size_t changedPixelCount(const std::filesystem::path& first,
                              const std::filesystem::path& second)
{
    const auto a=readPpm(first),b=readPpm(second);
    if(a.width!=b.width||a.height!=b.height)
        throw std::runtime_error("shadow receiver comparison dimensions differ");
    std::size_t changed=0;
    for(std::size_t pixel=0;pixel<a.pixels.size()/3;++pixel){
        const std::size_t offset=pixel*3;
        if(a.pixels[offset]!=b.pixels[offset]
           ||a.pixels[offset+1]!=b.pixels[offset+1]
           ||a.pixels[offset+2]!=b.pixels[offset+2])++changed;
    }
    return changed;
}

std::string sha256(const std::filesystem::path& path)
{
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("missing hash input: "+path.string());
    CC_SHA256_CTX context;CC_SHA256_Init(&context);
    std::array<char,64*1024> chunk{};
    while(input){
        input.read(chunk.data(),static_cast<std::streamsize>(chunk.size()));
        const auto count=input.gcount();
        if(count>0)CC_SHA256_Update(&context,chunk.data(),static_cast<CC_LONG>(count));
    }
    std::array<unsigned char,CC_SHA256_DIGEST_LENGTH> digest{};
    CC_SHA256_Final(digest.data(),&context);
    constexpr char hex[]="0123456789abcdef";std::string encoded;encoded.reserve(digest.size()*2);
    for(unsigned char value:digest){encoded.push_back(hex[value>>4]);encoded.push_back(hex[value&15]);}
    return encoded;
}

std::filesystem::path copyAssetEvidence(const std::filesystem::path& sourceRoot,
                                        const std::filesystem::path& output)
{
    const auto evidenceRoot=output/"asset_root";
    std::filesystem::create_directories(evidenceRoot);
    for(const auto& entry:std::filesystem::recursive_directory_iterator(sourceRoot)){
        if(!entry.is_regular_file())continue;
        const auto relative=std::filesystem::relative(entry.path(),sourceRoot);
        const auto target=evidenceRoot/relative;
        std::filesystem::create_directories(target.parent_path());
        std::filesystem::copy_file(entry.path(),target,
                                   std::filesystem::copy_options::overwrite_existing);
    }
    return evidenceRoot;
}

double hardBlackFractionInTableRoi(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::string magic; int width=0,height=0,maximum=0;
    input>>magic>>width>>height>>maximum;input.get();
    if(magic!="P6"||width!=1920||height!=1080||maximum!=255)
        throw std::runtime_error("invalid quality screenshot header");
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width*height*3));
    input.read(reinterpret_cast<char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()));
    std::size_t dark=0,total=0;
    // The real venue includes intentionally black walls, pockets and rubber.
    // Restrict this regression gate to the playable cloth where a hard-black
    // projected "sticker" shadow is never physically plausible.
    for(int y=500;y<650;++y)for(int x=850;x<1050;++x){
        const std::size_t offset=static_cast<std::size_t>((y*width+x)*3);++total;
        if(pixels[offset]<5&&pixels[offset+1]<5&&pixels[offset+2]<5)++dark;
    }
    return static_cast<double>(dark)/static_cast<double>(total);
}

double srgbToLinear(unsigned char channel)
{
    const double value=static_cast<double>(channel)/255.0;
    return value<=0.04045?value/12.92:std::pow((value+0.055)/1.055,2.4);
}

struct RoiStats { std::array<double,3> average{{0,0,0}};double luminance=0,p95=0,p99=0; };

RoiStats roiStats(const PpmImage& image,int x0,int y0,int x1,int y1)
{
    RoiStats result;std::vector<double> luminance;
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
        const std::size_t offset=static_cast<std::size_t>((y*image.width+x)*3);
        std::array<double,3> linear{{srgbToLinear(image.pixels[offset]),srgbToLinear(image.pixels[offset+1]),srgbToLinear(image.pixels[offset+2])}};
        for(int channel=0;channel<3;++channel)result.average[static_cast<std::size_t>(channel)]+=linear[static_cast<std::size_t>(channel)];
        luminance.push_back(0.2126*linear[0]+0.7152*linear[1]+0.0722*linear[2]);
    }
    const double count=static_cast<double>(luminance.size());
    for(double value:luminance)result.luminance+=value;
    result.luminance/=count;for(double& value:result.average)value/=count;
    std::sort(luminance.begin(),luminance.end());
    result.p95=luminance[static_cast<std::size_t>(count*0.95)];
    result.p99=luminance[static_cast<std::size_t>(count*0.99)];
    return result;
}

void verifyLookdevColorGates(const std::filesystem::path& path)
{
    const auto image=readPpm(path);
    const auto felt=roiStats(image,760,430,1160,700);
    // The tighter break framing exposes the authored walnut venue trim across
    // the top wall; sample that stable material strip rather than mixing felt,
    // rubber, and a sliver of table body at the old right-edge coordinates.
    const auto wood=roiStats(image,0,95,1920,145);
    const auto lamp=roiStats(image,650,20,1300,180);
    if(felt.luminance<0.055||felt.luminance>0.16||felt.p95>0.18
       ||felt.average[1]<felt.average[0]*1.25||felt.average[2]<felt.average[1]*0.95)
        throw std::runtime_error("felt ROI failed deep-teal luminance/chroma gate");
    if(wood.luminance<0.005||wood.luminance>0.12||wood.p95>0.14)
        throw std::runtime_error("wood ROI failed dark-walnut luminance/chroma gate");
    if(lamp.p99>0.75)throw std::runtime_error("lamp diffuser highlight clips lookdev gate");
}

int whiteComponents(const PpmImage& image,int centerX,int centerY)
{
    const int x0=std::max(0,centerX-70),x1=std::min(image.width,centerX+71);
    const int y0=std::max(0,centerY-90),y1=std::min(image.height,centerY+91);
    const int width=x1-x0,height=y1-y0;std::vector<unsigned char> mask(static_cast<std::size_t>(width*height));
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const std::size_t source=static_cast<std::size_t>(((y+y0)*image.width+x+x0)*3);
        const auto r=image.pixels[source],g=image.pixels[source+1],b=image.pixels[source+2];
        mask[static_cast<std::size_t>(y*width+x)]=std::min({r,g,b})>120&&std::max({r,g,b})-std::min({r,g,b})<55;
    }
    int components=0;std::vector<int> stack;
    for(int start=0;start<width*height;++start){
        if(!mask[static_cast<std::size_t>(start)])continue;
        mask[static_cast<std::size_t>(start)]=0;stack.push_back(start);int area=0;
        while(!stack.empty()){
            const int index=stack.back();stack.pop_back();++area;const int x=index%width;
            const int neighbours[4]={index-1,index+1,index-width,index+width};
            for(int n=0;n<4;++n){const int next=neighbours[n];
                if(next<0||next>=width*height||(n==0&&x==0)||(n==1&&x==width-1))continue;
                if(mask[static_cast<std::size_t>(next)]){mask[static_cast<std::size_t>(next)]=0;stack.push_back(next);}
            }
        }
        if(area>=100)++components;
    }
    return components;
}

void verifyAtlasBadges(const std::filesystem::path& path,
    const billiardgl::PresentationSnapshot& snapshot);

double encodedLuma(const PpmImage& image,int x0,int y0,int x1,int y1)
{
    double sum=0;std::size_t count=0;
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
        const std::size_t offset=static_cast<std::size_t>((y*image.width+x)*3);
        sum+=(0.2126*image.pixels[offset]+0.7152*image.pixels[offset+1]+0.0722*image.pixels[offset+2])/255.0;++count;
    }
    return sum/static_cast<double>(count);
}

struct ProjectedBallCircle {
    double centerX=0.0,centerY=0.0,radius=0.0,sphereBottomY=0.0;
    bool valid=false;
};

struct ProjectedCueGeometry {
    billiardgl::render::PhysicalScreenPoint tipOrigin{};
    billiardgl::render::PhysicalScreenPoint tipBack{};
    billiardgl::render::PhysicalScreenPoint shaftBack{};
    double tipRadius=0.0;
    double shaftRadius=0.0;
    bool valid=false;
};

double pointSegmentDistanceSquared(double x,double y,
    const billiardgl::render::PhysicalScreenPoint& first,
    const billiardgl::render::PhysicalScreenPoint& second)
{
    const double dx=second.x-first.x,dy=second.y-first.y;
    const double lengthSquared=dx*dx+dy*dy;
    const double parameter=lengthSquared<=1e-12?0.0:std::clamp(
        ((x-first.x)*dx+(y-first.y)*dy)/lengthSquared,0.0,1.0);
    const double ex=x-(first.x+parameter*dx),ey=y-(first.y+parameter*dy);
    return ex*ex+ey*ey;
}

ProjectedCueGeometry projectCueGeometry(const billiardgl::PresentationSnapshot& snapshot,
    int physicalWidth,int physicalHeight)
{
    using namespace billiardgl::render;
    ProjectedCueGeometry result;
    if(!snapshot.cue.visible)return result;
    const SharedCameraProjection projection=buildSharedCameraProjection(
        snapshot.camera,physicalWidth,physicalHeight);
    if(!projection.valid)return result;
    const double shotLength=std::hypot(snapshot.cue.direction.x,snapshot.cue.direction.z);
    if(!std::isfinite(shotLength)||shotLength<=1e-8)return result;
    const double shotX=snapshot.cue.direction.x/shotLength;
    const double shotZ=snapshot.cue.direction.z/shotLength;
    const double backoff=std::max(static_cast<double>(snapshot.cue.modelOriginBackoffCm),3.4);
    // These dimensions are the authored LOD0 cue_tip/cue_shaft extents after
    // the renderer's cueModel radial/longitudinal scales.  Keeping the same
    // model, backoff and shared projection here makes this a rendered-object
    // identity check rather than an arbitrary dark-pixel proxy.
    constexpr double tipLengthCm=1.2*0.86;
    constexpr double shaftLengthCm=145.0*0.86;
    constexpr double tipRadiusCm=0.62*0.42;
    constexpr double shaftRadiusCm=1.5*0.42;
    const billiardgl::Point3 origin{static_cast<float>(snapshot.cue.referencePosition.x-shotX*backoff),
        snapshot.cue.referencePosition.y,
        static_cast<float>(snapshot.cue.referencePosition.z-shotZ*backoff)};
    const auto behind=[&](double distance){return billiardgl::Point3{
        static_cast<float>(origin.x-shotX*distance),origin.y,
        static_cast<float>(origin.z-shotZ*distance)};};
    const auto project=[&](const billiardgl::Point3& point,PhysicalScreenPoint& screen){
        return clipToPhysicalScreen(projection,transformWorldToClip(projection,point),screen);
    };
    const auto projectedRadius=[&](const billiardgl::Point3& center,double radius){
        PhysicalScreenPoint projectedCenter,horizontal,vertical;
        const billiardgl::Point3 across{static_cast<float>(center.x-shotZ*radius),center.y,
            static_cast<float>(center.z+shotX*radius)};
        const billiardgl::Point3 above{center.x,static_cast<float>(center.y+radius),center.z};
        if(!project(center,projectedCenter)||!project(across,horizontal)||!project(above,vertical))
            return 0.0;
        return std::max(std::hypot(horizontal.x-projectedCenter.x,horizontal.y-projectedCenter.y),
                        std::hypot(vertical.x-projectedCenter.x,vertical.y-projectedCenter.y));
    };
    const billiardgl::Point3 tipBack=behind(tipLengthCm),shaftBack=behind(shaftLengthCm);
    if(!project(origin,result.tipOrigin)||!project(tipBack,result.tipBack))return result;
    auto shaftFirst=transformWorldToClip(projection,origin);
    auto shaftLast=transformWorldToClip(projection,shaftBack);
    if(!clipHomogeneousSegment(shaftFirst,shaftLast)
       ||!clipToPhysicalScreen(projection,shaftLast,result.shaftBack))return result;
    result.tipRadius=projectedRadius(origin,tipRadiusCm);
    // The visible shaft is clipped before it reaches the camera near plane.
    // Its origin radius is the stable authored width used for the ownership
    // corridor; rendered-pixel connectivity, not an inflated far-end radius,
    // establishes the actual cue silhouette.
    result.shaftRadius=projectedRadius(origin,shaftRadiusCm);
    result.valid=std::isfinite(result.tipRadius)&&std::isfinite(result.shaftRadius)
        &&result.tipRadius>0.0&&result.shaftRadius>0.0;
    return result;
}

struct CueClearanceEvidence {
    bool valid=false;
    std::string failure;
    std::size_t cyanGuide=0,cyanBallOverlap=0;
    std::size_t cueComponentPixels=0,cueWoodPixels=0,cueDarkPixels=0;
    std::size_t cueBallOverlap=0;
    double projectedClearancePixels=0.0;
};

CueClearanceEvidence detectProjectedCueClearance(const PpmImage& image,
    const ProjectedBallCircle& ball,const ProjectedCueGeometry& cue)
{
    CueClearanceEvidence evidence;
    const auto fail=[&](const std::string& reason){evidence.failure=reason;return evidence;};
    if(!ball.valid||!cue.valid||image.width<=0||image.height<=0
       ||image.pixels.size()!=static_cast<std::size_t>(image.width*image.height*3))
        return fail("invalid projection or image");
    const double closest=std::sqrt(pointSegmentDistanceSquared(ball.centerX,ball.centerY,
        cue.tipOrigin,cue.tipBack));
    evidence.projectedClearancePixels=closest-ball.radius-cue.tipRadius;
    if(evidence.projectedClearancePixels<=0.0)
        return fail("authored projected cue-tip capsule intersects cue ball");

    const double corridorRadius=std::max(cue.shaftRadius,cue.tipRadius)+2.0;
    const double corridorRadiusSquared=corridorRadius*corridorRadius;
    const double ballMaskRadius=ball.radius+2.0;
    const double ballMaskRadiusSquared=ballMaskRadius*ballMaskRadius;
    const int pixelCount=image.width*image.height;
    std::vector<unsigned char> eligible(static_cast<std::size_t>(pixelCount),0);
    std::vector<unsigned char> isWood(static_cast<std::size_t>(pixelCount),0);
    std::vector<unsigned char> isDark(static_cast<std::size_t>(pixelCount),0);
    std::vector<unsigned char> visited(static_cast<std::size_t>(pixelCount),0);
    // Start the image corridor at the projected ball centre.  The authored
    // capsule still owns the geometric clearance test above; this longer
    // corridor lets a genuinely intruding rendered wood/tip component be
    // followed into the ball, while disconnected black cloth and contact
    // shadow pixels remain unowned and cannot masquerade as cue evidence.
    const billiardgl::render::PhysicalScreenPoint corridorStart{ball.centerX,ball.centerY};
    for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){
        const int pixel=y*image.width+x;
        const std::size_t offset=static_cast<std::size_t>(pixel)*3;
        const int r=image.pixels[offset],g=image.pixels[offset+1],b=image.pixels[offset+2];
        const bool cyan=g>170&&b>175&&b-r>35&&g-r>35&&std::abs(g-b)<25;
        const bool wood=r>75&&r>static_cast<int>(g)*1.15&&g>static_cast<int>(b)*1.08;
        const bool dark=std::max({r,g,b})<38;
        const double dx=(x+0.5)-ball.centerX,dy=(y+0.5)-ball.centerY;
        const bool inBall=dx*dx+dy*dy<ballMaskRadiusSquared;
        if(cyan){++evidence.cyanGuide;if(inBall)++evidence.cyanBallOverlap;}
        if(!(wood||dark)||pointSegmentDistanceSquared(x+0.5,y+0.5,
            corridorStart,cue.shaftBack)>corridorRadiusSquared)continue;
        eligible[static_cast<std::size_t>(pixel)]=1;
        isWood[static_cast<std::size_t>(pixel)]=wood?1:0;
        isDark[static_cast<std::size_t>(pixel)]=dark?1:0;
    }

    struct Component {std::size_t pixels=0,wood=0,dark=0,overlap=0;double tipDistanceSquared=1e30;};
    Component selected;
    std::vector<int> pending;
    for(int start=0;start<pixelCount;++start){
        if(!eligible[static_cast<std::size_t>(start)]||visited[static_cast<std::size_t>(start)])continue;
        Component component;pending.push_back(start);visited[static_cast<std::size_t>(start)]=1;
        while(!pending.empty()){
            const int pixel=pending.back();pending.pop_back();
            const int x=pixel%image.width,y=pixel/image.width;++component.pixels;
            component.wood+=isWood[static_cast<std::size_t>(pixel)];
            component.dark+=isDark[static_cast<std::size_t>(pixel)];
            const double dx=(x+0.5)-ball.centerX,dy=(y+0.5)-ball.centerY;
            if(dx*dx+dy*dy<ballMaskRadiusSquared)++component.overlap;
            const double tx=(x+0.5)-cue.tipOrigin.x,ty=(y+0.5)-cue.tipOrigin.y;
            component.tipDistanceSquared=std::min(component.tipDistanceSquared,tx*tx+ty*ty);
            constexpr int nx[8]={-1,0,1,-1,1,-1,0,1};
            constexpr int ny[8]={-1,-1,-1,0,0,1,1,1};
            for(int direction=0;direction<8;++direction){
                const int xx=x+nx[direction],yy=y+ny[direction];
                if(xx<0||xx>=image.width||yy<0||yy>=image.height)continue;
                const int next=yy*image.width+xx;
                if(eligible[static_cast<std::size_t>(next)]&&!visited[static_cast<std::size_t>(next)]){
                    visited[static_cast<std::size_t>(next)]=1;pending.push_back(next);
                }
            }
        }
        const double tipOwnershipRadius=std::max(4.0,corridorRadius*2.0);
        if(component.wood>0&&component.dark>0
           &&component.tipDistanceSquared<=tipOwnershipRadius*tipOwnershipRadius
           &&component.wood>selected.wood)selected=component;
    }
    evidence.cueComponentPixels=selected.pixels;evidence.cueWoodPixels=selected.wood;
    evidence.cueDarkPixels=selected.dark;evidence.cueBallOverlap=selected.overlap;
    if(evidence.cyanGuide<400)return fail("world-projected shot guide visibility gate failed");
    if(evidence.cyanBallOverlap!=0)return fail("cyan guide overlaps projected cue ball");
    if(evidence.cueWoodPixels==0||evidence.cueDarkPixels==0)
        return fail("rendered cue wood/tip component was not found in projected cue corridor");
    if(evidence.cueWoodPixels<500)return fail("snapshot-driven cue shaft is not visibly readable");
    if(evidence.cueBallOverlap!=0)return fail("rendered cue wood/tip component overlaps projected cue ball");
    evidence.valid=true;return evidence;
}

void verifyProjectedCueClearanceMutations()
{
    const ProjectedBallCircle ball{70.0,35.0,20.0,55.0,true};
    const ProjectedCueGeometry safe{{70.0,65.0},{70.0,70.0},{70.0,158.0},
        2.0,4.0,true};
    const auto fixture=[&](bool blackPatch,bool contactShadow,bool cueIntrusion,bool guideIntrusion,
                           bool woodIntrusion){
        PpmImage image;image.width=140;image.height=165;
        image.pixels.assign(static_cast<std::size_t>(image.width*image.height*3),110);
        const auto paint=[&](int x0,int y0,int x1,int y1,std::array<unsigned char,3> colour){
            for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
                const std::size_t offset=static_cast<std::size_t>((y*image.width+x)*3);
                std::copy(colour.begin(),colour.end(),image.pixels.begin()+offset);
            }
        };
        paint(67,65,74,72,{{12,12,12}});
        paint(67,72,74,160,{{130,82,50}});
        // Independent guide evidence lives far from the ball in this fixture.
        paint(5,5,25,30,{{30,205,210}});
        if(blackPatch)paint(64,29,72,37,{{4,4,4}});
        if(contactShadow)paint(56,57,65,63,{{5,5,5}});
        if(cueIntrusion)paint(67,49,74,66,{{10,10,10}});
        if(woodIntrusion)paint(67,49,74,66,{{130,82,50}});
        if(guideIntrusion)paint(64,29,72,37,{{30,205,210}});
        return image;
    };
    const auto accepted=detectProjectedCueClearance(fixture(false,false,false,false,false),ball,safe);
    const auto blackAccepted=detectProjectedCueClearance(fixture(true,false,false,false,false),ball,safe);
    const auto shadowAccepted=detectProjectedCueClearance(fixture(false,true,false,false,false),ball,safe);
    if(!accepted.valid||!blackAccepted.valid||!shadowAccepted.valid)
        throw std::runtime_error("projected cue provenance positive/black-shadow self-check failed");
    if(detectProjectedCueClearance(fixture(false,false,true,false,false),ball,safe).valid
       ||detectProjectedCueClearance(fixture(false,false,false,false,true),ball,safe).valid
       ||detectProjectedCueClearance(fixture(false,false,false,true,false),ball,safe).valid)
        throw std::runtime_error("projected cue tip/wood/guide intrusion mutation was accepted");
    auto unsafe=safe;unsafe.tipOrigin={70.0,52.0};unsafe.tipBack={70.0,58.0};
    if(detectProjectedCueClearance(fixture(false,false,false,false,false),ball,unsafe).valid)
        throw std::runtime_error("projected authored cue capsule overlap mutation was accepted");
}

ProjectedBallCircle projectBallCircle(const billiardgl::PresentationSnapshot& snapshot,
    std::size_t ballIndex,int physicalWidth,int physicalHeight)
{
    using namespace billiardgl::render;
    ProjectedBallCircle result;
    const SharedCameraProjection projection=buildSharedCameraProjection(
        snapshot.camera,physicalWidth,physicalHeight);
    if(!projection.valid||ballIndex>=snapshot.balls.size()
       ||!snapshot.balls[ballIndex].visible)return result;
    const Float3 eye=sharedCameraEye(snapshot.camera);
    const Float3 target=sharedCameraTarget(snapshot.camera);
    const Float3 forward=normalizeProjectionVector({{
        target[0]-eye[0],target[1]-eye[1],target[2]-eye[2]}});
    const Float3 worldUp{{0.0f,1.0f,0.0f}};
    const Float3 stableUp=std::fabs(projectionDot(forward,worldUp))>0.98f
        ?Float3{{0.0f,0.0f,1.0f}}:worldUp;
    const Float3 right=normalizeProjectionVector(projectionCross(forward,stableUp));
    const Float3 up=normalizeProjectionVector(projectionCross(right,forward));
    constexpr float ballRadiusCm=2.8575f;
    const auto project=[&](float dx,float dy,float dz,
                           PhysicalScreenPoint& screen){
        const auto& position=snapshot.balls[ballIndex].position;
        return clipToPhysicalScreen(projection,transformWorldToClip(projection,
            {position.x+dx,position.y+dy,position.z+dz}),screen);
    };
    PhysicalScreenPoint center,rightEdge,upEdge,sphereBottom;
    if(!project(0.0f,0.0f,0.0f,center)
       ||!project(right[0]*ballRadiusCm,right[1]*ballRadiusCm,
                   right[2]*ballRadiusCm,rightEdge)
       ||!project(up[0]*ballRadiusCm,up[1]*ballRadiusCm,
                   up[2]*ballRadiusCm,upEdge)
       ||!project(0.0f,-ballRadiusCm,0.0f,sphereBottom))return result;
    result.centerX=center.x;result.centerY=center.y;
    result.radius=std::max(std::hypot(rightEdge.x-center.x,rightEdge.y-center.y),
                           std::hypot(upEdge.x-center.x,upEdge.y-center.y));
    result.sphereBottomY=sphereBottom.y;
    result.valid=std::isfinite(result.centerX)&&std::isfinite(result.centerY)
        &&std::isfinite(result.radius)&&std::isfinite(result.sphereBottomY)
        &&result.radius>=4.0;
    return result;
}

struct ProjectedBadgeEllipse {
    billiardgl::render::PhysicalScreenPoint center{};
    std::array<double,2> axisU{{0.0,0.0}};
    std::array<double,2> axisV{{0.0,0.0}};
    bool valid=false;
};

ProjectedBadgeEllipse projectBallBadge(const billiardgl::PresentationSnapshot& snapshot,
    std::size_t ballIndex,int physicalWidth,int physicalHeight)
{
    using namespace billiardgl::render;
    ProjectedBadgeEllipse result;
    const SharedCameraProjection projection=buildSharedCameraProjection(
        snapshot.camera,physicalWidth,physicalHeight);
    if(!projection.valid||ballIndex>=snapshot.balls.size()
       ||!snapshot.balls[ballIndex].visible)return result;
    const auto& ball=snapshot.balls[ballIndex];
    Float3 axis=normalizeProjectionVector({{ball.rotationAxis.x,
        ball.rotationAxis.y,ball.rotationAxis.z}});
    if(projectionDot(axis,axis)<0.99f)return result;
    const double angle=ball.rotationAngleDegrees*0.01745329251994329577;
    const auto rotate=[&](Float3 value){
        const Float3 crossed=projectionCross(axis,value);
        const float aligned=projectionDot(axis,value);
        const float cosine=static_cast<float>(std::cos(angle));
        const float sine=static_cast<float>(std::sin(angle));
        return Float3{{value[0]*cosine+crossed[0]*sine+axis[0]*aligned*(1.0f-cosine),
            value[1]*cosine+crossed[1]*sine+axis[1]*aligned*(1.0f-cosine),
            value[2]*cosine+crossed[2]*sine+axis[2]*aligned*(1.0f-cosine)}};
    };
    const Float3 normal=rotate({{0.0f,0.0f,1.0f}});
    const Float3 tangentU=rotate({{1.0f,0.0f,0.0f}});
    const Float3 tangentV=rotate({{0.0f,1.0f,0.0f}});
    constexpr double ballRadiusCm=2.8575;
    // The authored atlas is 8 columns across 4096 px and each identity patch
    // has radius 38 px.  Its angular footprint is therefore asset-derived,
    // not tuned to the captured screenshot.
    constexpr double badgeAngularRadius=2.0*3.14159265358979323846*38.0/512.0;
    const auto point=[&](Float3 direction){return billiardgl::Point3{
        static_cast<float>(ball.position.x+ballRadiusCm*direction[0]),
        static_cast<float>(ball.position.y+ballRadiusCm*direction[1]),
        static_cast<float>(ball.position.z+ballRadiusCm*direction[2])};};
    const auto boundary=[&](Float3 tangent){return normalizeProjectionVector({{
        static_cast<float>(normal[0]*std::cos(badgeAngularRadius)+tangent[0]*std::sin(badgeAngularRadius)),
        static_cast<float>(normal[1]*std::cos(badgeAngularRadius)+tangent[1]*std::sin(badgeAngularRadius)),
        static_cast<float>(normal[2]*std::cos(badgeAngularRadius)+tangent[2]*std::sin(badgeAngularRadius))}});};
    PhysicalScreenPoint edgeU,edgeV;
    const auto project=[&](const billiardgl::Point3& world,PhysicalScreenPoint& screen){
        return clipToPhysicalScreen(projection,transformWorldToClip(projection,world),screen);
    };
    if(!project(point(normal),result.center)
       ||!project(point(boundary(tangentU)),edgeU)
       ||!project(point(boundary(tangentV)),edgeV))return result;
    result.axisU={{edgeU.x-result.center.x,edgeU.y-result.center.y}};
    result.axisV={{edgeV.x-result.center.x,edgeV.y-result.center.y}};
    const double determinant=result.axisU[0]*result.axisV[1]-result.axisU[1]*result.axisV[0];
    result.valid=std::isfinite(determinant)&&std::fabs(determinant)>16.0;
    return result;
}

bool badgeCoordinates(const ProjectedBadgeEllipse& badge,double x,double y,
    double& u,double& v)
{
    const double determinant=badge.axisU[0]*badge.axisV[1]-badge.axisU[1]*badge.axisV[0];
    if(!badge.valid||std::fabs(determinant)<=1e-9)return false;
    const double dx=x-badge.center.x,dy=y-badge.center.y;
    u=(dx*badge.axisV[1]-dy*badge.axisV[0])/determinant;
    v=(badge.axisU[0]*dy-badge.axisU[1]*dx)/determinant;
    return std::isfinite(u)&&std::isfinite(v);
}

struct RenderedBadgeEvidence {
    bool valid=false;
    std::string failure;
    std::size_t area=0,neutralBackground=0,darkInk=0;
    double backgroundMedian=0.0,inkMedian=0.0,contrast=0.0;
};

RenderedBadgeEvidence detectRenderedBadge(const PpmImage& image,
    const ProjectedBadgeEllipse& badge)
{
    RenderedBadgeEvidence evidence;
    const auto fail=[&](const std::string& reason){evidence.failure=reason;return evidence;};
    if(!badge.valid||image.width<=0||image.height<=0
       ||image.pixels.size()!=static_cast<std::size_t>(image.width*image.height*3))
        return fail("invalid badge projection or image");
    const double extentX=std::fabs(badge.axisU[0])+std::fabs(badge.axisV[0]);
    const double extentY=std::fabs(badge.axisU[1])+std::fabs(badge.axisV[1]);
    const int x0=std::max(0,static_cast<int>(std::floor(badge.center.x-extentX))-1);
    const int x1=std::min(image.width,static_cast<int>(std::ceil(badge.center.x+extentX))+2);
    const int y0=std::max(0,static_cast<int>(std::floor(badge.center.y-extentY))-1);
    const int y1=std::min(image.height,static_cast<int>(std::ceil(badge.center.y+extentY))+2);
    std::vector<double> background,ink;
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
        double u=0.0,v=0.0;
        if(!badgeCoordinates(badge,x+0.5,y+0.5,u,v)||u*u+v*v>1.0)continue;
        ++evidence.area;
        const std::size_t offset=static_cast<std::size_t>((y*image.width+x)*3);
        const int r=image.pixels[offset],g=image.pixels[offset+1],b=image.pixels[offset+2];
        const double luminance=0.2126*r+0.7152*g+0.0722*b;
        if(std::max({r,g,b})-std::min({r,g,b})<65&&std::max({r,g,b})>=65)
            background.push_back(luminance);
        // The generated 5x7 glyph is vertically centred.  Excluding only the
        // lower projected rim prevents a contact shadow from being counted as
        // ink while retaining the complete rotated 1/8/11/15 glyphs.
        if(v>=-0.68&&v<=0.55&&std::max({r,g,b})<65)ink.push_back(luminance);
    }
    evidence.neutralBackground=background.size();evidence.darkInk=ink.size();
    if(evidence.neutralBackground<3000||evidence.darkInk<250)
        return fail("projected badge lacks the existing 3000 background / 250 ink evidence");
    if(evidence.darkInk*100>evidence.area*38)
        return fail("dark region overwhelms projected badge instead of forming readable ink");
    const auto median=[](std::vector<double>& values){
        const auto middle=values.begin()+static_cast<std::ptrdiff_t>(values.size()/2);
        std::nth_element(values.begin(),middle,values.end());return *middle;
    };
    evidence.backgroundMedian=median(background);evidence.inkMedian=median(ink);
    evidence.contrast=evidence.backgroundMedian-evidence.inkMedian;
    if(evidence.contrast<45.0)
        return fail("projected badge background/ink contrast is not readable");
    evidence.valid=true;return evidence;
}

void verifyRenderedBadgeMutations()
{
    const ProjectedBadgeEllipse badge{{200.0,180.0},{{60.0,0.0}},{{0.0,58.0}},true};
    const auto fixture=[&](int mutation){
        PpmImage image;image.width=400;image.height=360;
        image.pixels.assign(static_cast<std::size_t>(image.width*image.height*3),0);
        const auto set=[&](int x,int y,std::array<unsigned char,3> color){
            const std::size_t offset=static_cast<std::size_t>((y*image.width+x)*3);
            std::copy(color.begin(),color.end(),image.pixels.begin()+offset);
        };
        for(int y=110;y<250;++y)for(int x=125;x<275;++x){
            double u=0.0,v=0.0;if(!badgeCoordinates(badge,x+0.5,y+0.5,u,v)||u*u+v*v>1.0)continue;
            std::array<unsigned char,3> color{{110,108,105}};
            if(mutation==1)color={{125,35,20}}; // missing neutral badge
            if(mutation==3)color={{90,90,90}};  // insufficient local contrast
            if(mutation==4)color={{8,8,8}};     // opaque black patch
            set(x,y,color);
        }
        if(mutation!=2&&mutation!=1&&mutation!=4){
            const int shift=mutation==5?90:0;
            const std::array<unsigned char,3> ink=mutation==3
                ?std::array<unsigned char,3>{{60,60,60}}
                :std::array<unsigned char,3>{{18,18,18}};
            for(int y=145;y<215;++y)for(int x=185+shift;x<197+shift;++x)set(x,y,ink);
            for(int y=145;y<215;++y)for(int x=205+shift;x<217+shift;++x)set(x,y,ink);
        }
        return image;
    };
    if(!detectRenderedBadge(fixture(0),badge).valid)
        throw std::runtime_error("projected badge positive self-check failed");
    for(int mutation=1;mutation<=5;++mutation)
        if(detectRenderedBadge(fixture(mutation),badge).valid)
            throw std::runtime_error("missing/flat/low-contrast/black/displaced badge mutation accepted");
}

void verifyAtlasBadges(const std::filesystem::path& path,
    const billiardgl::PresentationSnapshot& snapshot)
{
    static const bool mutationsVerified=[](){verifyRenderedBadgeMutations();return true;}();
    (void)mutationsVerified;
    const auto image=readPpm(path);
    const std::array<std::size_t,4> identities{{1,8,11,15}};
    for(std::size_t identity:identities){
        const auto projected=projectBallBadge(snapshot,identity,image.width,image.height);
        const auto evidence=detectRenderedBadge(image,projected);
        if(!evidence.valid)throw std::runtime_error("ball atlas projected badge gate failed for identity "+
            std::to_string(identity)+": "+evidence.failure);
        std::cout<<"atlas-badge-evidence identity="<<identity
                 <<" center="<<projected.center.x<<","<<projected.center.y
                 <<" background="<<evidence.neutralBackground
                 <<" ink="<<evidence.darkInk
                 <<" contrast="<<evidence.contrast<<'\n';
    }
}

struct BallSilhouetteEvidence {
    bool valid=false;
    std::string failure;
    int x0=0,y0=0,x1=0,y1=0;
    std::size_t area=0;
    double centroidX=0.0,centroidY=0.0;
    double feltFraction=0.0,interiorFeltDistance=0.0;
};

BallSilhouetteEvidence detectProjectedBallSilhouette(const PpmImage& image,
    const ProjectedBallCircle& expected)
{
    BallSilhouetteEvidence evidence;
    const auto fail=[&](const std::string& reason){
        evidence.failure=reason;return evidence;
    };
    if(!expected.valid||image.width<=0||image.height<=0
       ||image.pixels.size()!=static_cast<std::size_t>(image.width*image.height*3))
        return fail("invalid projection or image");
    const int seedX=static_cast<int>(std::lround(expected.centerX));
    const int seedY=static_cast<int>(std::lround(expected.centerY));
    if(seedX<0||seedX>=image.width||seedY<0||seedY>=image.height)
        return fail("projected center is outside the image");
    const auto rgb=[&](int pixel,int channel){
        return static_cast<int>(image.pixels[static_cast<std::size_t>(pixel)*3+channel]);
    };
    const auto neutral=[&](int pixel){
        const int r=rgb(pixel,0),g=rgb(pixel,1),b=rgb(pixel,2);
        return std::max({r,g,b})-std::min({r,g,b})<=36;
    };
    const int seed=seedY*image.width+seedX;
    if(!neutral(seed))return fail("projected center is not on a low-colour-difference surface");

    const double floodRadius=expected.radius*1.16;
    const double floodRadiusSquared=floodRadius*floodRadius;
    std::vector<unsigned char> visited(static_cast<std::size_t>(image.width*image.height),0);
    std::vector<int> pending;pending.reserve(static_cast<std::size_t>(
        4.5*expected.radius*expected.radius));
    pending.push_back(seed);visited[static_cast<std::size_t>(seed)]=1;
    evidence.x0=evidence.x1=seedX;evidence.y0=evidence.y1=seedY;
    double sumX=0.0,sumY=0.0;
    std::array<double,3> interiorSum{{0.0,0.0,0.0}};
    while(!pending.empty()){
        const int pixel=pending.back();pending.pop_back();
        const int x=pixel%image.width,y=pixel/image.width;
        ++evidence.area;sumX+=x;sumY+=y;
        evidence.x0=std::min(evidence.x0,x);evidence.x1=std::max(evidence.x1,x);
        evidence.y0=std::min(evidence.y0,y);evidence.y1=std::max(evidence.y1,y);
        for(int channel=0;channel<3;++channel)interiorSum[static_cast<std::size_t>(channel)]+=rgb(pixel,channel);
        constexpr int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
        for(int direction=0;direction<4;++direction){
            const int nx=x+dx[direction],ny=y+dy[direction];
            if(nx<0||nx>=image.width||ny<0||ny>=image.height)continue;
            const double ex=nx-expected.centerX,ey=ny-expected.centerY;
            if(ex*ex+ey*ey>floodRadiusSquared)continue;
            const int neighbour=ny*image.width+nx;
            if(visited[static_cast<std::size_t>(neighbour)]||!neutral(neighbour))continue;
            int localDifference=0;
            for(int channel=0;channel<3;++channel)localDifference=std::max(
                localDifference,std::abs(rgb(neighbour,channel)-rgb(pixel,channel)));
            if(localDifference>48)continue;
            visited[static_cast<std::size_t>(neighbour)]=1;pending.push_back(neighbour);
        }
    }
    if(evidence.area==0)return fail("projected component is empty");
    evidence.centroidX=sumX/evidence.area;evidence.centroidY=sumY/evidence.area;
    const int width=evidence.x1-evidence.x0+1,height=evidence.y1-evidence.y0+1;
    const double expectedArea=3.14159265358979323846*expected.radius*expected.radius;
    if(width<expected.radius*1.75||width>expected.radius*2.18
       ||height<expected.radius*1.75||height>expected.radius*2.18)
        return fail("component bounds do not match the projected ball diameter");
    if(evidence.area<expectedArea*0.72||evidence.area>expectedArea*1.14)
        return fail("component area does not match the projected ball circle");
    if(std::hypot(evidence.centroidX-expected.centerX,
                  evidence.centroidY-expected.centerY)>expected.radius*0.09)
        return fail("component centroid is displaced from the projected ball");

    std::array<double,3> feltSum{{0.0,0.0,0.0}};
    std::size_t feltSamples=0,annulusSamples=0;
    const double annulusInnerSquared=expected.radius*expected.radius*1.18*1.18;
    const double annulusOuterSquared=expected.radius*expected.radius*1.36*1.36;
    const int radiusLimit=static_cast<int>(std::ceil(expected.radius*1.36));
    for(int y=std::max(0,seedY-radiusLimit);y<=std::min(image.height-1,seedY+radiusLimit);++y)
        for(int x=std::max(0,seedX-radiusLimit);x<=std::min(image.width-1,seedX+radiusLimit);++x){
            const double dx=x-expected.centerX,dy=y-expected.centerY,distance=dx*dx+dy*dy;
            if(distance<annulusInnerSquared||distance>annulusOuterSquared)continue;
            ++annulusSamples;const int pixel=y*image.width+x;
            const int r=rgb(pixel,0),g=rgb(pixel,1),b=rgb(pixel,2);
            if(g-r<30||b-r<35)continue;
            ++feltSamples;feltSum[0]+=r;feltSum[1]+=g;feltSum[2]+=b;
        }
    if(annulusSamples==0||feltSamples==0)return fail("projected ball has no readable felt annulus");
    evidence.feltFraction=static_cast<double>(feltSamples)/annulusSamples;
    if(evidence.feltFraction<0.55)return fail("projected ball is not surrounded by enough felt");
    double distanceSquared=0.0;
    for(int channel=0;channel<3;++channel){
        const double interior=interiorSum[static_cast<std::size_t>(channel)]/evidence.area;
        const double felt=feltSum[static_cast<std::size_t>(channel)]/feltSamples;
        distanceSquared+=(interior-felt)*(interior-felt);
    }
    evidence.interiorFeltDistance=std::sqrt(distanceSquared);
    if(evidence.interiorFeltDistance<55.0)return fail("ball interior lacks contrast against surrounding felt");
    evidence.valid=true;return evidence;
}

void verifyProjectedBallSilhouetteMutations()
{
    const ProjectedBallCircle expected{80.0,60.0,30.0,90.0,true};
    const auto fixture=[&](int centerX,int centerY,int radius){
        PpmImage image;image.width=160;image.height=120;
        image.pixels.resize(static_cast<std::size_t>(image.width*image.height*3));
        for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){
            const bool ball=radius>=0
                &&(x-centerX)*(x-centerX)+(y-centerY)*(y-centerY)<=radius*radius;
            const std::array<unsigned char,3> colour=ball
                ?std::array<unsigned char,3>{{220,218,210}}
                :std::array<unsigned char,3>{{25,119,131}};
            const std::size_t offset=static_cast<std::size_t>((y*image.width+x)*3);
            std::copy(colour.begin(),colour.end(),image.pixels.begin()+offset);
        }
        return image;
    };
    if(!detectProjectedBallSilhouette(fixture(80,60,30),expected).valid)
        throw std::runtime_error("projected cue-ball silhouette positive self-check failed");
    if(detectProjectedBallSilhouette(fixture(80,60,-1),expected).valid
       ||detectProjectedBallSilhouette(fixture(38,60,30),expected).valid
       ||detectProjectedBallSilhouette(fixture(80,60,7),expected).valid
       ||detectProjectedBallSilhouette(fixture(80,60,43),expected).valid)
        throw std::runtime_error("projected cue-ball silhouette mutation self-check failed");
}

struct ContactShadowEvidence {
    bool valid=false;
    std::string failure;
    double coreLuma=0.0,feltLuma=0.0,ratio=0.0,feltFraction=0.0;
    double contactCenterLuma=0.0,contactShoulderLuma=0.0;
    double contactCenterToShoulder=0.0;
    int coreX0=0,coreY0=0,coreX1=0,coreY1=0;
    int feltX0=0,feltX1=0;
    std::size_t ballOverlapPixels=0;
};

ContactShadowEvidence detectProjectedContactShadow(const PpmImage& image,
    const ProjectedBallCircle& ball,int outwardDirection)
{
    ContactShadowEvidence evidence;
    const auto fail=[&](const std::string& reason){
        evidence.failure=reason;return evidence;
    };
    if(!ball.valid||(outwardDirection!=-1&&outwardDirection!=1))
        return fail("invalid projected ball or outward direction");
    const int centerX=static_cast<int>(std::lround(ball.centerX));
    const int halfWidth=std::max(4,static_cast<int>(std::lround(ball.radius*0.65)));
    evidence.coreX0=centerX-halfWidth;evidence.coreX1=centerX+halfWidth;
    const double silhouetteBottom=std::max(ball.sphereBottomY,ball.centerY+ball.radius);
    evidence.coreY0=static_cast<int>(std::ceil(silhouetteBottom))+1;
    evidence.coreY1=evidence.coreY0+40;
    if(outwardDirection<0){
        evidence.feltX0=static_cast<int>(std::lround(ball.centerX-ball.radius*1.55));
        evidence.feltX1=static_cast<int>(std::lround(ball.centerX-ball.radius*1.15));
    }else{
        evidence.feltX0=static_cast<int>(std::lround(ball.centerX+ball.radius*1.15));
        evidence.feltX1=static_cast<int>(std::lround(ball.centerX+ball.radius*1.55));
    }
    if(evidence.coreX0<0||evidence.coreX1>image.width
       ||evidence.feltX0<0||evidence.feltX1>image.width
       ||evidence.coreY0<0||evidence.coreY1>image.height
       ||evidence.coreX0>=evidence.coreX1||evidence.feltX0>=evidence.feltX1)
        return fail("projected contact-shadow bands leave the image");
    for(int y=evidence.coreY0;y<evidence.coreY1;++y)
        for(int x=evidence.coreX0;x<evidence.coreX1;++x){
            const double dx=(x+0.5)-ball.centerX,dy=(y+0.5)-ball.centerY;
            if(dx*dx+dy*dy<=ball.radius*ball.radius)++evidence.ballOverlapPixels;
        }
    if(evidence.ballOverlapPixels!=0)
        return fail("contact-shadow core overlaps the projected ball silhouette");
    evidence.coreLuma=encodedLuma(image,evidence.coreX0,evidence.coreY0,
                                  evidence.coreX1,evidence.coreY1);
    evidence.feltLuma=encodedLuma(image,evidence.feltX0,evidence.coreY0,
                                  evidence.feltX1,evidence.coreY1);
    std::size_t feltLike=0,total=0;
    for(int y=evidence.coreY0;y<evidence.coreY1;++y)
        for(int x=evidence.feltX0;x<evidence.feltX1;++x){
            const std::size_t offset=static_cast<std::size_t>((y*image.width+x)*3);
            const int r=image.pixels[offset],g=image.pixels[offset+1],b=image.pixels[offset+2];
            if(g-r>=30&&b-r>=35)++feltLike;
            ++total;
        }
    evidence.feltFraction=total==0?0.0:static_cast<double>(feltLike)/total;
    if(evidence.feltFraction<0.90)return fail("contact-shadow comparison band is not unobstructed felt");
    evidence.ratio=evidence.feltLuma/std::max(evidence.coreLuma,0.001);
    if(evidence.ratio<1.12)return fail("contact shadow does not darken its same-row felt reference");
    const int contactY1=std::min(evidence.coreY1,evidence.coreY0+20);
    const int centerHalf=std::max(2,static_cast<int>(std::lround(ball.radius*0.18)));
    const int shoulderInner=std::max(centerHalf+1,
        static_cast<int>(std::lround(ball.radius*0.40)));
    const int shoulderOuter=std::max(shoulderInner+1,
        static_cast<int>(std::lround(ball.radius*0.65)));
    if(centerX-shoulderOuter<0||centerX+shoulderOuter>image.width)
        return fail("contact-shadow grounding profile leaves the image");
    evidence.contactCenterLuma=encodedLuma(
        image,centerX-centerHalf,evidence.coreY0,centerX+centerHalf,contactY1);
    const double leftShoulder=encodedLuma(
        image,centerX-shoulderOuter,evidence.coreY0,
        centerX-shoulderInner,contactY1);
    const double rightShoulder=encodedLuma(
        image,centerX+shoulderInner,evidence.coreY0,
        centerX+shoulderOuter,contactY1);
    evidence.contactShoulderLuma=(leftShoulder+rightShoulder)*0.5;
    evidence.contactCenterToShoulder=evidence.contactCenterLuma/
        std::max(evidence.contactShoulderLuma,0.001);
    if(evidence.contactCenterToShoulder>1.05)
        return fail("contact shadow is a detached hollow ring instead of grounding the ball centre");
    evidence.valid=true;return evidence;
}

void verifyProjectedContactShadowMutations()
{
    const ProjectedBallCircle expected{70.0,35.0,25.0,50.0,true};
    const auto fixture=[&](bool shadow,int shadowCenterX,bool darkReference,
                           bool hollowCenter=false){
        PpmImage image;image.width=180;image.height=120;
        image.pixels.resize(static_cast<std::size_t>(image.width*image.height*3));
        for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){
            const bool inShadow=shadow&&y>=61&&y<101&&std::abs(x-shadowCenterX)<=17
                &&(!hollowCenter||std::abs(x-shadowCenterX)>=8);
            const bool inReference=darkReference&&y>=61&&y<101&&x>=31&&x<41;
            const std::array<unsigned char,3> colour=(inShadow||inReference)
                ?std::array<unsigned char,3>{{12,55,60}}
                :std::array<unsigned char,3>{{25,119,131}};
            const std::size_t offset=static_cast<std::size_t>((y*image.width+x)*3);
            std::copy(colour.begin(),colour.end(),image.pixels.begin()+offset);
        }
        return image;
    };
    if(!detectProjectedContactShadow(fixture(true,70,false),expected,-1).valid)
        throw std::runtime_error("projected contact-shadow positive self-check failed");
    if(detectProjectedContactShadow(fixture(false,70,false),expected,-1).valid
       ||detectProjectedContactShadow(fixture(true,115,false),expected,-1).valid
       ||detectProjectedContactShadow(fixture(true,70,true),expected,-1).valid
       ||detectProjectedContactShadow(fixture(true,70,false,true),expected,-1).valid)
        throw std::runtime_error("projected contact-shadow mutation self-check failed");
}

struct PocketFramingEvidence {
    bool valid=false;
    double minX=0.0,minY=0.0,maxX=0.0,maxY=0.0;
    ProjectedBallCircle scaleBall{};
    std::string failure;
};

PocketFramingEvidence evaluatePocketFraming(
    const billiardgl::PresentationSnapshot& snapshot,int width,int height)
{
    using namespace billiardgl::render;
    PocketFramingEvidence evidence;
    const auto fail=[&](std::string reason){
        evidence.failure=std::move(reason);return evidence;
    };
    const SharedCameraProjection projection=buildSharedCameraProjection(
        snapshot.camera,width,height);
    if(!projection.valid)return fail("invalid shared camera projection");

    // Shipping table geometry: +X/+Z corner centre, 13.2 cm constant-width
    // mouth/throat band, then its visual cavity.  Sampling the two mouth lips
    // and the outer/deep cavity corners makes this a world-space framing gate,
    // not a fixed-pixel crop that can silently accept a half-visible pocket.
    constexpr float invSqrtTwo=0.70710678118f;
    constexpr float halfMouthWidthCm=6.6f;
    constexpr float visualCavityDepthCm=14.0f;
    constexpr float clothY=86.48398f;
    constexpr float cavityFloorY=72.0f;
    const billiardgl::Point3 centre{63.5f,clothY,127.0f};
    const std::array<float,2> outward{{invSqrtTwo,invSqrtTwo}};
    const std::array<float,2> tangent{{invSqrtTwo,-invSqrtTwo}};
    const auto localPoint=[&](float lateral,float depth,float y){
        return billiardgl::Point3{
            centre.x+tangent[0]*lateral+outward[0]*depth,y,
            centre.z+tangent[1]*lateral+outward[1]*depth};
    };
    const std::array<billiardgl::Point3,6> boundary{{
        localPoint(-halfMouthWidthCm,0.0f,clothY),
        localPoint(halfMouthWidthCm,0.0f,clothY),
        localPoint(-halfMouthWidthCm,visualCavityDepthCm,cavityFloorY),
        localPoint(halfMouthWidthCm,visualCavityDepthCm,cavityFloorY),
        localPoint(0.0f,visualCavityDepthCm,cavityFloorY),
        localPoint(0.0f,6.0f,cavityFloorY)}};
    evidence.minX=evidence.minY=std::numeric_limits<double>::infinity();
    evidence.maxX=evidence.maxY=-std::numeric_limits<double>::infinity();
    for(const auto& point:boundary){
        PhysicalScreenPoint screen;
        if(!clipToPhysicalScreen(projection,transformWorldToClip(projection,point),screen))
            return fail("mouth/cavity boundary is behind the camera or clipped");
        evidence.minX=std::min(evidence.minX,screen.x);
        evidence.minY=std::min(evidence.minY,screen.y);
        evidence.maxX=std::max(evidence.maxX,screen.x);
        evidence.maxY=std::max(evidence.maxY,screen.y);
    }
    constexpr double frameMargin=48.0;
    if(evidence.minX<frameMargin||evidence.minY<frameMargin
       ||evidence.maxX>static_cast<double>(width)-frameMargin
       ||evidence.maxY>static_cast<double>(height)-frameMargin)
        return fail("complete mouth/cavity envelope does not clear the frame margin: "+
            std::to_string(evidence.minX)+","+std::to_string(evidence.minY)+"-"+
            std::to_string(evidence.maxX)+","+std::to_string(evidence.maxY));
    if(evidence.maxX-evidence.minX<70.0||evidence.maxY-evidence.minY<70.0)
        return fail("mouth/cavity envelope is too small for visual inspection");

    evidence.scaleBall=projectBallCircle(snapshot,1,width,height);
    if(!evidence.scaleBall.valid
       ||evidence.scaleBall.centerX-evidence.scaleBall.radius<frameMargin
       ||evidence.scaleBall.centerY-evidence.scaleBall.radius<frameMargin
       ||evidence.scaleBall.centerX+evidence.scaleBall.radius>width-frameMargin
       ||evidence.scaleBall.centerY+evidence.scaleBall.radius>height-frameMargin)
        return fail("adjacent scale ball is absent or clipped");
    evidence.valid=true;
    return evidence;
}

void verifyPocketFramingMutations()
{
    const auto accepted=evaluatePocketFraming(pocketCloseupSnapshot(),1920,1080);
    if(!accepted.valid)
        throw std::runtime_error("pocket framing positive self-check failed: "+accepted.failure);
    auto clipped=pocketCloseupSnapshot();
    clipped.camera.eye={115.0f,215.0f,175.0f};
    clipped.camera.target={50.0f,89.0f,95.0f};
    clipped.camera.fieldOfViewDegrees=30.0f;
    auto missingScale=pocketCloseupSnapshot();
    missingScale.balls[1].visible=false;
    if(evaluatePocketFraming(clipped,1920,1080).valid
       ||evaluatePocketFraming(missingScale,1920,1080).valid)
        throw std::runtime_error("clipped/missing-scale pocket framing mutation was accepted");
}

NSDictionary* tacticalProjectionGate(const billiardgl::PresentationSnapshot& snapshot,
                                     const billiardgl::render::gltf::Asset& asset)
{
    using namespace billiardgl::render;
    const auto projection=buildSharedCameraProjection(snapshot.camera,1920,1080);
    const auto project=[&](const billiardgl::Point3& point){PhysicalScreenPoint screen;
        if(!clipToPhysicalScreen(projection,transformWorldToClip(projection,point),screen))
            throw std::runtime_error("tactical full-table projection invalid");return screen;};
    double minY=1e30,maxY=-1e30;
    for(float x:{-63.5f,63.5f})for(float z:{-127.0f,127.0f}){
        const auto p=project({x,billiardgl::kTableModelPlaySurfaceHeightCm,z});
        minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);
    }
    const double top=minY/1080.0,bottom=1.0-maxY/1080.0;
    if(top<.08||top>.12||bottom<.08||bottom>.12)
        throw std::runtime_error("tactical full-table8-12percent physical margin failed");
    NSMutableArray* mouths=[NSMutableArray array];
    for(const auto& socket:asset.sockets){
        if(socket.name.rfind("SOCKET_pocket_",0)!=0||socket.name.find("_mouth")==std::string::npos)continue;
        const auto p=project({socket.positionCm.x,socket.positionCm.y,socket.positionCm.z});
        if(p.x<48||p.x>1872||p.y<48||p.y>1032)
            throw std::runtime_error("tactical actual mouth anchor clipped");
        [mouths addObject:@{@"name":[NSString stringWithUTF8String:socket.name.c_str()],
            @"world_cm":@[@(socket.positionCm.x),@(socket.positionCm.y),@(socket.positionCm.z)],
            @"screen_pixels":@[@(p.x),@(p.y)]}];
    }
    if([mouths count]!=18)throw std::runtime_error("tactical requires18actual imported mouth anchors");
    NSMutableArray* balls=[NSMutableArray array];
    for(std::size_t id=0;id<snapshot.balls.size();++id){
        const auto& ball=snapshot.balls[id];
        if(!ball.visible)throw std::runtime_error("tactical requires all16visible balls");
        double x0=1e30,y0=1e30,x1=-1e30,y1=-1e30;const float r=billiardgl::kChineseBallRadiusCm;
        for(float dx:{-r,r})for(float dy:{-r,r})for(float dz:{-r,r}){
            const auto p=project({ball.position.x+dx,ball.position.y+dy,ball.position.z+dz});
            x0=std::min(x0,p.x);y0=std::min(y0,p.y);x1=std::max(x1,p.x);y1=std::max(y1,p.y);
        }
        if(x0<48||y0<48||x1>1872||y1>1032)throw std::runtime_error("tactical ball sphere envelope clipped");
        [balls addObject:@{@"identity":@(id),@"conservative_sphere_bbox_pixels":@[@(x0),@(y0),@(x1),@(y1)]}];
    }
    return @{@"status":@"PASS",@"top_playfield_margin_fraction":@(top),@"bottom_playfield_margin_fraction":@(bottom),
        @"actual_imported_mouth_anchors":mouths,@"visible_ball_sphere_envelopes":balls};
}

void verifyTargetedRealSceneGates(const std::filesystem::path& output)
{
    const auto overhead=readPpm(output/"tactical_overhead.ppm");
    if(encodedLuma(overhead,740,100,1180,980)<0.20)
        throw std::runtime_error("stable top-down VP gate failed: playfield is not readable");

    const auto aim=readPpm(output/"low_aim.ppm");std::size_t cyanGuide=0;
    verifyProjectedBallSilhouetteMutations();
    verifyProjectedCueClearanceMutations();
    const auto aimScene=aimSnapshot();
    const auto projectedBall=projectBallCircle(aimScene,0,aim.width,aim.height);
    const auto silhouette=detectProjectedBallSilhouette(aim,projectedBall);
    if(!silhouette.valid)throw std::runtime_error(
        "projected cue-ball silhouette was not detected: "+silhouette.failure);
    const int ballX0=silhouette.x0,ballY0=silhouette.y0;
    const int ballX1=silhouette.x1,ballY1=silhouette.y1;
    const auto projectedCue=projectCueGeometry(aimScene,aim.width,aim.height);
    const auto cueEvidence=detectProjectedCueClearance(aim,projectedBall,projectedCue);
    cyanGuide=cueEvidence.cyanGuide;
    if(!cueEvidence.valid)throw std::runtime_error(
        "cue/guide projected provenance gate failed: "+cueEvidence.failure);
    std::cout<<"cue-clearance-evidence bbox="<<ballX0<<","<<ballY0<<"-"<<ballX1<<","<<ballY1
             <<" projected_center="<<projectedBall.centerX<<","<<projectedBall.centerY
             <<" radius_px="<<projectedBall.radius<<" silhouette_area="<<silhouette.area
             <<" felt_fraction="<<silhouette.feltFraction
             <<" felt_contrast="<<silhouette.interiorFeltDistance
             <<" projected_cue_clearance="<<cueEvidence.projectedClearancePixels
             <<" silhouette_cyan="<<cueEvidence.cyanBallOverlap
             <<" silhouette_cue="<<cueEvidence.cueBallOverlap
             <<" cue_component="<<cueEvidence.cueComponentPixels
             <<" cue_wood="<<cueEvidence.cueWoodPixels
             <<" cue_tip_dark="<<cueEvidence.cueDarkPixels
             <<" cyan_guide="<<cyanGuide<<'\n';

    const auto balls=readPpm(output/"ball_closeup.ppm");
    verifyProjectedContactShadowMutations();
    const auto hero=heroBallSnapshot();
    const std::array<std::pair<std::size_t,int>,2> frontBalls{{{1,-1},{8,1}}};
    for(const auto& [ballIndex,outwardDirection]:frontBalls){
        const auto projected=projectBallCircle(hero,ballIndex,balls.width,balls.height);
        const auto shadow=detectProjectedContactShadow(balls,projected,outwardDirection);
        if(!shadow.valid)throw std::runtime_error("ball contact-shadow grounding gate failed for ball "+
            std::to_string(ballIndex)+": "+shadow.failure);
        std::cout<<"contact-shadow-evidence ball="<<ballIndex
                 <<" center="<<projected.centerX<<","<<projected.centerY
                 <<" sphere_bottom="<<projected.sphereBottomY
                 <<" silhouette_bottom="<<projected.centerY+projected.radius
                 <<" core="<<shadow.coreLuma<<" felt="<<shadow.feltLuma
                 <<" felt_fraction="<<shadow.feltFraction
                 <<" ball_overlap_pixels="<<shadow.ballOverlapPixels
                 <<" ratio="<<shadow.ratio
                 <<" contact_center="<<shadow.contactCenterLuma
                 <<" contact_shoulders="<<shadow.contactShoulderLuma
                 <<" center_to_shoulders="<<shadow.contactCenterToShoulder<<'\n';
    }

    const auto pocket=readPpm(output/"pocket_closeup.ppm");
    verifyPocketFramingMutations();
    const auto pocketFraming=evaluatePocketFraming(
        pocketCloseupSnapshot(),pocket.width,pocket.height);
    if(!pocketFraming.valid)
        throw std::runtime_error("pocket closeup physical framing gate failed: "+
                                 pocketFraming.failure);
    const int x0=std::max(0,static_cast<int>(std::floor(pocketFraming.minX)));
    const int y0=std::max(0,static_cast<int>(std::floor(pocketFraming.minY)));
    const int x1=std::min(pocket.width,static_cast<int>(std::ceil(pocketFraming.maxX)));
    const int y1=std::min(pocket.height,static_cast<int>(std::ceil(pocketFraming.maxY)));
    std::size_t dark=0,total=0;
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
        const std::size_t offset=static_cast<std::size_t>((y*pocket.width+x)*3);++total;
        if(std::max({pocket.pixels[offset],pocket.pixels[offset+1],pocket.pixels[offset+2]})<45)++dark;
    }
    const double darkFraction=total==0?0.0:static_cast<double>(dark)/static_cast<double>(total);
    if(darkFraction<0.08)
        throw std::runtime_error("pocket closeup framed cavity is not visibly dark");
    std::cout<<"pocket-framing-evidence bbox="<<pocketFraming.minX<<","<<pocketFraming.minY
             <<"-"<<pocketFraming.maxX<<","<<pocketFraming.maxY
             <<" scale_ball="<<pocketFraming.scaleBall.centerX<<","
             <<pocketFraming.scaleBall.centerY<<" r="<<pocketFraming.scaleBall.radius
             <<" dark_fraction="<<darkFraction<<'\n';
}

void verifyMetrics(const billiardgl::render::ModernFrameMetrics& metrics, int samples,
                   billiardgl::render::RenderQuality quality,std::size_t expectedPbrBytes)
{
    static const char* expected[] = {"shadow_depth", "hdr_opaque", "overlay", "aces_tone_map", "ui"};
    if (metrics.passes.size() != 5 || metrics.visibleBalls != 16
        || metrics.msaaSamples != samples || metrics.triangles == 0
        || !metrics.compositionSafe
        || metrics.ballNdcBounds[3] >= 0.86f || metrics.quality != quality
        || !metrics.pbrPackageLoaded || metrics.pbrTextureCount != 22
        || metrics.pbrGpuBytes != expectedPbrBytes
        || metrics.environmentGpuBytes == 0
        || !metrics.gltfSceneLoaded || !metrics.venueSceneLoaded || metrics.gltfSceneTriangles < 10000
        || metrics.environmentResolution != (quality == billiardgl::render::RenderQuality::Low ? 64 : 128)
        || metrics.shadowMapSize != billiardgl::render::qualitySettings(quality).shadowMapSize
        || metrics.pcfKernelWidth != billiardgl::render::qualitySettings(quality).pcfKernelWidth) {
        std::cerr<<"metric-debug passes="<<metrics.passes.size()<<" balls="<<metrics.visibleBalls
                 <<" samples="<<metrics.msaaSamples<<" tris="<<metrics.triangles
                 <<" safe="<<metrics.compositionSafe<<" ymax="<<metrics.ballNdcBounds[3]
                 <<" pbr="<<metrics.pbrPackageLoaded<<" tex="<<metrics.pbrTextureCount
                 <<" bytes="<<metrics.pbrGpuBytes<<" env="<<metrics.environmentGpuBytes
                 <<" gltf="<<metrics.gltfSceneLoaded<<" venue="<<metrics.venueSceneLoaded
                 <<" scene_tris="<<metrics.gltfSceneTriangles<<" envres="<<metrics.environmentResolution
                 <<" shadow="<<metrics.shadowMapSize<<" pcf="<<metrics.pcfKernelWidth<<'\n';
        throw std::runtime_error("modern frame metrics are incomplete");
    }
    for (std::size_t index = 0; index < metrics.passes.size(); ++index) {
        if (metrics.passes[index].name != expected[index]
            || metrics.passes[index].cpuMilliseconds < 0.0
            || metrics.passes[index].gpuMilliseconds < 0.0) {
            throw std::runtime_error("modern pass timing/order is invalid");
        }
    }
}

void renderConfiguration(int samples, billiardgl::render::RenderQuality quality,
                         const std::string& label, const std::filesystem::path& output,
                         std::uint64_t& breakHash, std::uint64_t& aimHash)
{
    const bool seamlessBeforeInitialize =
        glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) == GL_TRUE;
    const char* seamlessOverride =
        std::getenv("BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC");
    const bool seamlessExpectedWhileInitialized =
        seamlessOverride == nullptr || seamlessOverride[0] == '\0' ||
        std::string(seamlessOverride) == "1";
    billiardgl::render::ModernRendererOptions options;
    options.msaaSamples = samples;
    options.debugErrors = true;
    options.synchronousGpuTiming = quality != billiardgl::render::RenderQuality::Medium;
    options.quality = quality;
    options.lookdevManifestPath = std::string(BILLIARDGL_SOURCE_ROOT)
        + "/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
    std::string phase2=phase2AssetRoot();
    const char* sixOnlyValue=std::getenv("BILLIARDGL_MEDIUM_SIX_ONLY");
    const bool candidateSix=sixOnlyValue&&std::string(sixOnlyValue)=="1";
    if(candidateSix){
        // Load from the self-contained fresh evidence copy, not a copy made
        // after rendering and merely relabelled as the loaded package.
        phase2=copyAssetEvidence(phase2,output).string()+"/";
    }
    options.pbrParametersPath=phase2+"focused_practice_hall_pbr_v1.params.json";
    options.pbrAssetManifestPath=phase2+"focused_practice_hall_modular_v1.asset.json";
    options.lightingRigPath=phase2+"focused_practice_hall_lighting_rig_v1.json";
    options.gltfScenePath=phase2+"focused_practice_hall_modular_v1.gltf";
    options.venueScenePath=phase2+"focused_practice_hall_venue_v1.gltf";
    if(const char* value=std::getenv("BILLIARDGL_POCKET_INDIRECT_BOUNCE")){
        if(std::string(value)!="0"&&std::string(value)!="1")
            throw std::runtime_error("pocket indirect flag must be 0 or 1");
        options.enablePocketIndirectBounce=std::string(value)=="1";
    }
    billiardgl::render::ModernRenderer renderer(options);
    std::string error;
    if (!renderer.initialize({1920, 1080, true}, error)) {
        throw std::runtime_error("ModernRenderer initialize: " + error);
    }
    if ((glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) == GL_TRUE) !=
        seamlessExpectedWhileInitialized)
        throw std::runtime_error(
            "renderer did not apply the requested seamless cubemap state");
    if (!renderer.lookdev().loadedManifest
        || renderer.lookdev().settings.manifestId != "focused_practice_hall_v1") {
        throw std::runtime_error("approved lookdev manifest was not consumed");
    }
    if(!renderer.pbrUpload().loadedPackage || !renderer.pbrUpload().warnings.empty()
       || renderer.pbrPackage().lighting.id!="focused_practice_hall_lighting_rig_v1")
        throw std::runtime_error("approved PBR package/lighting rig was not consumed cleanly");

    renderer.render(breakSnapshot());
    verifyMetrics(renderer.metrics(), samples, quality,renderer.pbrPackage().expectedGpuBytes);
    const auto breakPath = output / ("break_" + label + ".ppm");
    if (!renderer.capture({breakPath.string(), 1920, 1080}, error)) throw std::runtime_error(error);
    if(quality==billiardgl::render::RenderQuality::Medium
       &&!renderer.capture({(output/"break.ppm").string(),1920,1080},error))throw std::runtime_error(error);
    breakHash = checksum(breakPath);
    if(hardBlackFractionInTableRoi(breakPath)>0.03)
        throw std::runtime_error("table ROI contains hard-black sticker shadows");
    if(quality==billiardgl::render::RenderQuality::High)verifyLookdevColorGates(breakPath);

    auto transformed = breakSnapshot();
    transformed.balls[0].position.x += 35.0f;
    renderer.render(transformed);
    const auto transformPath = output / ("transform_" + label + ".ppm");
    if (!renderer.capture({transformPath.string(), 1920, 1080}, error)) throw std::runtime_error(error);
    if (checksum(transformPath) == breakHash) {
        throw std::runtime_error("PresentationSnapshot ball transform was not consumed");
    }

    renderer.resize(1280, 720);
    renderer.render(aimSnapshot());
    renderer.resize(1920, 1080);
    renderer.render(aimSnapshot());
    verifyMetrics(renderer.metrics(), samples, quality,renderer.pbrPackage().expectedGpuBytes);
    const auto aimPath = output / ("aim_" + label + ".ppm");
    if (!renderer.capture({aimPath.string(), 1920, 1080}, error)) throw std::runtime_error(error);
    if(quality==billiardgl::render::RenderQuality::Medium
       &&!renderer.capture({(output/"low_aim.ppm").string(),1920,1080},error))throw std::runtime_error(error);
    aimHash = checksum(aimPath);

    renderer.render(atlasCloseupSnapshot(0.0f));
    const auto atlasPath=output/("atlas_closeup_"+label+".ppm");
    if(!renderer.capture({atlasPath.string(),1920,1080},error))throw std::runtime_error(error);
    const auto atlasHash=checksum(atlasPath);
    renderer.render(atlasCloseupSnapshot(33.0f));
    const auto rotatedPath=output/("atlas_rotated_"+label+".ppm");
    if(!renderer.capture({rotatedPath.string(),1920,1080},error))throw std::runtime_error(error);
    if(checksum(rotatedPath)==atlasHash)throw std::runtime_error("ball atlas did not track snapshot rotation");
    if(quality==billiardgl::render::RenderQuality::High){
        verifyAtlasBadges(atlasPath,atlasCloseupSnapshot(0.0f));
        verifyAtlasBadges(rotatedPath,atlasCloseupSnapshot(33.0f));
    }
    if(quality==billiardgl::render::RenderQuality::Medium){
        renderer.render(heroBallSnapshot());
        if(!renderer.capture({(output/"ball_closeup.ppm").string(),1920,1080},error))
            throw std::runtime_error(error);
        const struct { const char* file; billiardgl::PresentationSnapshot snapshot; } remaining[]={
            {"pocket_closeup.ppm",pocketCloseupSnapshot()},
            {"tactical_overhead.ppm",tacticalOverheadSnapshot()},
            {"post_shot_observe.ppm",postShotObserveSnapshot()}
        };
        for(const auto& scene:remaining){
            renderer.render(scene.snapshot);
            if(!renderer.capture({(output/scene.file).string(),1920,1080},error))throw std::runtime_error(error);
        }
        verifyTargetedRealSceneGates(output);
        constexpr int warmupFrames=120,measuredFrames=300;
        const struct {const char* id;const char* file;billiardgl::PresentationSnapshot snapshot;} scenes[]={
            {"break","break.ppm",breakSnapshot()},
            {"low_aim","low_aim.ppm",aimSnapshot()},
            {"ball_closeup","ball_closeup.ppm",heroBallSnapshot()},
            {"pocket_closeup","pocket_closeup.ppm",pocketCloseupSnapshot()},
            {"tactical_overhead","tactical_overhead.ppm",tacticalOverheadSnapshot()},
            {"post_shot_observe","post_shot_observe.ppm",postShotObserveSnapshot()}
        };
        NSMutableArray* sceneReceipts=[NSMutableArray array];
        NSDictionary* tacticalGate=nil;
        if(const char* full=std::getenv("BILLIARDGL_TACTICAL_FULL_TABLE_REVIEW");full&&std::string(full)=="1"){
            const auto imported=billiardgl::render::gltf::importAsset(options.gltfScenePath);
            if(!imported.ok)throw std::runtime_error("tactical socket import: "+imported.error);
            tacticalGate=tacticalProjectionGate(tacticalOverheadSnapshot(),imported.asset);
            auto clipped=tacticalOverheadSnapshot();clipped.camera.eye.y=345.0f;
            auto missing=tacticalOverheadSnapshot();missing.balls[15].visible=false;
            int rejected=0;
            for(const auto& negative:{clipped,missing}){
                try{tacticalProjectionGate(negative,imported.asset);}catch(const std::runtime_error&){++rejected;}
            }
            if(rejected!=2)throw std::runtime_error("tactical projection negative controls accepted");
        }
        double p95=0.0;std::uint32_t completeDrawCalls=0;
        for(const auto& scene:scenes){
            std::vector<double> frameMilliseconds;frameMilliseconds.reserve(measuredFrames);
            std::uint32_t sceneDraws=0;std::uint64_t sceneTriangles=0;bool gpuObserved=false;
            for(int frame=0;frame<warmupFrames+measuredFrames;++frame){
                renderer.render(scene.snapshot);double cpuMs=0,gpuMs=0;std::uint32_t draws=0;
                for(const auto& pass:renderer.metrics().passes){cpuMs+=pass.cpuMilliseconds;gpuMs+=pass.gpuMilliseconds;draws+=pass.drawCalls;}
                sceneDraws=std::max(sceneDraws,draws);
                sceneTriangles=std::max<std::uint64_t>(sceneTriangles,renderer.metrics().triangles);
                if(frame>=warmupFrames){frameMilliseconds.push_back(std::max(cpuMs,gpuMs));gpuObserved=gpuObserved||gpuMs>0.0;}
            }
            std::sort(frameMilliseconds.begin(),frameMilliseconds.end());
            const double sceneP95=frameMilliseconds[frameMilliseconds.size()*95/100];
            p95=std::max(p95,sceneP95);completeDrawCalls=std::max(completeDrawCalls,sceneDraws);
            const auto& camera=scene.snapshot.camera;
            NSDictionary* cameraReceipt=@{@"eye_cm":@[@(camera.eye.x),@(camera.eye.y),@(camera.eye.z)],
                @"target_cm":@[@(camera.target.x),@(camera.target.y),@(camera.target.z)],
                @"vertical_fov_degrees":@(camera.fieldOfViewDegrees),
                @"viewport_pixels":@[@(camera.viewportWidth),@(camera.viewportHeight)]};
            const double resident=double(renderer.pbrUpload().uploadedGpuBytes+renderer.pbrUpload().environmentGpuBytes)/(1024.0*1024.0);
            NSDictionary* performance=@{@"warmup_frames":@(warmupFrames),@"sample_frames":@(measuredFrames),
                @"p95_total_frame_ms":@(sceneP95),@"draw_calls_complete_scene":@(sceneDraws),
                @"triangles_max":@(sceneTriangles),@"texture_resident_mib":@(resident),
                @"measurement_method":@"gpu_timer_query_plus_cpu_frame",@"gpu_timer_queries_observed":@(gpuObserved)};
            [sceneReceipts addObject:[NSMutableDictionary dictionaryWithDictionary:@{
                @"scene_id":[NSString stringWithUTF8String:scene.id],@"file":[NSString stringWithUTF8String:scene.file],
                @"runtime_camera":cameraReceipt,@"performance":performance,
                @"screenshot_sha256":[NSString stringWithUTF8String:sha256(output/scene.file).c_str()]}]];
        }
        if(tacticalGate)for(NSMutableDictionary* scene in sceneReceipts)
            if([scene[@"scene_id"] isEqual:@"tactical_overhead"])scene[@"full_table_projection_gate"]=tacticalGate;
        const double textureMiB=static_cast<double>(renderer.pbrUpload().uploadedGpuBytes+renderer.pbrUpload().environmentGpuBytes)/(1024.0*1024.0);
        const auto evidenceRoot=candidateSix?std::filesystem::path(phase2):copyAssetEvidence(phase2,output);
        const auto modularHash=sha256(evidenceRoot/"focused_practice_hall_modular_v1.gltf");
        const auto atlasHash=sha256(evidenceRoot/"focused_practice_hall_ball_identity_atlas_v1.png");
        const auto assetManifestHash=sha256(evidenceRoot/"focused_practice_hall_modular_v1.asset.json");
        const auto pbrHash=sha256(evidenceRoot/"focused_practice_hall_pbr_v1.params.json");
        const auto lightingHash=sha256(evidenceRoot/"focused_practice_hall_lighting_rig_v1.json");
        const auto venueHash=sha256(evidenceRoot/"focused_practice_hall_venue_v1.gltf");
        GLint glMajor=0,glMinor=0;glGetIntegerv(GL_MAJOR_VERSION,&glMajor);glGetIntegerv(GL_MINOR_VERSION,&glMinor);
        std::uint32_t executableSize=4096;std::vector<char> executable(executableSize);
        if(_NSGetExecutablePath(executable.data(),&executableSize)!=0){executable.resize(executableSize);
            if(_NSGetExecutablePath(executable.data(),&executableSize)!=0)throw std::runtime_error("six-scene executable binding failed");}
        NSDictionary* performance=@{@"warmup_frames":@(warmupFrames),@"sample_frames":@(measuredFrames),
            @"p95_total_frame_ms":@(p95),@"draw_calls_complete_scene":@(completeDrawCalls),
            @"texture_resident_mib":@(textureMiB),@"measurement_method":@"gpu_timer_query_plus_cpu_frame"};
        NSMutableDictionary* manifest=[NSMutableDictionary dictionaryWithDictionary:@{
            @"renderer":@"modern",@"backend":[NSString stringWithFormat:@"OpenGL %d.%d Core",glMajor,glMinor],
            @"runtime_gl":@{@"major":@(glMajor),@"minor":@(glMinor),@"version":[NSString stringWithUTF8String:reinterpret_cast<const char*>(glGetString(GL_VERSION))]},
            @"production_gltf":@YES,@"modular_gltf":@YES,@"venue_gltf":@YES,@"asset_root":@"asset_root",
            @"modular_gltf_sha256":[NSString stringWithUTF8String:modularHash.c_str()],
            @"ball_atlas_sha256":[NSString stringWithUTF8String:atlasHash.c_str()],
            @"asset_manifest_sha256":[NSString stringWithUTF8String:assetManifestHash.c_str()],
            @"pbr_params_sha256":[NSString stringWithUTF8String:pbrHash.c_str()],
            @"lighting_sha256":[NSString stringWithUTF8String:lightingHash.c_str()],
            @"venue_gltf_sha256":[NSString stringWithUTF8String:venueHash.c_str()],
            @"binary_sha256":[NSString stringWithUTF8String:sha256(executable.data()).c_str()],
            @"capture_source_sha256":[NSString stringWithUTF8String:sha256(std::string(BILLIARDGL_SOURCE_ROOT)+"/spikes/render_core_profile/modern_renderer_slice_smoke.mm").c_str()],
            @"renderer_source_sha256":[NSString stringWithUTF8String:sha256(std::string(BILLIARDGL_SOURCE_ROOT)+"/src/Billiards/render/modern/modern_renderer.cpp").c_str()],
            @"width":@1920,@"height":@1080,@"quality":@"medium",@"profile":@"medium_4x",@"hud":@YES,
            @"pocket_indirect_enabled":@(options.enablePocketIndirectBounce),@"candidate_medium_six_only":@(candidateSix),
            @"formal_pass_claimed":@NO,@"default_promoted":@NO,@"performance":performance,@"scenes":sceneReceipts}];
        NSMutableDictionary* environment=[NSMutableDictionary dictionary];
        for(const char* name:{"BILLIARDGL_MEDIUM_SIX_ONLY","BILLIARDGL_POCKET_INDIRECT_BOUNCE","BILLIARDGL_MODERN_PHASE2_ROOT",
                             "BILLIARDGL_TACTICAL_FULL_TABLE_REVIEW","BILLIARDGL_OBJECT_PHOTOMETRY"}){
            const char* value=std::getenv(name);
            environment[[NSString stringWithUTF8String:name]]=value?[NSString stringWithUTF8String:value]:[NSNull null];
        }
        manifest[@"capture_environment"]=environment;
        for(NSMutableDictionary* scene in sceneReceipts){
            scene[@"binary_sha256"]=manifest[@"binary_sha256"];
            scene[@"capture_source_sha256"]=manifest[@"capture_source_sha256"];
            scene[@"renderer_source_sha256"]=manifest[@"renderer_source_sha256"];
            scene[@"modular_gltf_sha256"]=manifest[@"modular_gltf_sha256"];
            scene[@"asset_manifest_sha256"]=manifest[@"asset_manifest_sha256"];
            scene[@"lighting_sha256"]=manifest[@"lighting_sha256"];
        }
        if(candidateSix){
            const auto loadedRoot=std::filesystem::canonical(evidenceRoot);
            NSMutableArray* required=[NSMutableArray array];
            const std::pair<const char*,const char*> requiredAssets[]={
                {"modular_gltf","focused_practice_hall_modular_v1.gltf"},
                {"asset_manifest","focused_practice_hall_modular_v1.asset.json"},
                {"pbr_params","focused_practice_hall_pbr_v1.params.json"},
                {"lighting_rig","focused_practice_hall_lighting_rig_v1.json"},
                {"venue_gltf","focused_practice_hall_venue_v1.gltf"},
                {"ball_atlas","focused_practice_hall_ball_identity_atlas_v1.png"}};
            for(const auto& item:requiredAssets){
                const auto path=std::filesystem::canonical(loadedRoot/item.second);
                [required addObject:@{@"id":[NSString stringWithUTF8String:item.first],
                    @"canonical_path":[NSString stringWithUTF8String:path.c_str()],
                    @"sha256":[NSString stringWithUTF8String:sha256(path).c_str()]}];
            }
            NSMutableArray* dependencies=[NSMutableArray array];
            for(const auto& file:std::filesystem::directory_iterator(loadedRoot)){
                if(!file.is_regular_file())continue;
                const auto name=file.path().filename().string();
                if(file.path().extension()!=".png"||name.find("review")!=std::string::npos
                   ||name.find("closeup")!=std::string::npos)continue;
                const auto path=std::filesystem::canonical(file.path());
                [dependencies addObject:@{@"canonical_path":[NSString stringWithUTF8String:path.c_str()],
                    @"sha256":[NSString stringWithUTF8String:sha256(path).c_str()]}];
            }
            const auto& loaded=renderer.metrics();
            NSDictionary* loadChecks=@{@"pbr_loaded":@(loaded.pbrPackageLoaded),@"pbr_texture_count_22":@(loaded.pbrTextureCount==22),
                @"environment_enabled":@(loaded.environmentGpuBytes>0),@"gltf_scene_loaded":@(loaded.gltfSceneLoaded),
                @"venue_scene_loaded":@(loaded.venueSceneLoaded),@"medium_quality":@(loaded.quality==billiardgl::render::RenderQuality::Medium),
                @"msaa_4x":@(loaded.msaaSamples==4)};
            NSDictionary* attestation=@{@"schema":@"billiardgl_formal_asset_attestation_v1",
                @"asset_root":[NSString stringWithUTF8String:loadedRoot.c_str()],@"load_state":@"loaded",
                @"silent_material_fallback_allowed":@NO,@"load_errors":@[],@"renderer_load_checks":loadChecks,
                @"required_assets":required,@"runtime_dependencies":dependencies};
            for(NSMutableDictionary* scene in sceneReceipts)scene[@"loaded_asset_attestation"]=attestation;
            manifest[@"formal_asset_attestation_required"]=@YES;
        }
        if(const char* value=std::getenv("BILLIARDGL_OBJECT_PHOTOMETRY");value&&std::string(value)=="1"){
            if(!candidateSix)throw std::runtime_error("object photometry requires explicit candidate six-scene export");
            auto maskOptions=options;maskOptions.objectClassMaskPass=true;
            billiardgl::render::ModernRenderer maskRenderer(maskOptions);
            if(!maskRenderer.initialize({1920,1080,true},error))throw std::runtime_error("class-mask renderer initialize: "+error);
            NSDictionary* palette=@{@"other":@[@0,@0,@0],@"felt":@[@255,@0,@0],
                @"cue_ball":@[@0,@255,@0],@"venue":@[@0,@0,@255],@"numbered_balls":@[@255,@255,@0]};
            for(std::size_t index=0;index<6;++index){
                const auto& scene=scenes[index];
                // Same unchanged presentation snapshot: guides/UI remain as
                // depth/clip/alpha-correct other0 in the debug-only renderer.
                maskRenderer.render(scene.snapshot);
                const std::string file=std::string(scene.id)+"_object_classes.ppm";
                if(!maskRenderer.capture({(output/file).string(),1920,1080},error))throw std::runtime_error(error);
                const auto mask=readPpm(output/file);std::size_t venueContributions=0;
                for(std::size_t offset=2;offset<mask.pixels.size();offset+=3)if(mask.pixels[offset])++venueContributions;
                NSMutableDictionary* receipt=sceneReceipts[index];
                NSMutableDictionary* binding=[NSMutableDictionary dictionaryWithDictionary:@{
                    @"schema":@"billiardgl_visible_surface_photometry_v1",@"source_file":receipt[@"file"],
                    @"source_sha256":receipt[@"screenshot_sha256"],@"mask_file":[NSString stringWithUTF8String:file.c_str()],
                    @"mask_sha256":[NSString stringWithUTF8String:sha256(output/file).c_str()],
                    @"normal_camera":receipt[@"runtime_camera"],@"mask_camera":receipt[@"runtime_camera"],
                    @"venue_loaded":@(maskRenderer.metrics().venueSceneLoaded),
                    @"cue_ball_expected_visible":@(scene.snapshot.balls[0].visible),
                    @"sample_policy":@"all_exact_class_pixels_including_shadows_no_luminance_filter",
                    @"mixed_edge_policy":@"exclude_non_palette_samples_and_report_count",@"class_palette":palette,
                    @"venue_absence_geometry_verified":@(venueContributions==0),
                    @"venue_absence_evidence":venueContributions==0?@"Same authenticated depth/cull/snapshot GPU mesh-class pass has zero venue-channel contribution including mixed edge pixels; loaded venue remains verified. Closeup normal view requires independent review; N_A is not PASS.":@"Venue contributes visible samples; not absent."}];
                if(scene.snapshot.balls[0].visible){
                    const auto cue=projectBallCircle(scene.snapshot,0,1920,1080);
                    if(!cue.valid)throw std::runtime_error("actual cue0 ownership probe projection invalid");
                    binding[@"cue_center_probe_pixels"]=@[@(cue.centerX),@(cue.centerY)];
                }
                for(NSString* key in @[@"binary_sha256",@"renderer_source_sha256",@"modular_gltf_sha256"]){
                    binding[[NSString stringWithFormat:@"normal_%@",key]]=receipt[key];
                    binding[[NSString stringWithFormat:@"mask_%@",key]]=receipt[key];
                }
                receipt[@"object_photometry"]=binding;
            }
            maskRenderer.shutdown();
            manifest[@"object_photometry_schema"]=@"billiardgl_visible_surface_photometry_v1";
            manifest[@"object_photometry_replaces_fixed_roi"]=@NO;
        }
        NSError* writeError=nil;NSData* data=[NSJSONSerialization dataWithJSONObject:manifest options:NSJSONWritingPrettyPrinted error:&writeError];
        const auto manifestPath=output/"manifest.json";
        if(writeError||![data writeToFile:[NSString stringWithUTF8String:manifestPath.c_str()] atomically:YES])
            throw std::runtime_error("six-scene manifest write failed");
    }

    double cpu = 0.0, gpu = 0.0;
    for (const auto& pass : renderer.metrics().passes) {
        cpu += pass.cpuMilliseconds;
        gpu += pass.gpuMilliseconds;
        std::cout << "pass=" << pass.name << " cpu_ms=" << pass.cpuMilliseconds
                  << " gpu_ms=" << pass.gpuMilliseconds << " draws=" << pass.drawCalls << '\n';
    }
    std::cout << "modern-slice samples=" << samples << " total_cpu_ms=" << cpu
              << " total_gpu_ms=" << gpu << " triangles=" << renderer.metrics().triangles
              << " pbr_bytes=" << renderer.pbrUpload().uploadedGpuBytes
              << " env_bytes=" << renderer.pbrUpload().environmentGpuBytes
              << " anisotropy=" << renderer.pbrUpload().anisotropy << '\n';
    renderer.shutdown();
    if ((glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) == GL_TRUE) !=
        seamlessBeforeInitialize)
        throw std::runtime_error(
            "renderer shutdown did not restore the incoming seamless cubemap state");
}

void verifySeamlessCubemapFailureRestoration()
{
    billiardgl::render::ModernRendererOptions options;
    // This is readable but deliberately not JSON, so initialization fails
    // after the renderer has captured and applied its cubemap state.
    options.lookdevManifestPath =
        std::string(BILLIARDGL_SOURCE_ROOT) + "/CMakeLists.txt";

    unsetenv("BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC");
    glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    {
        billiardgl::render::ModernRenderer renderer(options);
        std::string error;
        if (renderer.initialize({64, 64, true}, error) || error.empty())
            throw std::runtime_error(
                "malformed lookdev did not fail renderer initialization");
        if (glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) == GL_TRUE)
            throw std::runtime_error(
                "failed default initialization leaked seamless cubemap enable");
    }

    setenv("BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC", "0", 1);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    {
        billiardgl::render::ModernRenderer renderer(options);
        std::string error;
        if (renderer.initialize({64, 64, true}, error) || error.empty())
            throw std::runtime_error(
                "malformed lookdev did not fail rollback initialization");
        if (glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) != GL_TRUE)
            throw std::runtime_error(
                "failed rollback initialization did not restore seamless cubemap enable");
    }
}

void verifySeamlessCubemapSuccessRestoration()
{
    billiardgl::render::ModernRendererOptions options;

    unsetenv("BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC");
    glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    {
        billiardgl::render::ModernRenderer renderer(options);
        std::string error;
        if (!renderer.initialize({64, 64, true}, error))
            throw std::runtime_error(
                "default seamless state fixture failed to initialize: " + error);
        if (glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) != GL_TRUE)
            throw std::runtime_error(
                "production default did not enable seamless cubemap filtering");
        renderer.shutdown();
        if (glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) == GL_TRUE)
            throw std::runtime_error(
                "default shutdown did not restore disabled cubemap state");
    }

    setenv("BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC", "0", 1);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    {
        billiardgl::render::ModernRenderer renderer(options);
        std::string error;
        if (!renderer.initialize({64, 64, true}, error))
            throw std::runtime_error(
                "seamless rollback fixture failed to initialize: " + error);
        if (glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) == GL_TRUE)
            throw std::runtime_error(
                "explicit zero did not disable seamless cubemap filtering");
        renderer.shutdown();
        if (glIsEnabled(GL_TEXTURE_CUBE_MAP_SEAMLESS) != GL_TRUE)
            throw std::runtime_error(
                "rollback shutdown did not restore enabled cubemap state");
    }
}

void verifyGltfShadowMapGpu(const std::filesystem::path& output)
{
    std::filesystem::create_directories(output);
    setenv("BILLIARDGL_GLTF_CONTACT_SHADOW_COMPENSATION","0",1);
    const std::string phase2=phase2AssetRoot();
    const auto makeOptions=[&](billiardgl::render::RenderQuality quality){
        billiardgl::render::ModernRendererOptions options;
        options.msaaSamples=0;
        options.debugErrors=true;
        options.synchronousGpuTiming=true;
        options.quality=quality;
        options.lookdevManifestPath=std::string(BILLIARDGL_SOURCE_ROOT)
            +"/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
        options.pbrParametersPath=phase2+"focused_practice_hall_pbr_v1.params.json";
        options.pbrAssetManifestPath=phase2+"focused_practice_hall_modular_v1.asset.json";
        options.lightingRigPath=phase2+"focused_practice_hall_lighting_rig_v1.json";
        options.gltfScenePath=phase2+"focused_practice_hall_modular_v1.gltf";
        options.venueScenePath=phase2+"focused_practice_hall_venue_v1.gltf";
        return options;
    };
    const auto shadowDraws=[](const billiardgl::render::ModernRenderer& renderer){
        const auto& passes=renderer.metrics().passes;
        const auto found=std::find_if(passes.begin(),passes.end(),[](const auto& pass){
            return pass.name=="shadow_depth";
        });
        if(found==passes.end())throw std::runtime_error("shadow depth pass missing");
        return found->drawCalls;
    };

    auto snapshot=breakSnapshot();
    snapshot.camera.viewportWidth=960;snapshot.camera.viewportHeight=540;
    snapshot.hud.viewportWidth=960;snapshot.hud.viewportHeight=540;
    snapshot.cue.visible=false;
    for(auto& ball:snapshot.balls)ball.visible=false;
    snapshot.balls[0].identity=0;
    snapshot.balls[0].position={0.0f,89.34f,0.0f};
    snapshot.balls[0].visible=true;
    snapshot.balls[0].castsShadow=false;

    billiardgl::render::ModernRenderer renderer(
        makeOptions(billiardgl::render::RenderQuality::Medium));
    std::string error;
    if(!renderer.initialize({960,540,true},error))
        throw std::runtime_error("shadow validation initialize: "+error);
    renderer.render(snapshot);
    const std::uint32_t staticDraws=shadowDraws(renderer);
    if(staticDraws==0)throw std::runtime_error("glTF rail/pocket casters submitted zero draws");
    const auto unshadowed=output/"receiver_without_ball_shadow.ppm";
    const auto staticDepth=output/"static_casters.pgm";
    if(!renderer.capture({unshadowed.string(),960,540},error)
       ||!renderer.captureShadowDepthForValidation(staticDepth.string(),error))
        throw std::runtime_error(error);

    snapshot.balls[0].castsShadow=true;
    renderer.render(snapshot);
    if(shadowDraws(renderer)<=staticDraws)
        throw std::runtime_error("visible ball did not add a glTF shadow caster draw");
    const auto shadowed=output/"receiver_with_ball_shadow.ppm";
    const auto stableDepth=output/"ball_shadow_camera_a.pgm";
    if(!renderer.capture({shadowed.string(),960,540},error)
       ||!renderer.captureShadowDepthForValidation(stableDepth.string(),error))
        throw std::runtime_error(error);
    const std::size_t receiverPixels=changedPixelCount(unshadowed,shadowed);
    if(receiverPixels<40||receiverPixels>50000)
        throw std::runtime_error("ball caster did not affect a bounded receiver ROI");

    auto cameraMoved=snapshot;
    cameraMoved.camera.eye={42.0f,145.0f,105.0f};
    cameraMoved.camera.target={0.0f,86.48f,0.0f};
    renderer.render(cameraMoved);
    const auto cameraDepth=output/"ball_shadow_camera_b.pgm";
    if(!renderer.captureShadowDepthForValidation(cameraDepth.string(),error))
        throw std::runtime_error(error);
    if(checksum(stableDepth)!=checksum(cameraDepth))
        throw std::runtime_error("camera motion changed the world-space shadow map");

    auto ballMoved=snapshot;
    ballMoved.balls[0].position.x=24.0f;
    renderer.render(ballMoved);
    const auto movedDepth=output/"ball_shadow_moved.pgm";
    if(!renderer.captureShadowDepthForValidation(movedDepth.string(),error))
        throw std::runtime_error(error);
    if(checksum(stableDepth)==checksum(movedDepth))
        throw std::runtime_error("ball motion did not move its light-space caster");

    auto outsideFrustum=snapshot;
    outsideFrustum.balls[0].position.x=160.0f;
    renderer.render(outsideFrustum);
    const auto outsideDepth=output/"ball_shadow_outside_frustum.pgm";
    if(!renderer.captureShadowDepthForValidation(outsideDepth.string(),error))
        throw std::runtime_error(error);
    if(checksum(staticDepth)!=checksum(outsideDepth))
        throw std::runtime_error("out-of-frustum caster polluted the shadow border");

    auto cueAdded=snapshot;
    cueAdded.cue.visible=true;
    cueAdded.cue.referencePosition=cueAdded.balls[0].position;
    cueAdded.cue.direction={0.816f,0.0f,-0.578f};
    renderer.render(cueAdded);
    const auto cueDepth=output/"cue_shadow.pgm";
    if(!renderer.captureShadowDepthForValidation(cueDepth.string(),error))
        throw std::runtime_error(error);
    if(checksum(stableDepth)==checksum(cueDepth))
        throw std::runtime_error("authored cue did not alter the light-space depth map");
    renderer.shutdown();

    // The legacy screen-space contact ellipse is a deliberately weak
    // compensation for a ball resting on the cloth.  It must not turn an
    // airborne/off-table ball into a shadow on the fixed cloth plane.  Keep
    // the ball and camera outside the authored light frustum so the real
    // light-space caster and receiver are both fail-lit; any image delta when
    // castsShadow changes is therefore a false fixed-plane contact shadow.
    setenv("BILLIARDGL_GLTF_CONTACT_SHADOW_COMPENSATION","1",1);
    billiardgl::render::ModernRenderer offTableRenderer(
        makeOptions(billiardgl::render::RenderQuality::Medium));
    if(!offTableRenderer.initialize({960,540,true},error))
        throw std::runtime_error("off-table shadow initialize: "+error);
    auto offTable=outsideFrustum;
    offTable.camera.eye={160.0f,145.0f,105.0f};
    offTable.camera.target={160.0f,100.0f,0.0f};
    offTable.balls[0].position={160.0f,120.0f,0.0f};
    offTable.balls[0].castsShadow=false;
    offTableRenderer.render(offTable);
    const auto offTableNoCaster=output/"off_table_receiver_no_caster.ppm";
    if(!offTableRenderer.capture({offTableNoCaster.string(),960,540},error))
        throw std::runtime_error(error);
    offTable.balls[0].castsShadow=true;
    offTableRenderer.render(offTable);
    const auto offTableCaster=output/"off_table_receiver_caster.ppm";
    if(!offTableRenderer.capture({offTableCaster.string(),960,540},error))
        throw std::runtime_error(error);
    if(changedPixelCount(offTableNoCaster,offTableCaster)!=0)
        throw std::runtime_error(
            "off-table ball projected a false fixed-cloth contact shadow");
    offTableRenderer.shutdown();
    setenv("BILLIARDGL_GLTF_CONTACT_SHADOW_COMPENSATION","0",1);

    for(const auto quality:{billiardgl::render::RenderQuality::Low,
                            billiardgl::render::RenderQuality::Medium,
                            billiardgl::render::RenderQuality::High}){
        billiardgl::render::ModernRenderer tierRenderer(makeOptions(quality));
        if(!tierRenderer.initialize({320,180,true},error))
            throw std::runtime_error("shadow tier initialize: "+error);
        tierRenderer.render(snapshot);
        const auto expected=billiardgl::render::qualitySettings(quality);
        if(tierRenderer.metrics().shadowMapSize!=expected.shadowMapSize
           ||tierRenderer.metrics().pcfKernelWidth!=expected.pcfKernelWidth
           ||shadowDraws(tierRenderer)==0)
            throw std::runtime_error("shadow quality tier was not realized by GL");
        tierRenderer.shutdown();
    }
    unsetenv("BILLIARDGL_GLTF_CONTACT_SHADOW_COMPENSATION");
    std::cout<<"gltf-shadow-evidence static_draws="<<staticDraws
             <<" receiver_changed_pixels="<<receiverPixels<<"\n";
}

void renderPocketOnly(const std::filesystem::path& output)
{
    std::filesystem::create_directories(output);
    billiardgl::render::ModernRendererOptions options;
    options.msaaSamples = 4;
    options.debugErrors = true;
    options.synchronousGpuTiming = false;
    options.quality = billiardgl::render::RenderQuality::Medium;
    options.suppressLegacyUi = true;
    options.lookdevManifestPath = std::string(BILLIARDGL_SOURCE_ROOT)
        + "/assets/modern/lookdev/focused_practice_hall_v1.lookdev.json";
    const std::string phase2 = phase2AssetRoot();
    options.pbrParametersPath = phase2 + "focused_practice_hall_pbr_v1.params.json";
    options.pbrAssetManifestPath = phase2 + "focused_practice_hall_modular_v1.asset.json";
    options.lightingRigPath = phase2 + "focused_practice_hall_lighting_rig_v1.json";
    options.gltfScenePath = phase2 + "focused_practice_hall_modular_v1.gltf";
    options.venueScenePath = phase2 + "focused_practice_hall_venue_v1.gltf";
    const char* indirectValue=std::getenv("BILLIARDGL_POCKET_INDIRECT_BOUNCE");
    const bool indirect=indirectValue && std::string(indirectValue)=="1";
    if(indirectValue && std::string(indirectValue)!="0" && !indirect)
        throw std::runtime_error("pocket indirect flag must be 0 or 1");
    options.enablePocketIndirectBounce=indirect;
    billiardgl::render::ModernRenderer renderer(options);
    std::string error;
    if (!renderer.initialize({1920, 1080, true}, error))
        throw std::runtime_error("ModernRenderer initialize: " + error);
    constexpr int warmupFrames = 120;
    constexpr int measuredFrames = 300;
    int reviewId=-1;
    if(const char* value=std::getenv("BILLIARDGL_POCKET_REVIEW_ID")){
        const std::string id(value);
        if(id.size()!=1||id[0]<'0'||id[0]>'5')
            throw std::runtime_error("pocket review ID must be 0..5");
        reviewId=id[0]-'0';
    }
    const auto snapshot = reviewId<0?pocketCloseupSnapshot():pocketReviewSnapshot(reviewId);
    std::unique_ptr<billiardgl::render::ModernRenderer> disabledRenderer;
    double disabledP95=0.0;
    if(indirect){
        auto disabled=options;disabled.enablePocketIndirectBounce=false;
        disabledRenderer=std::make_unique<billiardgl::render::ModernRenderer>(disabled);
        if(!disabledRenderer->initialize({1920,1080,true},error))throw std::runtime_error(error);
        std::vector<double> samples;
        for(int frame=0;frame<warmupFrames+measuredFrames;++frame){
            disabledRenderer->render(snapshot);double cpu=0,gpu=0;
            for(const auto& pass:disabledRenderer->metrics().passes){cpu+=pass.cpuMilliseconds;gpu+=pass.gpuMilliseconds;}
            if(frame>=warmupFrames)samples.push_back(std::max(cpu,gpu));
        }
        std::sort(samples.begin(),samples.end());disabledP95=samples[samples.size()*95/100];
        if(!disabledRenderer->capture({(output/"pocket_indirect_off.ppm").string(),1920,1080},error))throw std::runtime_error(error);
        if(const char* reference=std::getenv("BILLIARDGL_POCKET_INDIRECT_REFERENCE"))
            if(changedPixelCount(reference,output/"pocket_indirect_off.ppm")!=0)
                throw std::runtime_error("disabled pocket indirect changed the r25 reference");
    }
    std::vector<double> frameMilliseconds;
    frameMilliseconds.reserve(measuredFrames);
    std::uint32_t maximumDrawCalls = 0;
    std::uint64_t maximumTriangles = 0;
    for (int frame = 0; frame < warmupFrames + measuredFrames; ++frame) {
        renderer.render(snapshot);
        double cpuMilliseconds = 0.0;
        double gpuMilliseconds = 0.0;
        std::uint32_t drawCalls = 0;
        for (const auto& pass : renderer.metrics().passes) {
            cpuMilliseconds += pass.cpuMilliseconds;
            gpuMilliseconds += pass.gpuMilliseconds;
            drawCalls += pass.drawCalls;
        }
        maximumDrawCalls = std::max(maximumDrawCalls, drawCalls);
        maximumTriangles = std::max<std::uint64_t>(
            maximumTriangles, renderer.metrics().triangles);
        if (frame >= warmupFrames)
            frameMilliseconds.push_back(std::max(cpuMilliseconds, gpuMilliseconds));
    }
    const auto& metrics = renderer.metrics();
    const auto uiPass = std::find_if(metrics.passes.begin(), metrics.passes.end(),
        [](const auto& pass) { return pass.name == "ui"; });
    if (metrics.passes.size() != 5 || metrics.msaaSamples != 4 ||
        metrics.triangles == 0 || !metrics.pbrPackageLoaded ||
        !metrics.gltfSceneLoaded || !metrics.venueSceneLoaded ||
        uiPass == metrics.passes.end() || uiPass->drawCalls != 0)
        throw std::runtime_error("pocket-only frame metrics are incomplete");
    if (!renderer.capture({(output / "pocket_closeup.ppm").string(), 1920, 1080}, error))
        throw std::runtime_error(error);
    auto objectIdOptions = options;
    objectIdOptions.pocketObjectIdPass = true;
    billiardgl::render::ModernRenderer objectIdRenderer(objectIdOptions);
    if (!objectIdRenderer.initialize({1920, 1080, true}, error))
        throw std::runtime_error("Pocket object-ID renderer initialize: " + error);
    auto objectIdSnapshot = snapshot;
    objectIdSnapshot.cue.visible = false;
    for (auto& ball : objectIdSnapshot.balls) ball.visible = false;
    objectIdRenderer.render(objectIdSnapshot);
    if (!objectIdRenderer.capture(
            {(output / "pocket_object_ids.ppm").string(), 1920, 1080}, error))
        throw std::runtime_error(error);
    if(indirect){
        disabledRenderer->render(snapshot);
        if(!disabledRenderer->capture({(output/"pocket_indirect_off_restored.ppm").string(),1920,1080},error))throw std::runtime_error(error);
        if(changedPixelCount(output/"pocket_indirect_off.ppm",output/"pocket_indirect_off_restored.ppm")!=0)
            throw std::runtime_error("pocket indirect state leaked into disabled renderer");
        const auto before=readPpm(output/"pocket_indirect_off.ppm");
        const auto after=readPpm(output/"pocket_closeup.ppm");
        const auto ids=readPpm(output/"pocket_object_ids.ppm");
        std::size_t deepChanged=0;
        for(std::size_t i=0;i<ids.pixels.size();i+=3)
            if(ids.pixels[i]<3&&ids.pixels[i+1]<3&&ids.pixels[i+2]>220)
                if(before.pixels[i]!=after.pixels[i]||before.pixels[i+1]!=after.pixels[i+1]||before.pixels[i+2]!=after.pixels[i+2])++deepChanged;
        if(deepChanged)throw std::runtime_error("pocket indirect affected deep-cavity pixels");
        if(const char* darkRig=std::getenv("BILLIARDGL_POCKET_INDIRECT_DARK_RIG")){
            for(bool enabled:{false,true}){
                auto dark=options;dark.lightingRigPath=darkRig;dark.enablePocketIndirectBounce=enabled;
                billiardgl::render::ModernRenderer darkRenderer(dark);
                if(!darkRenderer.initialize({1920,1080,true},error))throw std::runtime_error(error);
                darkRenderer.render(snapshot);
                if(!darkRenderer.capture({(output/(enabled?"pocket_lamp_off_enabled.ppm":"pocket_lamp_off_disabled.ppm")).string(),1920,1080},error))throw std::runtime_error(error);
                darkRenderer.shutdown();
            }
            if(changedPixelCount(output/"pocket_lamp_off_enabled.ppm",output/"pocket_lamp_off_disabled.ppm")!=0)
                throw std::runtime_error("pocket indirect remains active with lamp off");
        }
    }
    std::sort(frameMilliseconds.begin(), frameMilliseconds.end());
    const double p95 = frameMilliseconds[
        static_cast<std::size_t>(frameMilliseconds.size() * 95 / 100)];
    const double textureMiB = static_cast<double>(
        renderer.pbrUpload().uploadedGpuBytes + renderer.pbrUpload().environmentGpuBytes)
        / (1024.0 * 1024.0);
    std::ofstream evidence(output / "pocket_gpu_metrics.json");
    evidence << "{\n"
             << "  \"evidence_scope\": \"phase2_pocket_v10c_single_scene_non_formal\",\n"
             << "  \"formal_pass_claimed\": false,\n"
             << "  \"visual_review_status\": \"PENDING_TA\",\n"
             << "  \"renderer\": \"modern\",\n"
             << "  \"backend\": \"OpenGL 4.1 Core\",\n"
             << "  \"production_gltf\": true,\n"
             << "  \"width\": 1920,\n"
             << "  \"height\": 1080,\n"
             << "  \"quality\": \"medium\",\n"
             << "  \"msaa_samples\": 4,\n"
             << "  \"camera\": ";
    writeRuntimeCamera(evidence,snapshot);
    evidence << ",\n  \"review_pocket_id\": " << reviewId << ",\n"
             << "  \"warmup_frames\": " << warmupFrames << ",\n"
             << "  \"sample_frames\": " << measuredFrames << ",\n"
             << "  \"pocket_only_ui_pass_draw_calls\": 0,\n"
             << "  \"full_frame_ui_delta_nonzero_pixels\": 0,\n"
             << "  \"p95_total_frame_ms\": " << p95 << ",\n"
             << "  \"pocket_indirect_enabled\": " << (indirect?"true":"false") << ",\n"
             << "  \"pocket_indirect_disabled_p95_ms\": " << disabledP95 << ",\n"
             << "  \"draw_calls_max\": " << maximumDrawCalls << ",\n"
             << "  \"triangles_max\": " << maximumTriangles << ",\n"
             << "  \"texture_resident_mib\": " << textureMiB << ",\n"
             << "  \"measurement_method\": \"gpu_timer_query_plus_cpu_frame\"\n"
             << "}\n";
    if (!evidence)
        throw std::runtime_error("pocket-only GPU metrics write failed");
    std::ofstream binding(output/"capture_binding.json");
    binding.precision(9);
    std::uint32_t executablePathSize=4096;
    std::vector<char> executablePath(executablePathSize);
    if(_NSGetExecutablePath(executablePath.data(),&executablePathSize)!=0){
        executablePath.resize(executablePathSize);
        if(_NSGetExecutablePath(executablePath.data(),&executablePathSize)!=0)
            throw std::runtime_error("cannot bind capture executable path");
    }
    binding<<"{\n  \"scope\":\"runtime_render_snapshot_receipt_non_formal\",\n"
           <<"  \"formal_pass_claimed\":false,\n  \"default_promoted\":false,\n"
           <<"  \"camera_factory\":\""<<(reviewId<0?"pocketCloseupSnapshot_B4x":"pocketReviewSnapshot_supplemental")<<"\",\n"
           <<"  \"review_pocket_id\":"<<reviewId<<",\n  \"camera\":";
    writeRuntimeCamera(binding,snapshot);
    binding<<",\n  \"asset_sha256\":\""<<sha256(options.gltfScenePath)<<"\",\n"
           <<"  \"manifest_sha256\":\""<<sha256(options.pbrAssetManifestPath)<<"\",\n"
           <<"  \"runtime_rig_sha256\":\""<<sha256(options.lightingRigPath)<<"\",\n"
           <<"  \"binary_sha256\":\""<<sha256(executablePath.data())<<"\",\n"
           <<"  \"capture_source_sha256\":\""<<sha256(std::string(BILLIARDGL_SOURCE_ROOT)+"/spikes/render_core_profile/modern_renderer_slice_smoke.mm")<<"\",\n"
           <<"  \"beauty_ppm_sha256\":\""<<sha256(output/"pocket_closeup.ppm")<<"\",\n"
           <<"  \"object_id_ppm_sha256\":\""<<sha256(output/"pocket_object_ids.ppm")<<"\",\n"
           <<"  \"pocket_indirect_enabled\":"<<(indirect?"true":"false")<<",\n"
           <<"  \"alignment_scope\":\"shipping_table_specs_static_mouth_anchors_not_dynamic_collision_proof\",\n"
           <<"  \"capture_center_plane_y_cm\":"
           <<billiardgl::kTableModelPlaySurfaceHeightCm-billiardgl::kChineseBallRadiusCm<<",\n"
           <<"  \"physical_mouth_anchors\":[";
    const auto projection=billiardgl::render::buildSharedCameraProjection(snapshot.camera,1920,1080);
    for(int id=0;id<6;++id){
        const float sx=(id==0||id==2||id==4)?-1.0f:1.0f;
        const float sz=id<2?-1.0f:1.0f;
        const bool corner=id<4;
        const float x=sx*billiardgl::kChinesePlayfieldWidthCm*0.5f;
        const float z=corner?sz*billiardgl::kChinesePlayfieldLengthCm*0.5f:0.0f;
        const float width=corner?billiardgl::kChineseCornerPocketMouthWidthCm:billiardgl::kChineseSidePocketMouthWidthCm;
        const float tx=corner?sx*0.70710678118f:0.0f;
        const float tz=corner?-sz*0.70710678118f:sx;
        if(id)binding<<",";
        binding<<"{\"pocket_id\":"<<id<<",\"mouth_width_cm\":"<<width<<",\"lips\":[";
        for(int side=0;side<2;++side){
            const float offset=(side?1.0f:-1.0f)*width*0.5f;
            const billiardgl::Point3 point{x+tx*offset,billiardgl::kTableModelPlaySurfaceHeightCm,z+tz*offset};
            billiardgl::render::PhysicalScreenPoint screen;
            const bool visible=billiardgl::render::clipToPhysicalScreen(projection,
                billiardgl::render::transformWorldToClip(projection,point),screen);
            if(side)binding<<",";
            binding<<"{\"world_cm\":["<<point.x<<","<<point.y<<","<<point.z<<"],\"projectable\":"<<(visible?"true":"false");
            if(visible)binding<<",\"screen_pixels\":["<<screen.x<<","<<screen.y<<"]";
            binding<<"}";
        }
        binding<<"]}";
    }
    binding<<"]\n}\n";
    if(!binding)throw std::runtime_error("runtime camera binding write failed");
    binding.close();
    if(const char* tracePath=std::getenv("BILLIARDGL_POCKET_DESCENT_TRACE")){
        NSError* jsonError=nil;
        NSData* traceData=[NSData dataWithContentsOfFile:[NSString stringWithUTF8String:tracePath]];
        if(!traceData)throw std::runtime_error("cannot read real physical descent trace");
        NSDictionary* trace=[NSJSONSerialization JSONObjectWithData:traceData options:0 error:&jsonError];
        if(jsonError||![trace isKindOfClass:[NSDictionary class]])
            throw std::runtime_error("invalid physical descent trace JSON");
        if(![trace[@"schema"] isEqual:@"six-pocket-physical-descent-v1"]
           ||![trace[@"state_source"] isEqual:@"production updatePhysics"]
           ||![trace[@"presentation_source"] isEqual:@"buildPresentationSnapshot(state,state,1)"])
            throw std::runtime_error("physical descent trace producer/schema mismatch");
        NSArray* fixtures=trace[@"fixtures"];
        if(![fixtures isKindOfClass:[NSArray class]])
            throw std::runtime_error("physical descent fixtures missing");
        const int selectedPocket=reviewId<0?3:reviewId;
        NSDictionary* selectedFixture=nil;
        for(NSDictionary* fixture in fixtures)
            if([fixture[@"pocket_id"] intValue]==selectedPocket)selectedFixture=fixture;
        if(!selectedFixture)throw std::runtime_error("selected physical descent fixture absent");
        if(std::fabs([selectedFixture[@"radius_cm"] doubleValue]-billiardgl::kChineseBallRadiusCm)>1e-5
           ||std::fabs([selectedFixture[@"cloth_y_cm"] doubleValue]-billiardgl::kTableModelPlaySurfaceHeightCm)>1e-5)
            throw std::runtime_error("physical descent table/ball dimensions mismatch");
        NSArray* checkpoints=selectedFixture[@"checkpoints"];
        if(![checkpoints isKindOfClass:[NSArray class]]||[checkpoints count]!=5)
            throw std::runtime_error("physical descent must supply five real checkpoints");
        const auto bindingPath=output/"capture_binding.json";
        NSData* bindingData=[NSData dataWithContentsOfFile:[NSString stringWithUTF8String:bindingPath.c_str()]];
        NSDictionary* staticBinding=[NSJSONSerialization JSONObjectWithData:bindingData options:0 error:&jsonError];
        if(jsonError||![staticBinding isKindOfClass:[NSDictionary class]])
            throw std::runtime_error("cannot read completed static capture binding");
        // Static A/B checks above deliberately interleave renderer instances.
        // Replay the real trajectory in a fresh production instance rather
        // than inheriting a deleted dark-rig instance's texture bindings.
        billiardgl::render::ModernRenderer descentRenderer(options);
        if(!descentRenderer.initialize({1920,1080,true},error))
            throw std::runtime_error("real descent renderer initialize: "+error);
        NSMutableArray* frameReceipts=[NSMutableArray array];
        int frameIndex=0;
        for(NSDictionary* checkpoint in checkpoints){
            NSArray* position=checkpoint[@"position_cm"];
            const int identity=[checkpoint[@"ball_identity"] intValue];
            if(![position isKindOfClass:[NSArray class]]||[position count]!=3||identity!=1)
                throw std::runtime_error("invalid real descent ball checkpoint");
            auto descent=snapshot;
            for(auto& ball:descent.balls)ball.visible=ball.castsShadow=false;
            auto& ball=descent.balls[identity];
            ball.identity=identity;
            ball.position={[position[0] floatValue],[position[1] floatValue],[position[2] floatValue]};
            ball.visible=[checkpoint[@"visible"] boolValue];
            ball.castsShadow=[checkpoint[@"casts_shadow"] boolValue];
            ball.pocketed=[checkpoint[@"pocketed"] boolValue];
            if(!std::isfinite(ball.position.x)||!std::isfinite(ball.position.y)||!std::isfinite(ball.position.z)
               ||ball.visible!=(frameIndex<4))
                throw std::runtime_error("real descent visibility/position contract invalid");
            descentRenderer.render(descent);
            const std::string file="descent_frame_"+std::to_string(frameIndex)+".ppm";
            if(!descentRenderer.capture({(output/file).string(),1920,1080},error))throw std::runtime_error(error);
            NSMutableDictionary* frame=[checkpoint mutableCopy];
            frame[@"file"]=[NSString stringWithUTF8String:file.c_str()];
            frame[@"beauty_ppm_sha256"]=[NSString stringWithUTF8String:sha256(output/file).c_str()];
            [frameReceipts addObject:frame];
            ++frameIndex;
        }
        NSMutableDictionary* receipt=[staticBinding mutableCopy];
        receipt[@"scope"]=@"real_updatePhysics_120hz_checkpoint_replay_gpu_non_formal";
        receipt[@"trajectory_scope"]=@"accepted_vertical_shaft_center_descent_not_horizontal_roll_in_or_all_legal_paths";
        receipt[@"state_source"]=trace[@"state_source"];
        receipt[@"presentation_source"]=trace[@"presentation_source"];
        receipt[@"trace_sha256"]=[NSString stringWithUTF8String:sha256(tracePath).c_str()];
        receipt[@"physical_fixture"]=selectedFixture;
        receipt[@"rendered_checkpoint_receipts"]=frameReceipts;
        receipt[@"collision_review_status"]=@"PENDING_TA_REAL_DESCENT_FRAMES";
        NSData* receiptData=[NSJSONSerialization dataWithJSONObject:receipt options:NSJSONWritingPrettyPrinted error:&jsonError];
        const auto receiptPath=output/"descent_capture_binding.json";
        if(jsonError||![receiptData writeToFile:[NSString stringWithUTF8String:receiptPath.c_str()] atomically:YES])
            throw std::runtime_error("real descent GPU receipt write failed");
        descentRenderer.shutdown();
    }
    renderer.shutdown();
}

void verifyMissingTextureFallback()
{
    const std::string phase2=phase2AssetRoot();
    auto package=billiardgl::render::loadPbrPackageDescription(
        phase2+"focused_practice_hall_pbr_v1.params.json",
        phase2+"focused_practice_hall_modular_v1.asset.json",
        phase2+"focused_practice_hall_lighting_rig_v1.json");
    const std::string materialName=package.materials.front().name;
    package.materials.front().normal.path += ".intentionally-missing";
    billiardgl::render::PbrTextureResources resources;
    resources.load(package,16);
    const auto* material=resources.material(materialName);
    if(material==nullptr || !material->errorFallback || resources.report().warnings.size()<2)
        throw std::runtime_error("missing texture did not select explicit checker/error material");
    const auto* felt=resources.material("felt");
    if(felt==nullptr)throw std::runtime_error("felt upload missing");
    const auto internalFormat=[](GLuint texture){
        GLint value=0;glBindTexture(GL_TEXTURE_2D,texture);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&value);
        return value;
    };
    const auto mipWidth=[](GLuint texture){
        GLint value=0;glBindTexture(GL_TEXTURE_2D,texture);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,1,GL_TEXTURE_WIDTH,&value);
        return value;
    };
    if(internalFormat(felt->baseColor.texture.id())!=GL_SRGB8_ALPHA8
       || internalFormat(felt->normal.texture.id())!=GL_RGBA8
       || internalFormat(felt->orm.texture.id())!=GL_RGBA8
       || internalFormat(resources.ballAtlas().texture.id())!=GL_SRGB8_ALPHA8
       || mipWidth(felt->baseColor.texture.id())<=0)
        throw std::runtime_error("PBR upload color-space or mip contract failed");
    resources.reset();
}

}  // namespace

int main(int argc, const char* argv[])
{
    @autoreleasepool {
        try {
            if (argc != 2) throw std::runtime_error("usage: modern_renderer_slice_smoke OUTPUT_DIR");
            const NSOpenGLPixelFormatAttribute attributes[] = {
                NSOpenGLPFAOpenGLProfile,
                static_cast<NSOpenGLPixelFormatAttribute>(NSOpenGLProfileVersion4_1Core),
                NSOpenGLPFAAccelerated,
                static_cast<NSOpenGLPixelFormatAttribute>(0)
            };
            NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
            NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
            if (format == nil || context == nil) throw std::runtime_error("OpenGL 4.1 Core unavailable");
            [context makeCurrentContext];
            if (const char* shadowOnly =
                    std::getenv("BILLIARDGL_GLTF_SHADOW_ONLY");
                shadowOnly != nullptr && std::string(shadowOnly) == "1") {
                SeamlessCubemapTestStateGuard stateGuard;
                verifyGltfShadowMapGpu(argv[1]);
                std::cout << "modern-renderer-gltf-shadow: pass\n";
                return 0;
            }
            if (const char* stateOnly =
                    std::getenv("BILLIARDGL_SEAMLESS_STATE_ONLY");
                stateOnly != nullptr && std::string(stateOnly) == "1") {
                SeamlessCubemapTestStateGuard stateGuard;
                verifySeamlessCubemapFailureRestoration();
                verifySeamlessCubemapSuccessRestoration();
                std::cout << "modern-renderer-seamless-state: default+rollback+failure restore pass\n";
                return 0;
            }
            if (const char* pocketOnly = std::getenv("BILLIARDGL_POCKET_ONLY");
                pocketOnly != nullptr && std::string(pocketOnly) == "1") {
                renderPocketOnly(argv[1]);
                [NSOpenGLContext clearCurrentContext];
                std::cout << "modern-renderer-slice-smoke: pocket-only 1080p medium=4x\n";
                return 0;
            }
            if(const char* sixOnly=std::getenv("BILLIARDGL_MEDIUM_SIX_ONLY");
               sixOnly!=nullptr&&std::string(sixOnly)=="1"){
                SeamlessCubemapTestStateGuard stateGuard;
                std::uint64_t breakHash=0,aimHash=0;
                renderConfiguration(4,billiardgl::render::RenderQuality::Medium,"medium_candidate",argv[1],breakHash,aimHash);
                if(!breakHash||breakHash==aimHash)throw std::runtime_error("medium six-scene break/aim evidence invalid");
                [NSOpenGLContext clearCurrentContext];
                std::cout<<"modern-renderer-slice-smoke: six-scene medium=4x capture/gates pass; formal approval separate\n";
                return 0;
            }
            {
                SeamlessCubemapTestStateGuard stateGuard;
                verifySeamlessCubemapFailureRestoration();

                std::uint64_t break0 = 0, aim0 = 0, break2 = 0, aim2 = 0;
                std::uint64_t break4 = 0, aim4 = 0, breakHigh=0,aimHigh=0;
                // The first renderer proves the production default enables
                // seamless sampling from an initially disabled GL state.
                unsetenv("BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC");
                glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
                renderConfiguration(0,billiardgl::render::RenderQuality::Low,"low",argv[1],break0,aim0);

                // The second proves explicit diagnostic rollback from an
                // initially enabled state and exact shutdown restoration.
                setenv("BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC", "0", 1);
                glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
                renderConfiguration(2,billiardgl::render::RenderQuality::Low,"low_2x",argv[1],break2,aim2);

                unsetenv("BILLIARDGL_CUBEMAP_SEAMLESS_DIAGNOSTIC");
                glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
                renderConfiguration(4,billiardgl::render::RenderQuality::Medium,"medium",argv[1],break4,aim4);
                renderConfiguration(4,billiardgl::render::RenderQuality::High,"high",argv[1],breakHigh,aimHigh);
                verifyMissingTextureFallback();
                if (break0 == aim0 || break2 == aim2 || break4 == aim4 || breakHigh==aimHigh
                    || break0 == 0 || break2 == 0 || break4 == 0) {
                    throw std::runtime_error("snapshot transform/visibility did not change capture evidence");
                }
            }
            [NSOpenGLContext clearCurrentContext];
            std::cout << "modern-renderer-slice-smoke: pass break+aim 1080p low=off+2x medium=4x high=4x\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "modern-renderer-slice-smoke: " << error.what() << '\n';
            return 1;
        }
    }
}
