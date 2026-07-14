#include "automation_controller.h"
#include "ball_ball_contact.h"

#include <cstdlib>
#include <iostream>

namespace {
void expect(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; std::exit(1); } }
billiardgl::ControllerResult send(billiardgl::AutomationController& c, int id, const char* command, const billiardgl::json::Value& p)
{ billiardgl::AutomationRequest r; r.id=id; r.version=1; r.command=command; r.params=p; return c.handle(r); }

billiardgl::json::Value setBall(int index, double x, double z, double vx, double vz)
{
    billiardgl::json::Value p=billiardgl::json::Value::object(); p["index"]=billiardgl::json::Value(index);
    billiardgl::json::Value pos=billiardgl::json::Value::object(); pos["x"]=billiardgl::json::Value(x); pos["y"]=billiardgl::json::Value(92.715); pos["z"]=billiardgl::json::Value(z);
    billiardgl::json::Value vel=billiardgl::json::Value::object(); vel["x"]=billiardgl::json::Value(vx); vel["y"]=billiardgl::json::Value(0); vel["z"]=billiardgl::json::Value(vz);
    p["position"]=pos; p["velocity"]=vel; return p;
}
}

int main()
{
    billiardgl::GameRuntime runtime;
    runtime.setPhysicsTraceEnabled(true);
    billiardgl::AutomationController controller(runtime, billiardgl::AutomationMode::Headless);
    expect(send(controller,1,"set_ball",setBall(0,-5,0,20,0)).response.at("ok").asBool(), "set cue ball");
    expect(send(controller,2,"set_ball",setBall(1,5,0,0,0)).response.at("ok").asBool(), "set object ball");
    billiardgl::json::Value wait=billiardgl::json::Value::object(); wait["condition"]=billiardgl::json::Value("ball_collision"); wait["max_steps"]=billiardgl::json::Value(20);
    expect(send(controller,3,"run_until",wait).response.at("ok").asBool(), "collision wait should succeed");
    expect(runtime.eventsSince(0).size() >= 1, "collision should be recorded");
    expect(runtime.state().balls[1].velocity.x > 0.0f, "collision should transfer velocity");
    expect(!runtime.physicsTrace().frames().empty() &&
        runtime.physicsTrace().frames().back().contacts.size() == 1 &&
        runtime.physicsTrace().frames().back().contacts[0].normalImpulseNs > 0.0,
        "headless automation should expose the production rigid impulse");

    const std::uint64_t before=runtime.tick(); billiardgl::json::Value step=billiardgl::json::Value::object(); step["ticks"]=billiardgl::json::Value(3);
    send(controller,4,"step",step); expect(runtime.tick()==before+3, "step should be exact");
    expect(!send(controller,5,"set_ball",setBall(16,0,0,0,0)).response.at("ok").asBool(), "invalid index should fail");

    billiardgl::json::Value impossible=billiardgl::json::Value::object(); impossible["condition"]=billiardgl::json::Value("eight_ball_pocketed"); impossible["max_steps"]=billiardgl::json::Value(1);
    const billiardgl::ControllerResult timeout=send(controller,6,"run_until",impossible);
    expect(!timeout.response.at("ok").asBool(), "bounded wait should time out");
    expect(timeout.response.at("error").at("code").asString()=="condition_not_met", "timeout code should be stable");
    return 0;
}
