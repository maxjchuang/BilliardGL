#include "training/trajectory_inset_renderer.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cmath>
#include <map>
using namespace billiardgl::training;
void expect(bool value,const std::string& why){if(!value)throw std::runtime_error(why);}

void check(const TrajectoryInsetDrawPackage& package,const char* fixture){
    std::string error;
    expect(validateTrajectoryInsetVisitEvidence(package,error),"visit validator: "+error);
    std::vector<std::size_t> marked;
    for(std::size_t i=0;i<package.probes.size();++i)
        if(!package.probes[i].sourceCoreMemberKey.empty())marked.push_back(i);
    expect(!marked.empty(),"original failed event must acquire visible marker evidence");
    const auto raster=rasterTrajectoryInsetSemantics(package,trajectoryInsetSampleProfile1x());
    for(auto i:marked){
        const auto& p=package.probes[i];
        const char* pattern=p.visitLabel=="1"?"010110010010111":
            p.visitLabel=="2"?"111001111100111":p.visitLabel=="T"?"111010010010010":nullptr;
        expect(pattern!=nullptr,"bounded regressions use independently readable 1/2/T markers");
        const float u=package.layout.uiScale;
        for(int row=0;row<5;++row)for(int col=0;col<3;++col){
            const float x=p.visitMarkerRect.x+p.visitMarkerRect.width*.5f-4.8f*u+(col+.5f)*3.2f*u;
            const float y=p.visitMarkerRect.y+2*u+(row+.5f)*3.2f*u;
            const int px=static_cast<int>(std::floor(x-package.layout.plotRect.x));
            const int py=static_cast<int>(std::floor(y-package.layout.plotRect.y));
            expect((raster.perSampleTokens[py*raster.width+px]==p.insetOwnershipToken)==(pattern[row*3+col]=='1'),
                "visible glyph differs from the declared ordinal/T shape");
        }
    }
    for(const auto& event:package.predictedDashEvents)
        expect(std::count_if(package.probes.begin(),package.probes.end(),[&](const auto& p){
            return p.parentSourceKey==event.eventKey;})==1,"every original event retains one independent probe");
    if(std::string(fixture)=="retrace"){
        expect(marked.size()==2 && package.probes[marked[0]].visitLabel=="1" &&
            package.probes[marked[1]].visitLabel=="2", "two retraced visits have distinct visible ordinal identities");
        expect(!package.probes[marked[0]].sourceCoreVisible && package.probes[marked[1]].sourceCoreVisible,
            "covered first core remains truthfully hidden; later core remains visible");
    }else expect(marked.size()==1 && package.probes[marked[0]].visitLabel=="T" &&
        !package.probes[marked[0]].sourceCoreVisible,"subpixel tail is marked without inventing visible core");
    auto bad=package;
    const auto selected=marked.front();
    bad.probes[selected].sourceCoreMemberKey.clear();
    expect(!validateTrajectoryInsetVisitEvidence(bad,error),"marker cannot discard original hidden-core provenance");
    bad=package;
    for(auto& vertex:bad.vertices)if(vertex.insetOwnershipToken==bad.probes[selected].insetOwnershipToken)
        vertex.insetOwnershipToken=0;
    expect(!validateTrajectoryInsetVisitEvidence(bad,error),"deleted badge cannot pass");
    bad=package;bad.probes[selected].visitLabel="9";
    expect(!validateTrajectoryInsetVisitEvidence(bad,error),"wrong visit order cannot pass");
    bad=package;bad.probes[selected].sourceCoreVisible=!bad.probes[selected].sourceCoreVisible;
    expect(!validateTrajectoryInsetVisitEvidence(bad,error),"covered core cannot be reported visible");
    bad=package;bad.probes[selected].sourceAnchor[0]+=100;
    expect(!validateTrajectoryInsetVisitEvidence(bad,error),"leader cannot invent a new canonical anchor");
    bad=package;bad.probes[selected].visitMarkerRect.x=-100;
    expect(!validateTrajectoryInsetVisitEvidence(bad,error),"badge outside plot cannot pass");
    if(marked.size()>1){
        bad=package;
        auto first=std::find_if(bad.semanticBatches.begin(),bad.semanticBatches.end(),
            [&](const auto& b){return b.insetMemberKey==bad.probes[marked[0]].insetMemberKey;});
        auto second=std::find_if(bad.semanticBatches.begin(),bad.semanticBatches.end(),
            [&](const auto& b){return b.insetMemberKey==bad.probes[marked[1]].insetMemberKey;});
        second->firstVertex=first->firstVertex;
        expect(!validateTrajectoryInsetVisitEvidence(bad,error),"second event cannot reuse first event vertex range after leader insertion");
    }
    bad=package;
    const auto endpoint=std::find_if(bad.semanticBatches.begin(),bad.semanticBatches.end(),
        [](const auto& b){return b.role==TrajectoryInsetRole::PredictedEndpointDiamond;});
    expect(endpoint!=bad.semanticBatches.end(),"canonical endpoint retained");
    bad.probes[selected].insetMemberKey=endpoint->insetMemberKey;
    bad.probes[selected].insetOwnershipToken=endpoint->insetOwnershipToken;
    expect(!validateTrajectoryInsetVisitEvidence(bad,error),"endpoint cannot impersonate a visit badge");
    bad=package;
    auto event=std::find_if(bad.predictedDashEvents.begin(),bad.predictedDashEvents.end(),
        [&](const auto& e){return e.eventKey==bad.probes[selected].parentSourceKey;});
    bad.predictedDashEvents.erase(event);
    expect(!validateTrajectoryInsetVisitEvidence(bad,error),"removing original event cannot pass");
    std::cout<<fixture<<" events="<<package.predictedDashEvents.size()<<" probes="<<package.probes.size()
        <<" markers="<<marked.size()<<" palette="<<static_cast<int>(package.palette)<<" PASS\n";
}

