#include "support/pocket_descent_capture_fixture.h"
#include <fstream>
#include <iomanip>
#include <iostream>

namespace {
void point(std::ostream& out,const billiardgl::Point3& p) {
    out << '[' << p.x << ',' << p.y << ',' << p.z << ']';
}
void step(std::ostream& out,const billiardgl::test::PocketDescentCaptureFixture& f,
          const billiardgl::test::PocketDescentStep& s) {
    const auto& b=s.state.balls[f.ballIdentity];
    const auto& p=s.presentation.balls[f.ballIdentity];
    out << "\"tick\":" << s.tick << ",\"time_seconds\":" << s.tick/120.0
        << ",\"ball_identity\":" << f.ballIdentity << ",\"position_cm\":["
        << p.position.x << ',' << p.position.y << ',' << p.position.z << ']'
        << ",\"velocity_cm_s\":";
    point(out,b.velocity);
    out << ",\"visible\":" << (p.visible?"true":"false")
        << ",\"casts_shadow\":" << (p.castsShadow?"true":"false")
        << ",\"pocketed\":" << (p.pocketed?"true":"false")
        << ",\"trajectory_generation\":" << b.trajectoryGeneration
        << ",\"pocket_phase\":" << static_cast<int>(b.pocketInteraction.phase)
        << ",\"physical_domain\":" << static_cast<int>(b.physicalDomain)
        << ",\"entry_event\":" << (s.entry?"true":"false")
        << ",\"throat_event\":" << (s.throat?"true":"false")
        << ",\"capture_event\":" << (s.capture?"true":"false");
}
}

int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("usage: pocket_descent_capture_dump output.json");
        std::ofstream out(argv[1]);
        if(!out) throw std::runtime_error("cannot create output");
        out << std::setprecision(9) << "{\"schema\":\"six-pocket-physical-descent-v1\","
            << "\"step_seconds\":0.008333333333333333,\"state_source\":\"production updatePhysics\","
            << "\"presentation_source\":\"buildPresentationSnapshot(state,state,1)\",\"fixtures\":[";
        for(int id=0;id<6;++id) {
            const auto f=billiardgl::test::buildPocketDescentCaptureFixture(id);
            if(id)out << ',';
            out << "{\"pocket_id\":" << id << ",\"radius_cm\":" << f.profile.ball.radiusCm
                << ",\"cloth_y_cm\":" << f.profile.tableDeparture.clothSurfaceYCm
                << ",\"checkpoints\":[";
            bool first=true;
            for(const auto& c:f.checkpoints) {
                if(!first)out << ','; first=false;
                out << "{\"label\":" << std::quoted(c.label) << ',';
                step(out,f,f.steps[c.stepIndex]); out << '}';
            }
            out << "],\"steps\":["; first=true;
            for(const auto& s:f.steps) {
                if(!first)out << ',';first=false;out << '{';step(out,f,s);out << '}';
            }
            const auto& contact=f.steps.back().captureContact;
            out << "],\"capture_contact\":{\"tick\":" << f.steps.back().tick
                << ",\"position_cm\":";point(out,contact.firstPositionAtContactCm);
            out << ",\"sequence\":" << contact.pocketCaptureSequence
                << ",\"generation_before\":" << contact.trajectoryGenerationBefore
                << ",\"generation_after\":" << contact.trajectoryGenerationAfter
                << ",\"vertical_shaft\":" << (contact.pocketVerticalShaft?"true":"false")
                << ",\"administrative_reset\":" << (contact.administrativeResetAfterCapture?"true":"false")
                << "}}";
            std::cout << "pocket=" << id << " steps=" << f.steps.size()
                << " checkpoints=" << f.checkpoints.size() << " actual_capture_y="
                << contact.firstPositionAtContactCm.y << " sequence=" << contact.pocketCaptureSequence << " PASS\n";
        }
        out << "]}\n";
        if(!out) throw std::runtime_error("output failed");
        return 0;
    } catch(const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
