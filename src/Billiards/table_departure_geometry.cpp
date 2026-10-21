#include "table_departure_geometry.h"

#include "ball_ball_contact.h"
#include "pocket_boundary.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>

namespace billiardgl {
namespace {

constexpr double kTimeEpsilon = 1e-7;

void advanceBallistic(BallState& ball, double time, float gravity);
void advanceSupported(BallState& ball, double time, float deceleration);

void advanceSupportedTorsionalSpin(BallState& ball, double time,
    float torsionalDeceleration)
{
    if (!(time > 0.0) || ball.angularVelocity.y == 0.0f) return;
    const double initial = ball.angularVelocity.y;
    const double direction = initial < 0.0 ? -1.0 : 1.0;
    const double magnitude = std::fabs(initial);
    const double duration = torsionalDeceleration > 0.0f
        ? std::min(time, magnitude / torsionalDeceleration) : time;
    const double integrated = direction *
        (magnitude * duration - 0.5 * torsionalDeceleration *
            duration * duration);
    const double finalMagnitude = torsionalDeceleration > 0.0f
        ? std::max(0.0, magnitude - torsionalDeceleration * time)
        : magnitude;
    ball.angularVelocity.y = static_cast<float>(direction * finalMagnitude);
    ball.rotationAngle += static_cast<float>(integrated * 180.0 /
        3.14159265358979323846);
}

void appendAnalyticSegment(TableDepartureStepResult& result, int ballIndex,
    double startOffset, double duration, const BallState& start,
    const Point3& acceleration, const BallState& end,
    BallMotionState motionState)
{
    if (!(duration > 0.0)) return;
    SurfaceMotionAnalyticSegment segment;
    segment.ballIndex = ballIndex;
    segment.trajectoryGeneration = start.trajectoryGeneration;
    segment.segmentOrdinal = static_cast<int>(std::count_if(
        result.analyticSegments.begin(), result.analyticSegments.end(),
        [ballIndex](const SurfaceMotionAnalyticSegment& existing) {
            return existing.ballIndex == ballIndex;
        }));
    segment.motionState = motionState;
    segment.wholeTickStartOffsetSeconds = startOffset;
    segment.durationSeconds = duration;
    segment.startPositionCm = start.position;
    segment.endPositionCm = end.position;
    segment.startVelocityCmS = start.velocity;
    segment.constantAccelerationCmS2 = acceleration;
    segment.analyticPathComplete = true;
    result.analyticSegments.push_back(segment);
}

void advanceBallisticRecorded(BallState& ball, double time, float gravity,
    TableDepartureStepResult& result, int ballIndex, double startOffset)
{
    if (!(time > 0.0)) return;
    const BallState start = ball;
    advanceBallistic(ball, time, gravity);
    appendAnalyticSegment(result, ballIndex, startOffset, time, start,
        Point3{0.0f, gravity, 0.0f}, ball, BallMotionState::Sliding);
}

void advanceSupportedRecorded(BallState& ball, double time,
    float deceleration, float torsionalDeceleration,
    TableDepartureStepResult& result, int ballIndex, double startOffset)
{
    if (!(time > 0.0)) return;
    const double speed = std::hypot(ball.velocity.x, ball.velocity.z);
    if (!(speed > 0.0)) {
        const BallState start = ball;
        advanceSupported(ball, time, deceleration);
        advanceSupportedTorsionalSpin(ball, time, torsionalDeceleration);
        appendAnalyticSegment(result, ballIndex, startOffset, time, start,
            Point3{}, ball, BallMotionState::Stationary);
        return;
    }

    const double movingDuration = deceleration > 0.0f
        ? std::min(time, speed / deceleration) : time;
    const BallState movingStart = ball;
    const Point3 movingAcceleration{
        static_cast<float>(-deceleration * ball.velocity.x / speed),
        0.0f,
        static_cast<float>(-deceleration * ball.velocity.z / speed)};
    advanceSupported(ball, movingDuration, deceleration);
    advanceSupportedTorsionalSpin(ball, movingDuration,
        torsionalDeceleration);
    appendAnalyticSegment(result, ballIndex, startOffset, movingDuration,
        movingStart, movingAcceleration, ball, BallMotionState::Rolling);

    const double stationaryDuration = time - movingDuration;
    if (stationaryDuration > 0.0) {
        const BallState stationaryStart = ball;
        advanceSupported(ball, stationaryDuration, deceleration);
        advanceSupportedTorsionalSpin(ball, stationaryDuration,
            torsionalDeceleration);
        appendAnalyticSegment(result, ballIndex,
            startOffset + movingDuration, stationaryDuration,
            stationaryStart, Point3{}, ball, BallMotionState::Stationary);
    }
}

void stableSortAnalyticSegments(TableDepartureStepResult& result)
{
    std::stable_sort(result.analyticSegments.begin(),
        result.analyticSegments.end(),
        [](const SurfaceMotionAnalyticSegment& first,
            const SurfaceMotionAnalyticSegment& second) {
            if (first.wholeTickStartOffsetSeconds !=
                    second.wholeTickStartOffsetSeconds)
                return first.wholeTickStartOffsetSeconds <
                    second.wholeTickStartOffsetSeconds;
            if (first.ballIndex != second.ballIndex)
                return first.ballIndex < second.ballIndex;
            return first.segmentOrdinal < second.segmentOrdinal;
        });
}

using Polynomial = std::vector<double>;

void trimPolynomial(Polynomial& coefficients)
{
    while (coefficients.size() > 1 && coefficients.back() == 0.0)
        coefficients.pop_back();
}

double evaluatePolynomial(const Polynomial& coefficients, double x)
{
    double value = 0.0;
    for (auto it = coefficients.rbegin(); it != coefficients.rend(); ++it)
        value = value * x + *it;
    return value;
}

double polynomialScale(const Polynomial& coefficients, double x)
{
    double scale = 0.0;
    double power = 1.0;
    for (double coefficient : coefficients) {
        scale += std::fabs(coefficient) * power;
        power *= std::fabs(x);
    }
    return std::max(1.0, scale);
}

Polynomial derivativePolynomial(const Polynomial& coefficients)
{
    Polynomial derivative;
    for (std::size_t power = 1; power < coefficients.size(); ++power)
        derivative.push_back(coefficients[power] * power);
    if (derivative.empty()) derivative.push_back(0.0);
    trimPolynomial(derivative);
    return derivative;
}

void appendRoot(std::vector<double>& roots, double root,
    double lower, double upper)
{
    if (!std::isfinite(root) || root < lower || root > upper) return;
    const double tolerance = 1e-9 * std::max(1.0, upper - lower);
    for (double existing : roots) {
        if (std::fabs(existing - root) <= tolerance) return;
    }
    roots.push_back(root);
}

std::vector<double> isolatePolynomialRoots(Polynomial coefficients,
    double lower, double upper)
{
    trimPolynomial(coefficients);
    std::vector<double> roots;
    const std::size_t degree = coefficients.size() - 1;
    if (degree == 0 || !(upper >= lower)) return roots;
    if (degree == 1) {
        appendRoot(roots, -coefficients[0] / coefficients[1], lower, upper);
        return roots;
    }
    std::vector<double> critical = isolatePolynomialRoots(
        derivativePolynomial(coefficients), lower, upper);
    std::sort(critical.begin(), critical.end());
    std::vector<double> boundaries{lower};
    boundaries.insert(boundaries.end(), critical.begin(), critical.end());
    boundaries.push_back(upper);
    for (double point : boundaries) {
        if (std::fabs(evaluatePolynomial(coefficients, point)) <=
                1e-12 * polynomialScale(coefficients, point))
            appendRoot(roots, point, lower, upper);
    }
    for (std::size_t index = 1; index < boundaries.size(); ++index) {
        double left = boundaries[index - 1];
        double right = boundaries[index];
        double leftValue = evaluatePolynomial(coefficients, left);
        double rightValue = evaluatePolynomial(coefficients, right);
        if (leftValue == 0.0 || rightValue == 0.0 ||
            (leftValue < 0.0) == (rightValue < 0.0))
            continue;
        for (int iteration = 0; iteration < 120; ++iteration) {
            const double middle = 0.5 * (left + right);
            const double middleValue = evaluatePolynomial(coefficients,
                middle);
            if ((middleValue < 0.0) == (leftValue < 0.0)) {
                left = middle;
                leftValue = middleValue;
            } else {
                right = middle;
            }
        }
        appendRoot(roots, 0.5 * (left + right), lower, upper);
    }
    std::sort(roots.begin(), roots.end());
    return roots;
}

bool finitePoint(const Point3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool finiteBall(const BallState& ball)
{
    return finitePoint(ball.position) && finitePoint(ball.velocity) &&
        finitePoint(ball.angularVelocity) && finitePoint(ball.rotationAxis) &&
        std::isfinite(ball.speed) && std::isfinite(ball.rotationAngle);
}

bool insideOuterFootprint(const Point3& position,
    const TableDepartureGeometry& geometry)
{
    return std::fabs(position.x) <= geometry.outerHalfWidthCm &&
        std::fabs(position.z) <= geometry.outerHalfLengthCm;
}

bool insideClothCenterEnvelope(const Point3& position,
    const TableDepartureGeometry& geometry)
{
    return std::fabs(position.x) <=
            geometry.playfieldHalfWidthCm + geometry.ballRadiusCm &&
        std::fabs(position.z) <=
            geometry.playfieldHalfLengthCm + geometry.ballRadiusCm;
}

bool insideRailProjection(const Point3& position,
    const TableDepartureGeometry& geometry)
{
    return insideOuterFootprint(position, geometry) &&
        !insideClothCenterEnvelope(position, geometry);
}

int signedPerimeterFeature(bool xAxis, float coordinate)
{
    if (xAxis) return coordinate < 0.0f ? 0 : 1;
    return coordinate < 0.0f ? 2 : 3;
}

int railTopFeatureAt(const Point3& position,
    const TableDepartureGeometry& geometry)
{
    const bool outsideX = std::fabs(position.x) >
        geometry.playfieldHalfWidthCm + geometry.ballRadiusCm;
    const bool outsideZ = std::fabs(position.z) >
        geometry.playfieldHalfLengthCm + geometry.ballRadiusCm;
    if (outsideX == outsideZ) return -1;
    return signedPerimeterFeature(outsideX,
        outsideX ? position.x : position.z);
}

bool insidePocketOpening(const Point3& position,
    const PhysicsProfile& profile);

bool penetratesRoundedUpperRailEdge(const Point3& position,
    const PhysicsProfile& profile,
    const TableDepartureGeometry& geometry)
{
    if (insidePocketOpening(position, profile)) return false;
    const double h = position.y - geometry.railTopSurfaceYCm;
    const double radius = geometry.ballRadiusCm;
    if (!(h > 0.0 && h < radius)) return false;
    const double minimumRadius = std::max(0.0,
        radius - profile.solver.penetrationSlopCm);
    for (bool xAxis : {true, false}) {
        const double half = (xAxis ? geometry.playfieldHalfWidthCm :
            geometry.playfieldHalfLengthCm) + radius;
        const double coordinate = xAxis ? position.x : position.z;
        for (double sign : {-1.0, 1.0}) {
            const double q = sign * coordinate - half;
            if (q > -radius && q < 0.0 &&
                q * q + h * h < minimumRadius * minimumRadius)
                return true;
        }
    }
    return false;
}

int pocketOpeningId(const Point3& position,
    const PhysicsProfile& profile)
{
    const std::array<PocketBoundaryFrame, 6> frames =
        buildPocketBoundaryFrames(profile.tableBoundary);
    for (const PocketBoundaryFrame& frame : frames) {
        const double dx = position.x - frame.mouthCenter.x;
        const double dz = position.z - frame.mouthCenter.z;
        const double tangent = dx * frame.tangent.x + dz * frame.tangent.z;
        const double allowed = frame.mouthWidthCm * 0.5 -
            profile.ball.radiusCm;
        if (allowed >= 0.0 && std::fabs(tangent) <= allowed + 1e-6 &&
            std::fabs(dx * frame.inward.x + dz * frame.inward.z) <=
                profile.ball.radiusCm + 1e-6)
            return frame.pocketId;
    }
    return -1;
}

bool insidePocketOpening(const Point3& position,
    const PhysicsProfile& profile)
{
    return pocketOpeningId(position, profile) >= 0;
}

double domainAccelerationY(const BallState& ball, double gravity)
{
    return ball.physicalDomain == PhysicalBallDomain::Airborne ?
        gravity : 0.0;
}

Polynomial crossDomainSeparationPolynomial(const BallState& first,
    const BallState& second, double gravity, double radiusSum)
{
    const double r0[3] = {second.position.x - first.position.x,
        second.position.y - first.position.y,
        second.position.z - first.position.z};
    const double velocity[3] = {second.velocity.x - first.velocity.x,
        second.velocity.y - first.velocity.y,
        second.velocity.z - first.velocity.z};
    const double halfAcceleration[3] = {0.0,
        0.5 * (domainAccelerationY(second, gravity) -
            domainAccelerationY(first, gravity)), 0.0};
    Polynomial result(5, 0.0);
    result[0] = -radiusSum * radiusSum;
    for (int axis = 0; axis < 3; ++axis) {
        result[0] += r0[axis] * r0[axis];
        result[1] += 2.0 * r0[axis] * velocity[axis];
        result[2] += velocity[axis] * velocity[axis] +
            2.0 * r0[axis] * halfAcceleration[axis];
        result[3] += 2.0 * velocity[axis] * halfAcceleration[axis];
        result[4] += halfAcceleration[axis] * halfAcceleration[axis];
    }
    trimPolynomial(result);
    return result;
}

double earliestCrossDomainTime(const BallState& first,
    const BallState& second, double maximumTime,
    const TableDepartureGeometry& geometry, double residualTolerance)
{
    const Polynomial polynomial = crossDomainSeparationPolynomial(first,
        second, geometry.gravityYCmS2, 2.0 * geometry.ballRadiusCm);
    const Polynomial derivative = derivativePolynomial(polynomial);
    const double initialValue = evaluatePolynomial(polynomial, 0.0);
    const double initialTolerance = 1e-5 * std::max(1.0,
        4.0 * geometry.ballRadiusCm * geometry.ballRadiusCm);
    if (std::fabs(initialValue) <= initialTolerance) {
        const double initialDerivative = evaluatePolynomial(derivative, 0.0);
        const double normalRelativeSpeed = initialDerivative /
            (2.0 * std::max(1e-12,
                2.0 * geometry.ballRadiusCm));
        if (normalRelativeSpeed < -residualTolerance)
        {
            return 0.0;
        }
        if (normalRelativeSpeed >= -residualTolerance)
        {
            return std::numeric_limits<double>::infinity();
        }
    }
    for (double root : isolatePolynomialRoots(polynomial, 0.0, maximumTime)) {
        const double derivativeValue = evaluatePolynomial(derivative, root);
        if (root > kTimeEpsilon && derivativeValue <
                -1e-10 * polynomialScale(derivative, root))
            return root;
    }
    return std::numeric_limits<double>::infinity();
}

bool crossDomainPair(const BallState& first, const BallState& second)
{
    const bool firstAirborne = first.physicalDomain ==
        PhysicalBallDomain::Airborne;
    const bool secondAirborne = second.physicalDomain ==
        PhysicalBallDomain::Airborne;
    return firstAirborne != secondAirborne &&
        first.physicalDomain != PhysicalBallDomain::PocketChannel &&
        second.physicalDomain != PhysicalBallDomain::PocketChannel;
}

void applySupportedContactReaction(BallState& ball,
    const TableDepartureGeometry& geometry)
{
    if (ball.physicalDomain == PhysicalBallDomain::Airborne ||
        ball.physicalDomain == PhysicalBallDomain::PocketChannel)
        return;
    if (ball.velocity.y > 0.0f) {
        ball.physicalDomain = PhysicalBallDomain::Airborne;
    } else if (ball.velocity.y < 0.0f) {
        ball.velocity.y = 0.0f;
        if (ball.physicalDomain == PhysicalBallDomain::ClothSupported)
            ball.position.y = geometry.clothSupportedCenterYCm;
        else if (ball.physicalDomain == PhysicalBallDomain::RailTopSupported)
            ball.position.y = geometry.railTopSupportedCenterYCm;
        else
            ball.position.y = geometry.outsideLandingSupportedCenterYCm;
    }
    ball.speed = std::sqrt(ball.velocity.x * ball.velocity.x +
        ball.velocity.y * ball.velocity.y +
        ball.velocity.z * ball.velocity.z);
}

void advanceDomainSegmentRecorded(BallState& ball, double time,
    const PhysicsProfile& profile, const TableDepartureGeometry& geometry,
    TableDepartureStepResult& result, int ballIndex, double startOffset)
{
    if (ball.physicalDomain == PhysicalBallDomain::Airborne)
        advanceBallisticRecorded(ball, time, geometry.gravityYCmS2,
            result, ballIndex, startOffset);
    else
        advanceSupportedRecorded(ball, time,
            profile.tableDeparture.supportedRollingDecelerationCmS2,
            profile.surface.torsionalSpinDecelerationRadS2,
            result, ballIndex, startOffset);
}

bool snapCrossDomainPairToAnalyticContact(BallState& first,
    const BallState& second, double radiusSum)
{
    const double dx = second.position.x - first.position.x;
    const double dy = second.position.y - first.position.y;
    const double dz = second.position.z - first.position.z;
    const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(distance) || !(distance > 0.0) ||
        std::fabs(distance - radiusSum) > 1e-4)
        return false;
    const double correction = distance - radiusSum;
    first.position.x += static_cast<float>(dx / distance * correction);
    first.position.y += static_cast<float>(dy / distance * correction);
    first.position.z += static_cast<float>(dz / distance * correction);
    const int axis = std::fabs(dx) >= std::fabs(dy) &&
            std::fabs(dx) >= std::fabs(dz) ? 0 :
        (std::fabs(dy) >= std::fabs(dz) ? 1 : 2);
    float* coordinate = axis == 0 ? &first.position.x :
        (axis == 1 ? &first.position.y : &first.position.z);
    const float target = axis == 0 ? second.position.x :
        (axis == 1 ? second.position.y : second.position.z);
    for (int adjustment = 0; adjustment < 4; ++adjustment) {
        const double afterDx = second.position.x - first.position.x;
        const double afterDy = second.position.y - first.position.y;
        const double afterDz = second.position.z - first.position.z;
        if (std::sqrt(afterDx * afterDx + afterDy * afterDy +
                afterDz * afterDz) <= radiusSum)
            break;
        *coordinate = std::nextafter(*coordinate, target);
    }
    return true;
}

Point3 ballisticPosition(const BallState& ball, double time,
    float gravity)
{
    return Point3{
        static_cast<float>(ball.position.x + ball.velocity.x * time),
        static_cast<float>(ball.position.y + ball.velocity.y * time +
            0.5 * gravity * time * time),
        static_cast<float>(ball.position.z + ball.velocity.z * time)};
}

double descendingPlaneTime(const BallState& ball, double planeY,
    double maximumTime, float gravity)
{
    const double a = 0.5 * gravity;
    const double b = ball.velocity.y;
    const double c = ball.position.y - planeY;
    const double discriminant = b * b - 4.0 * a * c;
    if (!(discriminant >= 0.0) || !std::isfinite(discriminant))
        return std::numeric_limits<double>::infinity();
    const double root = std::sqrt(discriminant);
    const double candidates[2] = {(-b - root) / (2.0 * a),
        (-b + root) / (2.0 * a)};
    double best = std::numeric_limits<double>::infinity();
    for (double time : candidates) {
        if (time < 0.0 || time > maximumTime + kTimeEpsilon)
            continue;
        if (ball.velocity.y + gravity * time >= 0.0) continue;
        best = std::min(best, time);
    }
    return best;
}

double outwardCrossingTime(double position, double velocity, double limit,
    double maximumTime)
{
    if (velocity > 0.0 && position < limit) {
        const double time = (limit - position) / velocity;
        if (time > kTimeEpsilon && time <= maximumTime + kTimeEpsilon)
            return time;
    }
    if (velocity < 0.0 && position > -limit) {
        const double time = (-limit - position) / velocity;
        if (time > kTimeEpsilon && time <= maximumTime + kTimeEpsilon)
            return time;
    }
    return std::numeric_limits<double>::infinity();
}

void advanceBallistic(BallState& ball, double time, float gravity)
{
    ball.position = ballisticPosition(ball, time, gravity);
    ball.velocity.y += static_cast<float>(gravity * time);
    ball.speed = std::sqrt(ball.velocity.x * ball.velocity.x +
        ball.velocity.y * ball.velocity.y +
        ball.velocity.z * ball.velocity.z);
    ball.rotationAngle += static_cast<float>(std::sqrt(
        ball.angularVelocity.x * ball.angularVelocity.x +
        ball.angularVelocity.y * ball.angularVelocity.y +
        ball.angularVelocity.z * ball.angularVelocity.z) * time * 180.0 /
        3.14159265358979323846);
}

void advanceSupported(BallState& ball, double time, float deceleration)
{
    const double speed = std::hypot(ball.velocity.x, ball.velocity.z);
    if (!(speed > 0.0) || !(time > 0.0)) {
        ball.speed = static_cast<float>(speed);
        return;
    }
    const double duration = deceleration > 0.0
        ? std::min(time, speed / deceleration) : time;
    const double distance = speed * duration -
        0.5 * deceleration * duration * duration;
    ball.position.x += static_cast<float>(ball.velocity.x / speed * distance);
    ball.position.z += static_cast<float>(ball.velocity.z / speed * distance);
    const bool reachesStop = deceleration > 0.0 &&
        time >= speed / deceleration;
    const double finalSpeed = reachesStop ? 0.0 :
        std::max(0.0, speed - deceleration * time);
    ball.velocity.x = static_cast<float>(ball.velocity.x / speed * finalSpeed);
    ball.velocity.z = static_cast<float>(ball.velocity.z / speed * finalSpeed);
    ball.velocity.y = 0.0f;
    ball.speed = static_cast<float>(finalSpeed);
}

double supportedClothEntryTime(const BallState& ball, double maximumTime,
    float deceleration, const TableDepartureGeometry& geometry)
{
    const double speed = std::hypot(ball.velocity.x, ball.velocity.z);
    if (!(speed > 0.0) || !(maximumTime > 0.0))
        return std::numeric_limits<double>::infinity();
    const double directionX = ball.velocity.x / speed;
    const double directionZ = ball.velocity.z / speed;
    const double limitX = geometry.playfieldHalfWidthCm +
        geometry.ballRadiusCm;
    const double limitZ = geometry.playfieldHalfLengthCm +
        geometry.ballRadiusCm;
    const auto entryDistance = [](double coordinate, double direction,
        double limit) {
        if (coordinate > limit)
            return direction < 0.0 ? (limit - coordinate) / direction :
                std::numeric_limits<double>::infinity();
        if (coordinate < -limit)
            return direction > 0.0 ? (-limit - coordinate) / direction :
                std::numeric_limits<double>::infinity();
        return 0.0;
    };
    const double distance = std::max(
        entryDistance(ball.position.x, directionX, limitX),
        entryDistance(ball.position.z, directionZ, limitZ));
    if (!std::isfinite(distance) || distance < 0.0)
        return std::numeric_limits<double>::infinity();
    const double maximumMovingTime = deceleration > 0.0f
        ? std::min(maximumTime, speed / deceleration) : maximumTime;
    const double maximumDistance = speed * maximumMovingTime -
        0.5 * deceleration * maximumMovingTime * maximumMovingTime;
    if (distance > maximumDistance)
        return std::numeric_limits<double>::infinity();
    double time = distance / speed;
    if (deceleration > 0.0f) {
        const double discriminant = speed * speed -
            2.0 * deceleration * distance;
        if (!(discriminant >= 0.0))
            return std::numeric_limits<double>::infinity();
        time = (speed - std::sqrt(discriminant)) / deceleration;
    }
    if (time < 0.0 || time > maximumTime)
        return std::numeric_limits<double>::infinity();
    return time;
}

const char* eventName(TableDepartureEventKind kind)
{
    switch (kind) {
    case TableDepartureEventKind::CushionNoseContact: return "nose";
    case TableDepartureEventKind::OuterDeparture: return "departure";
    case TableDepartureEventKind::RailTopLanding: return "rail_top";
    case TableDepartureEventKind::OutsideLanding: return "outside_landing";
    case TableDepartureEventKind::ClothReturn: return "cloth_return";
    case TableDepartureEventKind::PocketEntry: return "pocket_entry";
    case TableDepartureEventKind::CrossDomainBallContact:
        return "cross_domain_ball_contact";
    }
    return "invalid";
}

void appendEvent(TableDepartureStepResult& result,
    TableDepartureEventKind kind, int ballIndex, double absoluteTime,
    PhysicalBallDomain sourceDomain, const BallState& after,
    int featureId = -1,
    TableDepartureClothReturnProvenance clothReturnProvenance =
        TableDepartureClothReturnProvenance::NotApplicable)
{
    TableDepartureEvent event;
    event.kind = kind;
    event.ballIndex = ballIndex;
    event.ordinal = static_cast<int>(result.events.size());
    event.trajectoryGeneration = after.trajectoryGeneration;
    event.absoluteWholeTickTimeSeconds = absoluteTime;
    std::ostringstream identity;
    identity << ballIndex << ':' << event.ordinal << ':' << eventName(kind)
             << ':' << absoluteTime;
    event.stableIdentity = identity.str();
    event.transitionWitnessComplete = true;
    event.velocityAfterCmS = after.velocity;
    event.sourceDomain = sourceDomain;
    event.targetDomain = after.physicalDomain;
    event.clothReturnProvenance = clothReturnProvenance;
    if (kind == TableDepartureEventKind::OuterDeparture)
        event.boundaryFeatureId = featureId;
    else if (kind == TableDepartureEventKind::RailTopLanding)
        event.railFeatureId = featureId;
    else if (kind == TableDepartureEventKind::OutsideLanding)
        event.landingSurfaceId = featureId;
    else if (kind == TableDepartureEventKind::PocketEntry)
        event.pocketId = featureId;
    result.events.push_back(event);
}

struct Candidate {
    TableDepartureEventKind kind = TableDepartureEventKind::CushionNoseContact;
    double time = std::numeric_limits<double>::infinity();
    int featureId = -1;
    TableDepartureClothReturnProvenance clothReturnProvenance =
        TableDepartureClothReturnProvenance::NotApplicable;
    Point3 contactNormal;
    bool contactNormalComplete = false;
};

Candidate upperRailEdgeCandidate(const BallState& ball, double maximumTime,
    const PhysicsProfile& profile, const TableDepartureGeometry& geometry,
    bool xAxis, bool positiveSide)
{
    Candidate none;
    none.time = std::numeric_limits<double>::infinity();
    const double sign = positiveSide ? 1.0 : -1.0;
    const double coordinate = xAxis ? ball.position.x : ball.position.z;
    const double velocity = xAxis ? ball.velocity.x : ball.velocity.z;
    // `insideRailProjection` is the authoritative rail-top ownership seam:
    // a centre is fully over that support only beyond playfield half extent
    // plus one ball radius.  The fixed upper edge is therefore rooted at the
    // same horizontal seam, and its swept-sphere quarter circle extends one
    // radius inward to meet the cushion shoulder and one radius upward to
    // meet the rail-top support plane.
    const double half = (xAxis ? geometry.playfieldHalfWidthCm :
        geometry.playfieldHalfLengthCm) + geometry.ballRadiusCm;
    const double q0 = sign * coordinate - half;
    const double qv = sign * velocity;
    const double h0 = ball.position.y - geometry.railTopSurfaceYCm;
    const double vy = ball.velocity.y;
    const double g = geometry.gravityYCmS2;
    const double radius = geometry.ballRadiusCm;
    Polynomial separation{
        q0 * q0 + h0 * h0 - radius * radius,
        2.0 * (q0 * qv + h0 * vy),
        qv * qv + vy * vy + h0 * g,
        vy * g,
        0.25 * g * g};
    const Polynomial derivative = derivativePolynomial(separation);
    for (double root : isolatePolynomialRoots(separation, 0.0,
            maximumTime)) {
        const Point3 point = ballisticPosition(ball, root,
            geometry.gravityYCmS2);
        const double coordinateAtRoot = xAxis ? point.x : point.z;
        const double q = sign * coordinateAtRoot - half;
        const double h = point.y - geometry.railTopSurfaceYCm;
        const double geometryTolerance = 1e-5;
        // This is the rounded quarter of the upper inner rail edge.  Its
        // adjacent vertical face and horizontal top retain their existing
        // plane candidates, so exclude the two endpoints here.
        if (!(q > -radius + geometryTolerance && q < -geometryTolerance) ||
            !(h > geometryTolerance && h < radius - geometryTolerance) ||
            !insideOuterFootprint(point, geometry) ||
            insidePocketOpening(point, profile))
            continue;
        if (!(evaluatePolynomial(derivative, root) <
                -1e-10 * polynomialScale(derivative, root)))
            continue;
        const double inverseRadius = 1.0 / radius;
        none.kind = TableDepartureEventKind::CushionNoseContact;
        none.time = root;
        none.featureId = xAxis ? 0 : 1;
        none.contactNormal = xAxis ?
            Point3{static_cast<float>(sign * q * inverseRadius),
                static_cast<float>(h * inverseRadius), 0.0f} :
            Point3{0.0f, static_cast<float>(h * inverseRadius),
                static_cast<float>(sign * q * inverseRadius)};
        none.contactNormalComplete = true;
        return none;
    }
    return none;
}

TableDepartureStepResult fail(TableDepartureFailureCode code)
{
    TableDepartureStepResult result;
    result.failureCode = code;
    return result;
}

struct BallCandidateQuery {
    bool ok = false;
    TableDepartureFailureCode failureCode =
        TableDepartureFailureCode::InvalidState;
    std::vector<Candidate> candidates;
};

BallCandidateQuery queryFirstTableCandidatesForBall(
    const BallState& source, double maximumTime,
    const PhysicsProfile& profile, const TableDepartureGeometry& geometry,
    PhysicsBoundaryMode boundaryMode)
{
    BallCandidateQuery query;
    if (!finiteBall(source) || !std::isfinite(maximumTime) ||
        maximumTime < 0.0) return query;
    query.ok = true;
    query.failureCode = TableDepartureFailureCode::None;
    if (source.pocketed ||
        source.physicalDomain == PhysicalBallDomain::PocketChannel ||
        boundaryMode == PhysicsBoundaryMode::Unbounded)
        return query;

    BallState ball = source;
    bool departedRailTop = false;
    int departedRailFeature = -1;
    double elapsed = 0.0;
    double remaining = maximumTime;
    if (ball.physicalDomain == PhysicalBallDomain::ClothSupported) {
        if (std::fabs(ball.position.y - geometry.clothSupportedCenterYCm) >
                1e-4 || !insideClothCenterEnvelope(ball.position, geometry)) {
            query.ok = false;
            return query;
        }
        applySupportedContactReaction(ball, geometry);
        if (ball.physicalDomain == PhysicalBallDomain::ClothSupported)
            return query;
    }
    if (ball.physicalDomain == PhysicalBallDomain::RailTopSupported) {
        if (std::fabs(ball.position.y -
                geometry.railTopSupportedCenterYCm) > 1e-4 ||
            !insideRailProjection(ball.position, geometry)) {
            query.ok = false;
            return query;
        }
        applySupportedContactReaction(ball, geometry);
        if (ball.physicalDomain == PhysicalBallDomain::RailTopSupported) {
            const float deceleration =
                profile.tableDeparture.supportedRollingDecelerationCmS2;
            const double entryTime = supportedClothEntryTime(ball, remaining,
                deceleration, geometry);
            if (!std::isfinite(entryTime)) return query;
            departedRailFeature = railTopFeatureAt(ball.position, geometry);
            advanceSupported(ball, entryTime, deceleration);
            ball.physicalDomain = PhysicalBallDomain::Airborne;
            departedRailTop = true;
            elapsed += entryTime;
            remaining = std::max(0.0, remaining - entryTime);
        }
    }
    if (ball.physicalDomain ==
            PhysicalBallDomain::OutsideLandingSupported) {
        if (std::fabs(ball.position.y -
                geometry.outsideLandingSupportedCenterYCm) > 1e-4) {
            query.ok = false;
            return query;
        }
        applySupportedContactReaction(ball, geometry);
        if (ball.physicalDomain ==
                PhysicalBallDomain::OutsideLandingSupported)
            return query;
    }
    if (ball.position.y < geometry.outsideLandingSupportedCenterYCm - 1e-4 ||
        penetratesRoundedUpperRailEdge(ball.position, profile, geometry) ||
        (ball.physicalDomain == PhysicalBallDomain::Airborne &&
         ball.position.y < geometry.clothSupportedCenterYCm &&
         insideClothCenterEnvelope(ball.position, geometry)) ||
        (insideRailProjection(ball.position, geometry) &&
         ball.position.y < geometry.railTopSupportedCenterYCm - 1e-4 &&
         !insidePocketOpening(ball.position, profile))) {
        query.ok = false;
        query.failureCode =
            TableDepartureFailureCode::AmbiguousInitialPenetration;
        return query;
    }
    if (ball.pocketInteraction.phase != PocketInteractionPhase::Outside &&
        (std::fabs(std::fabs(ball.position.x) - geometry.outerHalfWidthCm) <=
             1e-4 ||
         std::fabs(std::fabs(ball.position.z) - geometry.outerHalfLengthCm) <=
             1e-4)) {
        query.ok = false;
        query.failureCode =
            TableDepartureFailureCode::ContradictoryPocketDeparture;
        return query;
    }

    const double pocketTime = descendingPlaneTime(ball,
        geometry.clothSupportedCenterYCm, remaining, geometry.gravityYCmS2);
    if (std::isfinite(pocketTime)) {
        const Point3 point = ballisticPosition(ball, pocketTime,
            geometry.gravityYCmS2);
        const int pocketId = pocketOpeningId(point, profile);
        if (pocketId >= 0)
            query.candidates.push_back({
                TableDepartureEventKind::PocketEntry, elapsed + pocketTime,
                pocketId});
        else if (insideClothCenterEnvelope(point, geometry))
            query.candidates.push_back({
                TableDepartureEventKind::ClothReturn, elapsed + pocketTime,
                departedRailTop ? departedRailFeature : -1,
                departedRailTop ?
                    TableDepartureClothReturnProvenance::RailTopLanding :
                    TableDepartureClothReturnProvenance::
                        DirectAirborneLanding});
    }
    const double railTopTime = descendingPlaneTime(ball,
        geometry.railTopSupportedCenterYCm, remaining,
        geometry.gravityYCmS2);
    if (std::isfinite(railTopTime)) {
        const Point3 point = ballisticPosition(ball, railTopTime,
            geometry.gravityYCmS2);
        if (insideRailProjection(point, geometry) &&
            !insidePocketOpening(point, profile))
            query.candidates.push_back({
                TableDepartureEventKind::RailTopLanding,
                elapsed + railTopTime, railTopFeatureAt(point, geometry)});
    }
    double noseTime = outwardCrossingTime(ball.position.x,
        ball.velocity.x, geometry.playfieldHalfWidthCm -
            geometry.ballRadiusCm, remaining);
    int noseFeature = 0;
    const double noseZ = outwardCrossingTime(ball.position.z,
        ball.velocity.z, geometry.playfieldHalfLengthCm -
            geometry.ballRadiusCm, remaining);
    if (noseZ < noseTime) { noseTime = noseZ; noseFeature = 1; }
    if (std::isfinite(noseTime)) {
        const Point3 point = ballisticPosition(ball, noseTime,
            geometry.gravityYCmS2);
        if (point.y - geometry.ballRadiusCm <=
                geometry.cushionNoseCenterYCm + 1e-5 &&
            point.y + geometry.ballRadiusCm >=
                geometry.clothSurfaceYCm - 1e-5 &&
            !insidePocketOpening(point, profile))
            query.candidates.push_back({
                TableDepartureEventKind::CushionNoseContact,
                elapsed + noseTime, noseFeature});
    }
    for (bool xAxis : {true, false}) {
        for (bool positiveSide : {false, true}) {
            Candidate edge = upperRailEdgeCandidate(ball, remaining,
                profile, geometry, xAxis, positiveSide);
            if (std::isfinite(edge.time)) {
                edge.time += elapsed;
                query.candidates.push_back(edge);
            }
        }
    }
    double outerTime = outwardCrossingTime(ball.position.x,
        ball.velocity.x, geometry.outerHalfWidthCm, remaining);
    bool outerX = true;
    const double outerZ = outwardCrossingTime(ball.position.z,
        ball.velocity.z, geometry.outerHalfLengthCm, remaining);
    if (outerZ < outerTime) { outerTime = outerZ; outerX = false; }
    if (std::isfinite(outerTime))
        query.candidates.push_back({TableDepartureEventKind::OuterDeparture,
            elapsed + outerTime, signedPerimeterFeature(outerX,
                outerX ? ball.velocity.x : ball.velocity.z)});
    const double floorTime = descendingPlaneTime(ball,
        geometry.outsideLandingSupportedCenterYCm, remaining,
        geometry.gravityYCmS2);
    if (std::isfinite(floorTime) && !insideOuterFootprint(
            ballisticPosition(ball, floorTime, geometry.gravityYCmS2),
            geometry))
        query.candidates.push_back({TableDepartureEventKind::OutsideLanding,
            elapsed + floorTime, 0});
    return query;
}

bool finiteAnalyticSegment(const SurfaceMotionAnalyticSegment& segment)
{
    const float values[] = {
        segment.startPositionCm.x, segment.startPositionCm.y,
        segment.startPositionCm.z, segment.endPositionCm.x,
        segment.endPositionCm.y, segment.endPositionCm.z,
        segment.startVelocityCmS.x, segment.startVelocityCmS.y,
        segment.startVelocityCmS.z, segment.constantAccelerationCmS2.x,
        segment.constantAccelerationCmS2.y,
        segment.constantAccelerationCmS2.z};
    if (!std::isfinite(segment.wholeTickStartOffsetSeconds) ||
        !std::isfinite(segment.durationSeconds) ||
        !(segment.durationSeconds > 0.0) ||
        !segment.analyticPathComplete)
        return false;
    for (float value : values)
        if (!std::isfinite(value)) return false;
    return true;
}

Point3 analyticPositionAt(const SurfaceMotionAnalyticSegment& segment,
    double absoluteTime)
{
    const double local = absoluteTime -
        segment.wholeTickStartOffsetSeconds;
    return Point3{
        static_cast<float>(segment.startPositionCm.x +
            segment.startVelocityCmS.x * local +
            0.5 * segment.constantAccelerationCmS2.x * local * local),
        static_cast<float>(segment.startPositionCm.y +
            segment.startVelocityCmS.y * local +
            0.5 * segment.constantAccelerationCmS2.y * local * local),
        static_cast<float>(segment.startPositionCm.z +
            segment.startVelocityCmS.z * local +
            0.5 * segment.constantAccelerationCmS2.z * local * local)};
}

Polynomial piecewiseSeparationPolynomial(
    const SurfaceMotionAnalyticSegment& first,
    const SurfaceMotionAnalyticSegment& second, double overlapStart,
    double radiusSum)
{
    const auto axisStateAt = [overlapStart](
        const SurfaceMotionAnalyticSegment& segment, int axis) {
        const double local = overlapStart -
            segment.wholeTickStartOffsetSeconds;
        const double position[3] = {segment.startPositionCm.x,
            segment.startPositionCm.y, segment.startPositionCm.z};
        const double velocity[3] = {segment.startVelocityCmS.x,
            segment.startVelocityCmS.y, segment.startVelocityCmS.z};
        const double acceleration[3] = {
            segment.constantAccelerationCmS2.x,
            segment.constantAccelerationCmS2.y,
            segment.constantAccelerationCmS2.z};
        return std::array<double, 2>{
            position[axis] + velocity[axis] * local +
                0.5 * acceleration[axis] * local * local,
            velocity[axis] + acceleration[axis] * local};
    };
    const double r0[3] = {
        axisStateAt(second, 0)[0] - axisStateAt(first, 0)[0],
        axisStateAt(second, 1)[0] - axisStateAt(first, 1)[0],
        axisStateAt(second, 2)[0] - axisStateAt(first, 2)[0]};
    const double velocity[3] = {
        axisStateAt(second, 0)[1] - axisStateAt(first, 0)[1],
        axisStateAt(second, 1)[1] - axisStateAt(first, 1)[1],
        axisStateAt(second, 2)[1] - axisStateAt(first, 2)[1]};
    const double halfAcceleration[3] = {
        0.5 * (second.constantAccelerationCmS2.x -
            first.constantAccelerationCmS2.x),
        0.5 * (second.constantAccelerationCmS2.y -
            first.constantAccelerationCmS2.y),
        0.5 * (second.constantAccelerationCmS2.z -
            first.constantAccelerationCmS2.z)};
    Polynomial polynomial(5, 0.0);
    polynomial[0] = -radiusSum * radiusSum;
    for (int axis = 0; axis < 3; ++axis) {
        polynomial[0] += r0[axis] * r0[axis];
        polynomial[1] += 2.0 * r0[axis] * velocity[axis];
        polynomial[2] += velocity[axis] * velocity[axis] +
            2.0 * r0[axis] * halfAcceleration[axis];
        polynomial[3] += 2.0 * velocity[axis] * halfAcceleration[axis];
        polynomial[4] += halfAcceleration[axis] * halfAcceleration[axis];
    }
    trimPolynomial(polynomial);
    return polynomial;
}

struct PiecewisePairQuery {
    bool ok = true;
    double time = std::numeric_limits<double>::infinity();
    double initialPenetrationCm = 0.0;
};

double solverRoundedSeparationSquared(
    const SurfaceMotionAnalyticSegment& first,
    const SurfaceMotionAnalyticSegment& second, double absoluteTime)
{
    const Point3 firstPosition = analyticPositionAt(first, absoluteTime);
    const Point3 secondPosition = analyticPositionAt(second, absoluteTime);
    const Point3 separation{
        static_cast<float>(secondPosition.x - firstPosition.x),
        static_cast<float>(secondPosition.y - firstPosition.y),
        static_cast<float>(secondPosition.z - firstPosition.z)};
    return separation.x * separation.x +
        separation.y * separation.y + separation.z * separation.z;
}

PiecewisePairQuery queryPiecewisePair(
    const std::vector<SurfaceMotionAnalyticSegment>& firstSegments,
    const std::vector<SurfaceMotionAnalyticSegment>& secondSegments,
    double maximumTime, double radiusSum, double residualTolerance,
    double penetrationSlop)
{
    PiecewisePairQuery query;
    const double minimumSeparationSquared =
        tableDepartureMinimumSeparationSquared(radiusSum, penetrationSlop);
    for (const SurfaceMotionAnalyticSegment& first : firstSegments) {
        const double firstEnd = first.wholeTickStartOffsetSeconds +
            first.durationSeconds;
        for (const SurfaceMotionAnalyticSegment& second : secondSegments) {
            const double secondEnd = second.wholeTickStartOffsetSeconds +
                second.durationSeconds;
            const double overlapStart = std::max(
                first.wholeTickStartOffsetSeconds,
                second.wholeTickStartOffsetSeconds);
            const double overlapEnd = std::min({firstEnd, secondEnd,
                maximumTime});
            if (overlapEnd < overlapStart) continue;
            const Polynomial polynomial = piecewiseSeparationPolynomial(
                first, second, overlapStart, radiusSum);
            const Polynomial derivative = derivativePolynomial(polynomial);
            // Contact-solver position projection stores positions and their
            // subtraction in Point3 floats before evaluating the separation
            // length.  Use that exact arithmetic at the interval boundary so
            // a one-ULP projection residue cannot become a false ambiguous
            // penetration in this double-precision root query.
            const double initialDistanceSquared =
                solverRoundedSeparationSquared(first, second, overlapStart);
            const double initial = initialDistanceSquared -
                radiusSum * radiusSum;
            if (initialDistanceSquared < minimumSeparationSquared) {
                query.ok = false;
                query.initialPenetrationCm = radiusSum - std::sqrt(
                    std::max(0.0, initialDistanceSquared));
                return query;
            }
            const double initialNormalSpeed = evaluatePolynomial(
                derivative, 0.0) /
                (2.0 * std::max(1e-12, radiusSum));
            if (initial <= 0.0 &&
                initialNormalSpeed < -residualTolerance)
                query.time = std::min(query.time, overlapStart);
            for (double root : isolatePolynomialRoots(polynomial, 0.0,
                    overlapEnd - overlapStart)) {
                const double derivativeValue = evaluatePolynomial(
                    derivative, root);
                const double normalRelativeSpeed = derivativeValue /
                    (2.0 * std::max(1e-12, radiusSum));
                const double absolute = overlapStart + root;
                if (normalRelativeSpeed < -residualTolerance &&
                    absolute < query.time)
                    query.time = absolute;
            }
        }
    }
    return query;
}

bool validateMotionPartitionForBall(
    const std::vector<SurfaceMotionAnalyticSegment>& segments,
    int ballIndex, const BallState& before, double requiredDuration,
    const PhysicsProfile& profile,
    std::vector<SurfaceMotionAnalyticSegment>& selected)
{
    for (const SurfaceMotionAnalyticSegment& segment : segments) {
        if (segment.ballIndex == ballIndex) selected.push_back(segment);
    }
    std::sort(selected.begin(), selected.end(),
        [](const SurfaceMotionAnalyticSegment& first,
            const SurfaceMotionAnalyticSegment& second) {
            if (first.wholeTickStartOffsetSeconds !=
                    second.wholeTickStartOffsetSeconds)
                return first.wholeTickStartOffsetSeconds <
                    second.wholeTickStartOffsetSeconds;
            return first.segmentOrdinal < second.segmentOrdinal;
        });
    if (!(requiredDuration > 0.0)) return true;
    if (selected.empty()) return false;
    double covered = 0.0;
    Point3 previousEnd = before.position;
    for (const SurfaceMotionAnalyticSegment& segment : selected) {
        if (!finiteAnalyticSegment(segment) ||
            segment.trajectoryGeneration != before.trajectoryGeneration ||
            std::fabs(segment.wholeTickStartOffsetSeconds - covered) >
                profile.solver.toiToleranceSeconds ||
            std::fabs(segment.startPositionCm.x - previousEnd.x) >
                profile.solver.penetrationSlopCm ||
            std::fabs(segment.startPositionCm.y - previousEnd.y) >
                profile.solver.penetrationSlopCm ||
            std::fabs(segment.startPositionCm.z - previousEnd.z) >
                profile.solver.penetrationSlopCm ||
            (covered == 0.0 &&
             (std::fabs(segment.startVelocityCmS.x - before.velocity.x) >
                    profile.solver.residualToleranceCmS ||
              std::fabs(segment.startVelocityCmS.y - before.velocity.y) >
                    profile.solver.residualToleranceCmS ||
              std::fabs(segment.startVelocityCmS.z - before.velocity.z) >
                    profile.solver.residualToleranceCmS)))
            return false;
        const Point3 integratedEnd = analyticPositionAt(segment,
            segment.wholeTickStartOffsetSeconds + segment.durationSeconds);
        if (std::fabs(segment.endPositionCm.x - integratedEnd.x) >
                profile.solver.penetrationSlopCm ||
            std::fabs(segment.endPositionCm.y - integratedEnd.y) >
                profile.solver.penetrationSlopCm ||
            std::fabs(segment.endPositionCm.z - integratedEnd.z) >
                profile.solver.penetrationSlopCm)
            return false;
        covered = segment.wholeTickStartOffsetSeconds +
            segment.durationSeconds;
        previousEnd = segment.endPositionCm;
        if (covered >= requiredDuration -
                profile.solver.toiToleranceSeconds)
            break;
    }
    return covered >= requiredDuration -
        profile.solver.toiToleranceSeconds;
}

TableDepartureStepResult advanceOne(BallState& ball, double timeStep,
    const PhysicsProfile& profile, PhysicsBoundaryMode boundaryMode,
    int maximumEvents, int ballIndex, bool deferEndpointEvent = false)
{
    const BallState snapshot = ball;
    TableDepartureStepResult result;
    const TableDepartureGeometry geometry =
        buildTableDepartureGeometry(profile);
    if (!validateTableDepartureGeometry(geometry).ok ||
        !validatePhysicsProfile(profile).ok) {
        return fail(TableDepartureFailureCode::InvalidGeometry);
    }
    if (!std::isfinite(timeStep) || timeStep < 0.0f || !finiteBall(ball) ||
        maximumEvents < 0) {
        return fail(TableDepartureFailureCode::InvalidState);
    }
    if (timeStep == 0.0f) {
        result.ok = true;
        result.failureCode = TableDepartureFailureCode::None;
        return result;
    }
    if (ball.pocketed || ball.physicalDomain ==
            PhysicalBallDomain::PocketChannel) {
        result.ok = true;
        result.failureCode = TableDepartureFailureCode::None;
        return result;
    }
    if (boundaryMode == PhysicsBoundaryMode::Unbounded) {
        applySupportedContactReaction(ball, geometry);
        advanceDomainSegmentRecorded(ball, timeStep, profile, geometry,
            result, ballIndex, 0.0);
        result.ok = true;
        result.failureCode = TableDepartureFailureCode::None;
        return result;
    }
    double elapsed = 0.0;
    double remaining = timeStep;
    bool departedRailTop = false;
    int departedRailFeature = -1;
    if (ball.physicalDomain == PhysicalBallDomain::ClothSupported) {
        if (std::fabs(ball.position.y - geometry.clothSupportedCenterYCm) >
                1e-4 || !insideClothCenterEnvelope(ball.position, geometry)) {
            return fail(TableDepartureFailureCode::InvalidState);
        }
        applySupportedContactReaction(ball, geometry);
        if (ball.physicalDomain == PhysicalBallDomain::ClothSupported) {
            advanceSupportedRecorded(ball, timeStep,
                profile.tableDeparture.supportedRollingDecelerationCmS2,
                profile.surface.torsionalSpinDecelerationRadS2,
                result, ballIndex, 0.0);
            ball.position.y = geometry.clothSupportedCenterYCm;
            result.ok = true;
            result.failureCode = TableDepartureFailureCode::None;
            return result;
        }
    }
    if (ball.physicalDomain == PhysicalBallDomain::RailTopSupported) {
        if (std::fabs(ball.position.y -
                geometry.railTopSupportedCenterYCm) > 1e-4 ||
            !insideRailProjection(ball.position, geometry)) {
            return fail(TableDepartureFailureCode::InvalidState);
        }
        applySupportedContactReaction(ball, geometry);
    }
    if (ball.physicalDomain == PhysicalBallDomain::RailTopSupported) {
        const float deceleration =
            profile.tableDeparture.supportedRollingDecelerationCmS2;
        const double entryTime = supportedClothEntryTime(ball, timeStep,
            deceleration, geometry);
        if (std::isfinite(entryTime)) {
            departedRailFeature = railTopFeatureAt(ball.position, geometry);
            advanceSupportedRecorded(ball, entryTime, deceleration,
                profile.surface.torsionalSpinDecelerationRadS2, result,
                ballIndex, 0.0);
            ball.physicalDomain = PhysicalBallDomain::Airborne;
            departedRailTop = true;
            elapsed = entryTime;
            remaining = std::max(0.0,
                static_cast<double>(timeStep) - entryTime);
        } else {
            advanceSupportedRecorded(ball, timeStep, deceleration,
                profile.surface.torsionalSpinDecelerationRadS2, result,
                ballIndex, 0.0);
            ball.position.y = geometry.railTopSupportedCenterYCm;
            result.ok = true;
            result.failureCode = TableDepartureFailureCode::None;
            return result;
        }
    }
    if (ball.physicalDomain ==
            PhysicalBallDomain::OutsideLandingSupported) {
        if (std::fabs(ball.position.y -
                geometry.outsideLandingSupportedCenterYCm) > 1e-4) {
            return fail(TableDepartureFailureCode::InvalidState);
        }
        applySupportedContactReaction(ball, geometry);
    }
    if (ball.physicalDomain ==
            PhysicalBallDomain::OutsideLandingSupported) {
        advanceSupportedRecorded(ball, timeStep,
            profile.tableDeparture.supportedRollingDecelerationCmS2,
            profile.surface.torsionalSpinDecelerationRadS2,
            result, ballIndex, 0.0);
        ball.position.y = geometry.outsideLandingSupportedCenterYCm;
        result.ok = true;
        result.failureCode = TableDepartureFailureCode::None;
        return result;
    }
    if (ball.position.y < geometry.outsideLandingSupportedCenterYCm - 1e-4 ||
        penetratesRoundedUpperRailEdge(ball.position, profile, geometry) ||
        (ball.physicalDomain == PhysicalBallDomain::Airborne &&
         ball.position.y < geometry.clothSupportedCenterYCm &&
         insideClothCenterEnvelope(ball.position, geometry)) ||
        (insideRailProjection(ball.position, geometry) &&
         ball.position.y < geometry.railTopSupportedCenterYCm - 1e-4 &&
         !insidePocketOpening(ball.position, profile))) {
        return fail(TableDepartureFailureCode::AmbiguousInitialPenetration);
    }
    if (ball.pocketInteraction.phase != PocketInteractionPhase::Outside &&
        (std::fabs(std::fabs(ball.position.x) - geometry.outerHalfWidthCm) <=
             1e-4 ||
         std::fabs(std::fabs(ball.position.z) - geometry.outerHalfLengthCm) <=
             1e-4)) {
        return fail(TableDepartureFailureCode::ContradictoryPocketDeparture);
    }

    // Candidate roots retain kTimeEpsilon as their topology threshold, but a
    // successful positive-duration call must still mutate and attest the
    // complete interval even when the whole tail is shorter than that bound.
    while (remaining > 0.0) {
        std::vector<Candidate> candidates;
        const double pocketTime = descendingPlaneTime(ball,
            geometry.clothSupportedCenterYCm, remaining,
            geometry.gravityYCmS2);
        if (std::isfinite(pocketTime)) {
            const Point3 clothPoint = ballisticPosition(ball, pocketTime,
                geometry.gravityYCmS2);
            const int pocketId = pocketOpeningId(clothPoint, profile);
            if (pocketId >= 0) {
                candidates.push_back({TableDepartureEventKind::PocketEntry,
                    pocketTime, pocketId});
            } else if (insideClothCenterEnvelope(clothPoint, geometry)) {
                candidates.push_back({TableDepartureEventKind::ClothReturn,
                    pocketTime,
                    departedRailTop ? departedRailFeature : -1,
                    departedRailTop ?
                        TableDepartureClothReturnProvenance::RailTopLanding :
                        TableDepartureClothReturnProvenance::
                            DirectAirborneLanding});
            }
        }

        const double railTopTime = descendingPlaneTime(ball,
            geometry.railTopSupportedCenterYCm, remaining,
            geometry.gravityYCmS2);
        if (std::isfinite(railTopTime)) {
            const Point3 point = ballisticPosition(ball, railTopTime,
                geometry.gravityYCmS2);
            if (insideRailProjection(point, geometry) &&
                !insidePocketOpening(point, profile))
                candidates.push_back({TableDepartureEventKind::RailTopLanding,
                    railTopTime, railTopFeatureAt(point, geometry)});
        }

        double noseTime = outwardCrossingTime(ball.position.x,
            ball.velocity.x, geometry.playfieldHalfWidthCm -
                geometry.ballRadiusCm, remaining);
        bool noseX = true;
        const double noseZ = outwardCrossingTime(ball.position.z,
            ball.velocity.z, geometry.playfieldHalfLengthCm -
                geometry.ballRadiusCm, remaining);
        if (noseZ < noseTime) { noseTime = noseZ; noseX = false; }
        if (std::isfinite(noseTime)) {
            const Point3 point = ballisticPosition(ball, noseTime,
                geometry.gravityYCmS2);
            if (point.y - geometry.ballRadiusCm <=
                    geometry.cushionNoseCenterYCm + 1e-5 &&
                point.y + geometry.ballRadiusCm >=
                    geometry.clothSurfaceYCm - 1e-5 &&
                !insidePocketOpening(point, profile))
                candidates.push_back({
                    TableDepartureEventKind::CushionNoseContact, noseTime,
                    noseX ? 0 : 1});
        }
        for (bool xAxis : {true, false}) {
            for (bool positiveSide : {false, true}) {
                Candidate edge = upperRailEdgeCandidate(ball, remaining,
                    profile, geometry, xAxis, positiveSide);
                if (std::isfinite(edge.time)) candidates.push_back(edge);
            }
        }

        double outerTime = outwardCrossingTime(ball.position.x,
            ball.velocity.x, geometry.outerHalfWidthCm, remaining);
        bool outerX = true;
        const double outerZ = outwardCrossingTime(ball.position.z,
            ball.velocity.z, geometry.outerHalfLengthCm, remaining);
        if (outerZ < outerTime) { outerTime = outerZ; outerX = false; }
        if (std::isfinite(outerTime))
            candidates.push_back({TableDepartureEventKind::OuterDeparture,
                outerTime, signedPerimeterFeature(outerX,
                    outerX ? ball.velocity.x : ball.velocity.z)});

        const double floorTime = descendingPlaneTime(ball,
            geometry.outsideLandingSupportedCenterYCm, remaining,
            geometry.gravityYCmS2);
        if (std::isfinite(floorTime) && !insideOuterFootprint(
                ballisticPosition(ball, floorTime, geometry.gravityYCmS2),
                geometry))
            candidates.push_back({TableDepartureEventKind::OutsideLanding,
                floorTime, 0});

        if (candidates.empty()) {
            advanceBallisticRecorded(ball, remaining, geometry.gravityYCmS2,
                result, ballIndex, elapsed);
            elapsed += remaining;
            remaining = 0.0;
            break;
        }
        std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& first, const Candidate& second) {
                if (std::fabs(first.time - second.time) > kTimeEpsilon)
                    return first.time < second.time;
                return static_cast<int>(first.kind) <
                    static_cast<int>(second.kind);
            });
        if (candidates.size() > 1 &&
            std::fabs(candidates[0].time - candidates[1].time) <=
                kTimeEpsilon &&
            ((candidates[0].kind == TableDepartureEventKind::PocketEntry &&
              candidates[1].kind == TableDepartureEventKind::OuterDeparture) ||
             (candidates[1].kind == TableDepartureEventKind::PocketEntry &&
              candidates[0].kind == TableDepartureEventKind::OuterDeparture))) {
            ball = snapshot;
            return fail(
                TableDepartureFailureCode::ContradictoryPocketDeparture);
        }
        const Candidate candidate = candidates.front();
        const bool deferSelectedEndpoint = deferEndpointEvent &&
            std::fabs(elapsed + candidate.time - timeStep) <=
                profile.solver.toiToleranceSeconds;
        if (!deferSelectedEndpoint &&
            static_cast<int>(result.events.size()) >= maximumEvents) {
            ball = snapshot;
            return fail(TableDepartureFailureCode::EventBudget);
        }
        advanceBallisticRecorded(ball, candidate.time,
            geometry.gravityYCmS2, result, ballIndex, elapsed);
        elapsed += candidate.time;
        remaining = std::max(0.0, remaining - candidate.time);
        if (deferSelectedEndpoint) {
            result.ok = true;
            result.failureCode = TableDepartureFailureCode::None;
            return result;
        }
        const PhysicalBallDomain sourceDomain = ball.physicalDomain;
        switch (candidate.kind) {
        case TableDepartureEventKind::CushionNoseContact:
            if (candidate.contactNormalComplete) {
                const double normalSpeed =
                    ball.velocity.x * candidate.contactNormal.x +
                    ball.velocity.y * candidate.contactNormal.y +
                    ball.velocity.z * candidate.contactNormal.z;
                ball.velocity.x -= static_cast<float>(2.0 * normalSpeed *
                    candidate.contactNormal.x);
                ball.velocity.y -= static_cast<float>(2.0 * normalSpeed *
                    candidate.contactNormal.y);
                ball.velocity.z -= static_cast<float>(2.0 * normalSpeed *
                    candidate.contactNormal.z);
            } else if (noseX) {
                ball.velocity.x = -ball.velocity.x;
            } else {
                ball.velocity.z = -ball.velocity.z;
            }
            appendEvent(result, candidate.kind, ballIndex, elapsed,
                sourceDomain, ball, candidate.featureId);
            break;
        case TableDepartureEventKind::OuterDeparture:
            appendEvent(result, candidate.kind, ballIndex, elapsed,
                sourceDomain, ball, candidate.featureId);
            break;
        case TableDepartureEventKind::RailTopLanding:
            ball.physicalDomain = PhysicalBallDomain::RailTopSupported;
            ball.position.y = geometry.railTopSupportedCenterYCm;
            ball.velocity.y = 0.0f;
            appendEvent(result, candidate.kind, ballIndex, elapsed,
                sourceDomain, ball, candidate.featureId);
            {
                const float deceleration =
                    profile.tableDeparture.supportedRollingDecelerationCmS2;
                const double entryTime = supportedClothEntryTime(ball,
                    remaining, deceleration, geometry);
                if (std::isfinite(entryTime)) {
                    advanceSupportedRecorded(ball, entryTime, deceleration,
                        profile.surface.torsionalSpinDecelerationRadS2,
                        result, ballIndex, elapsed);
                    ball.physicalDomain = PhysicalBallDomain::Airborne;
                    elapsed += entryTime;
                    remaining = std::max(0.0, remaining - entryTime);
                } else {
                    advanceSupportedRecorded(ball, remaining, deceleration,
                        profile.surface.torsionalSpinDecelerationRadS2,
                        result, ballIndex, elapsed);
                    ball.position.y = geometry.railTopSupportedCenterYCm;
                    remaining = 0.0;
                }
            }
            break;
        case TableDepartureEventKind::OutsideLanding:
            ball.physicalDomain =
                PhysicalBallDomain::OutsideLandingSupported;
            ball.position.y = geometry.outsideLandingSupportedCenterYCm;
            ball.velocity.y = 0.0f;
            appendEvent(result, candidate.kind, ballIndex, elapsed,
                sourceDomain, ball, candidate.featureId);
            advanceSupportedRecorded(ball, remaining,
                profile.tableDeparture.supportedRollingDecelerationCmS2,
                profile.surface.torsionalSpinDecelerationRadS2,
                result, ballIndex, elapsed);
            ball.position.y = geometry.outsideLandingSupportedCenterYCm;
            remaining = 0.0;
            break;
        case TableDepartureEventKind::PocketEntry:
            ball.physicalDomain = PhysicalBallDomain::PocketChannel;
            appendEvent(result, candidate.kind, ballIndex, elapsed,
                sourceDomain, ball, candidate.featureId);
            remaining = 0.0;
            break;
        case TableDepartureEventKind::ClothReturn:
            ball.physicalDomain = PhysicalBallDomain::ClothSupported;
            ball.position.y = geometry.clothSupportedCenterYCm;
            ball.velocity.y = 0.0f;
            appendEvent(result, candidate.kind, ballIndex, elapsed,
                sourceDomain, ball, candidate.featureId,
                candidate.clothReturnProvenance);
            advanceSupportedRecorded(ball, remaining,
                profile.tableDeparture.supportedRollingDecelerationCmS2,
                profile.surface.torsionalSpinDecelerationRadS2,
                result, ballIndex, elapsed);
            ball.position.y = geometry.clothSupportedCenterYCm;
            remaining = 0.0;
            break;
        case TableDepartureEventKind::CrossDomainBallContact:
            ball = snapshot;
            return fail(TableDepartureFailureCode::InvalidState);
        }
    }
    if (!finiteBall(ball)) {
        ball = snapshot;
        return fail(TableDepartureFailureCode::InvalidState);
    }
    result.ok = true;
    result.failureCode = TableDepartureFailureCode::None;
    return result;
}

}  // namespace