TrajectoryInsetBuildRequest load(const char* name) {
    std::ifstream input(std::string("tests/fixtures/trajectory_inset_")+name+"_20260930.txt");
    if(!input) throw std::runtime_error("fixture missing");
    TrajectoryInsetBuildRequest r;
    r.generationId="v19_generation_1";r.layout.profileId=name;
    std::string tag; input>>tag;
    if(tag!="FIX_LAYOUT")throw std::runtime_error("layout fixture");
    auto& l=r.layout; auto& p=l.panelRect;auto& q=l.plotRect;
    input>>l.physicalWidth>>l.physicalHeight>>l.uiScale>>p.x>>p.y>>p.width>>p.height
        >>q.x>>q.y>>q.width>>q.height;
    for(int t=0;t<2;++t){
        int id;std::size_t count; input>>tag>>id;
        if(tag!="FIX_TRACK"||id!=t)throw std::runtime_error("track fixture");
        auto& track=t?r.snapshot.actual:r.snapshot.predicted;
        input>>track.visible>>track.endpointVisible>>track.endpoint.x>>track.endpoint.y
            >>track.endpoint.z>>track.railCollisionEvents>>count;
        for(std::size_t i=0;i<count;++i){
            OverlaySample s;input>>tag>>s.tick>>s.position.x>>s.position.y>>s.position.z>>s.visible;
            if(tag!="FIX_SAMPLE")throw std::runtime_error("sample fixture");
            track.samples.push_back(s);
        }
    }
    input>>tag;if(!input||tag!="FIX_END")throw std::runtime_error("fixture truncated");
    return r;
}

