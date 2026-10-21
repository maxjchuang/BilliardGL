#if __has_include("support/six_pocket_horizontal_entry_fixture.h")
#include "support/six_pocket_horizontal_entry_fixture.h"
#define HORIZONTAL_FIXTURE_PRESENT 1
#else
#include "support/pocket_descent_capture_fixture.h"
#endif
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        for (int id=0;id<6;++id) {
#ifdef HORIZONTAL_FIXTURE_PRESENT
            const auto fixture=billiardgl::test::buildSixPocketHorizontalEntryFixture(id);
#else
            const auto fixture=billiardgl::test::buildPocketDescentCaptureFixture(id);
#endif
            const auto& first=fixture.steps.front().state.balls[1];
            const auto& profile=fixture.profile;
            if(first.physicalDomain!=billiardgl::PhysicalBallDomain::ClothSupported ||
                std::abs(first.position.y-profile.tableDeparture.clothSurfaceYCm-profile.ball.radiusCm)>.001f ||
                first.velocity.y!=0)
                throw std::runtime_error("fixture must start cloth-supported with horizontal velocity, not vertical descent");
#ifdef HORIZONTAL_FIXTURE_PRESENT
            const auto frame=billiardgl::buildPocketBoundaryFrames(profile.tableBoundary)[id];
            const auto local=billiardgl::pocketLocalPoint(frame,first.position);
            if(std::abs(local.depthCm+12)>.001 || std::abs(local.offsetCm)>.001 ||
                std::abs(std::hypot(first.velocity.x,first.velocity.z)-40)>.001 ||
                std::abs(first.angularVelocity.x-first.velocity.z/profile.ball.radiusCm)>.001 ||
                std::abs(first.angularVelocity.z+first.velocity.x/profile.ball.radiusCm)>.001)
                throw std::runtime_error("initial mouth centerline/speed/rolling spin contract");
            if(fixture.checkpoints.size()!=6)throw std::runtime_error("six fixed checkpoint slots required");
            auto replay=fixture.steps.front().state;
            for(std::size_t index=1;index<fixture.steps.size();++index) {
                (void)billiardgl::updatePhysics(replay,1.0f/120.0f,profile);
                const auto& a=replay.balls[1];const auto& b=fixture.steps[index].state.balls[1];
                const auto same=[](const billiardgl::Point3& x,const billiardgl::Point3& y) {
                    return x.x==y.x && x.y==y.y && x.z==y.z;
                };
                if(!same(a.position,b.position) || !same(a.velocity,b.velocity) ||
                    !same(a.angularVelocity,b.angularVelocity) || a.physicalDomain!=b.physicalDomain ||
                    a.pocketed!=b.pocketed || a.trajectoryGeneration!=b.trajectoryGeneration)
                    throw std::runtime_error("fixture state differs from independent unedited production replay");
            }
            for(const auto& step:fixture.steps) {
                const auto direct=billiardgl::buildPresentationSnapshot(step.state,step.state,1.0);
                const auto& expected=direct.balls[1];const auto& actual=step.presentation.balls[1];
                if(actual.position.x!=expected.position.x || actual.position.y!=expected.position.y ||
                    actual.position.z!=expected.position.z || actual.visible!=expected.visible ||
                    actual.castsShadow!=expected.castsShadow)
                    throw std::runtime_error("presentation must be the actual production snapshot");
                for(int ball=0;ball<16;++ball)
                    if(ball!=1 && step.presentation.balls[ball].visible)
                        throw std::runtime_error("other balls must remain hidden");
            }
#endif
        }
        std::cout<<"horizontal initial-state/source correctness PASS; capture success is not asserted\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