TableDepartureGeometry buildTableDepartureGeometry(
    const PhysicsProfile& profile)
{
    TableDepartureGeometry result;
    result.ballRadiusCm = profile.ball.radiusCm;
    result.clothSurfaceYCm = profile.tableDeparture.clothSurfaceYCm;
    result.clothSupportedCenterYCm = result.clothSurfaceYCm +
        result.ballRadiusCm;
    result.cushionNoseCenterYCm = result.clothSurfaceYCm +
        result.ballRadiusCm * profile.cushion.noseHeightRatio;
    result.railTopSurfaceYCm = profile.tableDeparture.railTopSurfaceYCm;
    result.railTopSupportedCenterYCm = result.railTopSurfaceYCm +
        result.ballRadiusCm;
    result.playfieldHalfWidthCm =
        profile.tableBoundary.playfieldWidthCm * 0.5f;
    result.playfieldHalfLengthCm =
        profile.tableBoundary.playfieldLengthCm * 0.5f;
    result.outerHalfWidthCm = profile.tableDeparture.outerRailHalfWidthCm;
    result.outerHalfLengthCm = profile.tableDeparture.outerRailHalfLengthCm;
    result.outsideLandingSurfaceYCm =
        profile.tableDeparture.outsideLandingSurfaceYCm;
    result.outsideLandingSupportedCenterYCm =
        result.outsideLandingSurfaceYCm + result.ballRadiusCm;
    result.gravityYCmS2 = profile.tableDeparture.gravityYCmS2;
    return result;
}