void checkActualTerminalOrder(){
 for(auto palette:{TrajectoryInsetPalette::Standard,TrajectoryInsetPalette::ColorVisionSafe}){
    auto r=load("actual_terminal_v18");r.palette=palette;
    const auto package=buildTrajectoryInsetDrawPackage(r);
    expect(package.ok,"v18 actual terminal: "+package.errorCode+":"+package.probeResolutionFailureMember);
    expect(package.actualMidpointWitnesses.size()==23,"all 23 actual source intervals remain");
    const auto parent=package.actualMidpointWitnesses.back().parentSourceKey;
    std::vector<std::size_t> indices;
    for(std::size_t i=0;i<package.semanticBatches.size();++i){const auto& b=package.semanticBatches[i];
        if(b.parentSourceKey==parent && (b.role==TrajectoryInsetRole::ActualLeftCore ||
            b.role==TrajectoryInsetRole::ActualRightCore || b.role==TrajectoryInsetRole::ActualEndpointCapsule))indices.push_back(i);
    }
    expect(indices.size()==4,"exact four-member terminal block");
    expect(package.semanticBatches[indices[0]].role==TrajectoryInsetRole::ActualEndpointCapsule &&
        package.semanticBatches[indices[1]].role==TrajectoryInsetRole::ActualEndpointCapsule &&
        package.semanticBatches[indices[2]].role==TrajectoryInsetRole::ActualLeftCore &&
        package.semanticBatches[indices[3]].role==TrajectoryInsetRole::ActualRightCore,
        "only terminal capsules precede their matching cores");
    auto legacy=package;
    const auto low=package.semanticBatches[indices.front()].firstVertex;
    const auto split=package.semanticBatches[indices[2]].firstVertex;
    const auto& last=package.semanticBatches[indices.back()];
    const auto high=last.firstVertex+last.vertexCount;
    expect(high-low==384,"v18 terminal 384 vertices retained");
    std::rotate(legacy.vertices.begin()+low,legacy.vertices.begin()+split,legacy.vertices.begin()+high);
    std::rotate(legacy.semanticBatches.begin()+indices[0],legacy.semanticBatches.begin()+indices[2],legacy.semanticBatches.begin()+indices[3]+1);
    auto cursor=low;for(auto i:indices){legacy.semanticBatches[i].firstVertex=cursor;cursor+=legacy.semanticBatches[i].vertexCount;}
    const auto equalVertices=[](const auto& a,const auto& b){return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),
        [](const auto& x,const auto& y){return x.xPhysicalPx==y.xPhysicalPx && x.yPhysicalPx==y.yPhysicalPx &&
            x.linearRgba==y.linearRgba && x.insetOwnershipToken==y.insetOwnershipToken;});};
    auto reproduced=legacy;std::string error;
    expect(applyActualTerminalOwnershipOrder(reproduced,parent,error) && equalVertices(reproduced.vertices,package.vertices),
        "one local permutation changes no vertex/color/token data");
    expect(reproduced.orderedParentSourceKeys==package.orderedParentSourceKeys &&
        reproduced.orderedProbeIds==package.orderedProbeIds && reproduced.probeDenominator==package.probeDenominator,
        "source and probe identities are not regenerated by local order");
    const auto retained=reproduced.vertices;
    expect(!applyActualTerminalOwnershipOrder(reproduced,parent,error) &&
        error=="trajectory_inset_terminal_order_already_applied" && equalVertices(retained,reproduced.vertices),
        "reapplying terminal correction is a rejected no-op");
    auto bad=legacy;bad.semanticBatches[indices[0]].parentSourceKey="foreign";
    expect(!applyActualTerminalOwnershipOrder(bad,parent,error) && equalVertices(bad.vertices,legacy.vertices),
        "foreign parent cannot enter local reorder");
    bad=legacy;bad.vertices[bad.semanticBatches[indices[2]].firstVertex].linearRgba[0]=.9f;
    const auto colored=bad.vertices;
    expect(!applyActualTerminalOwnershipOrder(bad,parent,error) && equalVertices(bad.vertices,colored),
        "different colors reject before mutating order");
    std::map<std::uint32_t,std::array<float,4>> colors;
    for(const auto& b:package.semanticBatches)colors[b.insetOwnershipToken]=package.vertices[b.firstVertex].linearRgba;
    for(const auto profile:{trajectoryInsetSampleProfile1x(),trajectoryInsetSampleProfile2x(),trajectoryInsetSampleProfile4x()}){
        const auto before=rasterTrajectoryInsetSemantics(legacy,profile);
        const auto after=rasterTrajectoryInsetSemantics(package,profile);
        expect(before.ok&&after.ok,"real semantic rasters");
        std::size_t colorChanges=0;
        for(std::size_t p=0;p<after.perSampleTokens.size();++p)
            colorChanges+=colors[before.perSampleTokens[p]]!=colors[after.perSampleTokens[p]];
        expect(colorChanges==0,"standard/CVD resolved sample RGBA is byte-identical");
        if(profile.positions.size()==1){const auto token=package.semanticBatches[indices[3]].insetOwnershipToken;
            expect(std::count(before.perSampleTokens.begin(),before.perSampleTokens.end(),token)==0,
                "old order remains the original 1x missing-core counterexample");}
        for(auto i:indices){const auto token=package.semanticBatches[i].insetOwnershipToken;
            expect(std::count(after.perSampleTokens.begin(),after.perSampleTokens.end(),token)>0,
                "all four members independently survive every real sample profile");}
    }
    std::cout<<"actual_terminal_v18 palette="<<static_cast<int>(palette)<<" members=4 samples=1/2/4 RGBA_unchanged PASS\n";
 }
}

