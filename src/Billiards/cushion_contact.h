#pragma once

#include "game_state.h"
#include "physics_profile.h"

#include <array>

namespace billiardgl {

enum class CushionContactRegime {
    NoContact,
    Separating,
    Frictionless,
    Stick,
    Slip
};

struct CushionContactResult {
    CushionContactRegime regime = CushionContactRegime::NoContact;
    bool velocityImpulseApplied = false;
    bool positionCorrected = false;
    bool rigidDomainExceeded = false;
    double penetrationM = 0.0;
    double positionSlopM = 0.0000001;
    double restitution = 0.0;
    double frictionCoefficient = 0.0;
    double noseHeightRatio = 1.0;
    double incidentSpeedMS = 0.0;
    double maximumRigidIncidentSpeedMS = 0.0;
    double normalImpulseNs = 0.0;
    double tangentialImpulseNs = 0.0;
    double normalRelativeSpeedBeforeMS = 0.0;
    double normalRelativeSpeedAfterMS = 0.0;
    double kineticEnergyBeforeJ = 0.0;
    double kineticEnergyAfterJ = 0.0;
    std::array<double, 3> contactNormal{{0.0, 0.0, 0.0}};
    std::array<double, 3> contactTangent{{0.0, 0.0, 0.0}};
    std::array<double, 3> contactArmM{{0.0, 0.0, 0.0}};
    std::array<double, 3> contactVelocityBeforeMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> contactVelocityAfterMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> impulseOnBallNs{{0.0, 0.0, 0.0}};
    std::array<double, 3> positionCorrectionM{{0.0, 0.0, 0.0}};
    std::array<double, 3> linearVelocityChangeMS{{0.0, 0.0, 0.0}};
    std::array<double, 3> angularVelocityChangeRadS{{0.0, 0.0, 0.0}};
};

CushionContactResult resolveCushionContact(
    BallState& ball, const Point3& inwardNormal, double penetrationM,
    const BallProperties& ballProperties,
    const CushionProperties& cushionProperties);
const char* cushionContactRegimeName(CushionContactRegime regime);

}  // namespace billiardgl