TableDepartureGeometryValidation validateTableDepartureGeometry(
    const TableDepartureGeometry& geometry)
{
    const float values[] = {geometry.ballRadiusCm,
        geometry.clothSurfaceYCm, geometry.clothSupportedCenterYCm,
        geometry.cushionNoseCenterYCm, geometry.railTopSurfaceYCm,
        geometry.railTopSupportedCenterYCm, geometry.playfieldHalfWidthCm,
        geometry.playfieldHalfLengthCm, geometry.outerHalfWidthCm,
        geometry.outerHalfLengthCm, geometry.outsideLandingSurfaceYCm,
        geometry.outsideLandingSupportedCenterYCm, geometry.gravityYCmS2};
    for (float value : values) {
        if (!std::isfinite(value))
            return {false, "nonfinite 3D table-departure geometry"};
    }
    if (!(geometry.ballRadiusCm > 0.0f) ||
        !(geometry.gravityYCmS2 < 0.0f) ||
        !(geometry.outsideLandingSurfaceYCm < geometry.clothSurfaceYCm) ||
        !(geometry.clothSurfaceYCm <= geometry.cushionNoseCenterYCm) ||
        !(geometry.cushionNoseCenterYCm < geometry.railTopSurfaceYCm) ||
        !(geometry.clothSurfaceYCm < geometry.railTopSurfaceYCm) ||
        !(geometry.outerHalfWidthCm > geometry.playfieldHalfWidthCm) ||
        !(geometry.outerHalfLengthCm > geometry.playfieldHalfLengthCm)) {
        return {false, "inconsistent 3D table-departure geometry"};
    }
    return {true, {}};
}