int main(){try{
    checkActualTerminalOrder();
    for(const char* fixture:{"retrace","subpixel_tail"})
    for(auto palette:{TrajectoryInsetPalette::Standard,TrajectoryInsetPalette::ColorVisionSafe}){
        auto request=load(fixture);request.palette=palette;
        if(std::string(fixture)=="retrace"){
            auto control=request;
            control.layout={"same_source_ui100",2560,1440,1.f,
                {2080,1040,448,288},{2096,1088,416,208}};
            const auto normal=buildTrajectoryInsetDrawPackage(control);
            expect(normal.ok && std::none_of(normal.probes.begin(),normal.probes.end(),
                [](const auto& p){return !p.sourceCoreMemberKey.empty();}),
                "same true route at UI100 remains a passing no-marker control");
        }
        const auto package=buildTrajectoryInsetDrawPackage(request);
        if(!package.ok)throw std::runtime_error(std::string(fixture)+": "+package.errorCode+":"+package.probeResolutionFailureMember);
        check(package,fixture);
        const auto endpoint=std::find_if(package.semanticBatches.begin(),package.semanticBatches.end(),
            [](const auto& b){return b.role==TrajectoryInsetRole::PredictedEndpointDiamond;});
        float left=1e9f,right=-1e9f,top=1e9f,bottom=-1e9f;
        for(std::size_t i=endpoint->firstVertex;i<endpoint->firstVertex+endpoint->vertexCount;++i){
            const auto& v=package.vertices[i];left=std::min(left,v.xPhysicalPx);right=std::max(right,v.xPhysicalPx);
            top=std::min(top,v.yPhysicalPx);bottom=std::max(bottom,v.yPhysicalPx);
        }
        const auto& table=package.canonicalTableRect;
        const auto& world=request.snapshot.predicted.endpoint;
        expect(std::fabs((left+right)/2-(table.x+(world.z/254.f+.5f)*table.width))<.001f &&
            std::fabs((top+bottom)/2-(table.y+(world.x/127.f+.5f)*table.height))<.001f,
            "callout cannot move the true route endpoint");
        auto crowded=request;crowded.layout.plotRect.width=96;crowded.layout.plotRect.height=48;
        const float worldSpan=18.f*254.f/(2.f*(48.f-24.f*crowded.layout.uiScale));
        crowded.snapshot.predicted.samples={{0,{0,86.5f,-70},true},
            {1,{0,86.5f,-70+worldSpan},true},{2,{0,86.5f,-70},true}};
        crowded.snapshot.predicted.endpoint=crowded.snapshot.predicted.samples.back().position;
        crowded.snapshot.actual={};crowded.snapshot.actual.visible=false;
        const auto rejected=buildTrajectoryInsetDrawPackage(crowded);
        expect(!rejected.ok && rejected.errorCode=="trajectory_inset_visit_marker_no_safe_position",
            "insufficient plot space fails rather than hiding a visit or covering the route: "+rejected.errorCode);
    }
    std::cout<<"event visit marker cases PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