double tableDepartureMinimumSeparationSquared(
    double radiusSumCm, double penetrationSlopCm)
{
    if (!std::isfinite(radiusSumCm) || !(radiusSumCm > 0.0) ||
        !std::isfinite(penetrationSlopCm) || penetrationSlopCm < 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    const double minimum = std::max(0.0,
        radiusSumCm - penetrationSlopCm);
    if (minimum == 0.0) return 0.0;
    float storedMinimum = static_cast<float>(minimum);
    if (static_cast<double>(storedMinimum) > minimum)
        storedMinimum = std::nextafter(storedMinimum,
            -std::numeric_limits<float>::infinity());
    const float storedSquared = storedMinimum * storedMinimum;
    return static_cast<double>(storedSquared);
}

TableDepartureEarliestBatch queryEarliestTableDepartureBatch(
    const GameState& state, double maximumTime,
    const PhysicsProfile& profile, PhysicsBoundaryMode boundaryMode,
    const std::vector<SurfaceMotionAnalyticSegment>& authoritativeMotion,
    const std::array<bool, kBallCount>& departureOwnershipMask)
{
    TableDepartureEarliestBatch query;
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(
        profile);
    if (!validateTableDepartureGeometry(geometry).ok ||
        !validatePhysicsProfile(profile).ok || !std::isfinite(maximumTime) ||
        maximumTime < 0.0) {
        query.failureCode = TableDepartureFailureCode::InvalidGeometry;
        return query;
    }
    for (const BallState& ball : state.balls) {
        if (!finiteBall(ball)) return query;
    }

    std::vector<TableDepartureCandidate> discovered;
    double firstTableTime = maximumTime;
    for (int ballIndex = 0; ballIndex < kBallCount; ++ballIndex) {
        if (!departureOwnershipMask[ballIndex]) continue;
        const BallCandidateQuery ballQuery =
            queryFirstTableCandidatesForBall(state.balls[ballIndex],
                maximumTime, profile, geometry, boundaryMode);
        if (!ballQuery.ok) {
            query.failureCode = ballQuery.failureCode;
            query.failingFirstBall = ballIndex;
            return query;
        }
        for (const Candidate& candidate : ballQuery.candidates) {
            discovered.push_back({candidate.kind, ballIndex, -1,
                candidate.time, candidate.featureId,
                candidate.clothReturnProvenance, candidate.contactNormal,
                candidate.contactNormalComplete});
            firstTableTime = std::min(firstTableTime, candidate.time);
        }
    }
    std::array<std::vector<SurfaceMotionAnalyticSegment>, kBallCount>
        motionByBall;
    std::array<bool, kBallCount> motionValidated{};
    for (int first = 0; first < kBallCount; ++first) {
        if (state.balls[first].pocketed) continue;
        for (int second = first + 1; second < kBallCount; ++second) {
            if (state.balls[second].pocketed ||
                (!departureOwnershipMask[first] &&
                 !departureOwnershipMask[second]))
                continue;
            const double crossHorizon = std::min(maximumTime,
                firstTableTime);
            for (int ballIndex : {first, second}) {
                if (motionValidated[ballIndex]) continue;
                if (!validateMotionPartitionForBall(authoritativeMotion,
                        ballIndex, state.balls[ballIndex], crossHorizon,
                        profile, motionByBall[ballIndex])) {
                    query.failureCode =
                        TableDepartureFailureCode::InvalidState;
                    query.failingFirstBall = first;
                    query.failingSecondBall = second;
                    return query;
                }
                motionValidated[ballIndex] = true;
            }
            double time = std::numeric_limits<double>::infinity();
            if (crossHorizon == 0.0) {
                time = earliestCrossDomainTime(state.balls[first],
                    state.balls[second], 0.0, geometry,
                    profile.solver.residualToleranceCmS);
            } else {
                const PiecewisePairQuery pair = queryPiecewisePair(
                    motionByBall[first], motionByBall[second], crossHorizon,
                    2.0 * geometry.ballRadiusCm,
                    profile.solver.residualToleranceCmS,
                    profile.solver.penetrationSlopCm);
                if (!pair.ok) {
                    query.failureCode =
                        TableDepartureFailureCode::AmbiguousInitialPenetration;
                    query.failingFirstBall = first;
                    query.failingSecondBall = second;
                    query.initialPenetrationCm =
                        pair.initialPenetrationCm;
                    return query;
                }
                time = pair.time;
            }
            if (std::isfinite(time))
                discovered.push_back({
                    TableDepartureEventKind::CrossDomainBallContact,
                    first, second, time});
        }
    }

    query.ok = true;
    query.failureCode = TableDepartureFailureCode::None;
    if (discovered.empty()) return query;
    std::sort(discovered.begin(), discovered.end(),
        [](const TableDepartureCandidate& first,
            const TableDepartureCandidate& second) {
            if (first.timeSeconds != second.timeSeconds)
                return first.timeSeconds < second.timeSeconds;
            if (first.kind != second.kind)
                return static_cast<int>(first.kind) <
                    static_cast<int>(second.kind);
            if (first.firstBall != second.firstBall)
                return first.firstBall < second.firstBall;
            return first.secondBall < second.secondBall;
        });
    query.earliestTimeSeconds = discovered.front().timeSeconds;
    for (const TableDepartureCandidate& candidate : discovered) {
        if (candidate.timeSeconds > query.earliestTimeSeconds +
                profile.solver.toiToleranceSeconds)
            break;
        query.candidates.push_back(candidate);
    }
    const auto cross = std::find_if(query.candidates.begin(),
        query.candidates.end(), [](const TableDepartureCandidate& candidate) {
            return candidate.kind ==
                TableDepartureEventKind::CrossDomainBallContact;
        });
    const auto table = std::find_if(query.candidates.begin(),
        query.candidates.end(), [](const TableDepartureCandidate& candidate) {
            return candidate.kind !=
                TableDepartureEventKind::CrossDomainBallContact;
        });
    const bool onlySupportLandings = std::all_of(query.candidates.begin(),
        query.candidates.end(), [](const TableDepartureCandidate& candidate) {
            return candidate.kind ==
                    TableDepartureEventKind::CrossDomainBallContact ||
                candidate.kind == TableDepartureEventKind::ClothReturn ||
                candidate.kind == TableDepartureEventKind::RailTopLanding ||
                candidate.kind == TableDepartureEventKind::OutsideLanding;
        });
    if (cross != query.candidates.end() && table != query.candidates.end() &&
        !onlySupportLandings) {
        query.ok = false;
        query.failureCode =
            TableDepartureFailureCode::AmbiguousCrossDomainTableContact;
        query.ambiguousCrossDomainContactTimeSeconds = cross->timeSeconds;
        query.ambiguousTableContactTimeSeconds = table->timeSeconds;
    }
    return query;
}

TableDepartureEarliestBatch queryEarliestTableDepartureBatch(
    const GameState& state, double maximumTime,
    const PhysicsProfile& profile, PhysicsBoundaryMode boundaryMode)
{
    TableDepartureEarliestBatch failed;
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(
        profile);
    if (!validateTableDepartureGeometry(geometry).ok ||
        !validatePhysicsProfile(profile).ok || !std::isfinite(maximumTime) ||
        maximumTime < 0.0)
        return failed;

    double predictionHorizon = maximumTime;
    for (int index = 0; index < kBallCount; ++index) {
        const BallCandidateQuery ballQuery =
            queryFirstTableCandidatesForBall(state.balls[index],
                maximumTime, profile, geometry, boundaryMode);
        if (!ballQuery.ok) {
            failed.failureCode = ballQuery.failureCode;
            failed.failingFirstBall = index;
            return failed;
        }
        for (const Candidate& candidate : ballQuery.candidates)
            predictionHorizon = std::min(predictionHorizon,
                candidate.time);
    }

    std::vector<SurfaceMotionAnalyticSegment> predicted;
    std::array<bool, kBallCount> ownership{};
    for (int index = 0; index < kBallCount; ++index) {
        const BallState& source = state.balls[index];
        ownership[index] = !source.pocketed &&
            (source.physicalDomain != PhysicalBallDomain::ClothSupported ||
             source.velocity.y > 0.0f);
        if (source.pocketed || source.physicalDomain ==
                PhysicalBallDomain::PocketChannel ||
            !(predictionHorizon > 0.0))
            continue;
        BallState copy = source;
        TableDepartureStepResult motion = advanceOne(copy,
            predictionHorizon, profile, boundaryMode, 0, index, true);
        if (!motion.ok || !motion.events.empty()) {
            failed.failureCode = motion.failureCode;
            failed.failingFirstBall = index;
            return failed;
        }
        predicted.insert(predicted.end(), motion.analyticSegments.begin(),
            motion.analyticSegments.end());
    }
    return queryEarliestTableDepartureBatch(state, maximumTime, profile,
        boundaryMode, predicted, ownership);
}

TableDepartureStepResult advanceTableDepartureDomains(
    GameState& state, double duration, const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode,
    const std::array<bool, kBallCount>& ownershipMask)
{
    const GameState snapshot = state;
    if (!std::isfinite(duration) || duration < 0.0)
        return fail(TableDepartureFailureCode::InvalidState);
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(
        profile);
    if (!validateTableDepartureGeometry(geometry).ok ||
        !validatePhysicsProfile(profile).ok)
        return fail(TableDepartureFailureCode::InvalidGeometry);

    GameState staged = state;
    TableDepartureStepResult combined;
    combined.ok = true;
    combined.failureCode = TableDepartureFailureCode::None;
    for (int index = 0; index < kBallCount; ++index) {
        if (!ownershipMask[index]) continue;
        const BallCandidateQuery tableQuery =
            queryFirstTableCandidatesForBall(staged.balls[index], duration,
                profile, geometry, boundaryMode);
        if (!tableQuery.ok) {
            state = snapshot;
            return fail(tableQuery.failureCode);
        }
        bool deferEndpointCandidate = false;
        for (const Candidate& candidate : tableQuery.candidates) {
            if (candidate.time < duration -
                    profile.solver.toiToleranceSeconds) {
                state = snapshot;
                return fail(TableDepartureFailureCode::EventBudget);
            }
            deferEndpointCandidate = true;
        }
        TableDepartureStepResult local = advanceOne(staged.balls[index],
            duration, profile, boundaryMode, 0, index,
            deferEndpointCandidate);
        if (!local.ok || !local.events.empty()) {
            state = snapshot;
            return fail(local.ok ? TableDepartureFailureCode::InvalidState :
                local.failureCode);
        }
        combined.analyticSegments.insert(combined.analyticSegments.end(),
            local.analyticSegments.begin(), local.analyticSegments.end());
    }
    stableSortAnalyticSegments(combined);
    state = staged;
    return combined;
}

TableDepartureStepResult resolveTableDepartureBatch(
    GameState& state, const TableDepartureEarliestBatch& batch,
    const PhysicsProfile& profile, double absoluteWholeTickTimeSeconds,
    int eventOrdinalBase)
{
    const GameState snapshot = state;
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(
        profile);
    if (!batch.ok || batch.candidates.empty() ||
        !validateTableDepartureGeometry(geometry).ok ||
        !validatePhysicsProfile(profile).ok ||
        !std::isfinite(absoluteWholeTickTimeSeconds) ||
        absoluteWholeTickTimeSeconds < 0.0 || eventOrdinalBase < 0)
        return fail(batch.ok ? TableDepartureFailureCode::InvalidState :
            batch.failureCode);

    GameState staged = state;
    TableDepartureStepResult result;
    result.ok = true;
    result.failureCode = TableDepartureFailureCode::None;
    std::array<bool, kBallCount> resolved{};
    auto appendResolvedEvent = [&](const TableDepartureCandidate& candidate,
                                   PhysicalBallDomain sourceDomain,
                                   const Point3& velocityBeforeCmS,
                                   const BallState& first,
                                   const BallState* second) {
        TableDepartureEvent event;
        event.kind = candidate.kind;
        event.ballIndex = candidate.firstBall;
        event.ordinal = eventOrdinalBase +
            static_cast<int>(result.events.size());
        event.trajectoryGeneration = first.trajectoryGeneration;
        event.sourceDomain = sourceDomain;
        event.clothReturnProvenance = candidate.clothReturnProvenance;
        if (candidate.kind == TableDepartureEventKind::OuterDeparture)
            event.boundaryFeatureId = candidate.featureId;
        else if (candidate.kind == TableDepartureEventKind::RailTopLanding)
            event.railFeatureId = candidate.featureId;
        else if (candidate.kind == TableDepartureEventKind::OutsideLanding)
            event.landingSurfaceId = candidate.featureId;
        else if (candidate.kind == TableDepartureEventKind::PocketEntry)
            event.pocketId = candidate.featureId;
        else if (candidate.kind == TableDepartureEventKind::ClothReturn &&
            candidate.clothReturnProvenance ==
                TableDepartureClothReturnProvenance::RailTopLanding)
            event.railFeatureId = candidate.featureId;
        event.absoluteWholeTickTimeSeconds = absoluteWholeTickTimeSeconds;
        event.transitionWitnessComplete = true;
        event.velocityBeforeCmS = velocityBeforeCmS;
        event.velocityBeforeWitnessComplete = true;
        event.velocityAfterCmS = first.velocity;
        event.targetDomain = first.physicalDomain;
        std::ostringstream identity;
        if (second != nullptr) {
            event.secondVelocityAfterCmS = second->velocity;
            event.secondTargetDomain = second->physicalDomain;
            identity << "cross_domain_ball_contact:" << candidate.firstBall
                     << ':' << candidate.secondBall << ':' << event.ordinal
                     << ':' << absoluteWholeTickTimeSeconds;
        } else {
            identity << candidate.firstBall << ':' << event.ordinal << ':'
                     << eventName(candidate.kind) << ':'
                     << absoluteWholeTickTimeSeconds;
        }
        event.stableIdentity = identity.str();
        result.events.push_back(event);
    };

    for (const TableDepartureCandidate& candidate : batch.candidates) {
        if (candidate.firstBall < 0 || candidate.firstBall >= kBallCount ||
            !std::isfinite(candidate.timeSeconds) ||
            std::fabs(candidate.timeSeconds - batch.earliestTimeSeconds) >
                profile.solver.toiToleranceSeconds ||
            resolved[candidate.firstBall]) {
            state = snapshot;
            return fail(TableDepartureFailureCode::InvalidState);
        }
        BallState& first = staged.balls[candidate.firstBall];
        const PhysicalBallDomain sourceDomain = first.physicalDomain;
        const Point3 velocityBeforeCmS = first.velocity;
        if (first.pocketed || first.physicalDomain ==
                PhysicalBallDomain::PocketChannel) {
            state = snapshot;
            return fail(TableDepartureFailureCode::InvalidState);
        }
        if (candidate.kind ==
                TableDepartureEventKind::CrossDomainBallContact) {
            if (candidate.secondBall < 0 || candidate.secondBall >= kBallCount ||
                candidate.secondBall == candidate.firstBall ||
                resolved[candidate.secondBall]) {
                state = snapshot;
                return fail(TableDepartureFailureCode::InvalidState);
            }
            BallState& second = staged.balls[candidate.secondBall];
            if (second.pocketed || second.physicalDomain ==
                    PhysicalBallDomain::PocketChannel ||
                !snapCrossDomainPairToAnalyticContact(first, second,
                    2.0 * geometry.ballRadiusCm)) {
                state = snapshot;
                return fail(TableDepartureFailureCode::InvalidState);
            }
            const BallBallContactResult contact = resolveBallBallContact(
                first, second, profile.ball, profile.ball);
            if (!contact.velocityImpulseApplied && !contact.positionCorrected) {
                state = snapshot;
                return fail(TableDepartureFailureCode::InvalidState);
            }
            applySupportedContactReaction(first, geometry);
            applySupportedContactReaction(second, geometry);
            appendResolvedEvent(candidate, sourceDomain, velocityBeforeCmS,
                first, &second);
            resolved[candidate.firstBall] = true;
            resolved[candidate.secondBall] = true;
            continue;
        }

        switch (candidate.kind) {
        case TableDepartureEventKind::CushionNoseContact:
            if (candidate.featureId != 0 && candidate.featureId != 1) {
                state = snapshot;
                return fail(TableDepartureFailureCode::InvalidState);
            }
            if (candidate.contactNormalComplete) {
                if (!finitePoint(candidate.contactNormal)) {
                    state = snapshot;
                    return fail(TableDepartureFailureCode::InvalidState);
                }
                const double length = std::sqrt(
                    candidate.contactNormal.x * candidate.contactNormal.x +
                    candidate.contactNormal.y * candidate.contactNormal.y +
                    candidate.contactNormal.z * candidate.contactNormal.z);
                if (std::fabs(length - 1.0) > 1e-4) {
                    state = snapshot;
                    return fail(TableDepartureFailureCode::InvalidState);
                }
                const double normalSpeed =
                    first.velocity.x * candidate.contactNormal.x +
                    first.velocity.y * candidate.contactNormal.y +
                    first.velocity.z * candidate.contactNormal.z;
                first.velocity.x -= static_cast<float>(2.0 * normalSpeed *
                    candidate.contactNormal.x);
                first.velocity.y -= static_cast<float>(2.0 * normalSpeed *
                    candidate.contactNormal.y);
                first.velocity.z -= static_cast<float>(2.0 * normalSpeed *
                    candidate.contactNormal.z);
            } else if (candidate.featureId == 0) {
                first.velocity.x = -first.velocity.x;
            } else {
                first.velocity.z = -first.velocity.z;
            }
            break;
        case TableDepartureEventKind::OuterDeparture:
            break;
        case TableDepartureEventKind::RailTopLanding:
            first.physicalDomain = PhysicalBallDomain::RailTopSupported;
            first.position.y = geometry.railTopSupportedCenterYCm;
            first.velocity.y = 0.0f;
            first.speed = std::hypot(first.velocity.x, first.velocity.z);
            break;
        case TableDepartureEventKind::OutsideLanding:
            first.physicalDomain =
                PhysicalBallDomain::OutsideLandingSupported;
            first.position.y = geometry.outsideLandingSupportedCenterYCm;
            first.velocity.y = 0.0f;
            first.speed = std::hypot(first.velocity.x, first.velocity.z);
            break;
        case TableDepartureEventKind::ClothReturn:
            first.physicalDomain = PhysicalBallDomain::ClothSupported;
            first.position.y = geometry.clothSupportedCenterYCm;
            first.velocity.y = 0.0f;
            first.speed = std::hypot(first.velocity.x, first.velocity.z);
            break;
        case TableDepartureEventKind::PocketEntry:
            first.physicalDomain = PhysicalBallDomain::PocketChannel;
            break;
        case TableDepartureEventKind::CrossDomainBallContact:
            state = snapshot;
            return fail(TableDepartureFailureCode::InvalidState);
        }
        appendResolvedEvent(candidate, sourceDomain, velocityBeforeCmS,
            first, nullptr);
        resolved[candidate.firstBall] = true;
    }
    state = staged;
    return result;
}

TableDepartureStepResult advanceTableDepartureMotion(
    BallState& ball, float timeStep, const PhysicsProfile& profile)
{
    return advanceOne(ball, timeStep, profile,
        PhysicsBoundaryMode::ProductionTable,
        profile.solver.maximumEventsPerTick, 0);
}

static TableDepartureStepResult advanceTableDeparturePhysicsInternal(
    GameState& state, float timeStep, const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode, int maximumEvents,
    bool allowZeroEventBudget)
{
    const GameState snapshot = state;
    GameState staged = state;
    TableDepartureStepResult combined;
    combined.ok = true;
    combined.failureCode = TableDepartureFailureCode::None;
    const TableDepartureGeometry geometry = buildTableDepartureGeometry(
        profile);
    if (!validateTableDepartureGeometry(geometry).ok ||
        !validatePhysicsProfile(profile).ok || !std::isfinite(timeStep) ||
        timeStep < 0.0f || maximumEvents < 0 ||
        (!allowZeroEventBudget && maximumEvents == 0)) {
        return fail(TableDepartureFailureCode::InvalidState);
    }

    double firstCrossTime = std::numeric_limits<double>::infinity();
    int crossFirst = -1;
    int crossSecond = -1;
    for (int first = 0; first < kBallCount; ++first) {
        if (staged.balls[first].pocketed) continue;
        for (int second = first + 1; second < kBallCount; ++second) {
            if (staged.balls[second].pocketed ||
                !crossDomainPair(staged.balls[first], staged.balls[second]))
                continue;
            const double time = earliestCrossDomainTime(staged.balls[first],
                staged.balls[second], timeStep, geometry,
                profile.solver.residualToleranceCmS);
            if (time < firstCrossTime) {
                firstCrossTime = time;
                crossFirst = first;
                crossSecond = second;
            }
        }
    }
    if (std::isfinite(firstCrossTime)) {
        if (firstCrossTime == 0.0) {
            state = snapshot;
            return fail(
                TableDepartureFailureCode::AmbiguousInitialPenetration);
        }
        GameState tableOnly = staged;
        const TableDepartureStepResult tableResult = [&]() {
            TableDepartureStepResult result;
            result.ok = true;
            result.failureCode = TableDepartureFailureCode::None;
            for (int index = 0; index < kBallCount; ++index) {
                if (tableOnly.balls[index].pocketed) continue;
                TableDepartureStepResult local = advanceOne(
                    tableOnly.balls[index], timeStep, profile, boundaryMode,
                    maximumEvents, index);
                if (!local.ok) return local;
                result.events.insert(result.events.end(), local.events.begin(),
                    local.events.end());
            }
            return result;
        }();
        if (!tableResult.ok) {
            state = snapshot;
            return tableResult;
        }
        double ambiguousTableTime = std::numeric_limits<double>::infinity();
        for (const TableDepartureEvent& event : tableResult.events) {
            if (std::fabs(event.absoluteWholeTickTimeSeconds - firstCrossTime) <=
                    profile.solver.toiToleranceSeconds) {
                ambiguousTableTime = event.absoluteWholeTickTimeSeconds;
                break;
            }
        }
        if (std::isfinite(ambiguousTableTime)) {
            TableDepartureStepResult result = fail(
                TableDepartureFailureCode::AmbiguousCrossDomainTableContact);
            result.ambiguousCrossDomainContactTimeSeconds = firstCrossTime;
            result.ambiguousTableContactTimeSeconds = ambiguousTableTime;
            state = snapshot;
            return result;
        }
        if (maximumEvents < 1) {
            state = snapshot;
            return fail(TableDepartureFailureCode::EventBudget);
        }
        for (int index = 0; index < kBallCount; ++index) {
            if (!staged.balls[index].pocketed &&
                staged.balls[index].physicalDomain !=
                    PhysicalBallDomain::PocketChannel)
                advanceDomainSegmentRecorded(staged.balls[index],
                    firstCrossTime, profile, geometry, combined, index, 0.0);
        }
        const PhysicalBallDomain crossFirstSourceDomain =
            staged.balls[crossFirst].physicalDomain;
        if (!snapCrossDomainPairToAnalyticContact(staged.balls[crossFirst],
                staged.balls[crossSecond], 2.0 * geometry.ballRadiusCm)) {
            state = snapshot;
            return fail(TableDepartureFailureCode::InvalidState);
        }
        const BallBallContactResult contact = resolveBallBallContact(
            staged.balls[crossFirst], staged.balls[crossSecond],
            profile.ball, profile.ball);
        if (!contact.velocityImpulseApplied && !contact.positionCorrected) {
            state = snapshot;
            return fail(TableDepartureFailureCode::InvalidState);
        }
        applySupportedContactReaction(staged.balls[crossFirst], geometry);
        applySupportedContactReaction(staged.balls[crossSecond], geometry);
        TableDepartureEvent crossEvent;
        crossEvent.kind = TableDepartureEventKind::CrossDomainBallContact;
        crossEvent.ballIndex = crossFirst;
        crossEvent.ordinal = 0;
        crossEvent.trajectoryGeneration =
            staged.balls[crossFirst].trajectoryGeneration;
        crossEvent.sourceDomain = crossFirstSourceDomain;
        crossEvent.absoluteWholeTickTimeSeconds = firstCrossTime;
        std::ostringstream identity;
        identity << "cross_domain_ball_contact:" << crossFirst << ':'
                 << crossSecond << ':' << crossEvent.ordinal << ':'
                 << firstCrossTime;
        crossEvent.stableIdentity = identity.str();
        crossEvent.transitionWitnessComplete = true;
        crossEvent.velocityAfterCmS = staged.balls[crossFirst].velocity;
        crossEvent.targetDomain = staged.balls[crossFirst].physicalDomain;
        crossEvent.secondVelocityAfterCmS =
            staged.balls[crossSecond].velocity;
        crossEvent.secondTargetDomain =
            staged.balls[crossSecond].physicalDomain;
        combined.events.push_back(crossEvent);
        const double remaining = std::max(0.0,
            static_cast<double>(timeStep) - firstCrossTime);
        TableDepartureStepResult tail = advanceTableDeparturePhysicsInternal(
            staged,
            static_cast<float>(remaining), profile, boundaryMode,
            maximumEvents - 1, true);
        if (!tail.ok) {
            state = snapshot;
            return tail;
        }
        std::array<int, kBallCount> analyticOrdinalBase{};
        for (const SurfaceMotionAnalyticSegment& segment :
                combined.analyticSegments) {
            if (segment.ballIndex >= 0 && segment.ballIndex < kBallCount)
                ++analyticOrdinalBase[segment.ballIndex];
        }
        for (SurfaceMotionAnalyticSegment segment : tail.analyticSegments) {
            segment.wholeTickStartOffsetSeconds += firstCrossTime;
            if (segment.ballIndex >= 0 && segment.ballIndex < kBallCount)
                segment.segmentOrdinal += analyticOrdinalBase[
                    segment.ballIndex];
            combined.analyticSegments.push_back(segment);
        }
        for (TableDepartureEvent event : tail.events) {
            event.ordinal = static_cast<int>(combined.events.size());
            event.absoluteWholeTickTimeSeconds += firstCrossTime;
            std::ostringstream rebased;
            if (event.kind == TableDepartureEventKind::CrossDomainBallContact) {
                const std::size_t timeSeparator =
                    event.stableIdentity.rfind(':');
                const std::size_t ordinalSeparator = timeSeparator ==
                        std::string::npos ? std::string::npos :
                    event.stableIdentity.rfind(':', timeSeparator - 1);
                if (ordinalSeparator == std::string::npos) {
                    state = snapshot;
                    return fail(TableDepartureFailureCode::InvalidState);
                }
                rebased << event.stableIdentity.substr(0, ordinalSeparator)
                    << ':' << event.ordinal << ':'
                    << event.absoluteWholeTickTimeSeconds;
            } else {
                rebased << event.ballIndex << ':' << event.ordinal << ':'
                    << eventName(event.kind) << ':'
                    << event.absoluteWholeTickTimeSeconds;
            }
            event.stableIdentity = rebased.str();
            combined.events.push_back(event);
        }
        stableSortAnalyticSegments(combined);
        state = staged;
        return combined;
    }
    int remainingEvents = maximumEvents;
    for (int index = 0; index < kBallCount; ++index) {
        TableDepartureStepResult result = advanceOne(staged.balls[index],
            timeStep, profile, boundaryMode, remainingEvents, index);
        if (!result.ok) {
            state = snapshot;
            return result;
        }
        for (TableDepartureEvent event : result.events) {
            event.ordinal = static_cast<int>(combined.events.size());
            std::ostringstream identity;
            identity << event.ballIndex << ':' << event.ordinal << ':'
                     << eventName(event.kind) << ':'
                     << event.absoluteWholeTickTimeSeconds;
            event.stableIdentity = identity.str();
            combined.events.push_back(event);
        }
        combined.analyticSegments.insert(combined.analyticSegments.end(),
            result.analyticSegments.begin(), result.analyticSegments.end());
        remainingEvents -= static_cast<int>(result.events.size());
    }
    stableSortAnalyticSegments(combined);
    state = staged;
    return combined;
}

TableDepartureStepResult advanceTableDeparturePhysics(
    GameState& state, float timeStep, const PhysicsProfile& profile,
    PhysicsBoundaryMode boundaryMode, int maximumEvents)
{
    return advanceTableDeparturePhysicsInternal(state, timeStep, profile,
        boundaryMode, maximumEvents, false);
}

int countTableDepartureEvents(const TableDepartureStepResult& result,
    TableDepartureEventKind kind)
{
    return static_cast<int>(std::count_if(result.events.begin(),
        result.events.end(), [kind](const TableDepartureEvent& event) {
            return event.kind == kind;
        }));
}

}  // namespace billiardgl
