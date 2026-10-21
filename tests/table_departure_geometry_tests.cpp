#include "game_state.h"
#include "ball_ball_contact.h"
#include "physics.h"
#include "physics_boundary_mode.h"
#include "physics_profile.h"
#include "table_departure_geometry.h"

#include <cmath>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <limits>
#include <sstream>
#include <iomanip>
#include <string>
#include <type_traits>
#include <tuple>
#include <utility>

namespace {

void expect(bool value, const char* message)
{
    if (!value) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool close(double first, double second, double tolerance = 1e-4)
{
    return std::fabs(first - second) <= tolerance;
}

std::string compactWhitespace(const std::string& source)
{
    std::string compact;
    bool pendingSpace = false;
    for (unsigned char character : source) {
        if (std::isspace(character)) {
            pendingSpace = !compact.empty();
            continue;
        }
        if (pendingSpace && !compact.empty() && compact.back() != '(' &&
            compact.back() != '{' && character != ')' && character != '}' &&
            character != ';' && character != ',') compact.push_back(' ');
        compact.push_back(static_cast<char>(character));
        pendingSpace = false;
    }
    return compact;
}

std::string extractCompleteFunction(const std::string& source,
    const std::string& signatureAnchor)
{
    const std::size_t signature = source.find(signatureAnchor);
    if (signature == std::string::npos) return {};
    const std::size_t open = source.find('{', signature);
    if (open == std::string::npos) return {};
    int depth = 0;
    for (std::size_t index = open; index < source.size(); ++index) {
        if (source[index] == '{') ++depth;
        if (source[index] == '}' && --depth == 0)
            return source.substr(signature, index - signature + 1);
    }
    return {};
}

std::string extractBracedStatement(const std::string& source,
    std::size_t statementStart)
{
    if (statementStart == std::string::npos) return {};
    const std::size_t open = source.find('{', statementStart);
    if (open == std::string::npos) return {};
    int depth = 0;
    for (std::size_t index = open; index < source.size(); ++index) {
        if (source[index] == '{') ++depth;
        if (source[index] == '}' && --depth == 0)
            return source.substr(statementStart, index - statementStart + 1);
    }
    return {};
}

int countToken(const std::string& source, const std::string& token)
{
    int count = 0;
    for (std::size_t position = 0;
         (position = source.find(token, position)) != std::string::npos;
         position += token.size()) ++count;
    return count;
}

const std::array<const char*, 6> kCanonicalTelemetryLedgers{{
    "contacts", "solverEvents", "surfaceMotion", "analyticSurfaceMotion",
    "planeCrossings", "planeAttestations"}};

bool clearsAllCanonicalTelemetry(const std::string& helperSource)
{
    const std::string text = compactWhitespace(helperSource);
    if (text.empty() || text.find("if(") != std::string::npos ||
        text.find("switch(") != std::string::npos ||
        text.find("for(") != std::string::npos ||
        text.find("while(") != std::string::npos) return false;
    for (const char* ledger : kCanonicalTelemetryLedgers) {
        const std::string clear = std::string("telemetry.") + ledger +
            ".clear();";
        if (countToken(text, clear) != 1) return false;
    }
    return true;
}

bool hasComposedFinalGateContract(const std::string& functionSource,
    const std::string& clearHelperSource,
    const std::string& validatorHelperSource)
{
    const std::string text = compactWhitespace(functionSource);
    const std::string validator = compactWhitespace(validatorHelperSource);
    const std::string scheduler = "runComposedEventScheduler(";
    const std::string validate = "validateComposedFinalState(";
    const std::string partition = "verifyComposedAnalyticPartition(";
    const std::string commit = "state = staged;";
    const std::string publish = "appendPlaneObservationEvidence(";
    const std::size_t schedulerAt = text.find(scheduler);
    const std::size_t validateAt = text.find(validate);
    const std::size_t partitionAt = text.find(partition, validateAt);
    const std::size_t publishAt = text.find(publish, validateAt);
    const std::size_t commitAt = text.find(commit);
    if (schedulerAt == std::string::npos || validateAt == std::string::npos ||
        partitionAt == std::string::npos ||
        commitAt == std::string::npos || publishAt == std::string::npos ||
        !(schedulerAt < validateAt && validateAt < partitionAt &&
          partitionAt < publishAt && publishAt < commitAt) ||
        countToken(text, validate) != 1 ||
        text.find("solverEvents.size() *") != std::string::npos ||
        text.find("BILLIARDGL_TEST") != std::string::npos ||
        text.find("injectFinal") != std::string::npos ||
        !clearsAllCanonicalTelemetry(clearHelperSource) ||
        countToken(validator, "totalMechanicalEnergyIncludingGravityJ(") != 2 ||
        validator.find("profile.solver.passiveEnergyToleranceJ") ==
            std::string::npos ||
        validator.find("representationBound") == std::string::npos ||
        validator.find("PhysicsFailureCode::NonfiniteState") ==
            std::string::npos ||
        validator.find("PhysicsFailureCode::NonfiniteEnergy") ==
            std::string::npos ||
        validator.find("PhysicsFailureCode::PassiveEnergyCreation") ==
            std::string::npos ||
        validator.find("translationalKineticEnergyJ(") != std::string::npos ||
        validator.find("solverEvents.size() *") != std::string::npos)
        return false;
    const std::size_t failOpen = text.find("if (!validateComposedFinalState(");
    if (failOpen == std::string::npos || failOpen > commitAt) return false;
    const std::string failure = extractBracedStatement(text, failOpen);
    if (failure.empty()) return false;
    const std::size_t partitionFailOpen = text.find(
        "if (!verifyComposedAnalyticPartition(", validateAt);
    const std::string partitionFailure = extractBracedStatement(text,
        partitionFailOpen);
    if (partitionFailure.empty()) return false;
    const std::size_t planeFailOpen = text.find(
        "if (!attestation.complete)", publishAt);
    const std::string planeFailure = extractBracedStatement(text,
        planeFailOpen);
    if (planeFailure.empty()) return false;
    return failure.find("state = snapshot;") != std::string::npos &&
        failure.find("clearCanonicalTelemetry(telemetry);") !=
            std::string::npos &&
        failure.find("return telemetry;") != std::string::npos &&
        partitionFailure.find("state = snapshot;") != std::string::npos &&
        partitionFailure.find("clearCanonicalTelemetry(telemetry);") !=
            std::string::npos &&
        partitionFailure.find("return telemetry;") != std::string::npos &&
        planeFailure.find("state = snapshot;") != std::string::npos &&
        planeFailure.find("clearCanonicalTelemetry(telemetry);") !=
            std::string::npos &&
        planeFailure.find("return telemetry;") != std::string::npos;
}

bool hasPrivateLineageConsumerContract(const std::string& functionSource,
    const std::string& verifyHelperSource,
    const std::string& appendHelperSource)
{
    const std::string text = compactWhitespace(functionSource);
    const std::string verifier = compactWhitespace(verifyHelperSource);
    const std::string appender = compactWhitespace(appendHelperSource);
    const std::string verify = "verifyPrivateAnalyticLineage(";
    const std::string append = "appendRebasedPrivateAnalyticSegments(";
    const std::size_t verifyAt = text.find(verify);
    const std::size_t appendAt = text.find(append, verifyAt);
    const std::size_t validateAt = text.find("validateComposedFinalState(",
        appendAt);
    const std::size_t planeAt = text.find("appendPlaneObservationEvidence(",
        validateAt);
    const std::size_t commitAt = text.find("state = staged;");
    if (verifyAt == std::string::npos || appendAt == std::string::npos ||
        validateAt == std::string::npos || commitAt == std::string::npos ||
        planeAt == std::string::npos || !(verifyAt < appendAt &&
            appendAt < validateAt && validateAt < planeAt &&
            planeAt < commitAt) || verifier.empty() || appender.empty() ||
        text.find("if (!attestation.complete)", planeAt) ==
            std::string::npos ||
        text.find("contacts.push_back(privateEvent)") != std::string::npos ||
        text.find("solverEvents.push_back(privateEvent)") !=
            std::string::npos ||
        text.find("tail.events.size()") != std::string::npos)
        return false;
    const std::array<const char*, 13> verifierTokens{{
        "result.analyticSegments", "result.events", "before.balls",
        "after.balls", "duration", "profile.solver.toiToleranceSeconds",
        "profile.solver.penetrationSlopCm", "stableIdentity",
        "segment.motionState", "segment.startPositionCm",
        "segment.endPositionCm", "segment.startVelocityCmS",
        "segment.constantAccelerationCmS2"}};
    for (const char* token : verifierTokens)
        if (verifier.find(token) == std::string::npos) return false;
    const std::string rebase =
        "segment.wholeTickStartOffsetSeconds += prefixDuration;";
    const std::string ordinal =
        "segment.segmentOrdinal = static_cast<int>(telemetry.analyticSurfaceMotion.size());";
    if (countToken(appender, rebase) != 1 ||
        countToken(appender, ordinal) != 1 ||
        appender.find("telemetry.analyticSurfaceMotion.push_back(segment);") ==
            std::string::npos ||
        appender.find("endPositionCm =") != std::string::npos ||
        appender.find("startPositionCm =") != std::string::npos ||
        appender.find("constantAccelerationCmS2 =") != std::string::npos ||
        appender.find("contacts.push_back") != std::string::npos ||
        appender.find("solverEvents.push_back") != std::string::npos ||
        appender.find("result.events") != std::string::npos)
        return false;
    const std::size_t planeFailOpen = text.find(
        "if (!attestation.complete)", planeAt);
    const std::string planeFailure = extractBracedStatement(text,
        planeFailOpen);
    if (planeFailure.empty()) return false;
    if (planeFailure.find("state = snapshot;") == std::string::npos ||
        planeFailure.find("clearCanonicalTelemetry(telemetry);") ==
            std::string::npos ||
        planeFailure.find("return telemetry;") == std::string::npos)
        return false;
    return true;
}

struct IndependentSweptSphereRoot {
    bool hit = false;
    double timeSeconds = std::numeric_limits<double>::quiet_NaN();
    double approachDerivative = std::numeric_limits<double>::quiet_NaN();
};

double domainAccelerationY(const billiardgl::BallState& ball, double gravity)
{
    return ball.physicalDomain == billiardgl::PhysicalBallDomain::Airborne
        ? gravity : 0.0;
}

std::array<double, 3> independentPositionAt(
    const billiardgl::BallState& ball, double time, double gravity)
{
    return {{
        ball.position.x + ball.velocity.x * time,
        ball.position.y + ball.velocity.y * time +
            0.5 * domainAccelerationY(ball, gravity) * time * time,
        ball.position.z + ball.velocity.z * time,
    }};
}

std::array<double, 3> independentVelocityAt(
    const billiardgl::BallState& ball, double time, double gravity)
{
    return {{ball.velocity.x,
        ball.velocity.y + domainAccelerationY(ball, gravity) * time,
        ball.velocity.z}};
}

double independentSeparationFunction(const billiardgl::BallState& first,
    const billiardgl::BallState& second, double time, double gravity,
    double radiusSum)
{
    const std::array<double, 3> a = independentPositionAt(
        first, time, gravity);
    const std::array<double, 3> b = independentPositionAt(
        second, time, gravity);
    const double dx = b[0] - a[0];
    const double dy = b[1] - a[1];
    const double dz = b[2] - a[2];
    return dx * dx + dy * dy + dz * dz - radiusSum * radiusSum;
}

using IndependentPolynomial = std::vector<double>;

void trimIndependentPolynomial(IndependentPolynomial& coefficients)
{
    while (coefficients.size() > 1 && coefficients.back() == 0.0)
        coefficients.pop_back();
}

double evaluateIndependentPolynomial(
    const IndependentPolynomial& coefficients, double x)
{
    double value = 0.0;
    for (auto it = coefficients.rbegin(); it != coefficients.rend(); ++it)
        value = value * x + *it;
    return value;
}

double independentPolynomialScale(
    const IndependentPolynomial& coefficients, double x)
{
    double scale = 0.0;
    double power = 1.0;
    for (double coefficient : coefficients) {
        scale += std::fabs(coefficient) * power;
        power *= std::fabs(x);
    }
    return std::max(1.0, scale);
}

IndependentPolynomial derivativeIndependentPolynomial(
    const IndependentPolynomial& coefficients)
{
    IndependentPolynomial derivative;
    for (std::size_t power = 1; power < coefficients.size(); ++power)
        derivative.push_back(coefficients[power] * power);
    if (derivative.empty()) derivative.push_back(0.0);
    trimIndependentPolynomial(derivative);
    return derivative;
}

void appendIndependentRoot(std::vector<double>& roots, double root,
    double lower, double upper)
{
    if (!std::isfinite(root) || root < lower || root > upper) return;
    const double tolerance = 1e-9 * std::max(1.0, std::fabs(upper - lower));
    for (double existing : roots) {
        if (std::fabs(existing - root) <= tolerance) return;
    }
    roots.push_back(root);
}

std::vector<double> isolateIndependentPolynomialRoots(
    IndependentPolynomial coefficients, double lower, double upper)
{
    trimIndependentPolynomial(coefficients);
    std::vector<double> roots;
    const std::size_t degree = coefficients.size() - 1;
    if (degree == 0 || !(upper >= lower)) return roots;
    if (degree == 1) {
        appendIndependentRoot(roots, -coefficients[0] / coefficients[1],
            lower, upper);
        return roots;
    }
    std::vector<double> critical = isolateIndependentPolynomialRoots(
        derivativeIndependentPolynomial(coefficients), lower, upper);
    std::sort(critical.begin(), critical.end());
    std::vector<double> boundaries{lower};
    boundaries.insert(boundaries.end(), critical.begin(), critical.end());
    boundaries.push_back(upper);
    for (double point : boundaries) {
        const double value = evaluateIndependentPolynomial(
            coefficients, point);
        const double tolerance = 1e-12 *
            independentPolynomialScale(coefficients, point);
        if (std::fabs(value) <= tolerance)
            appendIndependentRoot(roots, point, lower, upper);
    }
    for (std::size_t index = 1; index < boundaries.size(); ++index) {
        double left = boundaries[index - 1];
        double right = boundaries[index];
        double leftValue = evaluateIndependentPolynomial(coefficients, left);
        double rightValue = evaluateIndependentPolynomial(coefficients, right);
        if (!std::isfinite(leftValue) || !std::isfinite(rightValue) ||
            leftValue == 0.0 || rightValue == 0.0 ||
            (leftValue < 0.0) == (rightValue < 0.0))
            continue;
        for (int iteration = 0; iteration < 120; ++iteration) {
            const double middle = 0.5 * (left + right);
            const double middleValue = evaluateIndependentPolynomial(
                coefficients, middle);
            if ((middleValue < 0.0) == (leftValue < 0.0)) {
                left = middle;
                leftValue = middleValue;
            } else {
                right = middle;
                rightValue = middleValue;
            }
        }
        appendIndependentRoot(roots, 0.5 * (left + right), lower, upper);
    }
    std::sort(roots.begin(), roots.end());
    return roots;
}

IndependentPolynomial independentSeparationPolynomial(
    const billiardgl::BallState& first,
    const billiardgl::BallState& second, double gravity, double radiusSum)
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
    IndependentPolynomial coefficients(5, 0.0);
    coefficients[0] = -radiusSum * radiusSum;
    for (int axis = 0; axis < 3; ++axis) {
        coefficients[0] += r0[axis] * r0[axis];
        coefficients[1] += 2.0 * r0[axis] * velocity[axis];
        coefficients[2] += velocity[axis] * velocity[axis] +
            2.0 * r0[axis] * halfAcceleration[axis];
        coefficients[3] += 2.0 * velocity[axis] * halfAcceleration[axis];
        coefficients[4] += halfAcceleration[axis] * halfAcceleration[axis];
    }
    trimIndependentPolynomial(coefficients);
    return coefficients;
}

IndependentSweptSphereRoot independentEarliestSweptSphereRoot(
    const billiardgl::BallState& first,
    const billiardgl::BallState& second, double maximumTime,
    double gravity, double radiusSum)
{
    IndependentSweptSphereRoot result;
    const IndependentPolynomial polynomial = independentSeparationPolynomial(
        first, second, gravity, radiusSum);
    const IndependentPolynomial derivative =
        derivativeIndependentPolynomial(polynomial);
    const std::vector<double> roots = isolateIndependentPolynomialRoots(
        polynomial, 0.0, maximumTime);
    for (double root : roots) {
        const double derivativeValue = evaluateIndependentPolynomial(
            derivative, root);
        const double strictTolerance = 1e-10 *
            independentPolynomialScale(derivative, root);
        if (derivativeValue < -strictTolerance) {
            result.hit = true;
            result.timeSeconds = root;
            result.approachDerivative = derivativeValue;
            return result;
        }
    }
    return result;
}

double independentEarliestDescendingPlaneRoot(
    const billiardgl::BallState& ball, double planeY, double maximumTime,
    double gravity)
{
    IndependentPolynomial polynomial{
        ball.position.y - planeY, ball.velocity.y,
        0.5 * domainAccelerationY(ball, gravity)};
    const std::vector<double> roots = isolateIndependentPolynomialRoots(
        polynomial, 0.0, maximumTime);
    for (double root : roots) {
        if (independentVelocityAt(ball, root, gravity)[1] < 0.0)
            return root;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

double independentEarliestPositivePlaneRoot(double position,
    double velocity, double plane, double maximumTime,
    double acceleration = 0.0)
{
    if (!std::isfinite(position) || !std::isfinite(velocity) ||
        !std::isfinite(plane) || !std::isfinite(acceleration))
        return std::numeric_limits<double>::quiet_NaN();
    const std::vector<double> roots = isolateIndependentPolynomialRoots(
        {position - plane, velocity, 0.5 * acceleration},
        0.0, maximumTime);
    for (double root : roots) {
        if (root > 0.0 && velocity + acceleration * root > 0.0)
            return root;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

void expectIndependentFirstApproachingRoot(
    const IndependentSweptSphereRoot& root,
    const billiardgl::BallState& first,
    const billiardgl::BallState& second, double gravity, double radiusSum,
    double expectedTime, double tolerance, const char* message)
{
    expect(root.hit && root.approachDerivative < 0.0 &&
        std::fabs(root.timeSeconds - expectedTime) <= tolerance,
        message);
}

void advanceIndependentToContact(billiardgl::BallState& ball, double time,
    double gravity)
{
    const std::array<double, 3> position = independentPositionAt(
        ball, time, gravity);
    const std::array<double, 3> velocity = independentVelocityAt(
        ball, time, gravity);
    ball.position = {static_cast<float>(position[0]),
        static_cast<float>(position[1]), static_cast<float>(position[2])};
    ball.velocity = {static_cast<float>(velocity[0]),
        static_cast<float>(velocity[1]), static_cast<float>(velocity[2])};
}

double independentMagnitude(const billiardgl::Point3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y +
        value.z * value.z);
}

void advanceIndependentDomainTail(billiardgl::BallState& ball, double time,
    double gravity, double supportedDeceleration,
    double torsionalDeceleration)
{
    if (ball.physicalDomain ==
            billiardgl::PhysicalBallDomain::Airborne) {
        const double angularSpeed = independentMagnitude(ball.angularVelocity);
        advanceIndependentToContact(ball, time, gravity);
        ball.speed = static_cast<float>(independentMagnitude(ball.velocity));
        ball.rotationAngle += static_cast<float>(angularSpeed * time *
            180.0 / 3.14159265358979323846);
        return;
    }
    const double speed = std::hypot(ball.velocity.x, ball.velocity.z);
    if (speed > 0.0 && time > 0.0) {
        const double duration = supportedDeceleration > 0.0
            ? std::min(time, speed / supportedDeceleration) : time;
        const double distance = speed * duration -
            0.5 * supportedDeceleration * duration * duration;
        ball.position.x += static_cast<float>(ball.velocity.x / speed *
            distance);
        ball.position.z += static_cast<float>(ball.velocity.z / speed *
            distance);
        const double finalSpeed = std::max(
            0.0, speed - supportedDeceleration * time);
        ball.velocity.x = static_cast<float>(ball.velocity.x / speed *
            finalSpeed);
        ball.velocity.z = static_cast<float>(ball.velocity.z / speed *
            finalSpeed);
        ball.speed = static_cast<float>(finalSpeed);
    }
    ball.velocity.y = 0.0f;
    ball.speed = static_cast<float>(std::hypot(
        ball.velocity.x, ball.velocity.z));
    if (time > 0.0 && ball.angularVelocity.y != 0.0f) {
        const double initial = ball.angularVelocity.y;
        const double direction = initial < 0.0 ? -1.0 : 1.0;
        const double magnitude = std::fabs(initial);
        const double duration = torsionalDeceleration > 0.0
            ? std::min(time, magnitude / torsionalDeceleration) : time;
        const double integrated = direction *
            (magnitude * duration - 0.5 * torsionalDeceleration *
                duration * duration);
        const double finalMagnitude = torsionalDeceleration > 0.0
            ? std::max(0.0, magnitude - torsionalDeceleration * time)
            : magnitude;
        ball.angularVelocity.y = static_cast<float>(direction *
            finalMagnitude);
        ball.rotationAngle += static_cast<float>(integrated * 180.0 /
            3.14159265358979323846);
    }
}

bool finiteIndependentBall(const billiardgl::BallState& ball)
{
    const float values[] = {ball.position.x, ball.position.y, ball.position.z,
        ball.velocity.x, ball.velocity.y, ball.velocity.z,
        ball.angularVelocity.x, ball.angularVelocity.y,
        ball.angularVelocity.z, ball.rotationAxis.x, ball.rotationAxis.y,
        ball.rotationAxis.z, ball.rotationAngle, ball.speed};
    for (float value : values) {
        if (!std::isfinite(value)) return false;
    }
    return true;
}

bool projectIndependentAnalyticContact(billiardgl::BallState& first,
    const billiardgl::BallState& second, double radiusSum,
    std::array<double, 3>& savedNormal, double& excess)
{
    const double separation[3] = {second.position.x - first.position.x,
        second.position.y - first.position.y,
        second.position.z - first.position.z};
    const double distance = std::sqrt(separation[0] * separation[0] +
        separation[1] * separation[1] + separation[2] * separation[2]);
    if (!std::isfinite(distance) || !(distance > 0.0) ||
        distance < radiusSum) return false;
    excess = distance - radiusSum;
    if (!(excess > 0.0 && excess <= 1e-4)) return false;
    for (int axis = 0; axis < 3; ++axis)
        savedNormal[axis] = separation[axis] / distance;
    first.position.x += static_cast<float>(savedNormal[0] * excess);
    first.position.y += static_cast<float>(savedNormal[1] * excess);
    first.position.z += static_cast<float>(savedNormal[2] * excess);
    return true;
}

struct IndependentCanonicalContact {
    bool ok = false;
    int axis = -1;
    int iterations = 0;
    std::array<double, 3> normal{};
    double excess = 0.0;
};

IndependentCanonicalContact canonicalizeIndependentContact(
    billiardgl::BallState& first, const billiardgl::BallState& second,
    double radiusSum)
{
    IndependentCanonicalContact result;
    const double separation[3] = {second.position.x - first.position.x,
        second.position.y - first.position.y,
        second.position.z - first.position.z};
    const double distance = std::sqrt(separation[0] * separation[0] +
        separation[1] * separation[1] + separation[2] * separation[2]);
    if (!std::isfinite(distance) || !(distance > 0.0) ||
        std::fabs(distance - radiusSum) > 1e-4) return result;
    result.excess = distance - radiusSum;
    for (int axis = 0; axis < 3; ++axis)
        result.normal[axis] = separation[axis] / distance;
    first.position.x += static_cast<float>(
        result.normal[0] * result.excess);
    first.position.y += static_cast<float>(
        result.normal[1] * result.excess);
    first.position.z += static_cast<float>(
        result.normal[2] * result.excess);
    result.axis = std::fabs(separation[0]) >= std::fabs(separation[1]) &&
            std::fabs(separation[0]) >= std::fabs(separation[2]) ? 0 :
        (std::fabs(separation[1]) >= std::fabs(separation[2]) ? 1 : 2);
    float* coordinate = result.axis == 0 ? &first.position.x :
        (result.axis == 1 ? &first.position.y : &first.position.z);
    const float target = result.axis == 0 ? second.position.x :
        (result.axis == 1 ? second.position.y : second.position.z);
    for (; result.iterations < 4; ++result.iterations) {
        const double dx = second.position.x - first.position.x;
        const double dy = second.position.y - first.position.y;
        const double dz = second.position.z - first.position.z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) <= radiusSum) break;
        *coordinate = std::nextafter(*coordinate, target);
    }
    result.ok = true;
    return result;
}

void expectCompleteIndependentRootIsolation()
{
    billiardgl::BallState narrowFirst;
    narrowFirst.position = {0.0f, 0.0f, 0.0f};
    narrowFirst.velocity = {100.0f, 0.0f, 0.0f};
    billiardgl::BallState narrowSecond;
    narrowSecond.position = {5.00002f, 0.0f, 0.0f};
    const IndependentPolynomial narrowPolynomial =
        independentSeparationPolynomial(narrowFirst, narrowSecond, 0.0,
            0.00002);
    const std::vector<double> narrowRoots =
        isolateIndependentPolynomialRoots(narrowPolynomial, 0.0, 0.1);
    expect(narrowRoots.size() == 2 &&
        close(narrowRoots[0], 0.05, 2e-8) &&
        close(narrowRoots[1], 0.0500004, 2e-8) &&
        narrowRoots[1] - narrowRoots[0] < 0.1 / 200000.0,
        "independent quartic oracle must isolate two roots narrower than the former scan step");

    billiardgl::BallState tangentFirst;
    tangentFirst.position = {0.0f, 0.0f, 0.0f};
    tangentFirst.velocity = {100.0f, 0.0f, 0.0f};
    billiardgl::BallState tangentSecond;
    tangentSecond.position = {5.0f, 0.0f, 1.0f};
    const IndependentPolynomial tangentPolynomial =
        independentSeparationPolynomial(tangentFirst, tangentSecond, 0.0,
            1.0);
    const std::vector<double> tangentRoots =
        isolateIndependentPolynomialRoots(tangentPolynomial, 0.0, 0.1);
    const IndependentSweptSphereRoot tangentContact =
        independentEarliestSweptSphereRoot(tangentFirst, tangentSecond, 0.1,
            0.0, 1.0);
    expect(tangentRoots.size() == 1 &&
        close(tangentRoots.front(), 0.05, 1e-10) &&
        close(evaluateIndependentPolynomial(
            derivativeIndependentPolynomial(tangentPolynomial),
            tangentRoots.front()), 0.0, 1e-10) &&
        !tangentContact.hit,
        "independent quartic oracle must enumerate stationary tangency but reject it as non-approaching");
}

bool sameFloatBits(float first, float second)
{
    std::uint32_t left = 0;
    std::uint32_t right = 0;
    std::memcpy(&left, &first, sizeof(left));
    std::memcpy(&right, &second, sizeof(right));
    return left == right;
}

bool samePocket(const billiardgl::PocketInteractionState& first,
    const billiardgl::PocketInteractionState& second)
{
    return first.phase == second.phase && first.pocketId == second.pocketId &&
        first.captureSequence == second.captureSequence;
}

bool sameTransactionalBall(const billiardgl::BallState& first,
    const billiardgl::BallState& second)
{
    return sameFloatBits(first.position.x, second.position.x) &&
        sameFloatBits(first.position.y, second.position.y) &&
        sameFloatBits(first.position.z, second.position.z) &&
        sameFloatBits(first.velocity.x, second.velocity.x) &&
        sameFloatBits(first.velocity.y, second.velocity.y) &&
        sameFloatBits(first.velocity.z, second.velocity.z) &&
        sameFloatBits(first.angularVelocity.x, second.angularVelocity.x) &&
        sameFloatBits(first.angularVelocity.y, second.angularVelocity.y) &&
        sameFloatBits(first.angularVelocity.z, second.angularVelocity.z) &&
        sameFloatBits(first.rotationAxis.x, second.rotationAxis.x) &&
        sameFloatBits(first.rotationAxis.y, second.rotationAxis.y) &&
        sameFloatBits(first.rotationAxis.z, second.rotationAxis.z) &&
        sameFloatBits(first.rotationAngle, second.rotationAngle) &&
        sameFloatBits(first.speed, second.speed) &&
        first.motionState == second.motionState &&
        first.pocketed == second.pocketed &&
        first.outOfPlay == second.outOfPlay &&
        first.texture == second.texture &&
        first.physicalDomain == second.physicalDomain &&
        first.trajectoryGeneration == second.trajectoryGeneration &&
        samePocket(first.pocketInteraction, second.pocketInteraction);
}

bool sameTransactionalState(const billiardgl::GameState& first,
    const billiardgl::GameState& second)
{
    if (first.nextPocketCaptureSequence != second.nextPocketCaptureSequence ||
        first.pocketedBallCount != second.pocketedBallCount ||
        first.ballsMoving != second.ballsMoving ||
        first.events.ballCollision != second.events.ballCollision ||
        first.events.railCollision != second.events.railCollision ||
        first.events.ballPocketed != second.events.ballPocketed ||
        first.events.cueBallPocketed != second.events.cueBallPocketed ||
        first.events.eightBallPocketed != second.events.eightBallPocketed ||
        first.events.shotEnded != second.events.shotEnded)
        return false;
    for (int index = 0; index < billiardgl::kBallCount; ++index) {
        if (!sameTransactionalBall(first.balls[index], second.balls[index]))
            return false;
    }
    return true;
}

bool sameLegacyContact(const billiardgl::PhysicsContactRecord& first,
    const billiardgl::PhysicsContactRecord& second)
{
    const auto samePoint = [](const billiardgl::Point3& a,
                              const billiardgl::Point3& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    };
    return first.solverEventId == second.solverEventId &&
        first.solverIslandId == second.solverIslandId &&
        first.kind == second.kind && first.firstBall == second.firstBall &&
        first.secondBall == second.secondBall &&
        first.boundaryFeatureId == second.boundaryFeatureId &&
        first.normal.x == second.normal.x && first.normal.y == second.normal.y &&
        first.normal.z == second.normal.z &&
        first.normalImpulseNs == second.normalImpulseNs &&
        first.penetrationCm == second.penetrationCm &&
        first.tangentialImpulseNs == second.tangentialImpulseNs &&
        first.solverResidualCmS == second.solverResidualCmS &&
        first.solverProjectionCm == second.solverProjectionCm &&
        first.timeOfImpactSeconds == second.timeOfImpactSeconds &&
        first.wholeTickTimeOfImpactSeconds ==
            second.wholeTickTimeOfImpactSeconds &&
        first.firstPositionAtContactCm.x == second.firstPositionAtContactCm.x &&
        first.firstPositionAtContactCm.y == second.firstPositionAtContactCm.y &&
        first.firstPositionAtContactCm.z == second.firstPositionAtContactCm.z &&
        first.secondPositionAtContactCm.x == second.secondPositionAtContactCm.x &&
        first.secondPositionAtContactCm.y == second.secondPositionAtContactCm.y &&
        first.secondPositionAtContactCm.z == second.secondPositionAtContactCm.z &&
        first.velocityImpulseApplied == second.velocityImpulseApplied &&
        first.regime == second.regime &&
        samePoint(first.contactTangent, second.contactTangent) &&
        samePoint(first.firstContactArmCm, second.firstContactArmCm) &&
        samePoint(first.secondContactArmCm, second.secondContactArmCm) &&
        samePoint(first.relativeContactVelocityBeforeCmS,
            second.relativeContactVelocityBeforeCmS) &&
        samePoint(first.relativeContactVelocityAfterCmS,
            second.relativeContactVelocityAfterCmS) &&
        samePoint(first.impulseOnSecondNs, second.impulseOnSecondNs) &&
        samePoint(first.firstPositionCorrectionCm,
            second.firstPositionCorrectionCm) &&
        samePoint(first.secondPositionCorrectionCm,
            second.secondPositionCorrectionCm) &&
        first.normalRelativeSpeedBeforeCmS ==
            second.normalRelativeSpeedBeforeCmS &&
        first.normalRelativeSpeedAfterCmS ==
            second.normalRelativeSpeedAfterCmS &&
        first.frictionCoefficient == second.frictionCoefficient &&
        first.restitution == second.restitution &&
        first.positionSlopCm == second.positionSlopCm &&
        first.cushionRegime == second.cushionRegime &&
        samePoint(first.cushionContactArmCm, second.cushionContactArmCm) &&
        first.cushionContactHeightCm == second.cushionContactHeightCm &&
        samePoint(first.cushionContactVelocityBeforeCmS,
            second.cushionContactVelocityBeforeCmS) &&
        samePoint(first.cushionContactVelocityAfterCmS,
            second.cushionContactVelocityAfterCmS) &&
        samePoint(first.impulseOnBallNs, second.impulseOnBallNs) &&
        samePoint(first.positionCorrectionCm, second.positionCorrectionCm) &&
        first.noseHeightRatio == second.noseHeightRatio &&
        first.incidentSpeedCmS == second.incidentSpeedCmS &&
        first.maximumRigidIncidentSpeedCmS ==
            second.maximumRigidIncidentSpeedCmS &&
        first.rigidDomainExceeded == second.rigidDomainExceeded &&
        first.positionCorrected == second.positionCorrected &&
        first.pocketId == second.pocketId &&
        first.pocketKind == second.pocketKind &&
        first.pocketBoundaryEvent == second.pocketBoundaryEvent &&
        first.pocketPhaseBefore == second.pocketPhaseBefore &&
        first.pocketPhaseAfter == second.pocketPhaseAfter &&
        first.pocketLocal.offsetCm == second.pocketLocal.offsetCm &&
        first.pocketLocal.depthCm == second.pocketLocal.depthCm &&
        samePoint(first.pocketJawCenterCm, second.pocketJawCenterCm) &&
        first.pocketJawRadiusCm == second.pocketJawRadiusCm &&
        first.pocketThroatSignedDistanceCm ==
            second.pocketThroatSignedDistanceCm &&
        first.pocketCaptureSignedDistanceCm ==
            second.pocketCaptureSignedDistanceCm &&
        first.pocketPassable == second.pocketPassable &&
        first.pocketCaptureSequence == second.pocketCaptureSequence &&
        first.kineticEnergyBeforeJ == second.kineticEnergyBeforeJ &&
        first.kineticEnergyAfterJ == second.kineticEnergyAfterJ;
}

bool sameLegacySolverEvent(const billiardgl::SolverEventRecord& first,
    const billiardgl::SolverEventRecord& second)
{
    return first.eventId == second.eventId &&
        first.islandId == second.islandId &&
        first.candidateCount == second.candidateCount &&
        first.contactCount == second.contactCount &&
        first.duplicateCandidatesRemoved == second.duplicateCandidatesRemoved &&
        first.velocityIterations == second.velocityIterations &&
        first.positionIterations == second.positionIterations &&
        first.maximumResidualCmS == second.maximumResidualCmS &&
        first.maximumPenetrationCm == second.maximumPenetrationCm &&
        first.kineticEnergyBeforeJ == second.kineticEnergyBeforeJ &&
        first.kineticEnergyAfterJ == second.kineticEnergyAfterJ &&
        first.islandLimitExceeded == second.islandLimitExceeded &&
        std::string(first.failureCode) == second.failureCode;
}

double floatUlp(float value)
{
    return std::fabs(static_cast<double>(std::nextafter(value,
        std::numeric_limits<float>::infinity())) - value);
}

bool x05SemanticBall(const billiardgl::BallState& first,
    const billiardgl::BallState& second,
    const billiardgl::PhysicsProfile& profile, double wholeTick)
{
    const double positionTolerance = profile.solver.penetrationSlopCm;
    const double velocityTolerance = profile.solver.residualToleranceCmS;
    const double angularTolerance = velocityTolerance /
        profile.ball.radiusCm;
    const double angleTolerance = angularTolerance * wholeTick * 180.0 /
        3.14159265358979323846 + 2.0 * std::max(
            floatUlp(first.rotationAngle), floatUlp(second.rotationAngle));
    const auto pointWithin = [](const billiardgl::Point3& a,
                                 const billiardgl::Point3& b, double limit) {
        return std::isfinite(a.x) && std::isfinite(a.y) &&
            std::isfinite(a.z) && std::isfinite(b.x) &&
            std::isfinite(b.y) && std::isfinite(b.z) &&
            std::fabs(a.x - b.x) <= limit &&
            std::fabs(a.y - b.y) <= limit &&
            std::fabs(a.z - b.z) <= limit;
    };
    const double firstAxisLength = independentMagnitude(first.rotationAxis);
    const double secondAxisLength = independentMagnitude(second.rotationAxis);
    const bool zeroAxes = firstAxisLength == 0.0 && secondAxisLength == 0.0;
    const bool axesEquivalent = zeroAxes ?
        sameFloatBits(first.rotationAxis.x, second.rotationAxis.x) &&
            sameFloatBits(first.rotationAxis.y, second.rotationAxis.y) &&
            sameFloatBits(first.rotationAxis.z, second.rotationAxis.z) :
        pointWithin(first.rotationAxis, second.rotationAxis,
            angularTolerance) &&
            std::fabs(firstAxisLength - 1.0) <= angularTolerance &&
            std::fabs(secondAxisLength - 1.0) <= angularTolerance;
    return pointWithin(first.position, second.position, positionTolerance) &&
        pointWithin(first.velocity, second.velocity, velocityTolerance) &&
        pointWithin(first.angularVelocity, second.angularVelocity,
            angularTolerance) && axesEquivalent &&
        std::isfinite(first.rotationAngle) &&
        std::isfinite(second.rotationAngle) &&
        std::fabs(first.rotationAngle - second.rotationAngle) <=
            angleTolerance && std::isfinite(first.speed) &&
        std::isfinite(second.speed) &&
        std::fabs(first.speed - second.speed) <= velocityTolerance &&
        first.motionState == second.motionState &&
        first.pocketed == second.pocketed &&
        first.physicalDomain == second.physicalDomain &&
        samePocket(first.pocketInteraction, second.pocketInteraction);
}

bool x05SemanticState(const billiardgl::GameState& first,
    const billiardgl::GameState& second,
    const billiardgl::PhysicsProfile& profile, double wholeTick)
{
    if (first.nextPocketCaptureSequence != second.nextPocketCaptureSequence ||
        first.pocketedBallCount != second.pocketedBallCount ||
        first.ballsMoving != second.ballsMoving)
        return false;
    for (int index = 0; index < billiardgl::kBallCount; ++index) {
        if (!x05SemanticBall(first.balls[index], second.balls[index],
                profile, wholeTick)) return false;
    }
    return true;
}

bool sameAttestation(const billiardgl::PhysicsPlaneObservationAttestation& first,
    const billiardgl::PhysicsPlaneObservationAttestation& second)
{
    return first.observationId == second.observationId &&
        first.ballIndex == second.ballIndex &&
        first.wholeTickDurationSeconds == second.wholeTickDurationSeconds &&
        first.coveredSegmentCount == second.coveredSegmentCount &&
        first.crossingRecordCount == second.crossingRecordCount &&
        first.complete == second.complete;
}

std::string withoutWhitespace(const std::string& text)
{
    std::string result;
    result.reserve(text.size());
    for (unsigned char character : text) {
        if (!std::isspace(character)) result.push_back(character);
    }
    return result;
}

bool recursivelyPropagatesTailFailureVerbatim(const std::string& source)
{
    const std::string compact = withoutWhitespace(source);
    const std::string marker = "if(!tail.ok){";
    const std::size_t markerAt = compact.find(marker);
    if (markerAt == std::string::npos ||
        compact.find(marker, markerAt + marker.size()) != std::string::npos)
        return false;
    const std::size_t blockAt = markerAt + marker.size() - 1;
    int depth = 0;
    std::size_t blockEnd = std::string::npos;
    for (std::size_t index = blockAt; index < compact.size(); ++index) {
        if (compact[index] == '{') ++depth;
        if (compact[index] == '}' && --depth == 0) {
            blockEnd = index;
            break;
        }
    }
    if (blockEnd == std::string::npos) return false;
    const std::string body = compact.substr(
        blockAt + 1, blockEnd - blockAt - 1);
    return body == "state=snapshot;returntail;" &&
        compact.find("tail.failureCode") == std::string::npos &&
        compact.find("child.failureCode") == std::string::npos;
}

bool sameCrossing(const billiardgl::PhysicsPlaneCrossingRecord& first,
    const billiardgl::PhysicsPlaneCrossingRecord& second)
{
    return first.observationId == second.observationId &&
        first.ballIndex == second.ballIndex && first.axis == second.axis &&
        first.planeCoordinateCm == second.planeCoordinateCm &&
        first.direction == second.direction &&
        first.wholeTickTimeOfImpactSeconds ==
            second.wholeTickTimeOfImpactSeconds &&
        first.segmentOrdinal == second.segmentOrdinal &&
        first.signedDistanceBeforeCm == second.signedDistanceBeforeCm &&
        first.signedDistanceAfterCm == second.signedDistanceAfterCm &&
        first.axisVelocityAtCrossingCmS ==
            second.axisVelocityAtCrossingCmS &&
        first.complete == second.complete;
}

const billiardgl::TableDepartureEvent* crossDomainContact(
    const billiardgl::TableDepartureStepResult& result, int first, int second)
{
    const std::string token = "cross_domain_ball_contact:" +
        std::to_string(first) + ':' + std::to_string(second);
    for (const billiardgl::TableDepartureEvent& event : result.events) {
        if (event.stableIdentity.find(token) != std::string::npos)
            return &event;
    }
    return nullptr;
}

template <typename T, typename = void>
struct HasCanonicalTelemetryPublished : std::false_type {};
template <typename T>
struct HasCanonicalTelemetryPublished<T, std::void_t<decltype(
    std::declval<T>().canonicalTelemetryPublished)>> : std::true_type {};
template <typename T, typename = void>
struct HasShotFactsPublished : std::false_type {};
template <typename T>
struct HasShotFactsPublished<T, std::void_t<decltype(
    std::declval<T>().shotFactsPublished)>> : std::true_type {};
template <typename T, typename = void>
struct HasRulesOutcomePublished : std::false_type {};
template <typename T>
struct HasRulesOutcomePublished<T, std::void_t<decltype(
    std::declval<T>().rulesOutcomePublished)>> : std::true_type {};
template <typename T, typename = void>
struct HasProductUiPublished : std::false_type {};
template <typename T>
struct HasProductUiPublished<T, std::void_t<decltype(
    std::declval<T>().productUiPublished)>> : std::true_type {};
template <typename T, typename = void>
struct HasAmbiguousCandidateTimes : std::false_type {};
template <typename T>
struct HasAmbiguousCandidateTimes<T, std::void_t<decltype(
    std::declval<T>().ambiguousCrossDomainContactTimeSeconds), decltype(
    std::declval<T>().ambiguousTableContactTimeSeconds)>> : std::true_type {};
template <typename T, typename = void>
struct HasPrivateAnalyticSegments : std::false_type {};
template <typename T>
struct HasPrivateAnalyticSegments<T, std::void_t<decltype(
    std::declval<T>().analyticSegments)>> : std::true_type {};

template <typename T>
const std::vector<billiardgl::SurfaceMotionAnalyticSegment>&
privateAnalyticSegments(const T& result)
{
    static const std::vector<billiardgl::SurfaceMotionAnalyticSegment> empty;
    if constexpr (HasPrivateAnalyticSegments<T>::value)
        return result.analyticSegments;
    return empty;
}

bool samePrivateSegment(const billiardgl::SurfaceMotionAnalyticSegment& a,
    const billiardgl::SurfaceMotionAnalyticSegment& b)
{
    return a.ballIndex == b.ballIndex &&
        a.segmentOrdinal == b.segmentOrdinal &&
        a.motionState == b.motionState &&
        a.wholeTickStartOffsetSeconds == b.wholeTickStartOffsetSeconds &&
        a.durationSeconds == b.durationSeconds &&
        a.startPositionCm.x == b.startPositionCm.x &&
        a.startPositionCm.y == b.startPositionCm.y &&
        a.startPositionCm.z == b.startPositionCm.z &&
        a.endPositionCm.x == b.endPositionCm.x &&
        a.endPositionCm.y == b.endPositionCm.y &&
        a.endPositionCm.z == b.endPositionCm.z &&
        a.startVelocityCmS.x == b.startVelocityCmS.x &&
        a.startVelocityCmS.y == b.startVelocityCmS.y &&
        a.startVelocityCmS.z == b.startVelocityCmS.z &&
        a.constantAccelerationCmS2.x == b.constantAccelerationCmS2.x &&
        a.constantAccelerationCmS2.y == b.constantAccelerationCmS2.y &&
        a.constantAccelerationCmS2.z == b.constantAccelerationCmS2.z &&
        a.analyticPathComplete == b.analyticPathComplete;
}

std::uint64_t privateAnalyticDigest(
    const std::vector<billiardgl::SurfaceMotionAnalyticSegment>& segments)
{
    std::uint64_t hash = 1469598103934665603ULL;
    const auto appendBytes = [&](const void* address, std::size_t size) {
        const auto* bytes = static_cast<const unsigned char*>(address);
        for (std::size_t index = 0; index < size; ++index) {
            hash ^= bytes[index];
            hash *= 1099511628211ULL;
        }
    };
    const std::uint64_t count = segments.size();
    appendBytes(&count, sizeof(count));
    for (const auto& segment : segments) {
        const int motionState = static_cast<int>(segment.motionState);
        appendBytes(&segment.ballIndex, sizeof(segment.ballIndex));
        appendBytes(&segment.segmentOrdinal, sizeof(segment.segmentOrdinal));
        appendBytes(&motionState, sizeof(motionState));
        appendBytes(&segment.wholeTickStartOffsetSeconds,
            sizeof(segment.wholeTickStartOffsetSeconds));
        appendBytes(&segment.durationSeconds, sizeof(segment.durationSeconds));
        for (const billiardgl::Point3* point : {&segment.startPositionCm,
                &segment.endPositionCm, &segment.startVelocityCmS,
                &segment.constantAccelerationCmS2}) {
            appendBytes(&point->x, sizeof(point->x));
            appendBytes(&point->y, sizeof(point->y));
            appendBytes(&point->z, sizeof(point->z));
        }
        appendBytes(&segment.analyticPathComplete,
            sizeof(segment.analyticPathComplete));
    }
    return hash;
}

bool privateBoundary(const billiardgl::TableDepartureStepResult& result,
    const billiardgl::TableDepartureEvent& event, bool requireFollowing,
    double tolerance)
{
    bool preceding = false;
    bool following = !requireFollowing;
    for (const auto& segment : privateAnalyticSegments(result)) {
        if (segment.ballIndex != event.ballIndex) continue;
        const double start = segment.wholeTickStartOffsetSeconds;
        const double end = start + segment.durationSeconds;
        preceding = preceding || std::fabs(end -
            event.absoluteWholeTickTimeSeconds) <= tolerance;
        following = following || std::fabs(start -
            event.absoluteWholeTickTimeSeconds) <= tolerance;
        if (start < event.absoluteWholeTickTimeSeconds - tolerance &&
            end > event.absoluteWholeTickTimeSeconds + tolerance)
            return false;
    }
    return preceding && following;
}

bool validPrivateAnalyticLineage(
    const std::vector<billiardgl::SurfaceMotionAnalyticSegment>& segments,
    const std::vector<int>& activeBalls, double duration,
    double positionTolerance, double timeTolerance)
{
    if (!std::isfinite(duration) || duration < 0.0) return false;
    if (duration == 0.0) return segments.empty();
    if (segments.empty()) return false;
    std::tuple<double, int, int> previousOrder{-1.0, -1, -1};
    bool havePreviousOrder = false;
    for (const auto& segment : segments) {
        const auto order = std::make_tuple(
            segment.wholeTickStartOffsetSeconds, segment.ballIndex,
            segment.segmentOrdinal);
        if (havePreviousOrder && !(previousOrder < order)) return false;
        previousOrder = order;
        havePreviousOrder = true;
        const bool ballistic = segment.constantAccelerationCmS2.x == 0.0f &&
            segment.constantAccelerationCmS2.y != 0.0f &&
            segment.constantAccelerationCmS2.z == 0.0f;
        const bool stationary = segment.startVelocityCmS.x == 0.0f &&
            segment.startVelocityCmS.y == 0.0f &&
            segment.startVelocityCmS.z == 0.0f &&
            segment.constantAccelerationCmS2.x == 0.0f &&
            segment.constantAccelerationCmS2.y == 0.0f &&
            segment.constantAccelerationCmS2.z == 0.0f;
        const billiardgl::BallMotionState expectedMotion = ballistic ?
            billiardgl::BallMotionState::Sliding : stationary ?
            billiardgl::BallMotionState::Stationary :
            billiardgl::BallMotionState::Rolling;
        if (segment.motionState != expectedMotion) return false;
    }
    std::vector<int> balls = activeBalls;
    std::sort(balls.begin(), balls.end());
    if (std::adjacent_find(balls.begin(), balls.end()) != balls.end())
        return false;
    for (int ballIndex : balls) {
        double covered = 0.0;
        int ordinal = 0;
        billiardgl::Point3 previousEnd;
        bool havePrevious = false;
        int count = 0;
        for (const auto& segment : segments) {
            if (segment.ballIndex != ballIndex) continue;
            const auto finitePoint = [](const billiardgl::Point3& point) {
                return std::isfinite(point.x) && std::isfinite(point.y) &&
                    std::isfinite(point.z);
            };
            if (segment.segmentOrdinal != ordinal++ ||
                !segment.analyticPathComplete ||
                !(segment.durationSeconds > 0.0) ||
                !std::isfinite(segment.wholeTickStartOffsetSeconds) ||
                !std::isfinite(segment.durationSeconds) ||
                !finitePoint(segment.startPositionCm) ||
                !finitePoint(segment.endPositionCm) ||
                !finitePoint(segment.startVelocityCmS) ||
                !finitePoint(segment.constantAccelerationCmS2) ||
                std::fabs(segment.wholeTickStartOffsetSeconds - covered) >
                    timeTolerance) return false;
            const double t = segment.durationSeconds;
            const billiardgl::Point3 integrated{
                static_cast<float>(segment.startPositionCm.x +
                    segment.startVelocityCmS.x * t + 0.5 *
                    segment.constantAccelerationCmS2.x * t * t),
                static_cast<float>(segment.startPositionCm.y +
                    segment.startVelocityCmS.y * t + 0.5 *
                    segment.constantAccelerationCmS2.y * t * t),
                static_cast<float>(segment.startPositionCm.z +
                    segment.startVelocityCmS.z * t + 0.5 *
                    segment.constantAccelerationCmS2.z * t * t)};
            if (std::fabs(integrated.x - segment.endPositionCm.x) >
                    positionTolerance ||
                std::fabs(integrated.y - segment.endPositionCm.y) >
                    positionTolerance ||
                std::fabs(integrated.z - segment.endPositionCm.z) >
                    positionTolerance ||
                (havePrevious &&
                    (std::fabs(previousEnd.x - segment.startPositionCm.x) >
                         positionTolerance ||
                     std::fabs(previousEnd.y - segment.startPositionCm.y) >
                         positionTolerance ||
                     std::fabs(previousEnd.z - segment.startPositionCm.z) >
                         positionTolerance))) return false;
            previousEnd = segment.endPositionCm;
            havePrevious = true;
            covered = segment.wholeTickStartOffsetSeconds + t;
            ++count;
        }
        if (count == 0 || std::fabs(covered - duration) > timeTolerance)
            return false;
    }
    for (const auto& segment : segments) {
        if (!std::binary_search(balls.begin(), balls.end(), segment.ballIndex))
            return false;
    }
    return true;
}

bool hasPrivateBoundaryForBall(
    const std::vector<billiardgl::SurfaceMotionAnalyticSegment>& segments,
    int ballIndex, double time, double tolerance)
{
    bool ending = false;
    bool starting = false;
    for (const auto& segment : segments) {
        if (segment.ballIndex != ballIndex) continue;
        ending = ending || std::fabs(segment.wholeTickStartOffsetSeconds +
            segment.durationSeconds - time) <= tolerance;
        starting = starting || std::fabs(
            segment.wholeTickStartOffsetSeconds - time) <= tolerance;
    }
    return ending && starting;
}

bool anchoredPrivateBallLineage(
    const billiardgl::TableDepartureStepResult& result, int ballIndex,
    const billiardgl::BallState& before, const billiardgl::BallState& after,
    double duration, const billiardgl::PhysicsProfile& profile)
{
    const auto& segments = privateAnalyticSegments(result);
    const auto pointClose = [&](const billiardgl::Point3& first,
                                  const billiardgl::Point3& second) {
        return close(first.x, second.x, profile.solver.penetrationSlopCm) &&
            close(first.y, second.y, profile.solver.penetrationSlopCm) &&
            close(first.z, second.z, profile.solver.penetrationSlopCm);
    };
    const auto velocityClose = [&](const billiardgl::Point3& first,
                                     const billiardgl::Point3& second) {
        return close(first.x, second.x,
                   profile.solver.residualToleranceCmS) &&
            close(first.y, second.y,
                profile.solver.residualToleranceCmS) &&
            close(first.z, second.z,
                profile.solver.residualToleranceCmS);
    };
    const billiardgl::SurfaceMotionAnalyticSegment* first = nullptr;
    const billiardgl::SurfaceMotionAnalyticSegment* last = nullptr;
    for (const auto& segment : segments) {
        if (segment.ballIndex != ballIndex) continue;
        if (!first) first = &segment;
        last = &segment;
        const bool ballistic = segment.motionState ==
            billiardgl::BallMotionState::Sliding;
        const bool rolling = segment.motionState ==
            billiardgl::BallMotionState::Rolling;
        if (ballistic && (segment.constantAccelerationCmS2.x != 0.0f ||
                segment.constantAccelerationCmS2.y !=
                    profile.tableDeparture.gravityYCmS2 ||
                segment.constantAccelerationCmS2.z != 0.0f)) return false;
        if (rolling) {
            const double speed = std::hypot(segment.startVelocityCmS.x,
                segment.startVelocityCmS.z);
            const float expectedX = static_cast<float>(
                -profile.tableDeparture.supportedRollingDecelerationCmS2 *
                segment.startVelocityCmS.x / speed);
            const float expectedZ = static_cast<float>(
                -profile.tableDeparture.supportedRollingDecelerationCmS2 *
                segment.startVelocityCmS.z / speed);
            if (!(speed > 0.0) ||
                segment.constantAccelerationCmS2.x != expectedX ||
                segment.constantAccelerationCmS2.z != expectedZ ||
                segment.startVelocityCmS.x *
                        segment.constantAccelerationCmS2.x +
                    segment.startVelocityCmS.z *
                        segment.constantAccelerationCmS2.z >= 0.0)
                return false;
        }
    }
    if (duration == 0.0) return first == nullptr;
    if (!first || !last || !pointClose(first->startPositionCm,
            before.position) || !pointClose(last->endPositionCm,
            after.position)) return false;
    billiardgl::Point3 canonicalBeforeVelocity = before.velocity;
    if (before.physicalDomain != billiardgl::PhysicalBallDomain::Airborne &&
        before.physicalDomain !=
            billiardgl::PhysicalBallDomain::PocketChannel &&
        canonicalBeforeVelocity.y < 0.0f)
        canonicalBeforeVelocity.y = 0.0f;
    if (!velocityClose(first->startVelocityCmS, canonicalBeforeVelocity))
        return false;
    const billiardgl::TableDepartureEvent* terminalEvent = nullptr;
    bool terminalPrimary = false;
    double requiredDuration = duration;
    int pocketEntries = 0;
    for (const auto& event : result.events) {
        bool participates = event.ballIndex == ballIndex;
        if (event.kind == billiardgl::TableDepartureEventKind::
                CrossDomainBallContact && !participates) {
            const std::string prefix = "cross_domain_ball_contact:" +
                std::to_string(event.ballIndex) + ":" +
                std::to_string(ballIndex) + ":";
            participates = event.stableIdentity.rfind(prefix, 0) == 0;
        }
        if (participates && close(
                event.absoluteWholeTickTimeSeconds, duration,
                profile.solver.toiToleranceSeconds)) {
            if (terminalEvent) return false;
            terminalEvent = &event;
            terminalPrimary = event.ballIndex == ballIndex;
        }
        if (event.ballIndex == ballIndex && event.kind ==
                billiardgl::TableDepartureEventKind::PocketEntry) {
            requiredDuration = event.absoluteWholeTickTimeSeconds;
            ++pocketEntries;
        }
    }
    if (after.physicalDomain == billiardgl::PhysicalBallDomain::PocketChannel &&
        pocketEntries != 1) return false;
    if (!close(last->wholeTickStartOffsetSeconds + last->durationSeconds,
            requiredDuration, profile.solver.toiToleranceSeconds))
        return false;
    if (terminalEvent) {
        const billiardgl::Point3& witnessedVelocity = terminalPrimary ?
            terminalEvent->velocityAfterCmS :
            terminalEvent->secondVelocityAfterCmS;
        const billiardgl::PhysicalBallDomain witnessedDomain =
            terminalPrimary ? terminalEvent->targetDomain :
            terminalEvent->secondTargetDomain;
        if (!terminalEvent->transitionWitnessComplete ||
            witnessedDomain != after.physicalDomain ||
            !velocityClose(witnessedVelocity, after.velocity)) return false;
    } else if (after.physicalDomain !=
            billiardgl::PhysicalBallDomain::PocketChannel) {
        const auto finalVelocity = [&](float start, float acceleration) {
            return start + static_cast<float>(acceleration *
                last->durationSeconds);
        };
        if (!close(finalVelocity(last->startVelocityCmS.x,
                    last->constantAccelerationCmS2.x), after.velocity.x,
                profile.solver.residualToleranceCmS) ||
            !close(finalVelocity(last->startVelocityCmS.y,
                    last->constantAccelerationCmS2.y), after.velocity.y,
                profile.solver.residualToleranceCmS) ||
            !close(finalVelocity(last->startVelocityCmS.z,
                    last->constantAccelerationCmS2.z), after.velocity.z,
                profile.solver.residualToleranceCmS)) return false;
    }
    return true;
}

template <typename T>
double ambiguousCrossTime(const T& result)
{
    if constexpr (HasAmbiguousCandidateTimes<T>::value)
        return result.ambiguousCrossDomainContactTimeSeconds;
    return std::numeric_limits<double>::quiet_NaN();
}

template <typename T>
double ambiguousTableTime(const T& result)
{
    if constexpr (HasAmbiguousCandidateTimes<T>::value)
        return result.ambiguousTableContactTimeSeconds;
    return std::numeric_limits<double>::quiet_NaN();
}

std::string rebaseStableIdentity(const billiardgl::TableDepartureEvent& event,
    int newOrdinal, double newAbsoluteTime)
{
    std::string identity = event.stableIdentity;
    const std::string oldOrdinal = ":" + std::to_string(event.ordinal) + ":";
    const std::size_t ordinalAt = identity.find(oldOrdinal);
    if (ordinalAt != std::string::npos) {
        identity.replace(ordinalAt, oldOrdinal.size(),
            ":" + std::to_string(newOrdinal) + ":");
    }
    const std::size_t lastColon = identity.rfind(':');
    if (lastColon != std::string::npos) {
        std::ostringstream value;
        value << newAbsoluteTime;
        identity.replace(lastColon + 1, std::string::npos, value.str());
    }
    return identity;
}

bool sameEventSemantic(const billiardgl::TableDepartureEvent& first,
    const billiardgl::TableDepartureEvent& second, double tolerance)
{
    return first.kind == second.kind && first.scope == second.scope &&
        first.ballIndex == second.ballIndex &&
        first.ordinal == second.ordinal &&
        std::fabs(first.absoluteWholeTickTimeSeconds -
            second.absoluteWholeTickTimeSeconds) <= tolerance &&
        first.stableIdentity == second.stableIdentity;
}

billiardgl::BallState ballAt(float x, float y, float z,
    float vx, float vy, float vz, billiardgl::PhysicalBallDomain domain)
{
    billiardgl::BallState ball;
    ball.position = {x, y, z};
    ball.velocity = {vx, vy, vz};
    ball.speed = std::sqrt(vx * vx + vy * vy + vz * vz);
    ball.physicalDomain = domain;
    return ball;
}

billiardgl::GameState isolatedState(const billiardgl::BallState& ball)
{
    billiardgl::GameState state;
    billiardgl::initializeBalls(state);
    for (int index = 0; index < billiardgl::kBallCount; ++index) {
        state.balls[index].pocketed = true;
        state.balls[index].physicalDomain =
            billiardgl::PhysicalBallDomain::PocketChannel;
    }
    state.balls[0] = ball;
    state.balls[0].pocketed = false;
    state.ballsMoving = ball.speed > 0.0f;
    return state;
}

void expectPrivateCanonicalOrder(
    const billiardgl::TableDepartureStepResult& result)
{
    double previousTime = -1.0;
    int previousOrdinal = -1;
    for (const billiardgl::TableDepartureEvent& event : result.events) {
        expect(event.scope ==
                billiardgl::TableDepartureEventScope::PhysicsPrivateDiagnostic,
            "D6-A events must remain physics-private diagnostics");
        expect(event.absoluteWholeTickTimeSeconds >= 0.0 &&
                event.absoluteWholeTickTimeSeconds >= previousTime &&
                event.ordinal == previousOrdinal + 1,
            "D6-A recursive events must retain absolute whole-tick order");
        previousTime = event.absoluteWholeTickTimeSeconds;
        previousOrdinal = event.ordinal;
    }
}

void expectStaticConsumerBoundary()
{
    using Result = billiardgl::TableDepartureStepResult;
    static_assert(!HasCanonicalTelemetryPublished<Result>::value,
        "D6-A result must not expose canonical telemetry publication");
    static_assert(!HasShotFactsPublished<Result>::value,
        "D6-A result must not expose ShotFacts publication");
    static_assert(!HasRulesOutcomePublished<Result>::value,
        "D6-A result must not expose rules publication");
    static_assert(!HasProductUiPublished<Result>::value,
        "D6-A result must not expose product UI publication");
#ifndef BILLIARDGL_SOURCE_ROOT
#define BILLIARDGL_SOURCE_ROOT "."
#endif
    const std::string paths[] = {
        std::string(BILLIARDGL_SOURCE_ROOT) +
            "/src/Billiards/table_departure_geometry.h",
        std::string(BILLIARDGL_SOURCE_ROOT) +
            "/src/Billiards/table_departure_geometry.cpp",
    };
    const char* forbidden[] = {
        "physics_telemetry", "shot_fact", "rules.h", "rules/",
        "game_runtime", "modern_product", "product_ui", "renderer",
    };
    for (const std::string& path : paths) {
        std::ifstream input(path);
        expect(input.good(), "D6-A static audit source must be readable");
        std::ostringstream stream;
        stream << input.rdbuf();
        const std::string text = stream.str();
        for (const char* token : forbidden) {
            expect(text.find(token) == std::string::npos,
                "D6-A source must not name a telemetry/rules/UI consumer");
        }
    }
}

void expectRejectedWithoutMutation(billiardgl::BallState ball,
    const billiardgl::PhysicsProfile& profile, const char* message)
{
    billiardgl::GameState state = isolatedState(ball);
    state.balls[0].rotationAxis = {0.2f, 0.3f, 0.4f};
    state.balls[0].rotationAngle = 17.0f;
    state.balls[0].pocketInteraction.phase =
        billiardgl::PocketInteractionPhase::Approaching;
    state.balls[0].pocketInteraction.pocketId = 4;
    state.balls[0].pocketInteraction.captureSequence = 7;
    state.nextPocketCaptureSequence = 41;
    const billiardgl::GameState before = state;
    const billiardgl::TableDepartureStepResult result =
        billiardgl::advanceTableDeparturePhysics(state, 0.1f, profile,
            billiardgl::PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(!result.ok && result.events.empty() &&
        sameTransactionalState(state, before), message);
}

}  // namespace

int main()
{
    using namespace billiardgl;
    PhysicsProfile profile = defaultChinesePoolPhysicsProfile();
    const TableDepartureGeometry geometry =
        buildTableDepartureGeometry(profile);
    expect(validateTableDepartureGeometry(geometry).ok,
        "sealed D6-A geometry must validate");
    expect(close(geometry.clothSurfaceYCm, 86.483976) &&
        close(geometry.clothSupportedCenterYCm, 89.341476) &&
        close(geometry.railTopSurfaceYCm, 92.104021) &&
        close(geometry.railTopSupportedCenterYCm, 94.961521) &&
        close(geometry.outerHalfWidthCm, 83.4336812635) &&
        close(geometry.outerHalfLengthCm, 146.0679224206) &&
        close(geometry.outsideLandingSurfaceYCm, 0.0) &&
        close(geometry.gravityYCmS2, -980.665),
        "D6-A profile must expose the frozen analytic dimensions");
    expectStaticConsumerBoundary();
    expectCompleteIndependentRootIsolation();

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const BallState validAirborne = ballAt(0.0f, 100.0f, 0.0f,
        10.0f, 20.0f, -5.0f, PhysicalBallDomain::Airborne);
    BallState invalidBall = validAirborne;
    invalidBall.position.x = nan;
    expectRejectedWithoutMutation(invalidBall, profile,
        "nonfinite position.x must fail closed");
    invalidBall = validAirborne;
    invalidBall.position.y = infinity;
    expectRejectedWithoutMutation(invalidBall, profile,
        "nonfinite position.y must fail closed");
    invalidBall = validAirborne;
    invalidBall.position.z = -infinity;
    expectRejectedWithoutMutation(invalidBall, profile,
        "nonfinite position.z must fail closed");
    invalidBall = validAirborne;
    invalidBall.velocity.x = nan;
    expectRejectedWithoutMutation(invalidBall, profile,
        "nonfinite velocity.x must fail closed");
    invalidBall = validAirborne;
    invalidBall.velocity.y = infinity;
    expectRejectedWithoutMutation(invalidBall, profile,
        "nonfinite velocity.y must fail closed");
    invalidBall = validAirborne;
    invalidBall.velocity.z = -infinity;
    expectRejectedWithoutMutation(invalidBall, profile,
        "nonfinite velocity.z must fail closed");

    PhysicsProfile invalidProfile = profile;
    invalidProfile.tableDeparture.gravityYCmS2 = nan;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "NaN acceleration must fail closed");
    invalidProfile = profile;
    invalidProfile.tableDeparture.gravityYCmS2 = -infinity;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "infinite acceleration must fail closed");
    invalidProfile = profile;
    invalidProfile.tableDeparture.gravityYCmS2 = 0.0f;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "zero gravity must fail closed");
    invalidProfile = profile;
    invalidProfile.tableDeparture.gravityYCmS2 = 1.0f;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "positive gravity must fail closed");

    invalidProfile = profile;
    invalidProfile.tableDeparture.clothSurfaceYCm = nan;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "nonfinite cloth height must fail closed");
    invalidProfile = profile;
    invalidProfile.tableDeparture.railTopSurfaceYCm = infinity;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "nonfinite rail-top height must fail closed");
    invalidProfile = profile;
    invalidProfile.tableDeparture.outsideLandingSurfaceYCm = -infinity;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "nonfinite outside-floor height must fail closed");
    invalidProfile = profile;
    invalidProfile.tableDeparture.outerRailHalfWidthCm = nan;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "nonfinite outer width must fail closed");
    invalidProfile = profile;
    invalidProfile.tableDeparture.outerRailHalfLengthCm = infinity;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "nonfinite outer length must fail closed");
    invalidProfile = profile;
    invalidProfile.tableBoundary.playfieldWidthCm = nan;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "nonfinite playfield width must fail closed");
    invalidProfile = profile;
    invalidProfile.tableBoundary.playfieldLengthCm = infinity;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "nonfinite playfield length must fail closed");
    invalidProfile = profile;
    invalidProfile.ball.radiusCm = nan;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "nonfinite ball radius must fail closed");
    invalidProfile = profile;
    invalidProfile.ball.radiusCm = 0.0f;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "zero ball radius must fail closed");
    invalidProfile = profile;
    invalidProfile.ball.radiusCm = -1.0f;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "negative ball radius must fail closed");
    invalidProfile = profile;
    invalidProfile.cushion.noseHeightRatio = nan;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "nonfinite cushion nose ratio must fail closed");

    invalidProfile = profile;
    invalidProfile.tableDeparture.railTopSurfaceYCm =
        invalidProfile.tableDeparture.clothSurfaceYCm;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "rail top not above cloth must fail closed");
    invalidProfile = profile;
    invalidProfile.tableDeparture.outsideLandingSurfaceYCm =
        invalidProfile.tableDeparture.clothSurfaceYCm;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "outside floor not below cloth must fail closed");
    invalidProfile = profile;
    invalidProfile.tableDeparture.outerRailHalfWidthCm =
        invalidProfile.tableBoundary.playfieldWidthCm * 0.5f;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "outer width not beyond playfield must fail closed");
    invalidProfile = profile;
    invalidProfile.tableDeparture.outerRailHalfLengthCm =
        invalidProfile.tableBoundary.playfieldLengthCm * 0.5f;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "outer length not beyond playfield must fail closed");
    invalidProfile = profile;
    invalidProfile.tableDeparture.outerRailHalfWidthCm = 0.0f;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "zero outer width must fail closed");
    invalidProfile = profile;
    invalidProfile.tableDeparture.outerRailHalfLengthCm = -1.0f;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "negative outer length must fail closed");
    invalidProfile = profile;
    invalidProfile.cushion.noseHeightRatio =
        (invalidProfile.tableDeparture.railTopSurfaceYCm -
            invalidProfile.tableDeparture.clothSurfaceYCm) /
        invalidProfile.ball.radiusCm;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "nose center at rail top must fail closed");
    invalidProfile = profile;
    invalidProfile.cushion.noseHeightRatio = -0.1f;
    expectRejectedWithoutMutation(validAirborne, invalidProfile,
        "nose center below cloth solid must fail closed");

    GameState initialized;
    initializeBalls(initialized);
    for (const BallState& ball : initialized.balls) {
        expect(ball.physicalDomain == PhysicalBallDomain::ClothSupported &&
                close(ball.position.y, geometry.clothSupportedCenterYCm),
            "rack/reset must initialize every active ball on cloth domain");
    }
    initialized.balls[0].physicalDomain = PhysicalBallDomain::Airborne;
    initialized.balls[0].position.y += 20.0f;
    initializeBalls(initialized);
    expect(initialized.balls[0].physicalDomain ==
            PhysicalBallDomain::ClothSupported &&
            close(initialized.balls[0].position.y,
                geometry.clothSupportedCenterYCm),
        "repeat rack/reset must remove stale airborne state");

    BallState control = ballAt(0.0f,
        geometry.clothSupportedCenterYCm, 20.0f, 10.0f, 0.0f, 0.0f,
        PhysicalBallDomain::ClothSupported);
    const BallState controlBefore = control;
    const TableDepartureStepResult controlResult =
        advanceTableDepartureMotion(control, 0.1f, profile);
    expect(controlResult.ok && controlResult.events.empty() &&
        control.physicalDomain == PhysicalBallDomain::ClothSupported &&
        close(control.position.y, controlBefore.position.y),
        "cloth control must retain its support height and produce no D6 event");
    expectPrivateCanonicalOrder(controlResult);

    BallState ballistic = ballAt(0.0f, 100.0f, 0.0f,
        10.0f, 20.0f, -5.0f, PhysicalBallDomain::Airborne);
    const TableDepartureStepResult ballisticResult =
        advanceTableDepartureMotion(ballistic, 0.1f, profile);
    expect(ballisticResult.ok && ballisticResult.events.empty() &&
        close(ballistic.position.x, 1.0) &&
        close(ballistic.position.y, 100.0 + 2.0 - 4.903325, 1e-3) &&
        close(ballistic.position.z, -0.5) &&
        close(ballistic.velocity.y, 20.0 - 98.0665, 1e-3),
        "airborne motion must use exact constant gravity");
    expectPrivateCanonicalOrder(ballisticResult);

    const double r01MaximumTime = 0.2;
    BallState clothLanding = ballAt(20.0f,
        geometry.clothSupportedCenterYCm + 3.0f, 10.0f,
        12.0f, -20.0f, 4.0f, PhysicalBallDomain::Airborne);
    const BallState clothLandingInitial = clothLanding;
    const double r01Root = independentEarliestDescendingPlaneRoot(
        clothLanding, geometry.clothSupportedCenterYCm, r01MaximumTime,
        geometry.gravityYCmS2);
    expect(std::isfinite(r01Root) && r01Root > 0.0 &&
        r01Root < r01MaximumTime,
        "R01 independent quadratic oracle must prove a descending cloth-plane root");
    const std::array<double, 3> r01Contact = independentPositionAt(
        clothLanding, r01Root, geometry.gravityYCmS2);
    expect(std::fabs(r01Contact[0]) <=
            geometry.playfieldHalfWidthCm + geometry.ballRadiusCm &&
        std::fabs(r01Contact[2]) <=
            geometry.playfieldHalfLengthCm + geometry.ballRadiusCm &&
        std::fabs(r01Contact[0]) <
            geometry.playfieldHalfWidthCm - geometry.ballRadiusCm,
        "R01 independent contact must be inside cloth envelope and away from every mouth");
    BallState r01Expected = clothLanding;
    advanceIndependentToContact(r01Expected, r01Root,
        geometry.gravityYCmS2);
    r01Expected.physicalDomain = PhysicalBallDomain::ClothSupported;
    r01Expected.position.y = geometry.clothSupportedCenterYCm;
    r01Expected.velocity.y = 0.0f;
    advanceIndependentDomainTail(r01Expected, r01MaximumTime - r01Root,
        geometry.gravityYCmS2,
        profile.tableDeparture.supportedRollingDecelerationCmS2,
        profile.surface.torsionalSpinDecelerationRadS2);
    const TableDepartureStepResult clothLandingResult =
        advanceTableDepartureMotion(clothLanding,
            static_cast<float>(r01MaximumTime), profile);
    const TableDepartureEvent* r01Event = nullptr;
    for (const TableDepartureEvent& event : clothLandingResult.events) {
        if (event.kind == TableDepartureEventKind::ClothReturn)
            r01Event = &event;
    }
    expect(clothLandingResult.ok && r01Event != nullptr &&
        close(r01Event->absoluteWholeTickTimeSeconds, r01Root,
            profile.solver.toiToleranceSeconds) &&
        clothLanding.physicalDomain == PhysicalBallDomain::ClothSupported &&
        close(clothLanding.position.y, geometry.clothSupportedCenterYCm,
            profile.solver.penetrationSlopCm) &&
        clothLanding.velocity.y == 0.0f &&
        close(clothLanding.position.x, r01Expected.position.x,
            profile.solver.penetrationSlopCm) &&
        close(clothLanding.position.z, r01Expected.position.z,
            profile.solver.penetrationSlopCm),
        "R01 descending airborne ball must return to cloth at exact TOI and advance its supported tail");
    expectPrivateCanonicalOrder(clothLandingResult);

    BallState clothLandingSplit = clothLandingInitial;
    const TableDepartureStepResult r02Prefix = advanceTableDepartureMotion(
        clothLandingSplit, static_cast<float>(r01Root), profile);
    const TableDepartureStepResult r02Tail = advanceTableDepartureMotion(
        clothLandingSplit,
        static_cast<float>(r01MaximumTime - r01Root), profile);
    expect(r02Prefix.ok && r02Tail.ok &&
        countTableDepartureEvents(r02Prefix,
            TableDepartureEventKind::ClothReturn) == 1 &&
        x05SemanticBall(clothLanding, clothLandingSplit, profile,
            r01MaximumTime),
        "R02 cloth-return whole and split paths must remain semantically equivalent");

    BallState bSequentialEvent = ballAt(20.0f,
        geometry.clothSupportedCenterYCm + 3.0f, 10.0f,
        0.0f, -20.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState bSequentialNoEvent = ballAt(-20.0f,
        geometry.clothSupportedCenterYCm, 30.0f,
        12.0f, 0.0f, 3.0f, PhysicalBallDomain::ClothSupported);
    BallState bSequentialExpectedEvent = bSequentialEvent;
    const double bSequentialRoot = independentEarliestDescendingPlaneRoot(
        bSequentialExpectedEvent, geometry.clothSupportedCenterYCm,
        r01MaximumTime, geometry.gravityYCmS2);
    expect(std::isfinite(bSequentialRoot) && bSequentialRoot > 0.0 &&
        bSequentialRoot < r01MaximumTime,
        "B1 independent oracle must prove the event-producing ball root");
    advanceIndependentToContact(bSequentialExpectedEvent, bSequentialRoot,
        geometry.gravityYCmS2);
    bSequentialExpectedEvent.physicalDomain =
        PhysicalBallDomain::ClothSupported;
    bSequentialExpectedEvent.position.y = geometry.clothSupportedCenterYCm;
    bSequentialExpectedEvent.velocity.y = 0.0f;
    advanceIndependentDomainTail(bSequentialExpectedEvent,
        r01MaximumTime - bSequentialRoot, geometry.gravityYCmS2,
        profile.tableDeparture.supportedRollingDecelerationCmS2,
        profile.surface.torsionalSpinDecelerationRadS2);
    BallState bSequentialExpectedNoEvent = bSequentialNoEvent;
    advanceIndependentDomainTail(bSequentialExpectedNoEvent,
        r01MaximumTime, geometry.gravityYCmS2,
        profile.tableDeparture.supportedRollingDecelerationCmS2,
        profile.surface.torsionalSpinDecelerationRadS2);

    GameState bPublicZero = isolatedState(bSequentialEvent);
    bPublicZero.balls[1] = bSequentialNoEvent;
    bPublicZero.balls[1].pocketed = false;
    const GameState bPublicZeroBefore = bPublicZero;
    const TableDepartureStepResult bPublicZeroResult =
        advanceTableDeparturePhysics(bPublicZero,
            static_cast<float>(r01MaximumTime), profile,
            PhysicsBoundaryMode::ProductionTable, 0);
    expect(!bPublicZeroResult.ok && bPublicZeroResult.failureCode ==
            TableDepartureFailureCode::InvalidState &&
        bPublicZeroResult.events.empty() &&
        sameTransactionalState(bPublicZero, bPublicZeroBefore),
        "B0 public zero event budget must remain InvalidState with whole rollback");
    GameState bPublicNegative = bPublicZeroBefore;
    const GameState bPublicNegativeBefore = bPublicNegative;
    const TableDepartureStepResult bPublicNegativeResult =
        advanceTableDeparturePhysics(bPublicNegative,
            static_cast<float>(r01MaximumTime), profile,
            PhysicsBoundaryMode::ProductionTable, -1);
    expect(!bPublicNegativeResult.ok && bPublicNegativeResult.failureCode ==
            TableDepartureFailureCode::InvalidState &&
        bPublicNegativeResult.events.empty() &&
        sameTransactionalState(bPublicNegative, bPublicNegativeBefore),
        "B0 public negative event budget must remain InvalidState with whole rollback");

    GameState bSequentialForward = bPublicZeroBefore;
    const TableDepartureStepResult bSequentialForwardResult =
        advanceTableDeparturePhysics(bSequentialForward,
            static_cast<float>(r01MaximumTime), profile,
            PhysicsBoundaryMode::ProductionTable, 1);
    expect(bSequentialForwardResult.ok &&
        bSequentialForwardResult.events.size() == 1 &&
        bSequentialForwardResult.events[0].kind ==
            TableDepartureEventKind::ClothReturn &&
        bSequentialForwardResult.events[0].ballIndex == 0 &&
        x05SemanticBall(bSequentialForward.balls[0],
            bSequentialExpectedEvent, profile, r01MaximumTime) &&
        x05SemanticBall(bSequentialForward.balls[1],
            bSequentialExpectedNoEvent, profile, r01MaximumTime),
        "B1 a later no-candidate ball must advance after an earlier ball exhausts the event budget");

    GameState bSequentialReverse = isolatedState(bSequentialNoEvent);
    bSequentialReverse.balls[1] = bSequentialEvent;
    bSequentialReverse.balls[1].pocketed = false;
    const TableDepartureStepResult bSequentialReverseResult =
        advanceTableDeparturePhysics(bSequentialReverse,
            static_cast<float>(r01MaximumTime), profile,
            PhysicsBoundaryMode::ProductionTable, 1);
    expect(bSequentialReverseResult.ok &&
        bSequentialReverseResult.events.size() == 1 &&
        bSequentialReverseResult.events[0].kind ==
            TableDepartureEventKind::ClothReturn &&
        bSequentialReverseResult.events[0].ballIndex == 1 &&
        x05SemanticBall(bSequentialReverse.balls[0],
            bSequentialExpectedNoEvent, profile, r01MaximumTime) &&
        x05SemanticBall(bSequentialReverse.balls[1],
            bSequentialExpectedEvent, profile, r01MaximumTime),
        "B1 reversed ball order must preserve the same independently derived states");

    BallState bSecondCandidate = bSequentialEvent;
    bSecondCandidate.position.x = -20.0f;
    bSecondCandidate.position.y += 2.0f;
    bSecondCandidate.position.z = 30.0f;
    const double bSecondCandidateRoot = independentEarliestDescendingPlaneRoot(
        bSecondCandidate, geometry.clothSupportedCenterYCm,
        r01MaximumTime, geometry.gravityYCmS2);
    expect(std::isfinite(bSecondCandidateRoot) &&
        std::fabs(bSecondCandidateRoot - bSequentialRoot) >
            profile.solver.toiToleranceSeconds,
        "B2 later candidate must be a real non-same-TOI budget case");
    GameState bSecondCandidateState = isolatedState(bSequentialEvent);
    bSecondCandidateState.balls[1] = bSecondCandidate;
    bSecondCandidateState.balls[1].pocketed = false;
    const GameState bSecondCandidateBefore = bSecondCandidateState;
    const TableDepartureStepResult bSecondCandidateResult =
        advanceTableDeparturePhysics(bSecondCandidateState,
            static_cast<float>(r01MaximumTime), profile,
            PhysicsBoundaryMode::ProductionTable, 1);
    expect(!bSecondCandidateResult.ok &&
        bSecondCandidateResult.failureCode ==
            TableDepartureFailureCode::EventBudget &&
        bSecondCandidateResult.events.empty() &&
        sameTransactionalState(bSecondCandidateState,
            bSecondCandidateBefore),
        "B2 a later real candidate after budget exhaustion must fail exact EventBudget with whole rollback");

    BallState r04Edge = ballAt(
        geometry.playfieldHalfWidthCm - geometry.ballRadiusCm - 0.1f,
        geometry.clothSupportedCenterYCm + 2.0f, 30.0f,
        0.0f, -10.0f, 0.0f, PhysicalBallDomain::Airborne);
    const TableDepartureStepResult r04EdgeResult =
        advanceTableDepartureMotion(r04Edge, 0.2f, profile);
    expect(r04EdgeResult.ok && countTableDepartureEvents(r04EdgeResult,
            TableDepartureEventKind::ClothReturn) == 1 &&
        countTableDepartureEvents(r04EdgeResult,
            TableDepartureEventKind::PocketEntry) == 0,
        "R04 cloth-envelope edge away from a mouth must return to cloth");

    BallState r05Upward = ballAt(0.0f,
        geometry.clothSupportedCenterYCm + 1.0f, 20.0f,
        0.0f, 100.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState r05NoCross = ballAt(0.0f,
        geometry.clothSupportedCenterYCm + 20.0f, 20.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState r05Tangent = ballAt(0.0f,
        geometry.clothSupportedCenterYCm, 20.0f,
        10.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    const IndependentPolynomial r05TangentPlanePolynomial{
        r05Tangent.position.y - geometry.clothSupportedCenterYCm,
        r05Tangent.velocity.y,
        0.5 * geometry.gravityYCmS2};
    const std::vector<double> r05TangentRoots =
        isolateIndependentPolynomialRoots(r05TangentPlanePolynomial,
            0.0, 0.01);
    expect(!r05TangentRoots.empty() && r05TangentRoots.front() == 0.0 &&
        evaluateIndependentPolynomial(
            derivativeIndependentPolynomial(r05TangentPlanePolynomial),
            0.0) == 0.0,
        "R05 independent plane polynomial must prove a t0 stationary root");
    BallState r05Positive = r05Tangent;
    r05Positive.velocity.y = 0.0001f;
    const GameState r05TangentBefore = isolatedState(r05Tangent);
    GameState r05TangentState = r05TangentBefore;
    const TableDepartureStepResult r05UpwardResult =
        advanceTableDepartureMotion(r05Upward, 0.01f, profile);
    const TableDepartureStepResult r05NoCrossResult =
        advanceTableDepartureMotion(r05NoCross, 0.01f, profile);
    const TableDepartureStepResult r05TangentResult =
        advanceTableDeparturePhysics(r05TangentState, 0.01f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    const TableDepartureStepResult r05PositiveResult =
        advanceTableDepartureMotion(r05Positive, 0.00000001f, profile);
    expect(r05UpwardResult.ok && r05NoCrossResult.ok &&
        countTableDepartureEvents(r05UpwardResult,
            TableDepartureEventKind::ClothReturn) == 0 &&
        countTableDepartureEvents(r05NoCrossResult,
            TableDepartureEventKind::ClothReturn) == 0 &&
        r05TangentResult.ok && r05TangentResult.failureCode ==
            TableDepartureFailureCode::None &&
        r05TangentResult.events.empty() &&
        r05TangentState.balls[0].physicalDomain ==
            PhysicalBallDomain::Airborne &&
        r05PositiveResult.ok && r05PositiveResult.events.empty() &&
        r05Positive.physicalDomain == PhysicalBallDomain::Airborne,
        "R05 upward/no-cross/t0-stationary paths must not synthesize cloth return");

    BallState r06Below = ballAt(0.0f,
        geometry.clothSupportedCenterYCm - 0.01f, 20.0f,
        0.0f, -1.0f, 0.0f, PhysicalBallDomain::Airborne);
    GameState r06State = isolatedState(r06Below);
    const GameState r06Before = r06State;
    const TableDepartureStepResult r06Result = advanceTableDeparturePhysics(
        r06State, 0.01f, profile, PhysicsBoundaryMode::ProductionTable,
        profile.solver.maximumEventsPerTick);
    expect(!r06Result.ok && r06Result.failureCode ==
            TableDepartureFailureCode::AmbiguousInitialPenetration &&
        r06Result.events.empty() &&
        sameTransactionalState(r06State, r06Before),
        "R06 initial-below-cloth airborne state must rollback exactly");
    BallState r06MouthBelow = ballAt(
        -profile.tableBoundary.playfieldWidthCm * 0.5f,
        geometry.clothSupportedCenterYCm - 0.01f,
        -profile.tableBoundary.playfieldLengthCm * 0.5f,
        0.0f, -1.0f, 0.0f, PhysicalBallDomain::Airborne);
    GameState r06MouthState = isolatedState(r06MouthBelow);
    const GameState r06MouthBefore = r06MouthState;
    const TableDepartureStepResult r06MouthResult =
        advanceTableDeparturePhysics(r06MouthState, 0.01f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(!r06MouthResult.ok && r06MouthResult.failureCode ==
            TableDepartureFailureCode::AmbiguousInitialPenetration &&
        r06MouthResult.events.empty() &&
        sameTransactionalState(r06MouthState, r06MouthBefore),
        "R06 initial-below-mouth state must not bypass exact rollback");

    PhysicsProfile r07Profile = profile;
    const double r07TargetTime = 0.05;
    const float r07InitialHeightOffset = static_cast<float>(
        -0.5 * geometry.gravityYCmS2 * r07TargetTime * r07TargetTime);
    BallState r07Air = ballAt(0.0f,
        geometry.clothSupportedCenterYCm + r07InitialHeightOffset,
        0.0f, 100.0f, 0.0f, 0.0f,
        PhysicalBallDomain::Airborne);
    BallState r07Cloth = ballAt(static_cast<float>(
            100.0 * r07TargetTime + 2.0 * profile.ball.radiusCm),
        geometry.clothSupportedCenterYCm, 0.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    GameState r07State = isolatedState(r07Air);
    r07State.balls[1] = r07Cloth;
    r07State.balls[1].pocketed = false;
    const GameState r07Before = r07State;
    const IndependentSweptSphereRoot r07BallRoot =
        independentEarliestSweptSphereRoot(r07Air, r07Cloth, 0.1,
            geometry.gravityYCmS2, 2.0 * profile.ball.radiusCm);
    const double r07ClothRoot = independentEarliestDescendingPlaneRoot(
        r07Air, geometry.clothSupportedCenterYCm, 0.1,
        geometry.gravityYCmS2);
    expect(r07BallRoot.hit && std::isfinite(r07ClothRoot) &&
        close(r07BallRoot.timeSeconds, r07ClothRoot,
            r07Profile.solver.toiToleranceSeconds),
        "R07 independent quartic and quadratic roots must prove same-TOI topology");
    const TableDepartureStepResult r07Result = advanceTableDeparturePhysics(
        r07State, 0.1f, r07Profile,
        PhysicsBoundaryMode::ProductionTable,
        r07Profile.solver.maximumEventsPerTick);
    expect(!r07Result.ok && r07Result.failureCode ==
            TableDepartureFailureCode::AmbiguousCrossDomainTableContact &&
        r07Result.events.empty() &&
        close(r07Result.ambiguousCrossDomainContactTimeSeconds,
            r07BallRoot.timeSeconds,
            r07Profile.solver.toiToleranceSeconds) &&
        close(r07Result.ambiguousTableContactTimeSeconds,
            r07ClothRoot, r07Profile.solver.toiToleranceSeconds) &&
        sameTransactionalState(r07State, r07Before),
        "R07 equal-TOI cloth return and cross-domain contact must fail as one ambiguous topology");

    const double r08ContactTime = 0.04;
    const double r08ContactDy = -3.0;
    const double r08ContactDz = 2.0;
    const double r08RadiusSum = 2.0 * profile.ball.radiusCm;
    const double r08ContactDx = std::sqrt(r08RadiusSum * r08RadiusSum -
        r08ContactDy * r08ContactDy - r08ContactDz * r08ContactDz);
    BallState r08Air = ballAt(20.0f, static_cast<float>(
            geometry.clothSupportedCenterYCm - r08ContactDy -
            0.5 * geometry.gravityYCmS2 * r08ContactTime * r08ContactTime),
        20.0f, 100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState r08Cloth = ballAt(static_cast<float>(20.0 +
            100.0 * r08ContactTime + r08ContactDx),
        geometry.clothSupportedCenterYCm,
        static_cast<float>(20.0 + r08ContactDz),
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    const IndependentSweptSphereRoot r08BallRoot =
        independentEarliestSweptSphereRoot(r08Air, r08Cloth, 0.2,
            geometry.gravityYCmS2, r08RadiusSum);
    const double r08PreContactFreeFlightClothRoot =
        independentEarliestDescendingPlaneRoot(
        r08Air, geometry.clothSupportedCenterYCm, 0.2,
        geometry.gravityYCmS2);
    expect(r08BallRoot.hit &&
        std::isfinite(r08PreContactFreeFlightClothRoot) &&
        r08BallRoot.timeSeconds + profile.solver.toiToleranceSeconds <
            r08PreContactFreeFlightClothRoot,
        "R08 independent roots must prove cross-domain contact before cloth return");

    GameState airbornePair = isolatedState(ballAt(-10.0f, 120.0f, 0.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne));
    airbornePair.balls[1] = ballAt(0.0f, 120.0f, 0.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    airbornePair.balls[1].pocketed = false;
    const GameState airbornePairBefore = airbornePair;
    const IndependentSweptSphereRoot airbornePairRoot =
        independentEarliestSweptSphereRoot(airbornePair.balls[0],
            airbornePair.balls[1], 0.1, geometry.gravityYCmS2,
            2.0 * profile.ball.radiusCm);
    const TableDepartureEarliestBatch airbornePairQuery =
        queryEarliestTableDepartureBatch(airbornePair, 0.1, profile,
            PhysicsBoundaryMode::Unbounded);
    expect(airbornePairRoot.hit && airbornePairQuery.ok &&
        airbornePairQuery.candidates.size() == 1 &&
        airbornePairQuery.candidates[0].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        airbornePairQuery.candidates[0].firstBall == 0 &&
        airbornePairQuery.candidates[0].secondBall == 1 &&
        close(airbornePairQuery.earliestTimeSeconds,
            airbornePairRoot.timeSeconds,
            profile.solver.toiToleranceSeconds) &&
        sameTransactionalState(airbornePair, airbornePairBefore),
        "D6 piecewise query must own and detect Airborne-to-Airborne contact");

    const auto initialOverlapQuery = [&](double overlapCm,
                                         float firstVelocityX) {
        BallState firstBall = ballAt(0.0f, 50.0f, 30.0f,
            firstVelocityX, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
        BallState secondBall = ballAt(static_cast<float>(
                r08RadiusSum - overlapCm), 50.0f, 30.0f,
            0.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
        GameState firstPrediction = isolatedState(firstBall);
        GameState secondPrediction = isolatedState(secondBall);
        const TableDepartureStepResult firstMotion =
            advanceTableDeparturePhysics(firstPrediction, 0.01f, profile,
                PhysicsBoundaryMode::Unbounded,
                profile.solver.maximumEventsPerTick);
        const TableDepartureStepResult secondMotion =
            advanceTableDeparturePhysics(secondPrediction, 0.01f, profile,
                PhysicsBoundaryMode::Unbounded,
                profile.solver.maximumEventsPerTick);
        std::vector<SurfaceMotionAnalyticSegment> motion =
            firstMotion.analyticSegments;
        for (SurfaceMotionAnalyticSegment segment :
                secondMotion.analyticSegments) {
            segment.ballIndex = 1;
            motion.push_back(segment);
        }
        GameState pair = isolatedState(firstBall);
        pair.balls[1] = secondBall;
        pair.balls[1].pocketed = false;
        std::array<bool, kBallCount> ownership{};
        ownership[0] = true;
        return queryEarliestTableDepartureBatch(pair, 0.01, profile,
            PhysicsBoundaryMode::Unbounded, motion, ownership);
    };
    const TableDepartureEarliestBatch slopApproaching =
        initialOverlapQuery(0.5 * profile.solver.penetrationSlopCm, 10.0f);
    const TableDepartureEarliestBatch slopSeparating =
        initialOverlapQuery(0.5 * profile.solver.penetrationSlopCm, -10.0f);
    const TableDepartureEarliestBatch beyondSlop =
        initialOverlapQuery(1.01 * profile.solver.penetrationSlopCm, 10.0f);
    const double exactMinimumSeparation = r08RadiusSum -
        profile.solver.penetrationSlopCm;
    float storedMinimumSeparation =
        static_cast<float>(exactMinimumSeparation);
    if (static_cast<double>(storedMinimumSeparation) >
            exactMinimumSeparation)
        storedMinimumSeparation = std::nextafter(storedMinimumSeparation,
            -std::numeric_limits<float>::infinity());
    const float oneStorageUlpBelow = std::nextafter(
        storedMinimumSeparation,
        -std::numeric_limits<float>::infinity());
    const double sharedMinimumSquared =
        tableDepartureMinimumSeparationSquared(r08RadiusSum,
            profile.solver.penetrationSlopCm);
    const TableDepartureEarliestBatch storageBoundary =
        initialOverlapQuery(r08RadiusSum - storedMinimumSeparation, -10.0f);
    const TableDepartureEarliestBatch beyondStorageBoundary =
        initialOverlapQuery(r08RadiusSum - oneStorageUlpBelow, -10.0f);
    expect(slopApproaching.ok && slopApproaching.candidates.size() == 1 &&
        slopApproaching.candidates[0].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        close(slopApproaching.earliestTimeSeconds, 0.0,
            profile.solver.toiToleranceSeconds) &&
        slopSeparating.ok && slopSeparating.candidates.empty() &&
        !beyondSlop.ok && beyondSlop.failureCode ==
            TableDepartureFailureCode::AmbiguousInitialPenetration &&
        beyondSlop.failingFirstBall == 0 &&
        beyondSlop.failingSecondBall == 1 &&
        beyondSlop.initialPenetrationCm >
            profile.solver.penetrationSlopCm &&
        static_cast<double>(storedMinimumSeparation) <=
            exactMinimumSeparation &&
        static_cast<double>(std::nextafter(storedMinimumSeparation,
            std::numeric_limits<float>::infinity())) >
            exactMinimumSeparation &&
        sharedMinimumSquared == static_cast<double>(
            storedMinimumSeparation * storedMinimumSeparation) &&
        storageBoundary.ok && storageBoundary.candidates.empty() &&
        !beyondStorageBoundary.ok &&
        beyondStorageBoundary.failureCode ==
            TableDepartureFailureCode::AmbiguousInitialPenetration,
        "D6 injected query must share the sealed solver penetration-slop boundary at t0");

    // Representative dense-rack coordinates expose the float projection
    // residue that the solver sees as within slop while a direct double
    // subtraction reports a small false excess.  Keep the coordinates
    // non-origin-centered so this fixture exercises the actual Point3
    // subtraction rounding rather than an idealized one-axis construction.
    BallState quantizedFirst = ballAt(10.0f, 89.0f, 80.0f,
        10.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState quantizedSecond = ballAt(10.152487754821777f,
        83.32621002197266f, 80.6592788696289f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    const auto directDoubleDistance = [](const BallState& first,
                                          const BallState& second) {
        const double dx = double(second.position.x) - first.position.x;
        const double dy = double(second.position.y) - first.position.y;
        const double dz = double(second.position.z) - first.position.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };
    const auto solverFloatDistance = [](const BallState& first,
                                         const BallState& second) {
        const Point3 separation{
            second.position.x - first.position.x,
            second.position.y - first.position.y,
            second.position.z - first.position.z};
        return std::sqrt(double(separation.x * separation.x +
            separation.y * separation.y + separation.z * separation.z));
    };
    const double directDoublePenetration = r08RadiusSum -
        directDoubleDistance(quantizedFirst, quantizedSecond);
    const double solverFloatPenetration = r08RadiusSum -
        solverFloatDistance(quantizedFirst, quantizedSecond);
    constexpr double observedInteractivePenetrationCm =
        0.0010004288048630627;
    const double representativeFloatQuantizationCm =
        directDoublePenetration - solverFloatPenetration;
    GameState quantizedFirstPrediction = isolatedState(quantizedFirst);
    GameState quantizedSecondPrediction = isolatedState(quantizedSecond);
    const TableDepartureStepResult quantizedFirstMotion =
        advanceTableDeparturePhysics(quantizedFirstPrediction, 0.01f,
            profile, PhysicsBoundaryMode::Unbounded,
            profile.solver.maximumEventsPerTick);
    const TableDepartureStepResult quantizedSecondMotion =
        advanceTableDeparturePhysics(quantizedSecondPrediction, 0.01f,
            profile, PhysicsBoundaryMode::Unbounded,
            profile.solver.maximumEventsPerTick);
    std::vector<SurfaceMotionAnalyticSegment> quantizedMotion =
        quantizedFirstMotion.analyticSegments;
    for (SurfaceMotionAnalyticSegment segment :
            quantizedSecondMotion.analyticSegments) {
        segment.ballIndex = 1;
        quantizedMotion.push_back(segment);
    }
    GameState quantizedPair = isolatedState(quantizedFirst);
    quantizedPair.balls[1] = quantizedSecond;
    quantizedPair.balls[1].pocketed = false;
    std::array<bool, kBallCount> quantizedOwnership{};
    quantizedOwnership[0] = true;
    const TableDepartureEarliestBatch quantizedResidualQuery =
        queryEarliestTableDepartureBatch(quantizedPair, 0.01, profile,
            PhysicsBoundaryMode::Unbounded, quantizedMotion,
            quantizedOwnership);
    expect(directDoublePenetration > profile.solver.penetrationSlopCm &&
        directDoublePenetration - profile.solver.penetrationSlopCm < 5e-7 &&
        solverFloatPenetration <= profile.solver.penetrationSlopCm &&
        observedInteractivePenetrationCm -
            profile.solver.penetrationSlopCm > 0.0 &&
        observedInteractivePenetrationCm -
            profile.solver.penetrationSlopCm <=
                representativeFloatQuantizationCm &&
        quantizedResidualQuery.ok &&
        quantizedResidualQuery.candidates.size() == 1 &&
        quantizedResidualQuery.candidates[0].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        close(quantizedResidualQuery.earliestTimeSeconds, 0.0,
            profile.solver.toiToleranceSeconds),
        "D6 initial overlap must honor the solver's Point3 float projection arithmetic");

    const double simultaneousTime = 0.04;
    BallState simultaneousAir = ballAt(0.0f, static_cast<float>(
            geometry.clothSupportedCenterYCm -
            0.5 * geometry.gravityYCmS2 * simultaneousTime *
                simultaneousTime),
        30.0f, 100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState simultaneousLegacy = ballAt(static_cast<float>(
            100.0 * simultaneousTime + r08RadiusSum -
            0.5 * 100.0 * profile.solver.toiToleranceSeconds),
        geometry.clothSupportedCenterYCm, 30.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    GameState simultaneousAirPrediction = isolatedState(simultaneousAir);
    const TableDepartureStepResult simultaneousAirMotion =
        advanceTableDeparturePhysics(simultaneousAirPrediction, 0.08f,
            profile, PhysicsBoundaryMode::Unbounded,
            profile.solver.maximumEventsPerTick);
    BallState simultaneousLegacyPrediction = simultaneousLegacy;
    const SurfaceMotionStep simultaneousLegacyMotion = advanceSurfaceMotion(
        simultaneousLegacyPrediction, 0.08f, profile.ball,
        profile.surface);
    std::vector<SurfaceMotionAnalyticSegment> simultaneousMotion =
        simultaneousAirMotion.analyticSegments;
    for (SurfaceMotionAnalyticSegment segment :
            simultaneousLegacyMotion.analyticSegments) {
        segment.ballIndex = 1;
        simultaneousMotion.push_back(segment);
    }
    GameState simultaneousState = isolatedState(simultaneousAir);
    simultaneousState.balls[1] = simultaneousLegacy;
    simultaneousState.balls[1].pocketed = false;
    std::array<bool, kBallCount> simultaneousOwnership{};
    simultaneousOwnership[0] = true;
    const TableDepartureEarliestBatch simultaneousSupportQuery =
        queryEarliestTableDepartureBatch(simultaneousState, 0.08, profile,
            PhysicsBoundaryMode::ProductionTable, simultaneousMotion,
            simultaneousOwnership);
    expect(simultaneousSupportQuery.ok &&
        simultaneousSupportQuery.candidates.size() == 2 &&
        close(simultaneousSupportQuery.earliestTimeSeconds,
            simultaneousTime, profile.solver.toiToleranceSeconds) &&
        std::count_if(simultaneousSupportQuery.candidates.begin(),
            simultaneousSupportQuery.candidates.end(), [](const auto& candidate) {
                return candidate.kind == TableDepartureEventKind::ClothReturn;
            }) == 1 &&
        std::count_if(simultaneousSupportQuery.candidates.begin(),
            simultaneousSupportQuery.candidates.end(), [](const auto& candidate) {
                return candidate.kind ==
                    TableDepartureEventKind::CrossDomainBallContact;
            }) == 1,
        "D6 query must return a normal support-landing plus cross-contact same-TOI batch for one shared island");

    BallState simultaneousOuterAir = simultaneousAir;
    simultaneousOuterAir.position.x = static_cast<float>(
        geometry.outerHalfWidthCm - 100.0 * simultaneousTime);
    simultaneousOuterAir.position.y = static_cast<float>(100.0 -
        0.5 * geometry.gravityYCmS2 * simultaneousTime *
            simultaneousTime);
    BallState simultaneousOuterLegacy = simultaneousLegacy;
    simultaneousOuterLegacy.position.x = static_cast<float>(
        geometry.outerHalfWidthCm + r08RadiusSum -
        0.5 * 100.0 * profile.solver.toiToleranceSeconds);
    simultaneousOuterLegacy.position.y = 100.0f;
    GameState simultaneousOuterAirPrediction =
        isolatedState(simultaneousOuterAir);
    const TableDepartureStepResult simultaneousOuterAirMotion =
        advanceTableDeparturePhysics(simultaneousOuterAirPrediction, 0.08f,
            profile, PhysicsBoundaryMode::Unbounded,
            profile.solver.maximumEventsPerTick);
    BallState simultaneousOuterLegacyPrediction = simultaneousOuterLegacy;
    const SurfaceMotionStep simultaneousOuterLegacyMotion =
        advanceSurfaceMotion(simultaneousOuterLegacyPrediction, 0.08f,
            profile.ball, profile.surface);
    std::vector<SurfaceMotionAnalyticSegment> simultaneousOuterMotion =
        simultaneousOuterAirMotion.analyticSegments;
    for (SurfaceMotionAnalyticSegment segment :
            simultaneousOuterLegacyMotion.analyticSegments) {
        segment.ballIndex = 1;
        simultaneousOuterMotion.push_back(segment);
    }
    GameState simultaneousOuterState = isolatedState(simultaneousOuterAir);
    simultaneousOuterState.balls[1] = simultaneousOuterLegacy;
    simultaneousOuterState.balls[1].pocketed = false;
    const TableDepartureEarliestBatch simultaneousOuterQuery =
        queryEarliestTableDepartureBatch(simultaneousOuterState, 0.08,
            profile, PhysicsBoundaryMode::ProductionTable,
            simultaneousOuterMotion, simultaneousOwnership);
    expect(!simultaneousOuterQuery.ok &&
        simultaneousOuterQuery.failureCode ==
            TableDepartureFailureCode::AmbiguousCrossDomainTableContact &&
        close(simultaneousOuterQuery.ambiguousCrossDomainContactTimeSeconds,
            simultaneousTime, profile.solver.toiToleranceSeconds) &&
        close(simultaneousOuterQuery.ambiguousTableContactTimeSeconds,
            simultaneousTime, profile.solver.toiToleranceSeconds),
        "D6 query must keep cross-contact plus true outer departure same-TOI fail-closed");

    const double decelContactTime = 0.04;
    const double decelSupportedSpeed = 20.0;
    const double deceleration =
        profile.tableDeparture.supportedRollingDecelerationCmS2;
    const double supportedDisplacement = decelSupportedSpeed *
        decelContactTime - 0.5 * deceleration * decelContactTime *
        decelContactTime;
    BallState decelAir = ballAt(20.0f, static_cast<float>(
            geometry.clothSupportedCenterYCm - r08ContactDy -
            0.5 * geometry.gravityYCmS2 * decelContactTime *
                decelContactTime),
        30.0f, 100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState decelSupported = ballAt(static_cast<float>(
            20.0 + 100.0 * decelContactTime + r08ContactDx -
            supportedDisplacement),
        geometry.clothSupportedCenterYCm,
        static_cast<float>(30.0 + r08ContactDz),
        static_cast<float>(decelSupportedSpeed), 0.0f, 0.0f,
        PhysicalBallDomain::ClothSupported);
    GameState decelAirPrediction = isolatedState(decelAir);
    GameState decelSupportedPrediction = isolatedState(decelSupported);
    const TableDepartureStepResult decelAirMotion =
        advanceTableDeparturePhysics(decelAirPrediction, 0.08f, profile,
            PhysicsBoundaryMode::Unbounded,
            profile.solver.maximumEventsPerTick);
    const TableDepartureStepResult decelSupportedMotion =
        advanceTableDeparturePhysics(decelSupportedPrediction, 0.08f,
            profile, PhysicsBoundaryMode::Unbounded,
            profile.solver.maximumEventsPerTick);
    std::vector<SurfaceMotionAnalyticSegment> decelAuthoritativeMotion =
        decelAirMotion.analyticSegments;
    for (SurfaceMotionAnalyticSegment segment :
            decelSupportedMotion.analyticSegments) {
        segment.ballIndex = 1;
        decelAuthoritativeMotion.push_back(segment);
    }
    GameState decelPair = isolatedState(decelAir);
    decelPair.balls[1] = decelSupported;
    decelPair.balls[1].pocketed = false;
    const GameState decelPairBefore = decelPair;
    std::array<bool, kBallCount> decelOwnership{};
    decelOwnership[0] = true;
    const TableDepartureEarliestBatch decelQuery =
        queryEarliestTableDepartureBatch(decelPair, 0.08, profile,
            PhysicsBoundaryMode::Unbounded, decelAuthoritativeMotion,
            decelOwnership);
    const IndependentSweptSphereRoot constantVelocityWrongRoot =
        independentEarliestSweptSphereRoot(decelAir, decelSupported, 0.08,
            geometry.gravityYCmS2, r08RadiusSum);
    expect(decelAirMotion.ok && decelSupportedMotion.ok && decelQuery.ok &&
        decelQuery.candidates.size() == 1 &&
        decelQuery.candidates[0].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        close(decelQuery.earliestTimeSeconds, decelContactTime,
            profile.solver.toiToleranceSeconds) &&
        constantVelocityWrongRoot.hit &&
        std::fabs(constantVelocityWrongRoot.timeSeconds -
            decelQuery.earliestTimeSeconds) >
                profile.solver.toiToleranceSeconds &&
        sameTransactionalState(decelPair, decelPairBefore),
        "D6 injected query must use authoritative supported deceleration rather than constant-velocity approximation");
    GameState decelOwnedAdvance = decelPairBefore;
    const TableDepartureStepResult decelOwnedAdvanceResult =
        advanceTableDepartureDomains(decelOwnedAdvance,
            decelQuery.earliestTimeSeconds, profile,
            PhysicsBoundaryMode::Unbounded, decelOwnership);
    expect(decelOwnedAdvanceResult.ok &&
        decelOwnedAdvanceResult.events.empty() &&
        decelOwnedAdvance.balls[0].position.x !=
            decelPairBefore.balls[0].position.x &&
        sameFloatBits(decelOwnedAdvance.balls[1].position.x,
            decelPairBefore.balls[1].position.x),
        "D6 owned advance must trust the scheduler cross horizon and never re-query legacy motion with a D6 model");
    std::vector<SurfaceMotionAnalyticSegment> missingLegacyMotion =
        decelAirMotion.analyticSegments;
    const TableDepartureEarliestBatch missingLegacyQuery =
        queryEarliestTableDepartureBatch(decelPairBefore, 0.08, profile,
            PhysicsBoundaryMode::Unbounded, missingLegacyMotion,
            decelOwnership);
    expect(!missingLegacyQuery.ok && missingLegacyQuery.failureCode ==
            TableDepartureFailureCode::InvalidState &&
        sameTransactionalState(decelPair, decelPairBefore),
        "D6 injected query must fail closed when an involved legacy trajectory partition is missing");

    BallState legacySupportedTemplate = ballAt(0.0f,
        geometry.clothSupportedCenterYCm, 42.0f,
        35.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    BallState legacySupportedAtTarget = legacySupportedTemplate;
    const SurfaceMotionStep legacyTargetMotion = advanceSurfaceMotion(
        legacySupportedAtTarget, static_cast<float>(decelContactTime),
        profile.ball, profile.surface);
    const double legacyTargetDisplacement =
        legacySupportedAtTarget.position.x -
        legacySupportedTemplate.position.x;
    BallState injectedLegacySupported = legacySupportedTemplate;
    injectedLegacySupported.position.x = static_cast<float>(
        20.0 + 100.0 * decelContactTime + r08ContactDx -
        legacyTargetDisplacement);
    injectedLegacySupported.position.z = static_cast<float>(
        30.0 + r08ContactDz);
    BallState injectedLegacyPrediction = injectedLegacySupported;
    const SurfaceMotionStep injectedLegacyMotion = advanceSurfaceMotion(
        injectedLegacyPrediction, 0.08f, profile.ball, profile.surface);
    GameState injectedAirPrediction = isolatedState(decelAir);
    const TableDepartureStepResult injectedAirMotion =
        advanceTableDeparturePhysics(injectedAirPrediction, 0.08f, profile,
            PhysicsBoundaryMode::Unbounded,
            profile.solver.maximumEventsPerTick);
    std::vector<SurfaceMotionAnalyticSegment> injectedAuthoritativeMotion =
        injectedAirMotion.analyticSegments;
    for (SurfaceMotionAnalyticSegment segment :
            injectedLegacyMotion.analyticSegments) {
        segment.ballIndex = 1;
        injectedAuthoritativeMotion.push_back(segment);
    }
    GameState injectedLegacyPair = isolatedState(decelAir);
    injectedLegacyPair.balls[1] = injectedLegacySupported;
    injectedLegacyPair.balls[1].pocketed = false;
    const GameState injectedLegacyBefore = injectedLegacyPair;
    std::array<bool, kBallCount> injectedOwnership{};
    injectedOwnership[0] = true;
    const TableDepartureEarliestBatch injectedLegacyQuery =
        queryEarliestTableDepartureBatch(injectedLegacyPair, 0.08,
            profile, PhysicsBoundaryMode::Unbounded,
            injectedAuthoritativeMotion, injectedOwnership);
    expect(!legacyTargetMotion.analyticSegments.empty() &&
        !injectedLegacyMotion.analyticSegments.empty() &&
        injectedAirMotion.ok && injectedLegacyQuery.ok &&
        injectedLegacyQuery.candidates.size() == 1 &&
        injectedLegacyQuery.candidates[0].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        close(injectedLegacyQuery.earliestTimeSeconds, decelContactTime,
            profile.solver.toiToleranceSeconds) &&
        sameTransactionalState(injectedLegacyPair, injectedLegacyBefore),
        "D6 injected query must consume the real legacy advanceSurfaceMotion trajectory without substituting D6 rolling");
    GameState legacyOnlyCompatibility = isolatedState(
        injectedLegacySupported);
    legacyOnlyCompatibility.balls[0].position.y += 10.0f;
    const GameState legacyOnlyCompatibilityBefore =
        legacyOnlyCompatibility;
    const TableDepartureEarliestBatch legacyOnlyCompatibilityQuery =
        queryEarliestTableDepartureBatch(legacyOnlyCompatibility, 0.08,
            profile, PhysicsBoundaryMode::ProductionTable, {},
            std::array<bool, kBallCount>{});
    expect(legacyOnlyCompatibilityQuery.ok &&
        legacyOnlyCompatibilityQuery.candidates.empty() &&
        sameTransactionalState(legacyOnlyCompatibility,
            legacyOnlyCompatibilityBefore),
        "D6 injected query must not impose D6 table geometry on a non-owned legacy-only ball");
    BallState r08ExpectedAirAtContact = r08Air;
    BallState r08ExpectedSupportedAtContact = r08Cloth;
    advanceIndependentToContact(r08ExpectedAirAtContact,
        r08BallRoot.timeSeconds, geometry.gravityYCmS2);
    advanceIndependentToContact(r08ExpectedSupportedAtContact,
        r08BallRoot.timeSeconds, geometry.gravityYCmS2);
    std::array<double, 3> r08ProjectionNormal{};
    double r08ProjectionExcess = 0.0;
    expect(projectIndependentAnalyticContact(r08ExpectedAirAtContact,
            r08ExpectedSupportedAtContact, r08RadiusSum,
            r08ProjectionNormal, r08ProjectionExcess),
        "R08 expected rigid-contact state must need only bounded projection");
    for (int r08Adjust = 0; r08Adjust < 4; ++r08Adjust) {
        const double dx = r08ExpectedSupportedAtContact.position.x -
            r08ExpectedAirAtContact.position.x;
        const double dy = r08ExpectedSupportedAtContact.position.y -
            r08ExpectedAirAtContact.position.y;
        const double dz = r08ExpectedSupportedAtContact.position.z -
            r08ExpectedAirAtContact.position.z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) <= r08RadiusSum) break;
        r08ExpectedAirAtContact.position.x = std::nextafter(
            r08ExpectedAirAtContact.position.x,
            r08ExpectedSupportedAtContact.position.x);
    }
    const BallBallContactResult r08RigidResult = resolveBallBallContact(
        r08ExpectedAirAtContact, r08ExpectedSupportedAtContact,
        profile.ball, profile.ball);
    const BallState r08SupportedAfterRigid = r08ExpectedSupportedAtContact;
    expect(r08RigidResult.velocityImpulseApplied &&
        r08SupportedAfterRigid.velocity.y < 0.0f,
        "R08 rigid resolver must produce the downward supported impulse used by the reaction fixture");
    BallState r08ExpectedAirFinal = r08ExpectedAirAtContact;
    BallState r08ExpectedSupportedFinal = r08SupportedAfterRigid;
    const Point3 r08PreservedSupportedVelocity =
        r08ExpectedSupportedFinal.velocity;
    const Point3 r08PreservedSupportedAngular =
        r08ExpectedSupportedFinal.angularVelocity;
    const Point3 r08PreservedSupportedAxis =
        r08ExpectedSupportedFinal.rotationAxis;
    const float r08PreservedSupportedAngle =
        r08ExpectedSupportedFinal.rotationAngle;
    r08ExpectedSupportedFinal.velocity.y = 0.0f;
    r08ExpectedSupportedFinal.position.y = geometry.clothSupportedCenterYCm;
    r08ExpectedSupportedFinal.speed = static_cast<float>(std::hypot(
        r08ExpectedSupportedFinal.velocity.x,
        r08ExpectedSupportedFinal.velocity.z));
    const BallState r08AirAfterReaction = r08ExpectedAirFinal;
    const BallState r08SupportedAfterReaction = r08ExpectedSupportedFinal;
    const double r08ReactionSeparation[3] = {
        r08SupportedAfterReaction.position.x -
            r08AirAfterReaction.position.x,
        r08SupportedAfterReaction.position.y -
            r08AirAfterReaction.position.y,
        r08SupportedAfterReaction.position.z -
            r08AirAfterReaction.position.z};
    const double r08ReactionRelativeVelocity[3] = {
        r08SupportedAfterReaction.velocity.x -
            r08AirAfterReaction.velocity.x,
        r08SupportedAfterReaction.velocity.y -
            r08AirAfterReaction.velocity.y,
        r08SupportedAfterReaction.velocity.z -
            r08AirAfterReaction.velocity.z};
    const double r08PostReactionNormalSpeed = (
        r08ReactionSeparation[0] * r08ReactionRelativeVelocity[0] +
        r08ReactionSeparation[1] * r08ReactionRelativeVelocity[1] +
        r08ReactionSeparation[2] * r08ReactionRelativeVelocity[2]) /
        std::sqrt(r08ReactionSeparation[0] * r08ReactionSeparation[0] +
            r08ReactionSeparation[1] * r08ReactionSeparation[1] +
            r08ReactionSeparation[2] * r08ReactionSeparation[2]);
    const double r08TailDuration = 0.2 - r08BallRoot.timeSeconds;
    const double r08PostContactClothTail =
        independentEarliestDescendingPlaneRoot(r08AirAfterReaction,
            geometry.clothSupportedCenterYCm, r08TailDuration,
            geometry.gravityYCmS2);
    const std::array<double, 3> r08PostContactClothPoint =
        independentPositionAt(r08AirAfterReaction,
            r08PostContactClothTail, geometry.gravityYCmS2);
    const std::array<double, 3> r08PostContactClothVelocity =
        independentVelocityAt(r08AirAfterReaction,
            r08PostContactClothTail, geometry.gravityYCmS2);
    const double r08PostContactClothAbsolute =
        r08BallRoot.timeSeconds + r08PostContactClothTail;
    expect(std::isfinite(r08PostContactClothTail) &&
        r08PostContactClothTail > 1e-7 &&
        r08PostContactClothTail < r08TailDuration &&
        std::fabs(r08PostContactClothPoint[0]) <=
            geometry.playfieldHalfWidthCm + geometry.ballRadiusCm &&
        std::fabs(r08PostContactClothPoint[2]) <=
            geometry.playfieldHalfLengthCm + geometry.ballRadiusCm &&
        std::fabs(r08PostContactClothPoint[0]) <
            geometry.playfieldHalfWidthCm - geometry.ballRadiusCm &&
        r08PostContactClothVelocity[1] < 0.0,
        "R08 post-contact oracle must prove a descending inside-nonmouth cloth return");
    const IndependentSweptSphereRoot r08PostReactionRoot =
        independentEarliestSweptSphereRoot(r08AirAfterReaction,
            r08SupportedAfterReaction,
            r08PostContactClothTail,
            geometry.gravityYCmS2, r08RadiusSum);
    expect(r08ExpectedSupportedFinal.velocity.x ==
            r08PreservedSupportedVelocity.x &&
        r08ExpectedSupportedFinal.velocity.z ==
            r08PreservedSupportedVelocity.z &&
        sameFloatBits(r08ExpectedSupportedFinal.angularVelocity.x,
            r08PreservedSupportedAngular.x) &&
        sameFloatBits(r08ExpectedSupportedFinal.angularVelocity.y,
            r08PreservedSupportedAngular.y) &&
        sameFloatBits(r08ExpectedSupportedFinal.angularVelocity.z,
            r08PreservedSupportedAngular.z) &&
        sameFloatBits(r08ExpectedSupportedFinal.rotationAxis.x,
            r08PreservedSupportedAxis.x) &&
        sameFloatBits(r08ExpectedSupportedFinal.rotationAxis.y,
            r08PreservedSupportedAxis.y) &&
        sameFloatBits(r08ExpectedSupportedFinal.rotationAxis.z,
            r08PreservedSupportedAxis.z) &&
        sameFloatBits(r08ExpectedSupportedFinal.rotationAngle,
            r08PreservedSupportedAngle) &&
        finiteIndependentBall(r08ExpectedSupportedFinal) &&
        r08PostReactionNormalSpeed >=
            -profile.solver.residualToleranceCmS &&
        !r08PostReactionRoot.hit,
        "R08 downward support reaction must preserve xz/angular/rotation and recompute finite speed");
    GameState r08SchedulerState = isolatedState(r08Air);
    r08SchedulerState.balls[1] = r08Cloth;
    r08SchedulerState.balls[1].pocketed = false;
    const GameState r08SchedulerBefore = r08SchedulerState;
    const TableDepartureEarliestBatch r08SchedulerQuery =
        queryEarliestTableDepartureBatch(r08SchedulerState, 0.2,
            profile, PhysicsBoundaryMode::ProductionTable);
    expect(r08SchedulerQuery.ok &&
        r08SchedulerQuery.candidates.size() == 1 &&
        r08SchedulerQuery.candidates[0].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        r08SchedulerQuery.candidates[0].firstBall == 0 &&
        r08SchedulerQuery.candidates[0].secondBall == 1 &&
        close(r08SchedulerQuery.earliestTimeSeconds,
            r08BallRoot.timeSeconds,
            profile.solver.toiToleranceSeconds) &&
        sameTransactionalState(r08SchedulerState, r08SchedulerBefore),
        "D6 earliest query must expose the full-state cross-domain pair without mutation");
    std::array<bool, kBallCount> r08SchedulerOwnership{};
    r08SchedulerOwnership[0] = true;
    r08SchedulerOwnership[1] = true;
    const TableDepartureStepResult r08SchedulerAdvance =
        advanceTableDepartureDomains(r08SchedulerState,
            r08SchedulerQuery.earliestTimeSeconds, profile,
            PhysicsBoundaryMode::ProductionTable, r08SchedulerOwnership);
    expect(r08SchedulerAdvance.ok && r08SchedulerAdvance.events.empty() &&
        validPrivateAnalyticLineage(r08SchedulerAdvance.analyticSegments,
            {0, 1}, r08SchedulerQuery.earliestTimeSeconds,
            profile.solver.penetrationSlopCm,
            profile.solver.toiToleranceSeconds),
        "D6 scheduler advance must move both cross-domain participants to the queried TOI without resolving");
    const TableDepartureStepResult r08SchedulerResolve =
        resolveTableDepartureBatch(r08SchedulerState, r08SchedulerQuery,
            profile, r08SchedulerQuery.earliestTimeSeconds, 3);
    expect(r08SchedulerResolve.ok &&
        r08SchedulerResolve.events.size() == 1 &&
        r08SchedulerResolve.events[0].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        r08SchedulerResolve.events[0].ordinal == 3 &&
        r08SchedulerResolve.events[0].transitionWitnessComplete &&
        x05SemanticBall(r08SchedulerState.balls[0],
            r08ExpectedAirFinal, profile,
            r08SchedulerQuery.earliestTimeSeconds) &&
        x05SemanticBall(r08SchedulerState.balls[1],
            r08ExpectedSupportedFinal, profile,
            r08SchedulerQuery.earliestTimeSeconds),
        "D6 single-batch resolver must reproduce the independent rigid contact and support reaction");
    GameState r08SchedulerSecond = r08SchedulerState;
    const TableDepartureEarliestBatch r08SchedulerSecondQuery =
        queryEarliestTableDepartureBatch(r08SchedulerSecond,
            r08TailDuration, profile,
            PhysicsBoundaryMode::ProductionTable);
    std::array<bool, kBallCount> r08SchedulerSecondOwnership{};
    r08SchedulerSecondOwnership[0] = true;
    const TableDepartureStepResult r08SchedulerSecondAdvance =
        advanceTableDepartureDomains(r08SchedulerSecond,
            r08SchedulerSecondQuery.earliestTimeSeconds, profile,
            PhysicsBoundaryMode::ProductionTable,
            r08SchedulerSecondOwnership);
    const TableDepartureStepResult r08SchedulerSecondResolve =
        resolveTableDepartureBatch(r08SchedulerSecond,
            r08SchedulerSecondQuery, profile,
            r08SchedulerQuery.earliestTimeSeconds +
                r08SchedulerSecondQuery.earliestTimeSeconds,
            4);
    expect(r08SchedulerSecondQuery.ok &&
        r08SchedulerSecondQuery.candidates.size() == 1 &&
        r08SchedulerSecondQuery.candidates[0].kind ==
            TableDepartureEventKind::ClothReturn &&
        r08SchedulerSecondAdvance.ok &&
        r08SchedulerSecondAdvance.events.empty() &&
        r08SchedulerSecondResolve.ok &&
        r08SchedulerSecondResolve.events.size() == 1 &&
        r08SchedulerSecondResolve.events[0].kind ==
            TableDepartureEventKind::ClothReturn &&
        r08SchedulerSecondResolve.events[0].ordinal == 4 &&
        r08SchedulerSecond.balls[0].physicalDomain ==
            PhysicalBallDomain::ClothSupported,
        "D6 scheduler primitives must re-query and consume a second same-tick event without tail replay");
    const double r08AirborneTailToCloth =
        r08PostContactClothTail;
    advanceIndependentDomainTail(r08ExpectedAirFinal,
        r08AirborneTailToCloth, geometry.gravityYCmS2,
        profile.tableDeparture.supportedRollingDecelerationCmS2,
        profile.surface.torsionalSpinDecelerationRadS2);
    expect(r08ExpectedAirFinal.velocity.y < 0.0f &&
        close(r08ExpectedAirFinal.position.y,
            geometry.clothSupportedCenterYCm,
            profile.solver.penetrationSlopCm) &&
        std::fabs(r08ExpectedAirFinal.position.x) <=
            geometry.playfieldHalfWidthCm + geometry.ballRadiusCm &&
        std::fabs(r08ExpectedAirFinal.position.z) <=
            geometry.playfieldHalfLengthCm + geometry.ballRadiusCm,
        "R08 independent expected path must descend into the cloth envelope at its sealed root");
    r08ExpectedAirFinal.physicalDomain = PhysicalBallDomain::ClothSupported;
    r08ExpectedAirFinal.position.y = geometry.clothSupportedCenterYCm;
    r08ExpectedAirFinal.velocity.y = 0.0f;
    r08ExpectedAirFinal.speed = static_cast<float>(std::hypot(
        r08ExpectedAirFinal.velocity.x, r08ExpectedAirFinal.velocity.z));
    advanceIndependentDomainTail(r08ExpectedAirFinal,
        0.2 - r08PostContactClothAbsolute, geometry.gravityYCmS2,
        profile.tableDeparture.supportedRollingDecelerationCmS2,
        profile.surface.torsionalSpinDecelerationRadS2);
    advanceIndependentDomainTail(r08ExpectedSupportedFinal, r08TailDuration,
        geometry.gravityYCmS2,
        profile.tableDeparture.supportedRollingDecelerationCmS2,
        profile.surface.torsionalSpinDecelerationRadS2);
    GameState r08BudgetState = isolatedState(r08Air);
    r08BudgetState.balls[1] = r08Cloth;
    r08BudgetState.balls[1].pocketed = false;
    const GameState r08BudgetBefore = r08BudgetState;
    const TableDepartureStepResult r08BudgetResult =
        advanceTableDeparturePhysics(r08BudgetState, 0.2f, profile,
            PhysicsBoundaryMode::ProductionTable, 1);
    const bool r08BudgetContract = !r08BudgetResult.ok &&
        r08BudgetResult.failureCode == TableDepartureFailureCode::EventBudget &&
        r08BudgetResult.events.empty() &&
        sameTransactionalState(r08BudgetState, r08BudgetBefore);
    GameState r08SuccessState = r08BudgetBefore;
    const TableDepartureStepResult r08SuccessResult =
        advanceTableDeparturePhysics(r08SuccessState, 0.2f, profile,
            PhysicsBoundaryMode::ProductionTable, 2);
    expect(r08BudgetContract,
        "R08 one-event budget must rollback a two-event contact-to-cloth-return chain");
    expect(r08SuccessResult.ok && r08SuccessResult.events.size() == 2 &&
        r08SuccessResult.events[0].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        r08SuccessResult.events[1].kind ==
            TableDepartureEventKind::ClothReturn &&
        r08SuccessResult.events[0].ballIndex == 0 &&
        r08SuccessResult.events[1].ballIndex == 0 &&
        r08SuccessResult.events[0].ordinal == 0 &&
        r08SuccessResult.events[1].ordinal == 1 &&
        close(r08SuccessResult.events[0].absoluteWholeTickTimeSeconds,
            r08BallRoot.timeSeconds,
            profile.solver.toiToleranceSeconds) &&
        close(r08SuccessResult.events[1].absoluteWholeTickTimeSeconds,
            r08PostContactClothAbsolute,
            profile.solver.toiToleranceSeconds) &&
        !r08SuccessResult.events[0].stableIdentity.empty() &&
        !r08SuccessResult.events[1].stableIdentity.empty() &&
        r08SuccessResult.events[0].stableIdentity.rfind(
            "cross_domain_ball_contact:0:1:0:", 0) == 0 &&
        r08SuccessResult.events[1].stableIdentity.rfind(
            "0:1:cloth_return:", 0) == 0 &&
        r08SuccessResult.events[0].absoluteWholeTickTimeSeconds <
            r08SuccessResult.events[1].absoluteWholeTickTimeSeconds &&
        r08SuccessResult.events[0].stableIdentity !=
            r08SuccessResult.events[1].stableIdentity &&
        countTableDepartureEvents(r08SuccessResult,
            TableDepartureEventKind::CrossDomainBallContact) == 1 &&
        r08SuccessState.balls[1].physicalDomain ==
            PhysicalBallDomain::ClothSupported &&
        r08SuccessState.balls[1].velocity.y == 0.0f &&
        close(r08SuccessState.balls[1].position.y,
            geometry.clothSupportedCenterYCm,
            profile.solver.penetrationSlopCm) &&
        std::isfinite(r08SuccessState.balls[1].speed) &&
        r08SuccessState.balls[0].physicalDomain ==
            PhysicalBallDomain::ClothSupported &&
        x05SemanticBall(r08SuccessState.balls[0], r08ExpectedAirFinal,
            profile, 0.2) &&
        x05SemanticBall(r08SuccessState.balls[1],
            r08ExpectedSupportedFinal, profile, 0.2),
        "R08 two-event budget must commit one ordered, deduped contact-to-cloth-return chain");

    struct R08UpCandidate {
        double anchorX;
        double nx;
        double ny;
        double targetTime;
        double speed;
    };
    const R08UpCandidate r08UpUniverse[] = {
        {70.0, 0.25, 0.25, 0.02, 20.0},
        {80.0, -0.75, 0.25, 0.02, 20.0},
        {80.0, -0.60, 0.20, 0.02, 40.0},
        {80.0, -0.70, 0.20, 0.02, 40.0},
    };
    const auto evaluateR08UpCandidate = [&](const R08UpCandidate& candidate) {
        const double candidateNormal[2] = {candidate.nx, candidate.ny};
        const double candidateNz = std::sqrt(1.0 -
            candidateNormal[0] * candidateNormal[0] -
            candidateNormal[1] * candidateNormal[1]);
        if (!std::isfinite(candidateNz) || !(candidateNz > 0.0)) return 1;
            BallState candidateSupported = ballAt(
                static_cast<float>(candidate.anchorX),
                geometry.railTopSupportedCenterYCm, 30.0f,
                0.0f, 0.0f, 0.0f,
                PhysicalBallDomain::RailTopSupported);
            const double candidateVelocity[3] = {
                candidate.speed * candidateNormal[0],
                candidate.speed * candidateNormal[1],
                candidate.speed * candidateNz};
            const double candidateContact[3] = {
                candidateSupported.position.x -
                    r08RadiusSum * candidateNormal[0],
                candidateSupported.position.y -
                    r08RadiusSum * candidateNormal[1],
                candidateSupported.position.z -
                    r08RadiusSum * candidateNz};
            const double candidateInitialVy = candidateVelocity[1] -
                geometry.gravityYCmS2 * candidate.targetTime;
            BallState candidateAir = ballAt(static_cast<float>(
                    candidateContact[0] -
                        candidateVelocity[0] * candidate.targetTime),
                static_cast<float>(candidateContact[1] -
                    candidateInitialVy * candidate.targetTime -
                    0.5 * geometry.gravityYCmS2 * candidate.targetTime *
                        candidate.targetTime),
                static_cast<float>(candidateContact[2] -
                    candidateVelocity[2] * candidate.targetTime),
                static_cast<float>(candidateVelocity[0]),
                static_cast<float>(candidateInitialVy),
                static_cast<float>(candidateVelocity[2]),
                PhysicalBallDomain::Airborne);
            const auto insideCloth = [&](const std::array<double, 3>& point) {
                return std::fabs(point[0]) <=
                        geometry.playfieldHalfWidthCm +
                            geometry.ballRadiusCm &&
                    std::fabs(point[2]) <=
                        geometry.playfieldHalfLengthCm +
                            geometry.ballRadiusCm;
            };
            const auto insideOuter = [&](const std::array<double, 3>& point) {
                return std::fabs(point[0]) <= geometry.outerHalfWidthCm &&
                    std::fabs(point[2]) <= geometry.outerHalfLengthCm;
            };
            const std::array<double, 3> candidateInitial = {{
                candidateAir.position.x, candidateAir.position.y,
                candidateAir.position.z}};
            if (candidateAir.position.y <
                    geometry.outsideLandingSupportedCenterYCm - 1e-4 ||
                (insideCloth(candidateInitial) && candidateAir.position.y <
                    geometry.clothSupportedCenterYCm) ||
                (insideOuter(candidateInitial) &&
                    !insideCloth(candidateInitial) &&
                    candidateAir.position.y <
                        geometry.railTopSupportedCenterYCm - 1e-4))
                return 9;
            for (double fraction : {0.0, 0.25, 0.5, 0.75, 1.0}) {
                const std::array<double, 3> point = independentPositionAt(
                    candidateAir, candidate.targetTime * fraction,
                    geometry.gravityYCmS2);
                if ((insideCloth(point) && point[1] <
                        geometry.clothSupportedCenterYCm) ||
                    (insideOuter(point) && !insideCloth(point) && point[1] <
                        geometry.railTopSupportedCenterYCm - 1e-4))
                    return 10;
            }
            const IndependentSweptSphereRoot candidateRoot =
                independentEarliestSweptSphereRoot(candidateAir,
                    candidateSupported, 0.05, geometry.gravityYCmS2,
                    r08RadiusSum);
            if (!candidateRoot.hit || !close(candidateRoot.timeSeconds,
                    candidate.targetTime,
                    profile.solver.toiToleranceSeconds))
                return 2;
            const IndependentPolynomial candidatePolynomial =
                independentSeparationPolynomial(candidateAir,
                    candidateSupported, geometry.gravityYCmS2,
                    r08RadiusSum);
            const std::vector<double> candidateRoots =
                isolateIndependentPolynomialRoots(candidatePolynomial,
                    0.0, 0.05);
            if (!(evaluateIndependentPolynomial(candidatePolynomial, 0.0) >
                    0.0) || std::count_if(candidateRoots.begin(),
                    candidateRoots.end(), [&](double root) {
                        return root <= candidate.targetTime +
                            profile.solver.toiToleranceSeconds;
                    }) != 1)
                return 3;
            const double candidateCloth =
                independentEarliestDescendingPlaneRoot(candidateAir,
                    geometry.clothSupportedCenterYCm, 0.05,
                    geometry.gravityYCmS2);
            const double candidateRail =
                independentEarliestDescendingPlaneRoot(candidateAir,
                    geometry.railTopSupportedCenterYCm, 0.05,
                    geometry.gravityYCmS2);
            const double candidateOutside =
                independentEarliestDescendingPlaneRoot(candidateAir,
                    geometry.outsideLandingSupportedCenterYCm, 0.05,
                    geometry.gravityYCmS2);
            const auto laterOrAbsent = [&](double root) {
                return !std::isfinite(root) || candidate.targetTime +
                    profile.solver.toiToleranceSeconds < root;
            };
            if (!laterOrAbsent(candidateCloth) ||
                !laterOrAbsent(candidateRail) ||
                !laterOrAbsent(candidateOutside)) return 4;
            advanceIndependentToContact(candidateAir,
                candidateRoot.timeSeconds, geometry.gravityYCmS2);
            advanceIndependentToContact(candidateSupported,
                candidateRoot.timeSeconds, geometry.gravityYCmS2);
            const IndependentCanonicalContact candidateProjection =
                canonicalizeIndependentContact(candidateAir,
                    candidateSupported, r08RadiusSum);
            if (!candidateProjection.ok) return 5;
            const BallBallContactResult candidateResult =
                resolveBallBallContact(candidateAir, candidateSupported,
                    profile.ball, profile.ball);
            if (!candidateResult.velocityImpulseApplied ||
                !(candidateSupported.velocity.y >
                    profile.solver.residualToleranceCmS) ||
                !finiteIndependentBall(candidateAir) ||
                !finiteIndependentBall(candidateSupported)) return 6;
            if (candidateResult.kineticEnergyAfterJ >
                    candidateResult.kineticEnergyBeforeJ +
                    0.5 * profile.solver.passiveEnergyToleranceJ)
                return 7;
            candidateSupported.physicalDomain =
                PhysicalBallDomain::Airborne;
            if (independentEarliestSweptSphereRoot(candidateAir,
                    candidateSupported, 0.05 - candidateRoot.timeSeconds,
                    geometry.gravityYCmS2, r08RadiusSum).hit)
                return 8;
            return 0;
    };
    int r08UpCandidateReasons[std::size(r08UpUniverse)]{};
    for (std::size_t index = 0; index < std::size(r08UpUniverse); ++index) {
        r08UpCandidateReasons[index] =
            evaluateR08UpCandidate(r08UpUniverse[index]);
    }
    expect(r08UpCandidateReasons[0] == 9 &&
        r08UpCandidateReasons[1] == 0 &&
        r08UpCandidateReasons[2] == 10 &&
        r08UpCandidateReasons[3] == 7,
        "R08 fixed independent lattice must reject the old initial-solid candidate and select the first exterior path with margin");
    const R08UpCandidate r08UpSelected = r08UpUniverse[1];
    const double r08UpTargetTime = r08UpSelected.targetTime;
    const std::array<double, 3> r08UpNormal = {{r08UpSelected.nx,
        r08UpSelected.ny,
        std::sqrt(1.0 - r08UpSelected.nx * r08UpSelected.nx -
            r08UpSelected.ny * r08UpSelected.ny)}};
    BallState r08UpSupported = ballAt(
        static_cast<float>(r08UpSelected.anchorX),
        geometry.railTopSupportedCenterYCm, 30.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::RailTopSupported);
    const std::array<double, 3> r08UpContactVelocity = {{
        r08UpSelected.speed * r08UpNormal[0],
        r08UpSelected.speed * r08UpNormal[1],
        r08UpSelected.speed * r08UpNormal[2]}};
    const std::array<double, 3> r08UpContactPosition = {{
        r08UpSupported.position.x - r08RadiusSum * r08UpNormal[0],
        r08UpSupported.position.y - r08RadiusSum * r08UpNormal[1],
        r08UpSupported.position.z - r08RadiusSum * r08UpNormal[2]}};
    const double r08UpInitialVy = r08UpContactVelocity[1] -
        geometry.gravityYCmS2 * r08UpTargetTime;
    BallState r08UpAir = ballAt(static_cast<float>(
            r08UpContactPosition[0] -
                r08UpContactVelocity[0] * r08UpTargetTime),
        static_cast<float>(r08UpContactPosition[1] -
            r08UpInitialVy * r08UpTargetTime -
            0.5 * geometry.gravityYCmS2 *
                r08UpTargetTime * r08UpTargetTime),
        static_cast<float>(r08UpContactPosition[2] -
            r08UpContactVelocity[2] * r08UpTargetTime),
        static_cast<float>(r08UpContactVelocity[0]),
        static_cast<float>(r08UpInitialVy),
        static_cast<float>(r08UpContactVelocity[2]),
        PhysicalBallDomain::Airborne);
    const IndependentSweptSphereRoot r08UpRoot =
        independentEarliestSweptSphereRoot(r08UpAir, r08UpSupported, 0.05,
            geometry.gravityYCmS2, r08RadiusSum);
    const IndependentPolynomial r08UpPolynomial =
        independentSeparationPolynomial(r08UpAir, r08UpSupported,
            geometry.gravityYCmS2, r08RadiusSum);
    const std::vector<double> r08UpRoots = isolateIndependentPolynomialRoots(
        r08UpPolynomial, 0.0, 0.05);
    const double r08UpClothCandidate = independentEarliestDescendingPlaneRoot(
        r08UpAir, geometry.clothSupportedCenterYCm, 0.05,
        geometry.gravityYCmS2);
    const double r08UpRailCandidate = independentEarliestDescendingPlaneRoot(
        r08UpAir, geometry.railTopSupportedCenterYCm, 0.05,
        geometry.gravityYCmS2);
    const double r08UpOutsideCandidate =
        independentEarliestDescendingPlaneRoot(r08UpAir,
            geometry.outsideLandingSupportedCenterYCm, 0.05,
            geometry.gravityYCmS2);
    expect(evaluateIndependentPolynomial(r08UpPolynomial, 0.0) > 0.0 &&
        r08UpRoot.hit && r08UpRoot.approachDerivative < 0.0 &&
        close(r08UpRoot.timeSeconds, r08UpTargetTime,
            profile.solver.toiToleranceSeconds) &&
        std::count_if(r08UpRoots.begin(), r08UpRoots.end(),
            [&](double root) {
                return root <= r08UpTargetTime +
                    profile.solver.toiToleranceSeconds;
            }) == 1 &&
        (!std::isfinite(r08UpClothCandidate) ||
            r08UpTargetTime + profile.solver.toiToleranceSeconds <
                r08UpClothCandidate) &&
        (!std::isfinite(r08UpRailCandidate) ||
            r08UpTargetTime + profile.solver.toiToleranceSeconds <
                r08UpRailCandidate) &&
        (!std::isfinite(r08UpOutsideCandidate) ||
            r08UpTargetTime + profile.solver.toiToleranceSeconds <
                r08UpOutsideCandidate),
        "R08 upward fixture must have one earliest approaching root before every table candidate");
    BallState r08ExpectedUpAir = r08UpAir;
    BallState r08ExpectedUpSupported = r08UpSupported;
    advanceIndependentToContact(r08ExpectedUpAir, r08UpRoot.timeSeconds,
        geometry.gravityYCmS2);
    advanceIndependentToContact(r08ExpectedUpSupported,
        r08UpRoot.timeSeconds, geometry.gravityYCmS2);
    const IndependentCanonicalContact r08UpProjection =
        canonicalizeIndependentContact(r08ExpectedUpAir,
            r08ExpectedUpSupported, r08RadiusSum);
    const BallState r08UpCanonicalAir = r08ExpectedUpAir;
    expect(r08UpProjection.ok && r08UpProjection.axis == 0 &&
        r08UpProjection.iterations >= 0 && r08UpProjection.iterations <= 4,
        "R08 upward expected contact must require only bounded projection");
    const BallBallContactResult r08UpRigidResult = resolveBallBallContact(
        r08ExpectedUpAir, r08ExpectedUpSupported,
        profile.ball, profile.ball);
    expect(r08UpRigidResult.velocityImpulseApplied &&
        r08ExpectedUpSupported.velocity.y > 0.0f &&
        r08UpRigidResult.kineticEnergyAfterJ <=
            r08UpRigidResult.kineticEnergyBeforeJ +
            profile.solver.passiveEnergyToleranceJ,
        "R08 upward oracle must prove a positive supported impulse and passive energy");
    r08ExpectedUpSupported.physicalDomain = PhysicalBallDomain::Airborne;
    const BallState r08UpPostResolverAir = r08ExpectedUpAir;
    const BallState r08UpPostResolverSupported = r08ExpectedUpSupported;
    const IndependentSweptSphereRoot r08UpPostContactRoot =
        independentEarliestSweptSphereRoot(r08ExpectedUpAir,
            r08ExpectedUpSupported, 0.05 - r08UpRoot.timeSeconds,
            geometry.gravityYCmS2, r08RadiusSum);
    const double r08UpRailTail = independentEarliestDescendingPlaneRoot(
        r08ExpectedUpSupported, geometry.railTopSupportedCenterYCm,
        0.05 - r08UpRoot.timeSeconds, geometry.gravityYCmS2);
    const std::array<double, 3> r08UpRailPoint = independentPositionAt(
        r08ExpectedUpSupported, r08UpRailTail, geometry.gravityYCmS2);
    const std::array<double, 3> r08UpRailVelocity = independentVelocityAt(
        r08ExpectedUpSupported, r08UpRailTail, geometry.gravityYCmS2);
    const double r08UpRailAbsolute =
        r08UpRoot.timeSeconds + r08UpRailTail;
    expect(!r08UpPostContactRoot.hit && std::isfinite(r08UpRailTail) &&
        r08UpRailTail > profile.solver.toiToleranceSeconds &&
        r08UpRailAbsolute + profile.solver.toiToleranceSeconds < 0.05 &&
        std::fabs(r08UpRailPoint[0]) <= geometry.outerHalfWidthCm &&
        std::fabs(r08UpRailPoint[0]) >
            geometry.playfieldHalfWidthCm + geometry.ballRadiusCm &&
        std::fabs(r08UpRailPoint[2]) <= geometry.outerHalfLengthCm &&
        r08UpRailVelocity[1] < 0.0,
        "R08 upward support reaction must separate before an independently ordered rail landing");
    advanceIndependentDomainTail(r08ExpectedUpAir,
        0.05 - r08UpRoot.timeSeconds, geometry.gravityYCmS2,
        profile.tableDeparture.supportedRollingDecelerationCmS2,
        profile.surface.torsionalSpinDecelerationRadS2);
    advanceIndependentDomainTail(r08ExpectedUpSupported,
        r08UpRailTail, geometry.gravityYCmS2,
        profile.tableDeparture.supportedRollingDecelerationCmS2,
        profile.surface.torsionalSpinDecelerationRadS2);
    r08ExpectedUpSupported.physicalDomain =
        PhysicalBallDomain::RailTopSupported;
    r08ExpectedUpSupported.position.y = geometry.railTopSupportedCenterYCm;
    r08ExpectedUpSupported.velocity.y = 0.0f;
    r08ExpectedUpSupported.speed = static_cast<float>(std::hypot(
        r08ExpectedUpSupported.velocity.x,
        r08ExpectedUpSupported.velocity.z));
    advanceIndependentDomainTail(r08ExpectedUpSupported,
        0.05 - r08UpRailAbsolute, geometry.gravityYCmS2,
        profile.tableDeparture.supportedRollingDecelerationCmS2,
        profile.surface.torsionalSpinDecelerationRadS2);
    GameState r08UpState = isolatedState(r08UpAir);
    r08UpState.balls[1] = r08UpSupported;
    r08UpState.balls[1].pocketed = false;
    const TableDepartureStepResult r08UpResult = advanceTableDeparturePhysics(
        r08UpState, 0.05f, profile, PhysicsBoundaryMode::ProductionTable,
        profile.solver.maximumEventsPerTick);
    expect(r08UpResult.ok && r08UpResult.events.size() == 2 &&
        r08UpResult.events[0].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        r08UpResult.events[1].kind ==
            TableDepartureEventKind::RailTopLanding &&
        r08UpResult.events[0].ballIndex == 0 &&
        r08UpResult.events[1].ballIndex == 1 &&
        r08UpResult.events[0].ordinal == 0 &&
        r08UpResult.events[1].ordinal == 1 &&
        close(r08UpResult.events[0].absoluteWholeTickTimeSeconds,
            r08UpRoot.timeSeconds, profile.solver.toiToleranceSeconds) &&
        close(r08UpResult.events[1].absoluteWholeTickTimeSeconds,
            r08UpRailAbsolute, profile.solver.toiToleranceSeconds) &&
        r08UpResult.events[0].stableIdentity.rfind(
            "cross_domain_ball_contact:0:1:0:", 0) == 0 &&
        r08UpResult.events[1].stableIdentity.rfind(
            "1:1:rail_top:", 0) == 0 &&
        r08UpState.balls[1].physicalDomain ==
            PhysicalBallDomain::RailTopSupported &&
        r08UpState.balls[1].velocity.y == 0.0f &&
        finiteIndependentBall(r08UpState.balls[1]) &&
        x05SemanticBall(r08UpState.balls[0], r08ExpectedUpAir,
            profile, 0.05) &&
        x05SemanticBall(r08UpState.balls[1], r08ExpectedUpSupported,
            profile, 0.05),
        "R08 positive support impulse must release the ball before its independently ordered rail landing");
    const double bCrossOnlyDuration = r08UpRoot.timeSeconds +
        0.5 * r08UpRailTail;
    BallState bExpectedShortAir = r08ExpectedUpAir;
    BallState bExpectedShortSupported = r08ExpectedUpSupported;
    bExpectedShortAir = r08UpAir;
    bExpectedShortSupported = r08UpSupported;
    advanceIndependentToContact(bExpectedShortAir,
        r08UpRoot.timeSeconds, geometry.gravityYCmS2);
    advanceIndependentToContact(bExpectedShortSupported,
        r08UpRoot.timeSeconds, geometry.gravityYCmS2);
    const IndependentCanonicalContact bShortProjection =
        canonicalizeIndependentContact(bExpectedShortAir,
            bExpectedShortSupported, r08RadiusSum);
    expect(bShortProjection.ok && bShortProjection.axis == 0 &&
        sameTransactionalBall(bExpectedShortAir, r08UpCanonicalAir),
        "B3 short-window expected contact must retain bounded projection");
    const BallBallContactResult bShortContact = resolveBallBallContact(
        bExpectedShortAir, bExpectedShortSupported,
        profile.ball, profile.ball);
    expect(bShortContact.velocityImpulseApplied &&
        bExpectedShortSupported.velocity.y >
            profile.solver.residualToleranceCmS,
        "B3 short window must independently retain the upward release");
    bExpectedShortSupported.physicalDomain = PhysicalBallDomain::Airborne;
    advanceIndependentDomainTail(bExpectedShortAir,
        bCrossOnlyDuration - r08UpRoot.timeSeconds,
        geometry.gravityYCmS2,
        profile.tableDeparture.supportedRollingDecelerationCmS2,
        profile.surface.torsionalSpinDecelerationRadS2);
    advanceIndependentDomainTail(bExpectedShortSupported,
        bCrossOnlyDuration - r08UpRoot.timeSeconds,
        geometry.gravityYCmS2,
        profile.tableDeparture.supportedRollingDecelerationCmS2,
        profile.surface.torsionalSpinDecelerationRadS2);
    GameState bCrossOnlyState = isolatedState(r08UpAir);
    bCrossOnlyState.balls[1] = r08UpSupported;
    bCrossOnlyState.balls[1].pocketed = false;
    const TableDepartureStepResult bCrossOnlyResult =
        advanceTableDeparturePhysics(bCrossOnlyState,
            static_cast<float>(bCrossOnlyDuration), profile,
            PhysicsBoundaryMode::ProductionTable, 1);
    expect(bCrossOnlyResult.ok && bCrossOnlyResult.events.size() == 1 &&
        bCrossOnlyResult.events[0].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        bCrossOnlyState.balls[1].physicalDomain ==
            PhysicalBallDomain::Airborne &&
        bCrossOnlyState.balls[1].velocity.y > 0.0f &&
        x05SemanticBall(bCrossOnlyState.balls[0], bExpectedShortAir,
            profile, bCrossOnlyDuration) &&
        x05SemanticBall(bCrossOnlyState.balls[1], bExpectedShortSupported,
            profile, bCrossOnlyDuration),
        "B3 recursive contact-only path must succeed with exact maxEvents one");
    GameState bFullOneEventState = isolatedState(r08UpAir);
    bFullOneEventState.balls[1] = r08UpSupported;
    bFullOneEventState.balls[1].pocketed = false;
    const GameState bFullOneEventBefore = bFullOneEventState;
    const TableDepartureStepResult bFullOneEventResult =
        advanceTableDeparturePhysics(bFullOneEventState, 0.05f, profile,
            PhysicsBoundaryMode::ProductionTable, 1);
    expect(!bFullOneEventResult.ok && bFullOneEventResult.failureCode ==
            TableDepartureFailureCode::EventBudget &&
        bFullOneEventResult.events.empty() &&
        sameTransactionalState(bFullOneEventState, bFullOneEventBefore),
        "B3 full upward contact-to-rail chain with max one must rollback exact EventBudget");

    {
        std::ifstream source(std::string(BILLIARDGL_SOURCE_ROOT) +
            "/src/Billiards/table_departure_geometry.cpp");
        std::ostringstream text;
        text << source.rdbuf();
        const std::string accepted =
            "if (!tail.ok) { state = snapshot; return tail; }";
        const std::string lineBreakMutation =
            "if (!tail.ok) { state = snapshot; if (tail.failureCode ==\n"
            "TableDepartureFailureCode::InvalidState) return fail("
            "TableDepartureFailureCode::EventBudget); return tail; }";
        const std::string ternaryMutation =
            "if (!tail.ok) { state = snapshot; return tail.failureCode == "
            "TableDepartureFailureCode::InvalidState ? fail("
            "TableDepartureFailureCode::EventBudget) : tail; }";
        const std::string helperMutation =
            "if (!tail.ok) { state = snapshot; return mapChildFailure(tail, "
            "maximumEvents); }";
        expect(source.good() &&
            recursivelyPropagatesTailFailureVerbatim(text.str()) &&
            recursivelyPropagatesTailFailureVerbatim(accepted) &&
            !recursivelyPropagatesTailFailureVerbatim(lineBreakMutation) &&
            !recursivelyPropagatesTailFailureVerbatim(ternaryMutation) &&
            !recursivelyPropagatesTailFailureVerbatim(helperMutation),
            "B4 a true child InvalidState must never be translated into EventBudget");
    }

    BallState r08SeparatingAir = ballAt(0.0f,
        geometry.clothSupportedCenterYCm, 0.0f,
        -10.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState r08SeparatingCloth = ballAt(r08RadiusSum,
        geometry.clothSupportedCenterYCm, 0.0f,
        10.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    GameState r08SeparatingState = isolatedState(r08SeparatingAir);
    r08SeparatingState.balls[1] = r08SeparatingCloth;
    r08SeparatingState.balls[1].pocketed = false;
    const TableDepartureStepResult r08SeparatingResult =
        advanceTableDeparturePhysics(r08SeparatingState, 0.01f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(r08SeparatingResult.ok && countTableDepartureEvents(
            r08SeparatingResult,
            TableDepartureEventKind::CrossDomainBallContact) == 0,
        "R08 t0 separating pair within residual bound must not duplicate contact");

    BallState r08ResidualAcceptedAir = r08SeparatingAir;
    r08ResidualAcceptedAir.velocity.x = static_cast<float>(
        profile.solver.residualToleranceCmS * 0.99);
    r08ResidualAcceptedAir.speed = std::fabs(
        r08ResidualAcceptedAir.velocity.x);
    BallState r08ResidualAcceptedCloth = r08SeparatingCloth;
    r08ResidualAcceptedCloth.velocity.x = 0.0f;
    r08ResidualAcceptedCloth.speed = 0.0f;
    GameState r08ResidualAcceptedState = isolatedState(
        r08ResidualAcceptedAir);
    r08ResidualAcceptedState.balls[1] = r08ResidualAcceptedCloth;
    r08ResidualAcceptedState.balls[1].pocketed = false;
    const TableDepartureStepResult r08ResidualAcceptedResult =
        advanceTableDeparturePhysics(r08ResidualAcceptedState, 0.000001f,
            profile, PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(r08ResidualAcceptedResult.ok && countTableDepartureEvents(
            r08ResidualAcceptedResult,
            TableDepartureEventKind::CrossDomainBallContact) == 0,
        "R08 t0 approach at 0.99 residual must be accepted without duplicate contact");

    BallState r08ResidualRejectedAir = r08SeparatingAir;
    r08ResidualRejectedAir.velocity.x = static_cast<float>(
        profile.solver.residualToleranceCmS * 1.01);
    r08ResidualRejectedAir.speed = std::fabs(
        r08ResidualRejectedAir.velocity.x);
    BallState r08ResidualRejectedCloth = r08SeparatingCloth;
    r08ResidualRejectedCloth.velocity.x = 0.0f;
    r08ResidualRejectedCloth.speed = 0.0f;
    GameState r08ResidualRejectedState = isolatedState(
        r08ResidualRejectedAir);
    r08ResidualRejectedState.balls[1] = r08ResidualRejectedCloth;
    r08ResidualRejectedState.balls[1].pocketed = false;
    const GameState r08ResidualRejectedBefore = r08ResidualRejectedState;
    const TableDepartureStepResult r08ResidualRejectedResult =
        advanceTableDeparturePhysics(r08ResidualRejectedState, 0.000001f,
            profile, PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(!r08ResidualRejectedResult.ok &&
        r08ResidualRejectedResult.failureCode ==
            TableDepartureFailureCode::AmbiguousInitialPenetration &&
        r08ResidualRejectedResult.events.empty() &&
        sameTransactionalState(r08ResidualRejectedState,
            r08ResidualRejectedBefore),
        "R08 t0 approach at 1.01 residual must fail with exact rollback");

    BallState r08ApproachingAir = r08SeparatingAir;
    r08ApproachingAir.velocity.x = 10.0f;
    BallState r08ApproachingCloth = r08SeparatingCloth;
    r08ApproachingCloth.velocity.x = -10.0f;
    GameState r08ApproachingState = isolatedState(r08ApproachingAir);
    r08ApproachingState.balls[1] = r08ApproachingCloth;
    r08ApproachingState.balls[1].pocketed = false;
    const GameState r08ApproachingBefore = r08ApproachingState;
    const TableDepartureStepResult r08ApproachingResult =
        advanceTableDeparturePhysics(r08ApproachingState, 0.01f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(!r08ApproachingResult.ok &&
        r08ApproachingResult.failureCode ==
            TableDepartureFailureCode::AmbiguousInitialPenetration &&
        r08ApproachingResult.events.empty() &&
        sameTransactionalState(r08ApproachingState, r08ApproachingBefore),
        "R08 t0 still-approaching pair must fail with exact whole-state rollback");

    PhysicsProfile crossProfile = profile;
    crossProfile.surface.rollingResistanceAccelerationCmS2 = 0.0f;
    crossProfile.ball.frictionCoefficient = 0.24f;
    const double crossRadiusSum = 2.0 * crossProfile.ball.radiusCm;
    const double exactTargetTime = 0.05;
    const double x01ContactDy = -3.0;
    const double x01ContactDz = 2.0;
    const double x01ContactDx = std::sqrt(crossRadiusSum * crossRadiusSum -
        x01ContactDy * x01ContactDy - x01ContactDz * x01ContactDz);
    BallState crossAir = ballAt(0.0f, static_cast<float>(
            geometry.clothSupportedCenterYCm - x01ContactDy -
            0.5 * geometry.gravityYCmS2 * exactTargetTime * exactTargetTime),
        0.0f, 100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState crossCloth = ballAt(static_cast<float>(
            100.0 * exactTargetTime + x01ContactDx),
        geometry.clothSupportedCenterYCm,
        static_cast<float>(x01ContactDz),
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    const IndependentSweptSphereRoot x01Root =
        independentEarliestSweptSphereRoot(crossAir, crossCloth, 0.1,
            geometry.gravityYCmS2, crossRadiusSum);
    const IndependentPolynomial x01Polynomial =
        independentSeparationPolynomial(crossAir, crossCloth,
            geometry.gravityYCmS2, crossRadiusSum);
    const std::vector<double> x01Roots = isolateIndependentPolynomialRoots(
        x01Polynomial, 0.0, 0.1);
    const double x01ClothRoot = independentEarliestDescendingPlaneRoot(
        crossAir, geometry.clothSupportedCenterYCm, 0.1,
        geometry.gravityYCmS2);
    expectIndependentFirstApproachingRoot(x01Root, crossAir, crossCloth,
        geometry.gravityYCmS2, crossRadiusSum, exactTargetTime,
        crossProfile.solver.toiToleranceSeconds,
        "X01 fixture must independently prove its earliest 3D root");
    expect(evaluateIndependentPolynomial(x01Polynomial, 0.0) > 0.0 &&
        std::count_if(x01Roots.begin(), x01Roots.end(), [&](double root) {
            return root <= exactTargetTime +
                crossProfile.solver.toiToleranceSeconds;
        }) == 1 && std::isfinite(x01ClothRoot) &&
        exactTargetTime + crossProfile.solver.toiToleranceSeconds <
            x01ClothRoot &&
        crossAir.position.y >= geometry.clothSupportedCenterYCm &&
        std::fabs(crossAir.position.x) <=
            geometry.playfieldHalfWidthCm + geometry.ballRadiusCm &&
        std::fabs(crossAir.position.z) <=
            geometry.playfieldHalfLengthCm + geometry.ballRadiusCm,
        "X01 3D contact must be nonpenetrating and precede every cloth-table candidate");
    const double x01Horizon = exactTargetTime +
        0.5 * (x01ClothRoot - exactTargetTime);
    GameState crossState = isolatedState(crossAir);
    crossState.balls[1] = crossCloth;
    crossState.balls[1].pocketed = false;
    const TableDepartureStepResult crossResult =
        advanceTableDeparturePhysics(crossState,
            static_cast<float>(x01Horizon), crossProfile,
            PhysicsBoundaryMode::ProductionTable,
            crossProfile.solver.maximumEventsPerTick);
    const TableDepartureEvent* clothContact =
        crossDomainContact(crossResult, 0, 1);
    expect(crossResult.ok && clothContact != nullptr &&
        close(clothContact->absoluteWholeTickTimeSeconds,
            x01Root.timeSeconds,
            crossProfile.solver.toiToleranceSeconds) &&
        crossState.balls[0].velocity.x < 100.0f &&
        crossState.balls[1].velocity.x > 0.0f &&
        crossState.balls[0].physicalDomain == PhysicalBallDomain::Airborne &&
        crossState.balls[1].physicalDomain ==
            PhysicalBallDomain::ClothSupported,
        "X01 airborne-to-cloth contact must resolve at exact swept TOI");

    const float exactTargetInitialHeightOffset = static_cast<float>(
        -0.5 * geometry.gravityYCmS2 * exactTargetTime * exactTargetTime);
    const float x02SupportedX = geometry.playfieldHalfWidthCm +
        geometry.ballRadiusCm + 1.0f;
    BallState railAir = ballAt(static_cast<float>(x02SupportedX -
            crossRadiusSum - 100.0 * exactTargetTime),
        geometry.railTopSupportedCenterYCm + exactTargetInitialHeightOffset,
        30.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState railSupported = ballAt(x02SupportedX,
        geometry.railTopSupportedCenterYCm, 30.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::RailTopSupported);
    const IndependentSweptSphereRoot x02Root =
        independentEarliestSweptSphereRoot(railAir, railSupported, 0.08,
            geometry.gravityYCmS2, crossRadiusSum);
    expectIndependentFirstApproachingRoot(x02Root, railAir, railSupported,
        geometry.gravityYCmS2, crossRadiusSum, exactTargetTime,
        crossProfile.solver.toiToleranceSeconds,
        "X02 fixture must independently prove its earliest 3D root");
    GameState railCrossState = isolatedState(railAir);
    railCrossState.balls[1] = railSupported;
    railCrossState.balls[1].pocketed = false;
    const TableDepartureStepResult railCrossResult =
        advanceTableDeparturePhysics(railCrossState, 0.08f, crossProfile,
            PhysicsBoundaryMode::ProductionTable,
            crossProfile.solver.maximumEventsPerTick);
    const TableDepartureEvent* railContact =
        crossDomainContact(railCrossResult, 0, 1);
    expect(railCrossResult.ok && railContact != nullptr &&
        close(railContact->absoluteWholeTickTimeSeconds,
            x02Root.timeSeconds,
            crossProfile.solver.toiToleranceSeconds) &&
        railCrossState.balls[0].physicalDomain == PhysicalBallDomain::Airborne &&
        railCrossState.balls[1].physicalDomain ==
            PhysicalBallDomain::RailTopSupported,
        "X02 airborne-to-rail-top contact must retain domains and exact TOI");

    const double x03ContactDy = -3.0;
    const double x03ContactDz = 2.0;
    const double x03ContactDx = std::sqrt(crossRadiusSum * crossRadiusSum -
        x03ContactDy * x03ContactDy - x03ContactDz * x03ContactDz);
    BallState outsideAir = ballAt(90.0f, static_cast<float>(
            geometry.outsideLandingSupportedCenterYCm - x03ContactDy -
            0.5 * geometry.gravityYCmS2 * exactTargetTime * exactTargetTime),
        30.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState outsideSupported = ballAt(static_cast<float>(90.0 +
            100.0 * exactTargetTime + x03ContactDx),
        geometry.outsideLandingSupportedCenterYCm,
        static_cast<float>(30.0 + x03ContactDz),
        0.0f, 0.0f, 0.0f,
        PhysicalBallDomain::OutsideLandingSupported);
    expect(independentSeparationFunction(outsideAir, outsideSupported, 0.0,
            geometry.gravityYCmS2, crossRadiusSum) > 0.0,
        "X03 fixture must begin without penetration");
    const IndependentSweptSphereRoot x03Root =
        independentEarliestSweptSphereRoot(outsideAir, outsideSupported,
            0.08, geometry.gravityYCmS2, crossRadiusSum);
    expectIndependentFirstApproachingRoot(x03Root, outsideAir,
        outsideSupported, geometry.gravityYCmS2, crossRadiusSum,
        exactTargetTime, crossProfile.solver.toiToleranceSeconds,
        "X03 fixture must independently prove its earliest 3D root");
    const double x03LandingRoot = independentEarliestDescendingPlaneRoot(
        outsideAir, geometry.outsideLandingSupportedCenterYCm, 0.2,
        geometry.gravityYCmS2);
    expect(std::isfinite(x03LandingRoot) &&
        x03Root.timeSeconds + crossProfile.solver.toiToleranceSeconds <
            x03LandingRoot,
        "X03 cross-domain root must strictly precede outside landing");
    GameState outsideCrossState = isolatedState(outsideAir);
    outsideCrossState.balls[1] = outsideSupported;
    outsideCrossState.balls[1].pocketed = false;
    const TableDepartureStepResult outsideCrossResult =
        advanceTableDeparturePhysics(outsideCrossState, 0.08f, crossProfile,
            PhysicsBoundaryMode::ProductionTable,
            crossProfile.solver.maximumEventsPerTick);
    const TableDepartureEvent* outsideContact =
        crossDomainContact(outsideCrossResult, 0, 1);
    expect(outsideCrossResult.ok && outsideContact != nullptr &&
        close(outsideContact->absoluteWholeTickTimeSeconds,
            x03Root.timeSeconds,
            crossProfile.solver.toiToleranceSeconds) &&
        outsideCrossState.balls[1].physicalDomain ==
            PhysicalBallDomain::OutsideLandingSupported,
        "X03 airborne-to-outside-supported contact must retain exact TOI");

    const double x04TargetTime = 0.045;
    const double x04TargetDy = -2.0;
    const double x04TargetDz = 2.5;
    const double x04TargetDx = std::sqrt(crossRadiusSum * crossRadiusSum -
        x04TargetDy * x04TargetDy - x04TargetDz * x04TargetDz);
    BallState obliqueAir = ballAt(0.0f, static_cast<float>(
            geometry.clothSupportedCenterYCm - x04TargetDy -
            0.5 * geometry.gravityYCmS2 * x04TargetTime * x04TargetTime),
        0.0f, 100.0f, 0.0f, 20.0f, PhysicalBallDomain::Airborne);
    obliqueAir.angularVelocity = {1.0f, 2.0f, 3.0f};
    BallState obliqueCloth = ballAt(static_cast<float>(
            100.0 * x04TargetTime + x04TargetDx),
        geometry.clothSupportedCenterYCm,
        static_cast<float>(20.0 * x04TargetTime + x04TargetDz),
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    const IndependentSweptSphereRoot x04Root =
        independentEarliestSweptSphereRoot(obliqueAir, obliqueCloth, 0.1,
            geometry.gravityYCmS2, crossRadiusSum);
    expectIndependentFirstApproachingRoot(x04Root, obliqueAir, obliqueCloth,
        geometry.gravityYCmS2, crossRadiusSum, x04TargetTime,
        crossProfile.solver.toiToleranceSeconds,
        "X04 fixture must independently prove a nontrivial 3D root");
    BallState x04ExpectedAir = obliqueAir;
    BallState x04ExpectedCloth = obliqueCloth;
    advanceIndependentToContact(x04ExpectedAir, x04Root.timeSeconds,
        geometry.gravityYCmS2);
    advanceIndependentToContact(x04ExpectedCloth, x04Root.timeSeconds,
        geometry.gravityYCmS2);
    const BallState x04RawAirAtContact = x04ExpectedAir;
    const BallState x04RawClothAtContact = x04ExpectedCloth;
    std::array<double, 3> x04SavedNormal{};
    double x04ProjectionExcess = 0.0;
    expect(projectIndependentAnalyticContact(x04ExpectedAir,
            x04ExpectedCloth, crossRadiusSum, x04SavedNormal,
            x04ProjectionExcess),
        "X04 independent float contact must require only bounded analytic projection");
    const double x04Displacement[3] = {
        x04ExpectedAir.position.x - x04RawAirAtContact.position.x,
        x04ExpectedAir.position.y - x04RawAirAtContact.position.y,
        x04ExpectedAir.position.z - x04RawAirAtContact.position.z};
    const double x04DisplacementAlongNormal =
        x04Displacement[0] * x04SavedNormal[0] +
        x04Displacement[1] * x04SavedNormal[1] +
        x04Displacement[2] * x04SavedNormal[2];
    const double x04TangentialDisplacementSquared = std::max(0.0,
        x04Displacement[0] * x04Displacement[0] +
        x04Displacement[1] * x04Displacement[1] +
        x04Displacement[2] * x04Displacement[2] -
        x04DisplacementAlongNormal * x04DisplacementAlongNormal);
    const double x04ProjectedSeparation[3] = {
        x04ExpectedCloth.position.x - x04ExpectedAir.position.x,
        x04ExpectedCloth.position.y - x04ExpectedAir.position.y,
        x04ExpectedCloth.position.z - x04ExpectedAir.position.z};
    const double x04ProjectedDistance = std::sqrt(
        x04ProjectedSeparation[0] * x04ProjectedSeparation[0] +
        x04ProjectedSeparation[1] * x04ProjectedSeparation[1] +
        x04ProjectedSeparation[2] * x04ProjectedSeparation[2]);
    double x04ProjectedNormalAgreement = 0.0;
    for (int axis = 0; axis < 3; ++axis)
        x04ProjectedNormalAgreement += x04ProjectedSeparation[axis] /
            x04ProjectedDistance * x04SavedNormal[axis];
    expect(std::isfinite(x04ProjectionExcess) &&
        x04ProjectionExcess > 0.0 && x04ProjectionExcess <= 1e-4 &&
        close(x04DisplacementAlongNormal, x04ProjectionExcess, 1e-6) &&
        x04TangentialDisplacementSquared <= 1e-12 &&
        close(x04ProjectedDistance, crossRadiusSum, 1e-5) &&
        x04ProjectedDistance >= crossRadiusSum - 1e-5 &&
        close(x04ProjectedNormalAgreement, 1.0, 1e-7),
        "X04 projection must be normal-only, exact-radius and nonpenetrating");
    BallState x04RejectedProjectionAir = x04RawAirAtContact;
    x04RejectedProjectionAir.position.x -= static_cast<float>(
        x04SavedNormal[0] * 2e-4);
    x04RejectedProjectionAir.position.y -= static_cast<float>(
        x04SavedNormal[1] * 2e-4);
    x04RejectedProjectionAir.position.z -= static_cast<float>(
        x04SavedNormal[2] * 2e-4);
    const BallState x04RejectedBefore = x04RejectedProjectionAir;
    std::array<double, 3> x04RejectedNormal{};
    double x04RejectedExcess = 0.0;
    expect(!projectIndependentAnalyticContact(x04RejectedProjectionAir,
            x04RawClothAtContact, crossRadiusSum, x04RejectedNormal,
            x04RejectedExcess) &&
        sameTransactionalBall(x04RejectedProjectionAir, x04RejectedBefore),
        "X04 projection beyond 1e-4 must reject without invoking resolver or mutation");
    const BallState x04AirBeforeImpulse = x04ExpectedAir;
    const BallState x04ClothBeforeImpulse = x04ExpectedCloth;
    const BallBallContactResult x04ExpectedContact = resolveBallBallContact(
        x04ExpectedAir, x04ExpectedCloth, crossProfile.ball,
        crossProfile.ball);
    const BallState x04AirAfterImpulse = x04ExpectedAir;
    const BallState x04ClothAfterImpulse = x04ExpectedCloth;
    const double x04Tail = 0.1 - x04Root.timeSeconds;
    advanceIndependentDomainTail(x04ExpectedAir, x04Tail,
        geometry.gravityYCmS2,
        crossProfile.tableDeparture.supportedRollingDecelerationCmS2,
        crossProfile.surface.torsionalSpinDecelerationRadS2);
    advanceIndependentDomainTail(x04ExpectedCloth, x04Tail,
        geometry.gravityYCmS2,
        crossProfile.tableDeparture.supportedRollingDecelerationCmS2,
        crossProfile.surface.torsionalSpinDecelerationRadS2);
    const double x04Mass = crossProfile.ball.massKg;
    const double impulseFromFirst[3] = {
        (x04AirAfterImpulse.velocity.x - x04AirBeforeImpulse.velocity.x) *
            x04Mass / 100.0,
        (x04AirAfterImpulse.velocity.y - x04AirBeforeImpulse.velocity.y) *
            x04Mass / 100.0,
        (x04AirAfterImpulse.velocity.z - x04AirBeforeImpulse.velocity.z) *
            x04Mass / 100.0,
    };
    const double impulseFromSecond[3] = {
        (x04ClothAfterImpulse.velocity.x - x04ClothBeforeImpulse.velocity.x) *
            x04Mass / 100.0,
        (x04ClothAfterImpulse.velocity.y - x04ClothBeforeImpulse.velocity.y) *
            x04Mass / 100.0,
        (x04ClothAfterImpulse.velocity.z - x04ClothBeforeImpulse.velocity.z) *
            x04Mass / 100.0,
    };
    double rebuiltNormalImpulse = 0.0;
    double rebuiltImpulseMagnitudeSquared = 0.0;
    double momentumResidualSquared = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
        rebuiltNormalImpulse += impulseFromSecond[axis] *
            x04ExpectedContact.contactNormal[axis];
        rebuiltImpulseMagnitudeSquared +=
            impulseFromSecond[axis] * impulseFromSecond[axis];
        momentumResidualSquared += (impulseFromFirst[axis] +
            impulseFromSecond[axis]) * (impulseFromFirst[axis] +
            impulseFromSecond[axis]);
    }
    const double rebuiltTangentialImpulse = std::sqrt(std::max(0.0,
        rebuiltImpulseMagnitudeSquared -
        rebuiltNormalImpulse * rebuiltNormalImpulse));
    GameState obliqueState = isolatedState(obliqueAir);
    obliqueState.balls[1] = obliqueCloth;
    obliqueState.balls[1].pocketed = false;
    const TableDepartureStepResult obliqueResult =
        advanceTableDeparturePhysics(obliqueState, 0.1f, crossProfile,
            PhysicsBoundaryMode::ProductionTable,
            crossProfile.solver.maximumEventsPerTick);
    const TableDepartureEvent* obliqueContact =
        crossDomainContact(obliqueResult, 0, 1);
    expect(obliqueResult.ok && obliqueContact != nullptr &&
        close(obliqueContact->absoluteWholeTickTimeSeconds,
            x04Root.timeSeconds,
            crossProfile.solver.toiToleranceSeconds) &&
        x04ExpectedContact.velocityImpulseApplied &&
        x04ExpectedContact.tangentialImpulseNs > 0.0 &&
        close(rebuiltNormalImpulse,
            x04ExpectedContact.normalImpulseNs, 1e-8) &&
        close(rebuiltTangentialImpulse,
            x04ExpectedContact.tangentialImpulseNs, 1e-8) &&
        momentumResidualSquared <= 1e-16 &&
        x04ExpectedContact.kineticEnergyAfterJ <=
            x04ExpectedContact.kineticEnergyBeforeJ +
            crossProfile.solver.passiveEnergyToleranceJ &&
        finiteIndependentBall(x04ExpectedAir) &&
        finiteIndependentBall(x04ExpectedCloth) &&
        close(obliqueState.balls[0].velocity.x,
            x04ExpectedAir.velocity.x, 1e-3) &&
        close(obliqueState.balls[0].velocity.y,
            x04ExpectedAir.velocity.y, 1e-3) &&
        close(obliqueState.balls[0].velocity.z,
            x04ExpectedAir.velocity.z, 1e-3) &&
        close(obliqueState.balls[1].velocity.x,
            x04ExpectedCloth.velocity.x, 1e-3) &&
        close(obliqueState.balls[1].velocity.y,
            x04ExpectedCloth.velocity.y, 1e-3) &&
        close(obliqueState.balls[1].velocity.z,
            x04ExpectedCloth.velocity.z, 1e-3) &&
        close(obliqueState.balls[0].angularVelocity.x,
            x04ExpectedAir.angularVelocity.x, 1e-4) &&
        close(obliqueState.balls[0].angularVelocity.y,
            x04ExpectedAir.angularVelocity.y, 1e-4) &&
        close(obliqueState.balls[0].angularVelocity.z,
            x04ExpectedAir.angularVelocity.z, 1e-4) &&
        close(obliqueState.balls[1].angularVelocity.x,
            x04ExpectedCloth.angularVelocity.x, 1e-4) &&
        close(obliqueState.balls[1].angularVelocity.y,
            x04ExpectedCloth.angularVelocity.y, 1e-4) &&
        close(obliqueState.balls[1].angularVelocity.z,
            x04ExpectedCloth.angularVelocity.z, 1e-4) &&
        close(obliqueState.balls[0].position.x,
            x04ExpectedAir.position.x, 1e-3) &&
        close(obliqueState.balls[0].position.y,
            x04ExpectedAir.position.y, 1e-3) &&
        close(obliqueState.balls[0].position.z,
            x04ExpectedAir.position.z, 1e-3) &&
        close(obliqueState.balls[1].position.x,
            x04ExpectedCloth.position.x, 1e-3) &&
        close(obliqueState.balls[1].position.y,
            x04ExpectedCloth.position.y, 1e-3) &&
        close(obliqueState.balls[1].position.z,
            x04ExpectedCloth.position.z, 1e-3),
        "X04 oblique cross-domain contact must reuse rigid impulse and preserve spin");

    BallState missAir = ballAt(0.0f, 110.0f, 0.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState missCloth = ballAt(10.715f,
        geometry.clothSupportedCenterYCm, 20.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    GameState missState = isolatedState(missAir);
    missState.balls[1] = missCloth;
    missState.balls[1].pocketed = false;
    const TableDepartureStepResult missResult =
        advanceTableDeparturePhysics(missState, 0.1f, crossProfile,
            PhysicsBoundaryMode::ProductionTable,
            crossProfile.solver.maximumEventsPerTick);
    expect(missResult.ok && crossDomainContact(missResult, 0, 1) == nullptr &&
        missState.balls[1].velocity.x == 0.0f,
        "X07 separated cross-domain paths must not produce contact");

    const double x05TargetTime = 0.04;
    const double x05ContactDy = -3.0;
    const double x05ContactDz = 2.0;
    const double x05ContactDx = std::sqrt(
        crossRadiusSum * crossRadiusSum -
        x05ContactDy * x05ContactDy - x05ContactDz * x05ContactDz);
    BallState contactThenLandAir = ballAt(90.0f, static_cast<float>(
            geometry.outsideLandingSupportedCenterYCm - x05ContactDy -
            0.5 * geometry.gravityYCmS2 * x05TargetTime * x05TargetTime),
        30.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState contactThenLandSupported = ballAt(static_cast<float>(90.0 +
            100.0 * x05TargetTime + x05ContactDx),
        geometry.outsideLandingSupportedCenterYCm,
        static_cast<float>(30.0 + x05ContactDz),
        0.0f, 0.0f, 0.0f,
        PhysicalBallDomain::OutsideLandingSupported);
    const IndependentSweptSphereRoot x05Root =
        independentEarliestSweptSphereRoot(contactThenLandAir,
            contactThenLandSupported, 0.2, geometry.gravityYCmS2,
            crossRadiusSum);
    expectIndependentFirstApproachingRoot(x05Root, contactThenLandAir,
        contactThenLandSupported, geometry.gravityYCmS2, crossRadiusSum,
        x05TargetTime, crossProfile.solver.toiToleranceSeconds,
        "X05 fixture must independently prove its earliest 3D root");
    const double x05IndependentLandingTime =
        independentEarliestDescendingPlaneRoot(contactThenLandAir,
            geometry.outsideLandingSupportedCenterYCm, 0.2,
            geometry.gravityYCmS2);
    expect(std::isfinite(x05IndependentLandingTime) &&
        x05Root.timeSeconds + crossProfile.solver.toiToleranceSeconds <
            x05IndependentLandingTime,
        "X05 independent contact root must precede a distinct landing root");
    GameState contactThenLandWhole = isolatedState(contactThenLandAir);
    contactThenLandWhole.balls[1] = contactThenLandSupported;
    contactThenLandWhole.balls[1].pocketed = false;
    GameState contactThenLandSplit = contactThenLandWhole;
    const TableDepartureStepResult contactThenLandWholeResult =
        advanceTableDeparturePhysics(contactThenLandWhole, 0.2f,
            crossProfile, PhysicsBoundaryMode::ProductionTable,
            crossProfile.solver.maximumEventsPerTick);
    const TableDepartureEvent* contactThenLandContact =
        crossDomainContact(contactThenLandWholeResult, 0, 1);
    expect(contactThenLandWholeResult.ok && contactThenLandContact != nullptr,
        "X05 requires a real cross-domain contact before landing");
    BallState x05ExpectedAirAtContact = contactThenLandAir;
    BallState x05ExpectedSupportedAtContact = contactThenLandSupported;
    advanceIndependentToContact(x05ExpectedAirAtContact,
        x05Root.timeSeconds, geometry.gravityYCmS2);
    advanceIndependentToContact(x05ExpectedSupportedAtContact,
        x05Root.timeSeconds, geometry.gravityYCmS2);
    const IndependentCanonicalContact x05Canonical =
        canonicalizeIndependentContact(x05ExpectedAirAtContact,
            x05ExpectedSupportedAtContact, crossRadiusSum);
    const BallBallContactResult x05ExpectedContact = resolveBallBallContact(
        x05ExpectedAirAtContact, x05ExpectedSupportedAtContact,
        crossProfile.ball, crossProfile.ball);
    expect(x05Canonical.ok && x05ExpectedContact.velocityImpulseApplied,
        "X05 independent post-contact state must use the frozen canonical resolver path");
    if (x05ExpectedSupportedAtContact.velocity.y > 0.0f) {
        x05ExpectedSupportedAtContact.physicalDomain =
            PhysicalBallDomain::Airborne;
    } else if (x05ExpectedSupportedAtContact.velocity.y < 0.0f) {
        x05ExpectedSupportedAtContact.velocity.y = 0.0f;
        x05ExpectedSupportedAtContact.position.y =
            geometry.outsideLandingSupportedCenterYCm;
    }
    x05ExpectedSupportedAtContact.speed = static_cast<float>(std::sqrt(
        x05ExpectedSupportedAtContact.velocity.x *
            x05ExpectedSupportedAtContact.velocity.x +
        x05ExpectedSupportedAtContact.velocity.y *
            x05ExpectedSupportedAtContact.velocity.y +
        x05ExpectedSupportedAtContact.velocity.z *
            x05ExpectedSupportedAtContact.velocity.z));
    const double x05PostContactLandingTail =
        independentEarliestDescendingPlaneRoot(x05ExpectedAirAtContact,
            geometry.outsideLandingSupportedCenterYCm,
            0.2 - x05Root.timeSeconds, geometry.gravityYCmS2);
    const double x05PostContactLandingAbsolute =
        x05Root.timeSeconds + x05PostContactLandingTail;
    const std::array<double, 3> x05LandingPoint = independentPositionAt(
        x05ExpectedAirAtContact, x05PostContactLandingTail,
        geometry.gravityYCmS2);
    expect(std::isfinite(x05PostContactLandingTail) &&
        x05Root.timeSeconds + crossProfile.solver.toiToleranceSeconds <
            x05PostContactLandingAbsolute &&
        std::fabs(x05LandingPoint[0]) > geometry.outerHalfWidthCm &&
        independentVelocityAt(x05ExpectedAirAtContact,
            x05PostContactLandingTail, geometry.gravityYCmS2)[1] < 0.0,
        "X05 post-contact oracle must prove a later outside landing");
    const float x05SplitTimeFloat = static_cast<float>(0.5 *
        (x05Root.timeSeconds + x05PostContactLandingAbsolute));
    const double x05SplitTime = static_cast<double>(x05SplitTimeFloat);
    const double x05SplitUlp = floatUlp(x05SplitTimeFloat);
    const double x05RequiredMargin = std::max(
        4.0 * crossProfile.solver.toiToleranceSeconds,
        8.0 * x05SplitUlp);
    expect(x05SplitTime - x05Root.timeSeconds > x05RequiredMargin &&
        x05PostContactLandingAbsolute - x05SplitTime > x05RequiredMargin,
        "X05 float-quantized split must retain sealed margins between both independent roots");
    GameState x05LeftControl = isolatedState(contactThenLandAir);
    x05LeftControl.balls[1] = contactThenLandSupported;
    x05LeftControl.balls[1].pocketed = false;
    const float x05LeftDuration = std::nextafter(
        static_cast<float>(x05Root.timeSeconds),
        -std::numeric_limits<float>::infinity());
    const TableDepartureStepResult x05LeftResult =
        advanceTableDeparturePhysics(x05LeftControl, x05LeftDuration,
            crossProfile, PhysicsBoundaryMode::ProductionTable,
            crossProfile.solver.maximumEventsPerTick);
    expect(x05LeftResult.ok && crossDomainContact(x05LeftResult, 0, 1) ==
            nullptr,
        "X05 duration immediately left of the root must not publish contact");
    const TableDepartureStepResult contactThenLandPrefix =
        advanceTableDeparturePhysics(contactThenLandSplit,
            x05SplitTimeFloat, crossProfile,
            PhysicsBoundaryMode::ProductionTable,
            crossProfile.solver.maximumEventsPerTick);
    GameState x05ExpectedPrefix = isolatedState(x05ExpectedAirAtContact);
    x05ExpectedPrefix.balls[1] = x05ExpectedSupportedAtContact;
    x05ExpectedPrefix.balls[1].pocketed = false;
    advanceIndependentDomainTail(x05ExpectedPrefix.balls[0],
        x05SplitTime - x05Root.timeSeconds, geometry.gravityYCmS2,
        crossProfile.tableDeparture.supportedRollingDecelerationCmS2,
        crossProfile.surface.torsionalSpinDecelerationRadS2);
    advanceIndependentDomainTail(x05ExpectedPrefix.balls[1],
        x05SplitTime - x05Root.timeSeconds, geometry.gravityYCmS2,
        crossProfile.tableDeparture.supportedRollingDecelerationCmS2,
        crossProfile.surface.torsionalSpinDecelerationRadS2);
    expect(contactThenLandPrefix.ok &&
        contactThenLandPrefix.events.size() == 1 &&
        contactThenLandPrefix.events[0].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        x05SemanticState(contactThenLandSplit, x05ExpectedPrefix,
            crossProfile, x05SplitTime),
        "X05 chosen split prefix must contain exact contact and match independent state");
    const float x05TailDuration = static_cast<float>(0.2 - x05SplitTime);
    const TableDepartureStepResult contactThenLandTail =
        advanceTableDeparturePhysics(contactThenLandSplit,
            x05TailDuration, crossProfile,
            PhysicsBoundaryMode::ProductionTable,
            crossProfile.solver.maximumEventsPerTick);
    expect(contactThenLandTail.ok && contactThenLandTail.events.size() == 1 &&
        contactThenLandTail.events[0].kind ==
            TableDepartureEventKind::OutsideLanding &&
        close(contactThenLandTail.events[0].absoluteWholeTickTimeSeconds,
            x05PostContactLandingAbsolute - x05SplitTime,
            crossProfile.solver.toiToleranceSeconds),
        "X05 tail must contain only the independently timed outside landing");
    std::vector<TableDepartureEvent> splitEvents = contactThenLandPrefix.events;
    for (const TableDepartureEvent& local : contactThenLandTail.events) {
        TableDepartureEvent rebased = local;
        rebased.ordinal = static_cast<int>(splitEvents.size());
        rebased.absoluteWholeTickTimeSeconds += x05SplitTime;
        rebased.stableIdentity = rebaseStableIdentity(local,
            rebased.ordinal, rebased.absoluteWholeTickTimeSeconds);
        splitEvents.push_back(rebased);
    }
    int x05ContactCount = 0;
    int x05LandingCount = 0;
    double x05LandingTime = -1.0;
    std::vector<std::string> x05Identities;
    for (const TableDepartureEvent& event : contactThenLandWholeResult.events) {
        x05Identities.push_back(event.stableIdentity);
        if (event.stableIdentity.find("cross_domain_ball_contact:0:1") !=
                std::string::npos)
            ++x05ContactCount;
        if (event.kind == TableDepartureEventKind::OutsideLanding ||
            event.kind == TableDepartureEventKind::RailTopLanding ||
            event.kind == TableDepartureEventKind::ClothReturn) {
            ++x05LandingCount;
            x05LandingTime = event.absoluteWholeTickTimeSeconds;
        }
    }
    std::sort(x05Identities.begin(), x05Identities.end());
    bool x05EventChainsEqual = splitEvents.size() ==
        contactThenLandWholeResult.events.size();
    for (std::size_t index = 0;
            x05EventChainsEqual && index < splitEvents.size(); ++index) {
        x05EventChainsEqual = sameEventSemantic(splitEvents[index],
            contactThenLandWholeResult.events[index],
            crossProfile.solver.toiToleranceSeconds);
    }
    expect(x05SemanticState(contactThenLandWhole, contactThenLandSplit,
            crossProfile, 0.2),
        "X05 whole/split states must remain within sealed semantic tolerances");
    GameState x05Mutation = contactThenLandWhole;
    x05Mutation.balls[0].position.x +=
        crossProfile.solver.penetrationSlopCm * 1.01f;
    expect(!x05SemanticState(contactThenLandWhole, x05Mutation,
            crossProfile, 0.2),
        "X05 comparator must reject above-slop position drift");
    x05Mutation = contactThenLandWhole;
    x05Mutation.balls[0].velocity.x +=
        crossProfile.solver.residualToleranceCmS * 1.01f;
    expect(!x05SemanticState(contactThenLandWhole, x05Mutation,
            crossProfile, 0.2),
        "X05 comparator must reject above-residual velocity drift");
    x05Mutation = contactThenLandWhole;
    x05Mutation.balls[0].speed +=
        crossProfile.solver.residualToleranceCmS * 1.01f;
    expect(!x05SemanticState(contactThenLandWhole, x05Mutation,
            crossProfile, 0.2),
        "X05 comparator must reject above-residual speed drift");
    x05Mutation = contactThenLandWhole;
    x05Mutation.balls[0].angularVelocity.x +=
        crossProfile.solver.residualToleranceCmS /
            crossProfile.ball.radiusCm * 1.01f;
    expect(!x05SemanticState(contactThenLandWhole, x05Mutation,
            crossProfile, 0.2),
        "X05 comparator must reject above-radius-scaled angular drift");
    x05Mutation = contactThenLandWhole;
    x05Mutation.balls[0].rotationAngle += static_cast<float>(
        crossProfile.solver.residualToleranceCmS /
        crossProfile.ball.radiusCm * 0.2 * 180.0 /
        3.14159265358979323846 +
        4.0 * floatUlp(contactThenLandWhole.balls[0].rotationAngle));
    expect(!x05SemanticState(contactThenLandWhole, x05Mutation,
            crossProfile, 0.2),
        "X05 comparator must reject above-bound rotation-angle drift");
    x05Mutation = contactThenLandWhole;
    x05Mutation.balls[0].rotationAxis = {1.0f, 0.0f, 0.0f};
    expect(!x05SemanticState(contactThenLandWhole, x05Mutation,
            crossProfile, 0.2),
        "X05 comparator must reject zero/nonzero rotation-axis mismatch");
    x05Mutation = contactThenLandWhole;
    x05Mutation.balls[0].physicalDomain = PhysicalBallDomain::PocketChannel;
    expect(!x05SemanticState(contactThenLandWhole, x05Mutation,
            crossProfile, 0.2),
        "X05 comparator must reject discrete domain flips");
    x05Mutation = contactThenLandWhole;
    x05Mutation.balls[0].motionState = static_cast<BallMotionState>(
        static_cast<int>(x05Mutation.balls[0].motionState) + 1);
    expect(!x05SemanticState(contactThenLandWhole, x05Mutation,
            crossProfile, 0.2),
        "X05 comparator must reject discrete motion-state flips");
    x05Mutation = contactThenLandWhole;
    x05Mutation.balls[0].pocketInteraction.pocketId += 1;
    expect(!x05SemanticState(contactThenLandWhole, x05Mutation,
            crossProfile, 0.2),
        "X05 comparator must reject discrete pocket-state flips");
    x05Mutation = contactThenLandWhole;
    ++x05Mutation.nextPocketCaptureSequence;
    expect(!x05SemanticState(contactThenLandWhole, x05Mutation,
            crossProfile, 0.2),
        "X05 comparator must reject discrete counter flips");
    x05Mutation = contactThenLandWhole;
    x05Mutation.ballsMoving = !x05Mutation.ballsMoving;
    expect(!x05SemanticState(contactThenLandWhole, x05Mutation,
            crossProfile, 0.2),
        "X05 comparator must reject balls-moving flips");
    expect(contactThenLandPrefix.ok && contactThenLandTail.ok &&
        x05ContactCount == 1 && x05LandingCount == 1 &&
        close(contactThenLandContact->absoluteWholeTickTimeSeconds,
            x05Root.timeSeconds,
            crossProfile.solver.toiToleranceSeconds) &&
        contactThenLandContact->absoluteWholeTickTimeSeconds < x05LandingTime &&
        std::adjacent_find(x05Identities.begin(), x05Identities.end()) ==
            x05Identities.end() &&
        x05EventChainsEqual &&
        x05SemanticState(contactThenLandWhole, contactThenLandSplit,
            crossProfile, 0.2),
        "X05 contact then landing must retain order, identity, dedupe and whole/split equivalence");

    PhysicsProfile sameToiProfile = crossProfile;
    const double x06CushionToi =
        independentEarliestPositivePlaneRoot(55.0, 100.0,
            geometry.playfieldHalfWidthCm - geometry.ballRadiusCm, 0.1);
    expect(std::isfinite(x06CushionToi),
        "X06 fixture must independently prove its earliest cushion root");
    BallState sameToiAir = ballAt(55.0f,
        static_cast<float>(geometry.clothSupportedCenterYCm -
            0.5 * geometry.gravityYCmS2 * x06CushionToi * x06CushionToi),
        30.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState sameToiCloth = ballAt(
        geometry.playfieldHalfWidthCm + geometry.ballRadiusCm,
        geometry.clothSupportedCenterYCm, 30.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    GameState sameToiState = isolatedState(sameToiAir);
    sameToiState.balls[1] = sameToiCloth;
    sameToiState.balls[1].pocketed = false;
    sameToiState.balls[0].pocketInteraction.phase =
        PocketInteractionPhase::Approaching;
    sameToiState.balls[0].pocketInteraction.pocketId = 4;
    sameToiState.balls[0].pocketInteraction.captureSequence = 9;
    sameToiState.nextPocketCaptureSequence = 77;
    sameToiState.balls[0].rotationAxis = {0.1f, 0.2f, 0.3f};
    sameToiState.balls[0].rotationAngle = 13.0f;
    const GameState sameToiBefore = sameToiState;
    const IndependentSweptSphereRoot x06Root =
        independentEarliestSweptSphereRoot(sameToiAir, sameToiCloth, 0.1,
            geometry.gravityYCmS2, crossRadiusSum);
    expectIndependentFirstApproachingRoot(x06Root, sameToiAir,
        sameToiCloth, geometry.gravityYCmS2, crossRadiusSum,
        x06CushionToi, sameToiProfile.solver.toiToleranceSeconds,
        "X06 fixture must independently prove its earliest 3D ball root");
    const double x06CrossDomainToi = x06Root.timeSeconds;
    expect(std::fabs(x06CrossDomainToi - x06CushionToi) <=
            sameToiProfile.solver.toiToleranceSeconds,
        "X06 fixture must analytically align cross-domain and cushion TOIs");
    const TableDepartureStepResult sameToiResult =
        advanceTableDeparturePhysics(sameToiState, 0.1f, sameToiProfile,
            PhysicsBoundaryMode::ProductionTable,
            sameToiProfile.solver.maximumEventsPerTick);
    expect(!sameToiResult.ok && sameToiResult.events.empty() &&
        sameToiResult.failureCode ==
            static_cast<TableDepartureFailureCode>(6) &&
        close(ambiguousCrossTime(sameToiResult), x06CrossDomainToi,
            sameToiProfile.solver.toiToleranceSeconds) &&
        close(ambiguousTableTime(sameToiResult), x06CushionToi,
            sameToiProfile.solver.toiToleranceSeconds) &&
        sameTransactionalState(sameToiState, sameToiBefore),
        "X06 equal-TOI cross-domain and table candidate must rollback whole state");

    const double x08TargetTime = 0.014;
    const double x08ContactDy = -3.0;
    const double x08ContactDz = 2.0;
    const double x08ContactDx = std::sqrt(crossRadiusSum * crossRadiusSum -
        x08ContactDy * x08ContactDy - x08ContactDz * x08ContactDz);
    BallState tunnelAir = ballAt(-20.0f, static_cast<float>(
            geometry.clothSupportedCenterYCm - x08ContactDy -
            0.5 * geometry.gravityYCmS2 * x08TargetTime * x08TargetTime),
        0.0f,
        1000.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    BallState tunnelCloth = ballAt(static_cast<float>(-20.0 +
            1000.0 * x08TargetTime + x08ContactDx),
        geometry.clothSupportedCenterYCm,
        static_cast<float>(x08ContactDz),
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    const IndependentSweptSphereRoot x08Root =
        independentEarliestSweptSphereRoot(tunnelAir, tunnelCloth, 0.04,
            geometry.gravityYCmS2, crossRadiusSum);
    const IndependentPolynomial x08Polynomial =
        independentSeparationPolynomial(tunnelAir, tunnelCloth,
            geometry.gravityYCmS2, crossRadiusSum);
    const std::vector<double> x08Roots = isolateIndependentPolynomialRoots(
        x08Polynomial, 0.0, 0.04);
    const double x08PreContactClothRoot =
        independentEarliestDescendingPlaneRoot(tunnelAir,
            geometry.clothSupportedCenterYCm, 0.04,
            geometry.gravityYCmS2);
    BallState x08ExpectedAir = tunnelAir;
    BallState x08ExpectedCloth = tunnelCloth;
    advanceIndependentToContact(x08ExpectedAir, x08Root.timeSeconds,
        geometry.gravityYCmS2);
    advanceIndependentToContact(x08ExpectedCloth, x08Root.timeSeconds,
        geometry.gravityYCmS2);
    const IndependentCanonicalContact x08Canonical =
        canonicalizeIndependentContact(x08ExpectedAir, x08ExpectedCloth,
            crossRadiusSum);
    const BallBallContactResult x08Contact = resolveBallBallContact(
        x08ExpectedAir, x08ExpectedCloth,
        crossProfile.ball, crossProfile.ball);
    if (x08ExpectedCloth.velocity.y > 0.0f) {
        x08ExpectedCloth.physicalDomain = PhysicalBallDomain::Airborne;
    } else if (x08ExpectedCloth.velocity.y < 0.0f) {
        x08ExpectedCloth.velocity.y = 0.0f;
        x08ExpectedCloth.position.y = geometry.clothSupportedCenterYCm;
    }
    x08ExpectedCloth.speed = static_cast<float>(std::sqrt(
        x08ExpectedCloth.velocity.x * x08ExpectedCloth.velocity.x +
        x08ExpectedCloth.velocity.y * x08ExpectedCloth.velocity.y +
        x08ExpectedCloth.velocity.z * x08ExpectedCloth.velocity.z));
    const double x08PostContactClothTail =
        independentEarliestDescendingPlaneRoot(x08ExpectedAir,
            geometry.clothSupportedCenterYCm,
            0.04 - x08Root.timeSeconds, geometry.gravityYCmS2);
    const double x08PostContactClothAbsolute = std::isfinite(
        x08PostContactClothTail) ?
        x08Root.timeSeconds + x08PostContactClothTail :
        std::numeric_limits<double>::infinity();
    const double x08DepartingSeparationRoot = x08Roots.size() > 1 ?
        x08Roots[1] : std::numeric_limits<double>::quiet_NaN();
    const double x08Horizon = 0.5 *
        (x08DepartingSeparationRoot + 0.04);
    const std::array<double, 3> x08InitialSeparation = {{
        tunnelCloth.position.x - tunnelAir.position.x,
        tunnelCloth.position.y - tunnelAir.position.y,
        tunnelCloth.position.z - tunnelAir.position.z}};
    expect(x08Root.hit && x08Root.approachDerivative < 0.0 &&
        close(x08Root.timeSeconds, x08TargetTime,
            crossProfile.solver.toiToleranceSeconds) &&
        evaluateIndependentPolynomial(x08Polynomial, 0.0) > 0.0 &&
        evaluateIndependentPolynomial(x08Polynomial, x08Horizon) > 0.0 &&
        std::count_if(x08Roots.begin(), x08Roots.end(), [&](double root) {
            return root <= x08TargetTime +
                crossProfile.solver.toiToleranceSeconds;
        }) == 1 && x08Roots.size() == 2 &&
        std::isfinite(x08DepartingSeparationRoot) &&
        x08Root.timeSeconds + crossProfile.solver.toiToleranceSeconds <
            x08DepartingSeparationRoot &&
        (!std::isfinite(x08PreContactClothRoot) ||
            x08Horizon + crossProfile.solver.toiToleranceSeconds <
                x08PreContactClothRoot) &&
        x08Canonical.ok && x08Contact.velocityImpulseApplied &&
        x08DepartingSeparationRoot +
            crossProfile.solver.toiToleranceSeconds < x08Horizon &&
        (!std::isfinite(x08PostContactClothAbsolute) ||
            x08Horizon + crossProfile.solver.toiToleranceSeconds <
                x08PostContactClothAbsolute) &&
        std::sqrt(x08InitialSeparation[0] * x08InitialSeparation[0] +
            x08InitialSeparation[1] * x08InitialSeparation[1] +
            x08InitialSeparation[2] * x08InitialSeparation[2]) >
                crossRadiusSum,
        "X08 continuous quartic hit must precede every table event while both endpoints remain disjoint");
    advanceIndependentDomainTail(x08ExpectedAir,
        x08Horizon - x08Root.timeSeconds, geometry.gravityYCmS2,
        crossProfile.tableDeparture.supportedRollingDecelerationCmS2,
        crossProfile.surface.torsionalSpinDecelerationRadS2);
    advanceIndependentDomainTail(x08ExpectedCloth,
        x08Horizon - x08Root.timeSeconds, geometry.gravityYCmS2,
        crossProfile.tableDeparture.supportedRollingDecelerationCmS2,
        crossProfile.surface.torsionalSpinDecelerationRadS2);
    GameState tunnelState = isolatedState(tunnelAir);
    tunnelState.balls[1] = tunnelCloth;
    tunnelState.balls[1].pocketed = false;
    const TableDepartureStepResult tunnelResult =
        advanceTableDeparturePhysics(tunnelState,
            static_cast<float>(x08Horizon), crossProfile,
            PhysicsBoundaryMode::ProductionTable,
            crossProfile.solver.maximumEventsPerTick);
    const TableDepartureEvent* x08ProductionContact =
        crossDomainContact(tunnelResult, 0, 1);
    const double x08FinalDx = tunnelState.balls[1].position.x -
        tunnelState.balls[0].position.x;
    const double x08FinalDy = tunnelState.balls[1].position.y -
        tunnelState.balls[0].position.y;
    const double x08FinalDz = tunnelState.balls[1].position.z -
        tunnelState.balls[0].position.z;
    expect(tunnelResult.ok && tunnelResult.events.size() == 1 &&
        x08ProductionContact != nullptr &&
        close(x08ProductionContact->absoluteWholeTickTimeSeconds,
            x08Root.timeSeconds,
            crossProfile.solver.toiToleranceSeconds) &&
        x08ProductionContact->stableIdentity.rfind(
            "cross_domain_ball_contact:0:1:0:", 0) == 0 &&
        tunnelState.balls[1].velocity.x > 0.0f &&
        std::sqrt(x08FinalDx * x08FinalDx + x08FinalDy * x08FinalDy +
            x08FinalDz * x08FinalDz) >= crossRadiusSum -
                crossProfile.solver.penetrationSlopCm &&
        x05SemanticBall(tunnelState.balls[0], x08ExpectedAir,
            crossProfile, x08Horizon) &&
        x05SemanticBall(tunnelState.balls[1], x08ExpectedCloth,
            crossProfile, x08Horizon),
        "X08 high-speed cross-domain path must not tunnel or use endpoint overlap");

    BallState lowNose = ballAt(55.0f,
        geometry.clothSupportedCenterYCm + 0.2f, 30.0f,
        400.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    const TableDepartureStepResult lowNoseResult =
        advanceTableDepartureMotion(lowNose, 0.1f, profile);
    expect(lowNoseResult.ok &&
        countTableDepartureEvents(lowNoseResult,
            TableDepartureEventKind::CushionNoseContact) == 1 &&
        lowNose.velocity.x < 0.0f,
        "low high-speed trajectory must hit the cushion nose without tunnelling");
    expectPrivateCanonicalOrder(lowNoseResult);

    const double noseTargetTime = (
        geometry.playfieldHalfWidthCm - geometry.ballRadiusCm - 55.0) /
        400.0;
    const double noseHorizon = noseTargetTime + 0.005;
    const double noseInitialY = geometry.clothSupportedCenterYCm + 4.0;
    const double noseBelowTargetY = geometry.cushionNoseCenterYCm +
        0.5 * geometry.ballRadiusCm;
    const double noseAboveTargetY = geometry.cushionNoseCenterYCm +
        geometry.ballRadiusCm +
        4.0 * profile.solver.penetrationSlopCm;
    const auto noseInitialVyForTarget = [&](double targetY) {
        return (targetY - noseInitialY -
            0.5 * geometry.gravityYCmS2 * noseTargetTime * noseTargetTime) /
            noseTargetTime;
    };
    BallState noseBelow = ballAt(55.0f,
        static_cast<float>(noseInitialY), 30.0f,
        400.0f, static_cast<float>(noseInitialVyForTarget(
            noseBelowTargetY)), 0.0f, PhysicalBallDomain::Airborne);
    BallState noseAbove = ballAt(55.0f,
        static_cast<float>(noseInitialY), 30.0f,
        400.0f, static_cast<float>(noseInitialVyForTarget(
            noseAboveTargetY)), 0.0f, PhysicalBallDomain::Airborne);
    const BallState noseBelowInitial = noseBelow;
    const std::array<double, 3> noseBelowPoint = independentPositionAt(
        noseBelow, noseTargetTime, geometry.gravityYCmS2);
    const std::array<double, 3> noseAbovePoint = independentPositionAt(
        noseAbove, noseTargetTime, geometry.gravityYCmS2);
    const double noseBelowClothRoot = independentEarliestDescendingPlaneRoot(
        noseBelow, geometry.clothSupportedCenterYCm, noseHorizon,
        geometry.gravityYCmS2);
    const double noseAboveClothRoot = independentEarliestDescendingPlaneRoot(
        noseAbove, geometry.clothSupportedCenterYCm, noseHorizon,
        geometry.gravityYCmS2);
    expect(noseInitialY >= geometry.clothSupportedCenterYCm +
            profile.solver.penetrationSlopCm &&
        close(noseBelowPoint[0],
            geometry.playfieldHalfWidthCm - geometry.ballRadiusCm,
            profile.solver.penetrationSlopCm) &&
        noseBelowPoint[1] - geometry.ballRadiusCm <=
            geometry.cushionNoseCenterYCm &&
        noseBelowPoint[1] + geometry.ballRadiusCm >=
            geometry.clothSurfaceYCm &&
        noseAbovePoint[1] - geometry.ballRadiusCm >=
            geometry.cushionNoseCenterYCm +
                3.0 * profile.solver.penetrationSlopCm &&
        (!std::isfinite(noseBelowClothRoot) ||
            noseTargetTime + profile.solver.toiToleranceSeconds <
                noseBelowClothRoot) &&
        (!std::isfinite(noseAboveClothRoot) ||
            noseTargetTime + profile.solver.toiToleranceSeconds <
                noseAboveClothRoot),
        "nose fixtures must start legal and reach hit/clear heights before every cloth candidate");
    const TableDepartureStepResult belowResult =
        advanceTableDepartureMotion(noseBelow,
            static_cast<float>(noseHorizon), profile);
    const TableDepartureStepResult aboveResult =
        advanceTableDepartureMotion(noseAbove,
            static_cast<float>(noseHorizon), profile);
    expect(belowResult.ok && aboveResult.ok &&
        belowResult.events.size() == 1 &&
        countTableDepartureEvents(belowResult,
            TableDepartureEventKind::CushionNoseContact) == 1 &&
        close(belowResult.events[0].absoluteWholeTickTimeSeconds,
            noseTargetTime, profile.solver.toiToleranceSeconds) &&
        belowResult.events[0].stableIdentity.rfind("0:0:nose:", 0) == 0 &&
        aboveResult.events.empty() &&
        countTableDepartureEvents(aboveResult,
            TableDepartureEventKind::CushionNoseContact) == 0 &&
        noseBelow.velocity.x < 0.0f && noseAbove.velocity.x > 0.0f,
        "swept sphere must hit below and clear above the sealed nose reference");

    BallState noseBoundaryHit = noseBelow;
    BallState noseBoundaryClear = noseAbove;
    noseBoundaryHit = ballAt(55.0f, static_cast<float>(noseInitialY), 30.0f,
        400.0f, static_cast<float>(noseInitialVyForTarget(
            geometry.cushionNoseCenterYCm + geometry.ballRadiusCm -
                1.01 * profile.solver.penetrationSlopCm)), 0.0f,
        PhysicalBallDomain::Airborne);
    noseBoundaryClear = ballAt(55.0f, static_cast<float>(noseInitialY), 30.0f,
        400.0f, static_cast<float>(noseInitialVyForTarget(
            geometry.cushionNoseCenterYCm + geometry.ballRadiusCm +
                1.01 * profile.solver.penetrationSlopCm)), 0.0f,
        PhysicalBallDomain::Airborne);
    const TableDepartureStepResult noseBoundaryHitResult =
        advanceTableDepartureMotion(noseBoundaryHit,
            static_cast<float>(noseHorizon), profile);
    const TableDepartureStepResult noseBoundaryClearResult =
        advanceTableDepartureMotion(noseBoundaryClear,
            static_cast<float>(noseHorizon), profile);
    expect(noseBoundaryHitResult.ok && noseBoundaryClearResult.ok &&
        countTableDepartureEvents(noseBoundaryHitResult,
            TableDepartureEventKind::CushionNoseContact) == 1 &&
        countTableDepartureEvents(noseBoundaryClearResult,
            TableDepartureEventKind::CushionNoseContact) == 0,
        "nose vertical boundary controls must separate hit and clear by 1.01 slop");

    // Shipping 200-power break regression: this path clears the vertical
    // nose, then descends into the rounded upper end-rail edge before either
    // of the old plane-only candidates applies.
    BallState descendingEndRailEdge = ballAt(-0.0004355f,
        94.06912586111112f, -122.2005855f, -0.001379f,
        -128.77994333333334f, -609.52167f,
        PhysicalBallDomain::Airborne);
    GameState descendingEndRailState = isolatedState(descendingEndRailEdge);
    const double descendingEndRailHorizon = 1.0 / 60.0;
    const TableDepartureEarliestBatch descendingEndRailBatch =
        queryEarliestTableDepartureBatch(descendingEndRailState,
            descendingEndRailHorizon, profile,
            PhysicsBoundaryMode::ProductionTable);
    const auto descendingEndRailContact = std::find_if(
        descendingEndRailBatch.candidates.begin(),
        descendingEndRailBatch.candidates.end(),
        [](const TableDepartureCandidate& candidate) {
            return candidate.firstBall == 0 && candidate.kind ==
                TableDepartureEventKind::CushionNoseContact;
        });
    expect(descendingEndRailBatch.ok &&
        descendingEndRailContact != descendingEndRailBatch.candidates.end() &&
        descendingEndRailContact->timeSeconds > 0.0 &&
        descendingEndRailContact->timeSeconds < descendingEndRailHorizon &&
        descendingEndRailContact->featureId == 1 &&
        descendingEndRailContact->contactNormalComplete &&
        close(descendingEndRailContact->contactNormal.x, 0.0) &&
        descendingEndRailContact->contactNormal.y > 0.0f &&
        descendingEndRailContact->contactNormal.z > 0.0f &&
        close(std::sqrt(
            descendingEndRailContact->contactNormal.y *
                descendingEndRailContact->contactNormal.y +
            descendingEndRailContact->contactNormal.z *
                descendingEndRailContact->contactNormal.z), 1.0, 1e-5),
        "descending sphere must report the real rounded end-rail edge normal");
    if (descendingEndRailContact != descendingEndRailBatch.candidates.end()) {
        const auto edgePoint = independentPositionAt(descendingEndRailEdge,
            descendingEndRailContact->timeSeconds, geometry.gravityYCmS2);
        const double outward = -edgePoint[2] -
            (geometry.playfieldHalfLengthCm + geometry.ballRadiusCm);
        const double above = edgePoint[1] - geometry.railTopSurfaceYCm;
        const double incomingNormalSpeed =
            descendingEndRailEdge.velocity.z *
                descendingEndRailContact->contactNormal.z +
            (descendingEndRailEdge.velocity.y + geometry.gravityYCmS2 *
                descendingEndRailContact->timeSeconds) *
                descendingEndRailContact->contactNormal.y;
        expect(outward < 0.0 && above > 0.0 &&
            close(std::hypot(outward, above), geometry.ballRadiusCm, 1e-4) &&
            incomingNormalSpeed < 0.0,
            "rounded end-rail receipt must bind the swept sphere surface and approaching velocity");
    }
    BallState descendingEndRailResolved = descendingEndRailEdge;
    const TableDepartureStepResult descendingEndRailResult =
        advanceTableDepartureMotion(descendingEndRailResolved,
            static_cast<float>(descendingEndRailHorizon), profile);
    expect(descendingEndRailResult.ok &&
        countTableDepartureEvents(descendingEndRailResult,
            TableDepartureEventKind::CushionNoseContact) == 1 &&
        !descendingEndRailResult.events.empty() &&
        descendingEndRailResult.events.front().velocityAfterCmS.z > 0.0f &&
        descendingEndRailResult.events.front().velocityAfterCmS.y >
            descendingEndRailEdge.velocity.y,
        "rounded end-rail contact must prevent the plane-only penetration path");

    const auto roundedEdgeCandidate = [&](bool xAxis, bool positiveSide,
                                           float tangentCoordinate) {
        const float half = xAxis ? geometry.playfieldHalfWidthCm :
            geometry.playfieldHalfLengthCm;
        const float direction = positiveSide ? 1.0f : -1.0f;
        const float radialStart = direction * (half - 2.0f);
        BallState edge = xAxis ?
            ballAt(radialStart, geometry.railTopSurfaceYCm + 2.0f,
                tangentCoordinate, direction * 600.0f, -100.0f, 0.0f,
                PhysicalBallDomain::Airborne) :
            ballAt(tangentCoordinate,
                geometry.railTopSurfaceYCm + 2.0f, radialStart,
                0.0f, -100.0f, direction * 600.0f,
                PhysicalBallDomain::Airborne);
        const TableDepartureEarliestBatch batch =
            queryEarliestTableDepartureBatch(isolatedState(edge), 0.02,
                profile, PhysicsBoundaryMode::ProductionTable);
        const auto contact = std::find_if(batch.candidates.begin(),
            batch.candidates.end(), [](const TableDepartureCandidate& value) {
                return value.kind ==
                    TableDepartureEventKind::CushionNoseContact &&
                    value.contactNormalComplete;
            });
        expect(batch.ok && contact != batch.candidates.end() &&
            contact->featureId == (xAxis ? 0 : 1) &&
            contact->contactNormal.y > 0.0f &&
            (xAxis ? contact->contactNormal.x * direction < 0.0f :
                contact->contactNormal.z * direction < 0.0f),
            "all four rounded rail edges must return inward/upward normals");
    };
    roundedEdgeCandidate(true, false, 30.0f);
    roundedEdgeCandidate(true, true, 30.0f);
    roundedEdgeCandidate(false, false, 0.0f);
    roundedEdgeCandidate(false, true, 0.0f);

    BallState railTopEndpoint = ballAt(
        geometry.playfieldHalfWidthCm + geometry.ballRadiusCm + 0.001f,
        geometry.railTopSupportedCenterYCm + 1.0f, 30.0f,
        0.0f, -100.0f, 0.0f, PhysicalBallDomain::Airborne);
    const TableDepartureEarliestBatch railTopEndpointBatch =
        queryEarliestTableDepartureBatch(isolatedState(railTopEndpoint),
            0.02, profile, PhysicsBoundaryMode::ProductionTable);
    expect(railTopEndpointBatch.ok &&
        std::count_if(railTopEndpointBatch.candidates.begin(),
            railTopEndpointBatch.candidates.end(),
            [](const TableDepartureCandidate& value) {
                return value.kind == TableDepartureEventKind::RailTopLanding;
            }) == 1 &&
        std::none_of(railTopEndpointBatch.candidates.begin(),
            railTopEndpointBatch.candidates.end(),
            [](const TableDepartureCandidate& value) {
                return value.kind ==
                    TableDepartureEventKind::CushionNoseContact &&
                    value.contactNormalComplete;
            }),
        "rounded edge top endpoint must delegate exactly to rail-top support");

    BallState sidePocketEdge = ballAt(
        -(geometry.playfieldHalfWidthCm - 2.0f),
        geometry.railTopSurfaceYCm + 2.0f, 0.0f,
        -600.0f, -100.0f, 0.0f, PhysicalBallDomain::Airborne);
    const TableDepartureEarliestBatch sidePocketEdgeBatch =
        queryEarliestTableDepartureBatch(isolatedState(sidePocketEdge),
            0.02, profile, PhysicsBoundaryMode::ProductionTable);
    expect(sidePocketEdgeBatch.ok &&
        std::none_of(sidePocketEdgeBatch.candidates.begin(),
            sidePocketEdgeBatch.candidates.end(),
            [](const TableDepartureCandidate& value) {
                return value.kind ==
                    TableDepartureEventKind::CushionNoseContact &&
                    value.contactNormalComplete;
            }),
        "rounded rail edge must remain open across the canonical pocket mouth");

    BallState highClearance = ballAt(55.0f,
        geometry.railTopSupportedCenterYCm + 2.0f, 30.0f,
        400.0f, 30.0f, 0.0f, PhysicalBallDomain::Airborne);
    const TableDepartureStepResult highResult =
        advanceTableDepartureMotion(highClearance, 0.05f, profile);
    expect(highResult.ok &&
        countTableDepartureEvents(highResult,
            TableDepartureEventKind::CushionNoseContact) == 0 &&
        highClearance.position.x > profile.tableBoundary.playfieldWidthCm * 0.5f,
        "high trajectory must clear the cushion nose");
    expectPrivateCanonicalOrder(highResult);

    BallState railLanding = ballAt(70.0f,
        geometry.railTopSupportedCenterYCm + 3.0f, 30.0f,
        0.0f, -20.0f, 0.0f, PhysicalBallDomain::Airborne);
    const BallState railLandingInitial = railLanding;
    const TableDepartureStepResult railLandingResult =
        advanceTableDepartureMotion(railLanding, 0.2f, profile);
    expect(railLandingResult.ok &&
        countTableDepartureEvents(railLandingResult,
            TableDepartureEventKind::RailTopLanding) == 1 &&
        railLanding.physicalDomain == PhysicalBallDomain::RailTopSupported &&
        close(railLanding.position.y, geometry.railTopSupportedCenterYCm),
        "descending ball over rail solid must land on rail top");
    expectPrivateCanonicalOrder(railLandingResult);

    BallState railReturn = ballAt(profile.tableBoundary.playfieldWidthCm * 0.5f +
            profile.ball.radiusCm + 0.1f,
        geometry.railTopSupportedCenterYCm, 30.0f,
        -20.0f, 0.0f, 0.0f, PhysicalBallDomain::RailTopSupported);
    const TableDepartureStepResult returnResult =
        advanceTableDepartureMotion(railReturn, 0.3f, profile);
    expect(returnResult.ok &&
        countTableDepartureEvents(returnResult,
            TableDepartureEventKind::ClothReturn) == 1 &&
        railReturn.physicalDomain == PhysicalBallDomain::ClothSupported &&
        close(railReturn.position.y, geometry.clothSupportedCenterYCm),
        "rail-top path must return continuously to cloth");
    expectPrivateCanonicalOrder(returnResult);

    BallState outsideLanding = ballAt(geometry.outerHalfWidthCm + 5.0f,
        12.0f, 30.0f, 0.0f, -30.0f, 0.0f,
        PhysicalBallDomain::Airborne);
    const BallState outsideLandingInitial = outsideLanding;
    const TableDepartureStepResult outsideResult =
        advanceTableDepartureMotion(outsideLanding, 0.5f, profile);
    expect(outsideResult.ok &&
        countTableDepartureEvents(outsideResult,
            TableDepartureEventKind::OutsideLanding) == 1 &&
        outsideLanding.physicalDomain ==
            PhysicalBallDomain::OutsideLandingSupported &&
        close(outsideLanding.position.y,
            geometry.outsideLandingSupportedCenterYCm),
        "outer-clear ball must land on the explicit outside floor");
    expectPrivateCanonicalOrder(outsideResult);
    const float outsideX = outsideLanding.position.x;
    expect(advanceTableDepartureMotion(outsideLanding, 2.0f, profile).ok &&
        outsideLanding.physicalDomain ==
            PhysicalBallDomain::OutsideLandingSupported &&
        outsideLanding.speed == 0.0f && outsideLanding.position.x >= outsideX,
        "outside supported motion must settle physically");

    BallState cornerPocket = ballAt(
        -profile.tableBoundary.playfieldWidthCm * 0.5f,
        geometry.clothSupportedCenterYCm + 1.0f,
        -profile.tableBoundary.playfieldLengthCm * 0.5f,
        0.0f, -20.0f, 0.0f, PhysicalBallDomain::Airborne);
    const BallState cornerPocketInitial = cornerPocket;
    const std::array<PocketBoundaryFrame, 6> r03Frames =
        buildPocketBoundaryFrames(profile.tableBoundary);
    const auto r03CanonicalMouth = [&](const BallState& ball) {
        for (const PocketBoundaryFrame& frame : r03Frames) {
            const double dx = ball.position.x - frame.mouthCenter.x;
            const double dz = ball.position.z - frame.mouthCenter.z;
            const double tangent = dx * frame.tangent.x +
                dz * frame.tangent.z;
            const double depth = dx * frame.inward.x +
                dz * frame.inward.z;
            if (std::fabs(tangent) <= frame.mouthWidthCm * 0.5 -
                    profile.ball.radiusCm + 1e-6 &&
                std::fabs(depth) <= profile.ball.radiusCm + 1e-6)
                return true;
        }
        return false;
    };
    expect(r03CanonicalMouth(cornerPocket),
        "R03 canonical frames must identify the corner mouth");
    const TableDepartureStepResult cornerResult =
        advanceTableDepartureMotion(cornerPocket, 0.1f, profile);
    expect(cornerResult.ok &&
        countTableDepartureEvents(cornerResult,
            TableDepartureEventKind::PocketEntry) == 1 &&
        countTableDepartureEvents(cornerResult,
            TableDepartureEventKind::OuterDeparture) == 0 &&
        cornerPocket.physicalDomain == PhysicalBallDomain::PocketChannel,
        "corner pocket entry must be pocket-channel-only");
    expect(countTableDepartureEvents(cornerResult,
            TableDepartureEventKind::ClothReturn) == 0,
        "R03 pocket entry and cloth return must remain disjoint");
    expectPrivateCanonicalOrder(cornerResult);

    BallState sidePocket = ballAt(
        -profile.tableBoundary.playfieldWidthCm * 0.5f,
        geometry.clothSupportedCenterYCm + 1.0f, 0.0f,
        0.0f, -20.0f, 0.0f, PhysicalBallDomain::Airborne);
    expect(r03CanonicalMouth(sidePocket),
        "R03 canonical frames must identify the side mouth");
    const TableDepartureStepResult sideResult =
        advanceTableDepartureMotion(sidePocket, 0.1f, profile);
    expect(sideResult.ok &&
        countTableDepartureEvents(sideResult,
            TableDepartureEventKind::PocketEntry) == 1 &&
        countTableDepartureEvents(sideResult,
            TableDepartureEventKind::OuterDeparture) == 0,
        "side pocket entry must be pocket-channel-only");
    expect(countTableDepartureEvents(sideResult,
            TableDepartureEventKind::ClothReturn) == 0,
        "R03 side-pocket entry must not also publish cloth return");
    BallState r03Neighbor = cornerPocketInitial;
    r03Neighbor.position.x += profile.tableBoundary.cornerMouthWidthCm;
    const double r03NeighborRoot = independentEarliestDescendingPlaneRoot(
        r03Neighbor, geometry.clothSupportedCenterYCm, 0.1,
        geometry.gravityYCmS2);
    const std::array<double, 3> r03NeighborLanding = independentPositionAt(
        r03Neighbor, r03NeighborRoot, geometry.gravityYCmS2);
    BallState r03NeighborLandingBall = r03Neighbor;
    r03NeighborLandingBall.position = {
        static_cast<float>(r03NeighborLanding[0]),
        static_cast<float>(r03NeighborLanding[1]),
        static_cast<float>(r03NeighborLanding[2])};
    expect(r03Neighbor.physicalDomain == PhysicalBallDomain::Airborne &&
        !r03Neighbor.pocketed &&
        r03Neighbor.pocketInteraction.phase ==
            PocketInteractionPhase::Outside &&
        finiteIndependentBall(r03Neighbor) &&
        std::isfinite(r03NeighborRoot) &&
        r03NeighborRoot > profile.solver.toiToleranceSeconds &&
        !r03CanonicalMouth(r03NeighborLandingBall) &&
        std::fabs(r03NeighborLanding[0]) <=
            geometry.playfieldHalfWidthCm + geometry.ballRadiusCm &&
        std::fabs(r03NeighborLanding[2]) <=
            geometry.playfieldHalfLengthCm + geometry.ballRadiusCm,
        "R03 immutable adjacent fixture must land inside the cloth envelope outside all six mouths");
    const TableDepartureStepResult r03NeighborResult =
        advanceTableDepartureMotion(r03Neighbor, 0.1f, profile);
    expect(r03NeighborResult.ok && r03NeighborResult.events.size() == 1 &&
        r03NeighborResult.events[0].kind ==
            TableDepartureEventKind::ClothReturn &&
        r03NeighborResult.events[0].ordinal == 0 &&
        close(r03NeighborResult.events[0].absoluteWholeTickTimeSeconds,
            r03NeighborRoot, profile.solver.toiToleranceSeconds) &&
        r03NeighborResult.events[0].stableIdentity.rfind(
            "0:0:cloth_return:", 0) == 0 &&
        countTableDepartureEvents(r03NeighborResult,
            TableDepartureEventKind::PocketEntry) == 0 &&
        countTableDepartureEvents(r03NeighborResult,
            TableDepartureEventKind::OuterDeparture) == 0 &&
        countTableDepartureEvents(r03NeighborResult,
            TableDepartureEventKind::ClothReturn) == 1 &&
        r03Neighbor.physicalDomain == PhysicalBallDomain::ClothSupported &&
        close(r03Neighbor.position.y, geometry.clothSupportedCenterYCm,
            profile.solver.penetrationSlopCm) &&
        r03Neighbor.velocity.y == 0.0f,
        "R03 adjacent nonmouth point must return to cloth only");
    expectPrivateCanonicalOrder(r03NeighborResult);

    BallState overPocket = ballAt(
        -profile.tableBoundary.playfieldWidthCm * 0.5f,
        geometry.railTopSupportedCenterYCm + 10.0f, 0.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    const TableDepartureStepResult overResult =
        advanceTableDepartureMotion(overPocket, 0.05f, profile);
    expect(overResult.ok && overPocket.physicalDomain ==
            PhysicalBallDomain::Airborne &&
        countTableDepartureEvents(overResult,
            TableDepartureEventKind::PocketEntry) == 0,
        "XZ mouth projection while high above the opening must not capture");
    expectPrivateCanonicalOrder(overResult);

    PhysicsProfile invalid = profile;
    invalid.tableDeparture.gravityYCmS2 =
        std::numeric_limits<float>::quiet_NaN();
    BallState rollback = ballistic;
    const BallState rollbackBefore = rollback;
    const TableDepartureStepResult rollbackResult =
        advanceTableDepartureMotion(rollback, 0.1f, invalid);
    expect(!rollbackResult.ok &&
        rollback.position.x == rollbackBefore.position.x &&
        rollback.position.y == rollbackBefore.position.y &&
        rollback.position.z == rollbackBefore.position.z &&
        rollback.velocity.x == rollbackBefore.velocity.x &&
        rollback.velocity.y == rollbackBefore.velocity.y &&
        rollback.velocity.z == rollbackBefore.velocity.z &&
        rollback.physicalDomain == rollbackBefore.physicalDomain,
        "invalid geometry must fail closed with exact ball-state rollback");

    BallState manualOutside = ballAt(geometry.outerHalfWidthCm + 10.0f,
        geometry.clothSupportedCenterYCm, 0.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    const BallState manualOutsideBefore = manualOutside;
    const TableDepartureStepResult manualResult =
        advanceTableDepartureMotion(manualOutside, 0.0f, profile);
    expect(manualResult.ok && manualResult.events.empty() &&
        privateAnalyticSegments(manualResult).empty() &&
        sameTransactionalBall(manualOutside, manualOutsideBefore),
        "zero-duration input must not mutate or synthesize departure evidence");

    BallState twoEventBall = ballAt(geometry.outerHalfWidthCm - 2.0f,
        100.0f, 30.0f, 50.0f, 5.0f, 0.0f,
        PhysicalBallDomain::Airborne);
    GameState twoEventState = isolatedState(twoEventBall);
    const TableDepartureStepResult twoEventResult =
        advanceTableDeparturePhysics(twoEventState, 0.6f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(twoEventResult.ok && twoEventResult.events.size() == 2 &&
        twoEventResult.events[0].kind ==
            TableDepartureEventKind::OuterDeparture &&
        twoEventResult.events[1].kind ==
            TableDepartureEventKind::OutsideLanding &&
        twoEventResult.events[0].absoluteWholeTickTimeSeconds <
            twoEventResult.events[1].absoluteWholeTickTimeSeconds &&
        twoEventState.balls[0].physicalDomain ==
            PhysicalBallDomain::OutsideLandingSupported,
        "recursive tail must rebase departure then landing on the whole tick");
    expectPrivateCanonicalOrder(twoEventResult);

    GameState mixed;
    initializeBalls(mixed);
    for (int index = 3; index < kBallCount; ++index)
        mixed.balls[index].pocketed = true;
    mixed.balls[0] = ballAt(0.0f, 110.0f, 70.0f,
        0.0f, 20.0f, 0.0f, PhysicalBallDomain::Airborne);
    mixed.balls[1] = ballAt(-10.0f, geometry.clothSupportedCenterYCm, 20.0f,
        100.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    mixed.balls[1].motionState = BallMotionState::Sliding;
    mixed.balls[2] = ballAt(-4.285f, geometry.clothSupportedCenterYCm, 20.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    mixed.balls[2].motionState = BallMotionState::Stationary;
    mixed.ballsMoving = true;
    GameState clothControl = mixed;
    clothControl.balls[0].pocketed = true;
    const PhysicsStepTelemetry controlTelemetry = updatePhysics(
        clothControl, 0.05f, profile, PhysicsBoundaryMode::ProductionTable);
    const PhysicsStepTelemetry mixedTelemetry = updatePhysics(
        mixed, 0.05f, profile, PhysicsBoundaryMode::ProductionTable);
    const auto ballBallContacts = [](const PhysicsStepTelemetry& telemetry) {
        std::vector<const PhysicsContactRecord*> result;
        for (const auto& contact : telemetry.contacts) {
            if (contact.kind == PhysicsContactKind::BallBall)
                result.push_back(&contact);
        }
        return result;
    };
    const auto sameSurfaceStep = [](const SurfaceMotionStep& first,
                                     const SurfaceMotionStep& second) {
        if (first.ballIndex != second.ballIndex ||
            first.before != second.before || first.after != second.after ||
            first.initialSlipSpeedCmS != second.initialSlipSpeedCmS ||
            first.finalSlipSpeedCmS != second.finalSlipSpeedCmS ||
            first.transitionTimeSeconds != second.transitionTimeSeconds ||
            first.frictionAccelerationCmS2.x !=
                second.frictionAccelerationCmS2.x ||
            first.frictionAccelerationCmS2.y !=
                second.frictionAccelerationCmS2.y ||
            first.frictionAccelerationCmS2.z !=
                second.frictionAccelerationCmS2.z ||
            first.angularAccelerationRadS2.x !=
                second.angularAccelerationRadS2.x ||
            first.angularAccelerationRadS2.y !=
                second.angularAccelerationRadS2.y ||
            first.angularAccelerationRadS2.z !=
                second.angularAccelerationRadS2.z ||
            first.analyticSegments.size() != second.analyticSegments.size())
            return false;
        for (std::size_t index = 0;
                index < first.analyticSegments.size(); ++index) {
            if (!samePrivateSegment(first.analyticSegments[index],
                    second.analyticSegments[index]))
                return false;
        }
        return true;
    };
    const auto mixedBallContacts = ballBallContacts(mixedTelemetry);
    const auto controlBallContacts = ballBallContacts(controlTelemetry);
    bool legacySurfaceStepsPreserved = true;
    for (const SurfaceMotionStep& control : controlTelemetry.surfaceMotion) {
        if (control.ballIndex != 1 && control.ballIndex != 2) continue;
        const auto matching = std::find_if(mixedTelemetry.surfaceMotion.begin(),
            mixedTelemetry.surfaceMotion.end(), [&](const auto& candidate) {
                return candidate.ballIndex == control.ballIndex &&
                    sameSurfaceStep(candidate, control);
            });
        legacySurfaceStepsPreserved = legacySurfaceStepsPreserved &&
            matching != mixedTelemetry.surfaceMotion.end();
    }
    expect(controlTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        mixedTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        sameTransactionalBall(mixed.balls[1], clothControl.balls[1]) &&
        sameTransactionalBall(mixed.balls[2], clothControl.balls[2]) &&
        mixedBallContacts.size() == 1 && controlBallContacts.size() == 1 &&
        sameLegacyContact(*mixedBallContacts.front(),
            *controlBallContacts.front()) &&
        std::none_of(mixedTelemetry.contacts.begin(),
            mixedTelemetry.contacts.end(), [](const auto& contact) {
                return contact.kind == PhysicsContactKind::Rail;
            }) &&
        legacySurfaceStepsPreserved,
        "mixed airborne domain must preserve legacy cloth solver state and telemetry");

    GameState planeMixed;
    initializeBalls(planeMixed);
    for (int index = 2; index < kBallCount; ++index)
        planeMixed.balls[index].pocketed = true;
    planeMixed.balls[0] = ballAt(0.0f, 110.0f, 70.0f,
        0.0f, 20.0f, 0.0f, PhysicalBallDomain::Airborne);
    planeMixed.balls[1] = ballAt(0.0f,
        geometry.clothSupportedCenterYCm, -5.0f,
        0.0f, 0.0f, 100.0f, PhysicalBallDomain::ClothSupported);
    planeMixed.balls[1].motionState = BallMotionState::Rolling;
    planeMixed.balls[1].angularVelocity.x =
        100.0f / profile.ball.radiusCm;
    planeMixed.ballsMoving = true;
    PhysicsProfile planeProfile = profile;
    planeProfile.surface.rollingResistanceAccelerationCmS2 = 0.0f;
    GameState planeControl = planeMixed;
    planeControl.balls[0].pocketed = true;
    PhysicsPlaneObservationRequest planeRequest;
    planeRequest.observationId = "d6a_mixed_legacy_plane";
    planeRequest.ballIndex = 1;
    planeRequest.axis = PhysicsPlaneAxis::Z;
    planeRequest.planeCoordinateCm = 0.0;
    planeRequest.direction =
        PhysicsPlaneCrossingDirection::NegativeToPositive;
    const PhysicsStepTelemetry planeControlTelemetry = updatePhysics(
        planeControl, 0.1f, planeProfile, PhysicsBoundaryMode::Unbounded,
        {planeRequest});
    const PhysicsStepTelemetry planeMixedTelemetry = updatePhysics(
        planeMixed, 0.1f, planeProfile, PhysicsBoundaryMode::Unbounded,
        {planeRequest});
    PhysicsPlaneCrossingRecord planeMixedPhysicalCrossing;
    bool planeMixedOrdinalOwnsCrossing = false;
    if (planeMixedTelemetry.planeCrossings.size() == 1) {
        planeMixedPhysicalCrossing = planeMixedTelemetry.planeCrossings[0];
        if (planeControlTelemetry.planeCrossings.size() == 1)
            planeMixedPhysicalCrossing.segmentOrdinal =
                planeControlTelemetry.planeCrossings[0].segmentOrdinal;
        for (const SurfaceMotionAnalyticSegment& segment :
                planeMixedTelemetry.analyticSurfaceMotion) {
            const double end = segment.wholeTickStartOffsetSeconds +
                segment.durationSeconds;
            if (segment.ballIndex == planeRequest.ballIndex &&
                segment.segmentOrdinal ==
                    planeMixedTelemetry.planeCrossings[0].segmentOrdinal &&
                planeMixedTelemetry.planeCrossings[0].
                    wholeTickTimeOfImpactSeconds >=
                    segment.wholeTickStartOffsetSeconds &&
                planeMixedTelemetry.planeCrossings[0].
                    wholeTickTimeOfImpactSeconds <= end) {
                planeMixedOrdinalOwnsCrossing = true;
            }
        }
    }
    expect(planeControlTelemetry.planeAttestations.size() == 1 &&
        planeControlTelemetry.planeCrossings.size() == 1 &&
        planeMixedTelemetry.planeAttestations.size() == 1 &&
        planeMixedTelemetry.planeCrossings.size() == 1 &&
        sameAttestation(planeControlTelemetry.planeAttestations[0],
            planeMixedTelemetry.planeAttestations[0]) &&
        sameCrossing(planeControlTelemetry.planeCrossings[0],
            planeMixedPhysicalCrossing) && planeMixedOrdinalOwnsCrossing,
        "mixed domain must preserve legacy plane crossing and attestation");

    GameState invalidPlaneState = planeMixed;
    const GameState invalidPlaneBefore = invalidPlaneState;
    PhysicsPlaneObservationRequest invalidPlane = planeRequest;
    invalidPlane.planeCoordinateCm =
        std::numeric_limits<double>::quiet_NaN();
    const PhysicsStepTelemetry invalidPlaneTelemetry = updatePhysics(
        invalidPlaneState, 0.1f, planeProfile, PhysicsBoundaryMode::Unbounded,
        {invalidPlane});
    expect(invalidPlaneTelemetry.stepStatus == PhysicsStepStatus::Failed &&
        invalidPlaneTelemetry.failureCode == PhysicsFailureCode::InvalidControls &&
        invalidPlaneTelemetry.planeAttestations.empty() &&
        invalidPlaneTelemetry.planeCrossings.empty() &&
        sameTransactionalState(invalidPlaneState, invalidPlaneBefore),
        "mixed invalid plane request must fail before mutation");
    GameState duplicatePlaneState = planeMixed;
    const GameState duplicatePlaneBefore = duplicatePlaneState;
    const PhysicsStepTelemetry duplicatePlaneTelemetry = updatePhysics(
        duplicatePlaneState, 0.1f, planeProfile,
        PhysicsBoundaryMode::Unbounded,
        {planeRequest, planeRequest});
    expect(duplicatePlaneTelemetry.stepStatus == PhysicsStepStatus::Failed &&
        duplicatePlaneTelemetry.failureCode ==
            PhysicsFailureCode::InvalidControls &&
        sameTransactionalState(duplicatePlaneState, duplicatePlaneBefore),
        "mixed duplicate plane request must fail before mutation");

    GameState exhaustedState = isolatedState(twoEventBall);
    exhaustedState.balls[0].pocketInteraction.phase =
        PocketInteractionPhase::Approaching;
    exhaustedState.balls[0].pocketInteraction.pocketId = 4;
    exhaustedState.nextPocketCaptureSequence = 41;
    const GameState exhaustedBefore = exhaustedState;
    const TableDepartureStepResult exhaustedResult =
        advanceTableDeparturePhysics(exhaustedState, 0.6f, profile,
            PhysicsBoundaryMode::ProductionTable, 1);
    expect(!exhaustedResult.ok && exhaustedResult.failureCode ==
            TableDepartureFailureCode::EventBudget &&
        exhaustedResult.events.empty() &&
        sameTransactionalState(exhaustedState, exhaustedBefore),
        "event exhaustion must rollback ball, pocket, domain and sequence");

    BallState embeddedBall = ballAt(70.0f,
        geometry.clothSupportedCenterYCm + 1.0f, 30.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    GameState embeddedState = isolatedState(embeddedBall);
    embeddedState.balls[0].pocketInteraction.phase =
        PocketInteractionPhase::JawContact;
    const GameState embeddedBefore = embeddedState;
    const TableDepartureStepResult embeddedResult =
        advanceTableDeparturePhysics(embeddedState, 0.1f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(!embeddedResult.ok && embeddedResult.failureCode ==
            TableDepartureFailureCode::AmbiguousInitialPenetration &&
        embeddedResult.events.empty() &&
        sameTransactionalState(embeddedState, embeddedBefore),
        "ambiguous initial rail-solid penetration must fail closed");

    BallState pocketShaft = ballAt(
        -profile.tableBoundary.playfieldWidthCm * 0.5f,
        geometry.railTopSupportedCenterYCm + 2.0f, 0.0f,
        0.0f, -30.0f, 0.0f, PhysicalBallDomain::Airborne);
    GameState pocketShaftState = isolatedState(pocketShaft);
    const TableDepartureStepResult shaftResult =
        advanceTableDeparturePhysics(pocketShaftState, 0.3f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(shaftResult.ok && countTableDepartureEvents(shaftResult,
            TableDepartureEventKind::RailTopLanding) == 0 &&
        countTableDepartureEvents(shaftResult,
            TableDepartureEventKind::PocketEntry) == 1 &&
        pocketShaftState.balls[0].physicalDomain ==
            PhysicalBallDomain::PocketChannel,
        "open pocket shaft must not receive a rail-top landing");

    BallState cabinetVoid = ballAt(70.0f, 5.0f, 30.0f,
        0.0f, -10.0f, 0.0f, PhysicalBallDomain::Airborne);
    GameState cabinetState = isolatedState(cabinetVoid);
    const GameState cabinetBefore = cabinetState;
    const TableDepartureStepResult cabinetResult =
        advanceTableDeparturePhysics(cabinetState, 0.2f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(!cabinetResult.ok && countTableDepartureEvents(cabinetResult,
            TableDepartureEventKind::OutsideLanding) == 0 &&
        sameTransactionalState(cabinetState, cabinetBefore),
        "outside floor may not support a ball inside the outer rail footprint");

    PhysicsProfile tieProfile = profile;
    tieProfile.tableDeparture.outerRailHalfWidthCm =
        tieProfile.tableBoundary.playfieldWidthCm * 0.5f + 0.1f;
    BallState tieBall = ballAt(
        -tieProfile.tableDeparture.outerRailHalfWidthCm,
        geometry.clothSupportedCenterYCm, 0.0f,
        -10.0f, -10.0f, 0.0f, PhysicalBallDomain::Airborne);
    GameState tieState = isolatedState(tieBall);
    tieState.balls[0].pocketInteraction.phase =
        PocketInteractionPhase::Approaching;
    tieState.balls[0].pocketInteraction.pocketId = 4;
    const GameState tieBefore = tieState;
    const TableDepartureStepResult tieResult =
        advanceTableDeparturePhysics(tieState, 0.1f, tieProfile,
            PhysicsBoundaryMode::ProductionTable,
            tieProfile.solver.maximumEventsPerTick);
    expect(!tieResult.ok && tieResult.failureCode ==
            TableDepartureFailureCode::ContradictoryPocketDeparture &&
        tieResult.events.empty() && sameTransactionalState(tieState, tieBefore),
        "equal-TOI pocket/departure topology must rollback without priority");

    GameState unboundedState = isolatedState(ballAt(
        geometry.outerHalfWidthCm + 10.0f,
        geometry.clothSupportedCenterYCm, 0.0f,
        10.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported));
    const TableDepartureStepResult unboundedResult =
        advanceTableDeparturePhysics(unboundedState, 0.1f, profile,
            PhysicsBoundaryMode::Unbounded,
            profile.solver.maximumEventsPerTick);
    expect(unboundedResult.ok && unboundedResult.events.empty() &&
        unboundedState.balls[0].physicalDomain ==
            PhysicalBallDomain::ClothSupported,
        "unbounded planar mode must not synthesize departure from coordinates");
    expectPrivateCanonicalOrder(unboundedResult);

    GameState storedPocketState = isolatedState(ballAt(
        geometry.outerHalfWidthCm + 10.0f, -100.0f, 0.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::PocketChannel));
    storedPocketState.balls[0].pocketed = true;
    const GameState storedBefore = storedPocketState;
    const TableDepartureStepResult storedResult =
        advanceTableDeparturePhysics(storedPocketState, 0.2f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(storedResult.ok && storedResult.events.empty() &&
        sameTransactionalState(storedPocketState, storedBefore),
        "post-pocket storage coordinates must be ignored by D6-A physics");

    for (std::size_t first = 0; first < twoEventResult.events.size(); ++first) {
        for (std::size_t second = first + 1;
                second < twoEventResult.events.size(); ++second) {
            expect(twoEventResult.events[first].stableIdentity !=
                    twoEventResult.events[second].stableIdentity,
                "recursive physical candidates must be deduped by stable identity");
        }
    }

    // C01/C02/C04/C07: this is a real legacy swept-contact/island.  Its
    // independently rooted 3D normal gives ball 0 positive vy and ball 1
    // negative vy; the unconsumed tail therefore has to leave the legacy
    // surface integrator at the contact TOI.
    PhysicsProfile seamProfile = interactiveChinesePoolPhysicsProfile();
    seamProfile.surface.legacyFrictionAccelerationCmS2 = 0.0f;
    seamProfile.surface.slidingFrictionCoefficient = 0.0f;
    seamProfile.surface.rollingResistanceAccelerationCmS2 = 0.0f;
    seamProfile.surface.torsionalSpinDecelerationRadS2 = 0.0f;
    const float seamDy = 1.0f;
    const float seamGap = 1.0f;
    const float seamSpeed = 100.0f;
    GameState seamInitial = isolatedState(ballAt(0.0f,
        geometry.clothSupportedCenterYCm + seamDy, 20.0f,
        seamSpeed, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported));
    seamInitial.balls[1] = ballAt(2.0f * profile.ball.radiusCm + seamGap,
        geometry.clothSupportedCenterYCm, 20.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
    seamInitial.balls[1].pocketed = false;
    seamInitial.ballsMoving = true;
    const IndependentSweptSphereRoot seamRoot =
        independentEarliestSweptSphereRoot(seamInitial.balls[0],
            seamInitial.balls[1], 0.05, 0.0,
            2.0 * seamProfile.ball.radiusCm);
    expect(seamRoot.hit && seamRoot.timeSeconds > 0.0 &&
        seamRoot.timeSeconds < 0.05 && seamRoot.approachDerivative < 0.0,
        "C01 real legacy island must have one independent approaching TOI");
    const float seamPrefixDuration = static_cast<float>(seamRoot.timeSeconds);
    const float seamBeforeDuration = static_cast<float>(
        seamRoot.timeSeconds - 4.0 * seamProfile.solver.toiToleranceSeconds);
    expect(std::fabs(seamPrefixDuration - seamRoot.timeSeconds) <=
            seamProfile.solver.toiToleranceSeconds &&
        seamBeforeDuration > 0.0f && seamBeforeDuration < seamPrefixDuration,
        "C04 float prefix must bind the exact solver TOI and a strict pre-TOI control");

    GameState seamBeforeControl = seamInitial;
    const PhysicsStepTelemetry seamBeforeTelemetry = updatePhysics(
        seamBeforeControl, seamBeforeDuration,
        seamProfile, PhysicsBoundaryMode::Unbounded);
    expect(seamBeforeTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        seamBeforeTelemetry.contacts.empty() &&
        seamBeforeTelemetry.solverEvents.empty() &&
        seamBeforeControl.balls[0].physicalDomain ==
            PhysicalBallDomain::ClothSupported &&
        seamBeforeControl.balls[1].physicalDomain ==
            PhysicalBallDomain::ClothSupported,
        "C04 horizon strictly before TOI must not claim a handoff");

    GameState seamPrefix = seamInitial;
    const PhysicsStepTelemetry seamPrefixTelemetry = updatePhysics(
        seamPrefix, seamPrefixDuration, seamProfile,
        PhysicsBoundaryMode::Unbounded);
    expect(seamPrefixTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        seamPrefixTelemetry.contacts.size() == 1 &&
        seamPrefixTelemetry.solverEvents.size() == 1 &&
        seamPrefixTelemetry.contacts[0].kind ==
            PhysicsContactKind::BallBall &&
        seamPrefixTelemetry.contacts[0].firstBall == 0 &&
        seamPrefixTelemetry.contacts[0].secondBall == 1 &&
        seamPrefixTelemetry.contacts[0].velocityImpulseApplied &&
        std::fabs(seamPrefixTelemetry.contacts[0].wholeTickTimeOfImpactSeconds -
            seamRoot.timeSeconds) <= seamProfile.solver.toiToleranceSeconds &&
        seamPrefix.balls[0].velocity.y > 0.0f &&
        seamPrefix.balls[1].velocity.y < 0.0f,
        "C01/C02 existing legacy solver must produce the signed 3D support impulses");

    GameState seamManual = seamPrefix;
    seamManual.balls[0].physicalDomain = PhysicalBallDomain::Airborne;
    const float seamWholeDuration = 0.08f;
    const float seamTailDuration = seamWholeDuration - seamPrefixDuration;
    const TableDepartureStepResult seamManualTail =
        advanceTableDeparturePhysics(seamManual, seamTailDuration,
            seamProfile, PhysicsBoundaryMode::ProductionTable,
            seamProfile.solver.maximumEventsPerTick - 1);
    const double seamReturnRoot = independentEarliestDescendingPlaneRoot(
        BallState{seamPrefix.balls[0].position,
            seamPrefix.balls[0].velocity,
            seamPrefix.balls[0].angularVelocity,
            seamPrefix.balls[0].rotationAxis,
            seamPrefix.balls[0].speed,
            seamPrefix.balls[0].rotationAngle,
            seamPrefix.balls[0].pocketed,
            seamPrefix.balls[0].outOfPlay,
            seamPrefix.balls[0].texture,
            seamPrefix.balls[0].motionState,
            PhysicalBallDomain::Airborne,
            seamPrefix.balls[0].pocketInteraction},
        geometry.clothSupportedCenterYCm, seamTailDuration,
        geometry.gravityYCmS2);
    expect(seamManualTail.ok && seamManualTail.events.size() == 1 &&
        seamManualTail.events[0].kind == TableDepartureEventKind::ClothReturn &&
        std::isfinite(seamReturnRoot) && seamReturnRoot > 0.0 &&
        seamReturnRoot < seamTailDuration &&
        close(seamPrefixDuration +
                seamManualTail.events[0].absoluteWholeTickTimeSeconds,
            seamPrefixDuration + seamReturnRoot,
            seamProfile.solver.toiToleranceSeconds) &&
        seamManual.balls[0].physicalDomain ==
            PhysicalBallDomain::ClothSupported &&
        seamManual.balls[0].position.y == geometry.clothSupportedCenterYCm &&
        seamManual.balls[0].velocity.y == 0.0f &&
        seamManual.balls[1].physicalDomain ==
            PhysicalBallDomain::ClothSupported &&
        seamManual.balls[1].position.y == geometry.clothSupportedCenterYCm &&
        seamManual.balls[1].velocity.y == 0.0f,
        "C01 manual D6 tail must release, return, and react downward support at exact TOI");
    const auto& seamPrivateSegments = privateAnalyticSegments(seamManualTail);
    bool seamPrivateCoverage = HasPrivateAnalyticSegments<
        TableDepartureStepResult>::value && validPrivateAnalyticLineage(
            seamPrivateSegments, {0, 1}, seamTailDuration,
            seamProfile.solver.penetrationSlopCm,
            seamProfile.solver.toiToleranceSeconds);
    std::array<double, 2> seamPrivateCoveredUntil{{0.0, 0.0}};
    std::array<int, 2> seamPrivateOrdinal{{-1, -1}};
    std::array<bool, 2> seamPrivateSawBallistic{{false, false}};
    std::array<bool, 2> seamPrivateSawSupported{{false, false}};
    for (const SurfaceMotionAnalyticSegment& segment : seamPrivateSegments) {
        seamPrivateCoverage = seamPrivateCoverage &&
            (segment.ballIndex == 0 || segment.ballIndex == 1) &&
            segment.segmentOrdinal == ++seamPrivateOrdinal[segment.ballIndex] &&
            segment.durationSeconds > 0.0 && segment.analyticPathComplete &&
            std::fabs(segment.wholeTickStartOffsetSeconds -
                seamPrivateCoveredUntil[segment.ballIndex]) <=
                seamProfile.solver.toiToleranceSeconds;
        seamPrivateCoveredUntil[segment.ballIndex] =
            segment.wholeTickStartOffsetSeconds + segment.durationSeconds;
        seamPrivateSawBallistic[segment.ballIndex] =
            seamPrivateSawBallistic[segment.ballIndex] ||
            segment.constantAccelerationCmS2.y ==
                geometry.gravityYCmS2;
        seamPrivateSawSupported[segment.ballIndex] =
            seamPrivateSawSupported[segment.ballIndex] ||
            segment.constantAccelerationCmS2.y == 0.0f;
    }
    for (int ballIndex : {0, 1}) {
        seamPrivateCoverage = seamPrivateCoverage &&
            std::fabs(seamPrivateCoveredUntil[ballIndex] -
                seamTailDuration) <= seamProfile.solver.toiToleranceSeconds &&
            seamPrivateOrdinal[ballIndex] >= 0;
    }
    seamPrivateCoverage = seamPrivateCoverage &&
        seamPrivateSawBallistic[0] && seamPrivateSawSupported[0] &&
        seamPrivateSawSupported[1];
    expect(seamPrivateCoverage,
        "C08 D6 actual mutator must return complete local analytic lineage for every active tail ball");
    if (!seamPrivateSegments.empty()) {
        auto rejects = [&](std::vector<SurfaceMotionAnalyticSegment> mutation) {
            return !validPrivateAnalyticLineage(mutation, {0, 1},
                seamTailDuration, seamProfile.solver.penetrationSlopCm,
                seamProfile.solver.toiToleranceSeconds);
        };
        std::vector<SurfaceMotionAnalyticSegment> mutation =
            seamPrivateSegments;
        mutation.erase(mutation.begin());
        expect(rejects(mutation), "C08 mutation drop segment must fail");
        mutation = seamPrivateSegments;
        mutation.insert(mutation.begin(), mutation.front());
        expect(rejects(mutation), "C08 mutation duplicate segment must fail");
        mutation = seamPrivateSegments;
        mutation.front().wholeTickStartOffsetSeconds += 0.01;
        expect(rejects(mutation), "C08 mutation gap must fail");
        mutation = seamPrivateSegments;
        mutation.front().durationSeconds += 0.01;
        expect(rejects(mutation), "C08 mutation overlap must fail");
        mutation = seamPrivateSegments;
        mutation.front().ballIndex = 15;
        expect(rejects(mutation), "C08 mutation foreign ball must fail");
        mutation = seamPrivateSegments;
        mutation.front().wholeTickStartOffsetSeconds += seamPrefixDuration;
        expect(rejects(mutation), "C08 mutation pre-rebased local offset must fail");
        mutation = seamPrivateSegments;
        mutation.front().endPositionCm.x += 1.0f;
        expect(rejects(mutation), "C08 mutation endpoint must fail");
        mutation = seamPrivateSegments;
        mutation.front().startPositionCm.x += 1.0f;
        expect(rejects(mutation), "C08 mutation start position must fail");
        mutation = seamPrivateSegments;
        mutation.front().startVelocityCmS.x += 1.0f;
        expect(rejects(mutation), "C08 mutation velocity must fail");
        mutation = seamPrivateSegments;
        mutation.front().constantAccelerationCmS2.x += 1.0f;
        expect(rejects(mutation), "C08 mutation acceleration must fail");
        mutation = seamPrivateSegments;
        mutation.front().analyticPathComplete = false;
        expect(rejects(mutation), "C08 mutation incomplete must fail");
    }
    const auto& r08UpPrivateSegments = privateAnalyticSegments(r08UpResult);
    const TableDepartureEvent* r08UpCross = crossDomainContact(
        r08UpResult, 0, 1);
    const SurfaceMotionAnalyticSegment* r08UpPre[2] = {nullptr, nullptr};
    const SurfaceMotionAnalyticSegment* r08UpPost[2] = {nullptr, nullptr};
    if (r08UpCross) {
        for (const auto& segment : r08UpPrivateSegments) {
            if (segment.ballIndex < 0 || segment.ballIndex > 1) continue;
            const double end = segment.wholeTickStartOffsetSeconds +
                segment.durationSeconds;
            if (close(end, r08UpCross->absoluteWholeTickTimeSeconds,
                    profile.solver.toiToleranceSeconds))
                r08UpPre[segment.ballIndex] = &segment;
            if (close(segment.wholeTickStartOffsetSeconds,
                    r08UpCross->absoluteWholeTickTimeSeconds,
                    profile.solver.toiToleranceSeconds))
                r08UpPost[segment.ballIndex] = &segment;
        }
    }
    const auto segmentPointMatches = [&](const Point3& first,
                                           const Point3& second) {
        return close(first.x, second.x, profile.solver.penetrationSlopCm) &&
            close(first.y, second.y, profile.solver.penetrationSlopCm) &&
            close(first.z, second.z, profile.solver.penetrationSlopCm);
    };
    expect(HasPrivateAnalyticSegments<TableDepartureStepResult>::value &&
        r08UpCross != nullptr &&
        hasPrivateBoundaryForBall(r08UpPrivateSegments, 0,
            r08UpCross->absoluteWholeTickTimeSeconds,
            profile.solver.toiToleranceSeconds) &&
        hasPrivateBoundaryForBall(r08UpPrivateSegments, 1,
            r08UpCross->absoluteWholeTickTimeSeconds,
            profile.solver.toiToleranceSeconds) &&
        r08UpPre[0] && r08UpPre[1] && r08UpPost[0] && r08UpPost[1] &&
        segmentPointMatches(r08UpPre[0]->endPositionCm,
            r08UpCanonicalAir.position) &&
        segmentPointMatches(r08UpPre[1]->endPositionCm,
            r08UpSupported.position) &&
        segmentPointMatches(r08UpPost[0]->startPositionCm,
            r08UpPostResolverAir.position) &&
        segmentPointMatches(r08UpPost[1]->startPositionCm,
            r08UpPostResolverSupported.position) &&
        segmentPointMatches(r08UpPost[0]->startVelocityCmS,
            r08UpPostResolverAir.velocity) &&
        segmentPointMatches(r08UpPost[1]->startVelocityCmS,
            r08UpPostResolverSupported.velocity),
        "C08 cross-domain contact must split both participants at the exact shared TOI");
    if (r08UpCross && !r08UpPrivateSegments.empty()) {
        std::vector<SurfaceMotionAnalyticSegment> omitted =
            r08UpPrivateSegments;
        omitted.erase(std::remove_if(omitted.begin(), omitted.end(),
            [&](const SurfaceMotionAnalyticSegment& segment) {
                return segment.ballIndex == 1 &&
                    (close(segment.wholeTickStartOffsetSeconds,
                         r08UpCross->absoluteWholeTickTimeSeconds,
                         profile.solver.toiToleranceSeconds) ||
                     close(segment.wholeTickStartOffsetSeconds +
                             segment.durationSeconds,
                         r08UpCross->absoluteWholeTickTimeSeconds,
                         profile.solver.toiToleranceSeconds));
            }), omitted.end());
        expect(!hasPrivateBoundaryForBall(omitted, 1,
                r08UpCross->absoluteWholeTickTimeSeconds,
                profile.solver.toiToleranceSeconds),
            "C08 omitting either cross-contact participant boundary must fail");
    }

    GameState c08Stationary = isolatedState(ballAt(0.0f,
        geometry.clothSupportedCenterYCm, 0.0f, 0.0f, 0.0f, 0.0f,
        PhysicalBallDomain::ClothSupported));
    const TableDepartureStepResult c08StationaryResult =
        advanceTableDeparturePhysics(c08Stationary, 0.05f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(c08StationaryResult.ok && validPrivateAnalyticLineage(
            privateAnalyticSegments(c08StationaryResult), {0}, 0.05,
            profile.solver.penetrationSlopCm,
            profile.solver.toiToleranceSeconds),
        "C08 positive stationary no-event D6 call must still publish complete lineage");
    GameState c08Zero = c08Stationary;
    const TableDepartureStepResult c08ZeroResult =
        advanceTableDeparturePhysics(c08Zero, 0.0f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(c08ZeroResult.ok &&
        privateAnalyticSegments(c08ZeroResult).empty(),
        "C08 zero-duration successful D6 call must publish empty complete lineage");
    GameState c08Failure = c08Stationary;
    c08Failure.balls[0].physicalDomain = PhysicalBallDomain::Airborne;
    c08Failure.balls[0].position.y =
        geometry.clothSupportedCenterYCm - 1.0f;
    const TableDepartureStepResult c08FailureResult =
        advanceTableDeparturePhysics(c08Failure, 0.05f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(!c08FailureResult.ok && c08FailureResult.events.empty() &&
        privateAnalyticSegments(c08FailureResult).empty(),
        "C08 every failed D6 call must discard private events and analytic lineage");

    const float c08SubEpsilonDuration = 0.5e-7f;
    GameState c08SubEpsilon = isolatedState(ballAt(0.0f, 120.0f, 0.0f,
        3.0f, 4.0f, 5.0f, PhysicalBallDomain::Airborne));
    const GameState c08SubEpsilonBefore = c08SubEpsilon;
    const TableDepartureStepResult c08SubEpsilonResult =
        advanceTableDeparturePhysics(c08SubEpsilon, c08SubEpsilonDuration,
            profile, PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(c08SubEpsilonResult.ok && c08SubEpsilonResult.events.empty() &&
        validPrivateAnalyticLineage(
            privateAnalyticSegments(c08SubEpsilonResult), {0},
            c08SubEpsilonDuration, profile.solver.penetrationSlopCm,
            profile.solver.toiToleranceSeconds) &&
        privateAnalyticSegments(c08SubEpsilonResult).size() == 1 &&
        privateAnalyticSegments(c08SubEpsilonResult)[0].startPositionCm.x ==
            c08SubEpsilonBefore.balls[0].position.x,
        "C08 positive sub-epsilon duration must still mutate and publish one complete ballistic interval");

    const auto expectTinySupportedLaunchReturn = [&](float initialVy) {
        GameState tinyLaunch = isolatedState(ballAt(0.0f,
            geometry.clothSupportedCenterYCm, 0.0f,
            0.0f, initialVy, 0.0f,
            PhysicalBallDomain::ClothSupported));
        const GameState tinyLaunchBefore = tinyLaunch;
        const float duration = 1.0e-4f;
        const double expectedReturnTime = 2.0 *
            static_cast<double>(initialVy) /
            -static_cast<double>(geometry.gravityYCmS2);
        const TableDepartureEarliestBatch tinyQuery =
            queryEarliestTableDepartureBatch(tinyLaunch, duration, profile,
                PhysicsBoundaryMode::ProductionTable);
        expect(tinyQuery.ok && tinyQuery.candidates.size() == 1 &&
            tinyQuery.candidates[0].kind ==
                TableDepartureEventKind::ClothReturn &&
            tinyQuery.candidates[0].firstBall == 0 &&
            tinyQuery.candidates[0].secondBall == -1 &&
            tinyQuery.candidates[0].timeSeconds > 0.0 &&
            sameTransactionalState(tinyLaunch, tinyLaunchBefore),
            "D6 earliest query must expose a tiny positive ClothReturn without mutating state");
        std::array<bool, kBallCount> tinyOwnership{};
        tinyOwnership[0] = true;
        GameState tinyAtEvent = tinyLaunchBefore;
        const TableDepartureStepResult tinyAdvance =
            advanceTableDepartureDomains(tinyAtEvent,
                tinyQuery.earliestTimeSeconds, profile,
                PhysicsBoundaryMode::ProductionTable, tinyOwnership);
        expect(tinyAdvance.ok && tinyAdvance.events.empty() &&
            tinyAtEvent.balls[0].physicalDomain ==
                PhysicalBallDomain::Airborne &&
            tinyAtEvent.balls[0].velocity.y < 0.0f &&
            anchoredPrivateBallLineage(tinyAdvance, 0,
                tinyLaunchBefore.balls[0], tinyAtEvent.balls[0],
                tinyQuery.earliestTimeSeconds, profile),
            "D6 no-event advance must stop at the queried TOI without consuming its transition");
        const double tinyAbsoluteTime = 3.0 +
            tinyQuery.earliestTimeSeconds;
        const TableDepartureStepResult tinyResolved =
            resolveTableDepartureBatch(tinyAtEvent, tinyQuery, profile,
                tinyAbsoluteTime, 7);
        expect(tinyResolved.ok && tinyResolved.events.size() == 1 &&
            tinyResolved.events[0].kind ==
                TableDepartureEventKind::ClothReturn &&
            tinyResolved.events[0].ordinal == 7 &&
            tinyResolved.events[0].absoluteWholeTickTimeSeconds ==
                tinyAbsoluteTime &&
            tinyResolved.events[0].transitionWitnessComplete &&
            tinyResolved.events[0].targetDomain ==
                PhysicalBallDomain::ClothSupported &&
            tinyAtEvent.balls[0].physicalDomain ==
                PhysicalBallDomain::ClothSupported &&
            tinyAtEvent.balls[0].velocity.y == 0.0f,
            "D6 single-batch resolver must consume only the queried endpoint transition");
        const TableDepartureStepResult tinyResult =
            advanceTableDeparturePhysics(tinyLaunch, duration, profile,
                PhysicsBoundaryMode::ProductionTable,
                profile.solver.maximumEventsPerTick);
        const double timeTolerance = std::max(1.0e-24,
            expectedReturnTime * 1.0e-6);
        expect(tinyResult.ok && tinyResult.events.size() == 1 &&
            tinyResult.events[0].kind ==
                TableDepartureEventKind::ClothReturn &&
            tinyResult.events[0].absoluteWholeTickTimeSeconds > 0.0 &&
            close(tinyResult.events[0].absoluteWholeTickTimeSeconds,
                expectedReturnTime, timeTolerance) &&
            tinyLaunch.balls[0].physicalDomain ==
                PhysicalBallDomain::ClothSupported &&
            tinyLaunch.balls[0].velocity.y == 0.0f &&
            anchoredPrivateBallLineage(tinyResult, 0,
                tinyLaunchBefore.balls[0], tinyLaunch.balls[0], duration,
                profile),
            "C08 strictly-positive sub-epsilon launch must consume its exact descending cloth return");
    };
    expectTinySupportedLaunchReturn(1.0e-4f);
    expectTinySupportedLaunchReturn(1.0e-12f);

    GameState c08TimeZeroContact = isolatedState(ballAt(0.0f,
        geometry.clothSupportedCenterYCm, 0.0f,
        0.0f, -1.0f, 0.0f, PhysicalBallDomain::Airborne));
    const GameState c08TimeZeroBefore = c08TimeZeroContact;
    const TableDepartureEarliestBatch c08TimeZeroQuery =
        queryEarliestTableDepartureBatch(c08TimeZeroContact, 0.0,
            profile, PhysicsBoundaryMode::ProductionTable);
    expect(c08TimeZeroQuery.ok &&
        c08TimeZeroQuery.candidates.size() == 1 &&
        c08TimeZeroQuery.earliestTimeSeconds == 0.0 &&
        c08TimeZeroQuery.candidates[0].kind ==
            TableDepartureEventKind::ClothReturn &&
        sameTransactionalState(c08TimeZeroContact, c08TimeZeroBefore),
        "D6 query must expose an exact time-zero descending contact without mutation");
    const TableDepartureStepResult c08TimeZeroResolve =
        resolveTableDepartureBatch(c08TimeZeroContact, c08TimeZeroQuery,
            profile, 2.0, 4);
    expect(c08TimeZeroResolve.ok &&
        c08TimeZeroResolve.events.size() == 1 &&
        c08TimeZeroContact.balls[0].physicalDomain ==
            PhysicalBallDomain::ClothSupported &&
        c08TimeZeroContact.balls[0].velocity.y == 0.0f,
        "D6 resolver must consume an exact active time-zero descending contact");
    TableDepartureEarliestBatch duplicateTimeZero = c08TimeZeroQuery;
    duplicateTimeZero.candidates.push_back(
        duplicateTimeZero.candidates.front());
    GameState duplicateTimeZeroState = c08TimeZeroBefore;
    const TableDepartureStepResult duplicateTimeZeroResult =
        resolveTableDepartureBatch(duplicateTimeZeroState,
            duplicateTimeZero, profile, 2.0, 4);
    expect(!duplicateTimeZeroResult.ok &&
        duplicateTimeZeroResult.events.empty() &&
        sameTransactionalState(duplicateTimeZeroState,
            c08TimeZeroBefore),
        "D6 duplicate same-ball batch must fail with whole-state rollback");

    GameState c08ZeroReaction = isolatedState(ballAt(0.0f,
        geometry.clothSupportedCenterYCm, 0.0f,
        2.0f, -3.0f, 4.0f, PhysicalBallDomain::ClothSupported));
    const GameState c08ZeroReactionBefore = c08ZeroReaction;
    const TableDepartureStepResult c08ZeroReactionResult =
        advanceTableDeparturePhysics(c08ZeroReaction, 0.0f, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    expect(c08ZeroReactionResult.ok && c08ZeroReactionResult.events.empty() &&
        privateAnalyticSegments(c08ZeroReactionResult).empty() &&
        sameTransactionalState(c08ZeroReaction, c08ZeroReactionBefore),
        "C08 zero duration must not apply a supported reaction or mutate any state");

    PhysicsProfile c08StopProfile = profile;
    c08StopProfile.tableDeparture.supportedRollingDecelerationCmS2 = 12.5f;
    c08StopProfile.surface.torsionalSpinDecelerationRadS2 = 12.0f;
    const float c08StopDuration = 0.2f;
    const float c08StopSpeed = 1.0f;
    const double c08StopTime = c08StopSpeed /
        c08StopProfile.tableDeparture.supportedRollingDecelerationCmS2;
    GameState c08Stop = isolatedState(ballAt(0.0f,
        geometry.clothSupportedCenterYCm, 0.0f,
        c08StopSpeed, 0.0f, 0.0f,
        PhysicalBallDomain::ClothSupported));
    c08Stop.balls[0].motionState = BallMotionState::Rolling;
    const GameState c08StopBefore = c08Stop;
    const TableDepartureStepResult c08StopResult =
        advanceTableDeparturePhysics(c08Stop, c08StopDuration,
            c08StopProfile, PhysicsBoundaryMode::ProductionTable,
            c08StopProfile.solver.maximumEventsPerTick);
    const auto& c08StopSegments = privateAnalyticSegments(c08StopResult);
    const bool c08StopPiecewise = c08StopResult.ok &&
        c08StopSegments.size() == 2 &&
        c08StopSegments[0].motionState == BallMotionState::Rolling &&
        c08StopSegments[1].motionState == BallMotionState::Stationary &&
        close(c08StopSegments[0].durationSeconds, c08StopTime,
            c08StopProfile.solver.toiToleranceSeconds) &&
        close(c08StopSegments[1].wholeTickStartOffsetSeconds, c08StopTime,
            c08StopProfile.solver.toiToleranceSeconds) &&
        close(c08StopSegments[1].durationSeconds,
            c08StopDuration - c08StopTime,
            c08StopProfile.solver.toiToleranceSeconds) &&
        c08StopSegments[0].constantAccelerationCmS2.x == -12.5f &&
        c08StopSegments[1].startVelocityCmS.x == 0.0f &&
        c08StopSegments[1].constantAccelerationCmS2.x == 0.0f;
    expect(c08StopPiecewise,
        "C08 supported deceleration crossing stop time must emit exact moving and stationary pieces");
    const auto expectSupportedSpinStop = [&](PhysicalBallDomain domain,
                                              float x,
                                              float y) {
        GameState state = isolatedState(ballAt(x, y, 30.0f,
            0.0f, 0.0f, 0.0f, domain));
        state.balls[0].angularVelocity.y = 1.0f;
        const float beforeAngle = state.balls[0].rotationAngle;
        const TableDepartureStepResult result =
            advanceTableDeparturePhysics(state, 0.2f, c08StopProfile,
                PhysicsBoundaryMode::ProductionTable,
                c08StopProfile.solver.maximumEventsPerTick);
        return result.ok && result.events.empty() &&
            state.balls[0].physicalDomain == domain &&
            state.balls[0].angularVelocity.y == 0.0f &&
            state.balls[0].rotationAngle > beforeAngle;
    };
    expect(expectSupportedSpinStop(PhysicalBallDomain::RailTopSupported,
            70.0f, geometry.railTopSupportedCenterYCm) &&
        expectSupportedSpinStop(
            PhysicalBallDomain::OutsideLandingSupported,
            geometry.outerHalfWidthCm + 5.0f,
            geometry.outsideLandingSupportedCenterYCm),
        "D6 rail-top and outside supported owners must dissipate torsional spin under the sealed surface law");
    GameState c08LandingSpin = isolatedState(ballAt(0.0f,
        geometry.clothSupportedCenterYCm + 1.0f, 30.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne));
    c08LandingSpin.balls[0].angularVelocity.y = 1.0f;
    const double c08LandingSpinRoot = independentEarliestDescendingPlaneRoot(
        c08LandingSpin.balls[0], geometry.clothSupportedCenterYCm, 0.2,
        geometry.gravityYCmS2);
    const TableDepartureStepResult c08LandingSpinResult =
        advanceTableDeparturePhysics(c08LandingSpin, 0.2f,
            c08StopProfile, PhysicsBoundaryMode::ProductionTable,
            c08StopProfile.solver.maximumEventsPerTick);
    expect(c08LandingSpinResult.ok &&
        std::isfinite(c08LandingSpinRoot) &&
        c08LandingSpinRoot + 1.0 /
            c08StopProfile.surface.torsionalSpinDecelerationRadS2 < 0.2 &&
        countTableDepartureEvents(c08LandingSpinResult,
            TableDepartureEventKind::ClothReturn) == 1 &&
        c08LandingSpin.balls[0].physicalDomain ==
            PhysicalBallDomain::ClothSupported &&
        c08LandingSpin.balls[0].angularVelocity.y == 0.0f,
        "D6 airborne spin must remain free until landing and then dissipate over the supported remainder");
    GameState c08TinyResidualStop = isolatedState(ballAt(0.0f,
        geometry.clothSupportedCenterYCm, 0.0f,
        0.0100357616f, 0.0f, 0.0158164334f,
        PhysicalBallDomain::ClothSupported));
    const TableDepartureStepResult c08TinyResidualStopResult =
        advanceTableDeparturePhysics(c08TinyResidualStop, 0.01f,
            c08StopProfile, PhysicsBoundaryMode::ProductionTable,
            c08StopProfile.solver.maximumEventsPerTick);
    const auto& c08TinyResidualStopSegments =
        privateAnalyticSegments(c08TinyResidualStopResult);
    expect(c08TinyResidualStopResult.ok &&
        c08TinyResidualStopSegments.size() == 2 &&
        c08TinyResidualStopSegments[1].motionState ==
            BallMotionState::Stationary &&
        c08TinyResidualStopSegments[1].startVelocityCmS.x == 0.0f &&
        c08TinyResidualStopSegments[1].startVelocityCmS.y == 0.0f &&
        c08TinyResidualStopSegments[1].startVelocityCmS.z == 0.0f &&
        c08TinyResidualStop.balls[0].velocity.x == 0.0f &&
        c08TinyResidualStop.balls[0].velocity.y == 0.0f &&
        c08TinyResidualStop.balls[0].velocity.z == 0.0f &&
        c08TinyResidualStop.balls[0].speed == 0.0f,
        "C08 supported stop must canonicalize residual float velocity before stationary lineage");
    if (c08StopPiecewise) {
        std::vector<SurfaceMotionAnalyticSegment> collapsed = c08StopSegments;
        collapsed[0].durationSeconds = c08StopDuration;
        collapsed[0].endPositionCm = collapsed[1].endPositionCm;
        collapsed.erase(collapsed.begin() + 1);
        expect(!validPrivateAnalyticLineage(collapsed, {0},
                c08StopDuration, c08StopProfile.solver.penetrationSlopCm,
                c08StopProfile.solver.toiToleranceSeconds),
            "C08 collapse of moving and stationary supported pieces must fail");
        std::vector<SurfaceMotionAnalyticSegment> wrongMotion =
            c08StopSegments;
        wrongMotion[0].motionState = BallMotionState::Sliding;
        expect(!validPrivateAnalyticLineage(wrongMotion, {0},
                c08StopDuration, c08StopProfile.solver.penetrationSlopCm,
                c08StopProfile.solver.toiToleranceSeconds),
            "C08 motionState mutation must fail semantic validation");
    }

    GameState c08Unbounded = isolatedState(ballAt(0.0f, 120.0f, 0.0f,
        4.0f, 5.0f, 6.0f, PhysicalBallDomain::Airborne));
    const GameState c08UnboundedBefore = c08Unbounded;
    const TableDepartureStepResult c08UnboundedResult =
        advanceTableDeparturePhysics(c08Unbounded, 0.01f, profile,
            PhysicsBoundaryMode::Unbounded,
            profile.solver.maximumEventsPerTick);
    expect(c08UnboundedResult.ok && c08UnboundedResult.events.empty() &&
        validPrivateAnalyticLineage(privateAnalyticSegments(
                c08UnboundedResult), {0}, 0.01,
            profile.solver.penetrationSlopCm,
            profile.solver.toiToleranceSeconds) &&
        c08Unbounded.balls[0].position.x !=
            c08UnboundedBefore.balls[0].position.x,
        "C08 Unbounded positive-duration call must record its actual domain motion without table events");

    expect(anchoredPrivateBallLineage(c08SubEpsilonResult, 0,
            c08SubEpsilonBefore.balls[0], c08SubEpsilon.balls[0],
            c08SubEpsilonDuration, profile) &&
        anchoredPrivateBallLineage(c08StopResult, 0,
            c08StopBefore.balls[0], c08Stop.balls[0],
            c08StopDuration, c08StopProfile) &&
        anchoredPrivateBallLineage(c08UnboundedResult, 0,
            c08UnboundedBefore.balls[0], c08Unbounded.balls[0], 0.01,
            profile),
        "C08 producer lineage must bind first state, final state and sealed acceleration laws");
    if (!privateAnalyticSegments(c08SubEpsilonResult).empty()) {
        TableDepartureStepResult translated = c08SubEpsilonResult;
        for (auto& segment : translated.analyticSegments) {
            segment.startPositionCm.x += 1.0f;
            segment.endPositionCm.x += 1.0f;
        }
        expect(validPrivateAnalyticLineage(translated.analyticSegments, {0},
                c08SubEpsilonDuration, profile.solver.penetrationSlopCm,
                profile.solver.toiToleranceSeconds) &&
            !anchoredPrivateBallLineage(translated, 0,
                c08SubEpsilonBefore.balls[0], c08SubEpsilon.balls[0],
                c08SubEpsilonDuration, profile),
            "C08 whole-trajectory translation must be rejected by before/after anchors");
        TableDepartureStepResult alteredGravity = c08SubEpsilonResult;
        alteredGravity.analyticSegments[0].constantAccelerationCmS2.y += 1.0f;
        const double t = alteredGravity.analyticSegments[0].durationSeconds;
        alteredGravity.analyticSegments[0].endPositionCm.y =
            static_cast<float>(alteredGravity.analyticSegments[0].
                startPositionCm.y + alteredGravity.analyticSegments[0].
                startVelocityCmS.y * t + 0.5 * alteredGravity.
                analyticSegments[0].constantAccelerationCmS2.y * t * t);
        expect(!anchoredPrivateBallLineage(alteredGravity, 0,
                c08SubEpsilonBefore.balls[0], c08SubEpsilon.balls[0],
                c08SubEpsilonDuration, profile),
            "C08 changed gravity with self-consistent endpoint must still fail sealed-law validation");
    }

    const auto eventOfKind = [](const TableDepartureStepResult& candidate,
        TableDepartureEventKind kind) -> const TableDepartureEvent* {
        for (const auto& event : candidate.events) {
            if (event.kind == kind) return &event;
        }
        return nullptr;
    };
    const TableDepartureEvent* c08Nose = eventOfKind(belowResult,
        TableDepartureEventKind::CushionNoseContact);
    const TableDepartureEvent* c08Outer = eventOfKind(twoEventResult,
        TableDepartureEventKind::OuterDeparture);
    const TableDepartureEvent* c08Rail = eventOfKind(railLandingResult,
        TableDepartureEventKind::RailTopLanding);
    const TableDepartureEvent* c08Outside = eventOfKind(outsideResult,
        TableDepartureEventKind::OutsideLanding);
    const TableDepartureEvent* c08Cloth = eventOfKind(clothLandingResult,
        TableDepartureEventKind::ClothReturn);
    const TableDepartureEvent* c08Pocket = eventOfKind(cornerResult,
        TableDepartureEventKind::PocketEntry);
    expect(c08Nose && c08Outer && c08Rail && c08Outside && c08Cloth &&
        c08Pocket &&
        privateBoundary(belowResult, *c08Nose, true,
            profile.solver.toiToleranceSeconds) &&
        privateBoundary(twoEventResult, *c08Outer, true,
            profile.solver.toiToleranceSeconds) &&
        privateBoundary(railLandingResult, *c08Rail, true,
            profile.solver.toiToleranceSeconds) &&
        privateBoundary(outsideResult, *c08Outside, true,
            profile.solver.toiToleranceSeconds) &&
        privateBoundary(clothLandingResult, *c08Cloth, true,
            profile.solver.toiToleranceSeconds) &&
        privateBoundary(cornerResult, *c08Pocket, false,
            profile.solver.toiToleranceSeconds),
        "C08 all six table event kinds must terminate the actual pre-event interval and start only real nonterminal tails");
    expect(anchoredPrivateBallLineage(cornerResult, 0,
            cornerPocketInitial, cornerPocket, 0.1, profile),
        "C08 PocketEntry lineage must terminate exactly at the entry TOI and bind the actual terminal state without a fake tail");

    const auto terminalWitnessCase = [&](BallState initial, double duration,
        TableDepartureEventKind kind) {
        const BallState before = initial;
        const TableDepartureStepResult terminal =
            advanceTableDepartureMotion(initial, static_cast<float>(duration),
                profile);
        const TableDepartureEvent* event = eventOfKind(terminal, kind);
        if (!terminal.ok || !event || !event->transitionWitnessComplete ||
            !anchoredPrivateBallLineage(terminal, 0, before, initial,
                static_cast<float>(duration), profile)) return false;
        TableDepartureStepResult velocityMutation = terminal;
        for (auto& candidate : velocityMutation.events) {
            if (candidate.kind == kind)
                candidate.velocityAfterCmS.x += 1.0f;
        }
        TableDepartureStepResult domainMutation = terminal;
        for (auto& candidate : domainMutation.events) {
            if (candidate.kind == kind)
                candidate.targetDomain = candidate.targetDomain ==
                        PhysicalBallDomain::Airborne ?
                    PhysicalBallDomain::ClothSupported :
                    PhysicalBallDomain::Airborne;
        }
        return !anchoredPrivateBallLineage(velocityMutation, 0, before,
                initial, static_cast<float>(duration), profile) &&
            !anchoredPrivateBallLineage(domainMutation, 0, before, initial,
                static_cast<float>(duration), profile);
    };
    expect(terminalWitnessCase(noseBelowInitial, noseTargetTime,
            TableDepartureEventKind::CushionNoseContact) &&
        terminalWitnessCase(twoEventBall,
            c08Outer->absoluteWholeTickTimeSeconds,
            TableDepartureEventKind::OuterDeparture) &&
        terminalWitnessCase(railLandingInitial,
            c08Rail->absoluteWholeTickTimeSeconds,
            TableDepartureEventKind::RailTopLanding) &&
        terminalWitnessCase(outsideLandingInitial,
            c08Outside->absoluteWholeTickTimeSeconds,
            TableDepartureEventKind::OutsideLanding) &&
        terminalWitnessCase(clothLandingInitial, r01Root,
            TableDepartureEventKind::ClothReturn) &&
        terminalWitnessCase(cornerPocketInitial,
            c08Pocket->absoluteWholeTickTimeSeconds,
            TableDepartureEventKind::PocketEntry),
        "C08 every terminal table event must bind actual post-transition velocity/domain and reject witness mutations");

    GameState c08TerminalCross = isolatedState(r08UpAir);
    c08TerminalCross.balls[1] = r08UpSupported;
    c08TerminalCross.balls[1].pocketed = false;
    const GameState c08TerminalCrossBefore = c08TerminalCross;
    const float c08TerminalCrossDuration = std::nextafter(
        static_cast<float>(r08UpRoot.timeSeconds),
        std::numeric_limits<float>::infinity());
    const TableDepartureStepResult c08TerminalCrossResult =
        advanceTableDeparturePhysics(c08TerminalCross,
            c08TerminalCrossDuration, profile,
            PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
    const TableDepartureEvent* c08TerminalCrossEvent = crossDomainContact(
        c08TerminalCrossResult, 0, 1);
    bool c08TerminalCrossWitness = c08TerminalCrossResult.ok &&
        c08TerminalCrossEvent && c08TerminalCrossEvent->
            transitionWitnessComplete &&
        anchoredPrivateBallLineage(c08TerminalCrossResult, 0,
            c08TerminalCrossBefore.balls[0], c08TerminalCross.balls[0],
            c08TerminalCrossDuration, profile) &&
        anchoredPrivateBallLineage(c08TerminalCrossResult, 1,
            c08TerminalCrossBefore.balls[1], c08TerminalCross.balls[1],
            c08TerminalCrossDuration, profile);
    if (c08TerminalCrossWitness) {
        TableDepartureStepResult mutation = c08TerminalCrossResult;
        mutation.events[0].secondVelocityAfterCmS.x += 1.0f;
        c08TerminalCrossWitness = !anchoredPrivateBallLineage(mutation, 1,
            c08TerminalCrossBefore.balls[1], c08TerminalCross.balls[1],
            c08TerminalCrossDuration, profile);
    }
    expect(c08TerminalCrossWitness,
        "C08 terminal cross must bind both participants' actual resolver/reaction post-state");

    const auto& c08RailReturnSegments = privateAnalyticSegments(returnResult);
    const TableDepartureEvent* c08RailReturnCloth = eventOfKind(returnResult,
        TableDepartureEventKind::ClothReturn);
    bool c08RailEdgeContinuous = false;
    for (std::size_t index = 1; index < c08RailReturnSegments.size(); ++index) {
        const auto& beforeEdge = c08RailReturnSegments[index - 1];
        const auto& afterEdge = c08RailReturnSegments[index];
        if (beforeEdge.ballIndex == 0 && afterEdge.ballIndex == 0 &&
            beforeEdge.motionState == BallMotionState::Rolling &&
            afterEdge.motionState == BallMotionState::Sliding &&
            close(beforeEdge.wholeTickStartOffsetSeconds +
                    beforeEdge.durationSeconds,
                afterEdge.wholeTickStartOffsetSeconds,
                profile.solver.toiToleranceSeconds) &&
            segmentPointMatches(beforeEdge.endPositionCm,
                afterEdge.startPositionCm) &&
            close(beforeEdge.endPositionCm.y,
                geometry.railTopSupportedCenterYCm,
                profile.solver.penetrationSlopCm)) {
            c08RailEdgeContinuous = true;
        }
    }
    expect(c08RailEdgeContinuous && c08RailReturnCloth &&
        privateBoundary(returnResult, *c08RailReturnCloth, true,
            profile.solver.toiToleranceSeconds),
        "C08 rail edge must preserve position while releasing to a real ballistic drop before ClothReturn");

    const auto checkSupportedReaction = [&](PhysicalBallDomain domain,
        float supportedY, float x, float initialYVelocity,
        BallMotionState expectedMotion, PhysicalBallDomain expectedDomain) {
        GameState state = isolatedState(ballAt(x, supportedY, 20.0f,
            1.0f, initialYVelocity, 0.0f, domain));
        const TableDepartureStepResult reaction = advanceTableDeparturePhysics(
            state, 0.001f, profile, PhysicsBoundaryMode::ProductionTable,
            profile.solver.maximumEventsPerTick);
        const auto& segments = privateAnalyticSegments(reaction);
        return reaction.ok && !segments.empty() &&
            segments.front().startVelocityCmS.y ==
                (initialYVelocity < 0.0f ? 0.0f : initialYVelocity) &&
            segments.front().motionState == expectedMotion &&
            state.balls[0].physicalDomain == expectedDomain;
    };
    const float c08RailX = geometry.playfieldHalfWidthCm +
        geometry.ballRadiusCm + 1.0f;
    const float c08OutsideX = geometry.outerHalfWidthCm + 1.0f;
    expect(checkSupportedReaction(PhysicalBallDomain::ClothSupported,
            geometry.clothSupportedCenterYCm, 0.0f, -2.0f,
            BallMotionState::Rolling, PhysicalBallDomain::ClothSupported) &&
        checkSupportedReaction(PhysicalBallDomain::ClothSupported,
            geometry.clothSupportedCenterYCm, 0.0f, 2.0f,
            BallMotionState::Sliding, PhysicalBallDomain::Airborne) &&
        checkSupportedReaction(PhysicalBallDomain::RailTopSupported,
            geometry.railTopSupportedCenterYCm, c08RailX, -2.0f,
            BallMotionState::Rolling, PhysicalBallDomain::RailTopSupported) &&
        checkSupportedReaction(PhysicalBallDomain::RailTopSupported,
            geometry.railTopSupportedCenterYCm, c08RailX, 2.0f,
            BallMotionState::Sliding, PhysicalBallDomain::Airborne) &&
        checkSupportedReaction(PhysicalBallDomain::OutsideLandingSupported,
            geometry.outsideLandingSupportedCenterYCm, c08OutsideX, -2.0f,
            BallMotionState::Rolling,
            PhysicalBallDomain::OutsideLandingSupported) &&
        checkSupportedReaction(PhysicalBallDomain::OutsideLandingSupported,
            geometry.outsideLandingSupportedCenterYCm, c08OutsideX, 2.0f,
            BallMotionState::Sliding, PhysicalBallDomain::Airborne),
        "C08 Cloth/Rail/Outside supported reaction must canonicalize negative vy before recording and release positive vy as ballistic");

    GameState c08Repeat = seamPrefix;
    c08Repeat.balls[0].physicalDomain = PhysicalBallDomain::Airborne;
    const GameState c08RepeatBefore = c08Repeat;
    const TableDepartureStepResult c08RepeatResult =
        advanceTableDeparturePhysics(c08Repeat, seamTailDuration,
            seamProfile, PhysicsBoundaryMode::ProductionTable,
            seamProfile.solver.maximumEventsPerTick - 1);
    bool c08RepeatExact = c08RepeatResult.ok &&
        privateAnalyticSegments(c08RepeatResult).size() ==
            seamPrivateSegments.size();
    for (std::size_t index = 0; c08RepeatExact &&
            index < seamPrivateSegments.size(); ++index) {
        c08RepeatExact = samePrivateSegment(seamPrivateSegments[index],
            privateAnalyticSegments(c08RepeatResult)[index]);
    }
    const std::uint64_t c08Digest = privateAnalyticDigest(
        seamPrivateSegments);
    expect(c08RepeatExact && c08Digest == privateAnalyticDigest(
            privateAnalyticSegments(c08RepeatResult)) &&
        anchoredPrivateBallLineage(c08RepeatResult, 0,
            c08RepeatBefore.balls[0], c08Repeat.balls[0],
            seamTailDuration, seamProfile) &&
        anchoredPrivateBallLineage(c08RepeatResult, 1,
            c08RepeatBefore.balls[1], c08Repeat.balls[1],
            seamTailDuration, seamProfile),
        "C08 repeated producer execution must yield the exact ordered IEEE field digest");
    if (const char* printDigest = std::getenv("BILLIARDGL_PRINT_C08_DIGEST")) {
        if (std::string(printDigest) == "1")
            std::cout << "C08_PRIVATE_ANALYTIC_DIGEST " << std::hex <<
                c08Digest << std::dec << '\n';
    }

    const auto c08CrossPair = [&](double targetTime, double baseZ) {
        std::array<BallState, 2> pair;
        pair[0] = ballAt(0.0f, static_cast<float>(
                geometry.clothSupportedCenterYCm - x01ContactDy -
                0.5 * geometry.gravityYCmS2 * targetTime * targetTime),
            static_cast<float>(baseZ), 100.0f, 0.0f, 0.0f,
            PhysicalBallDomain::Airborne);
        pair[1] = ballAt(static_cast<float>(100.0 * targetTime +
                x01ContactDx), geometry.clothSupportedCenterYCm,
            static_cast<float>(baseZ + x01ContactDz),
            0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
        return pair;
    };
    const auto c08FirstPair = c08CrossPair(0.02, -40.0);
    const auto c08SecondPair = c08CrossPair(0.06, 40.0);
    GameState c08TwoCross = isolatedState(c08FirstPair[0]);
    c08TwoCross.balls[1] = c08FirstPair[1];
    c08TwoCross.balls[1].pocketed = false;
    c08TwoCross.balls[2] = c08SecondPair[0];
    c08TwoCross.balls[2].pocketed = false;
    c08TwoCross.balls[3] = c08SecondPair[1];
    c08TwoCross.balls[3].pocketed = false;
    const TableDepartureStepResult c08TwoCrossResult =
        advanceTableDeparturePhysics(c08TwoCross, 0.065f, crossProfile,
            PhysicsBoundaryMode::ProductionTable,
            crossProfile.solver.maximumEventsPerTick);
    expect(c08TwoCrossResult.ok && c08TwoCrossResult.events.size() == 2 &&
        c08TwoCrossResult.events[0].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        c08TwoCrossResult.events[1].kind ==
            TableDepartureEventKind::CrossDomainBallContact &&
        c08TwoCrossResult.events[0].ordinal == 0 &&
        c08TwoCrossResult.events[1].ordinal == 1 &&
        c08TwoCrossResult.events[0].stableIdentity.rfind(
            "cross_domain_ball_contact:0:1:0:", 0) == 0 &&
        c08TwoCrossResult.events[1].stableIdentity.rfind(
            "cross_domain_ball_contact:2:3:1:", 0) == 0 &&
        close(c08TwoCrossResult.events[0].absoluteWholeTickTimeSeconds,
            0.02, crossProfile.solver.toiToleranceSeconds) &&
        close(c08TwoCrossResult.events[1].absoluteWholeTickTimeSeconds,
            0.06, crossProfile.solver.toiToleranceSeconds),
        "C08 recursive second cross must rebase time and rebuild exact ordinal-bearing stable identity");

    const bool c08FailureMatrix = !rollbackResult.ok &&
        rollbackResult.events.empty() && rollbackResult.analyticSegments.empty() &&
        sameTransactionalBall(rollback, rollbackBefore) &&
        !r07Result.ok && r07Result.events.empty() &&
        r07Result.analyticSegments.empty() &&
        sameTransactionalState(r07State, r07Before) &&
        !exhaustedResult.ok && exhaustedResult.events.empty() &&
        exhaustedResult.analyticSegments.empty() &&
        sameTransactionalState(exhaustedState, exhaustedBefore) &&
        !tieResult.ok && tieResult.events.empty() &&
        tieResult.analyticSegments.empty() &&
        sameTransactionalState(tieState, tieBefore) &&
        !r08BudgetResult.ok && r08BudgetResult.events.empty() &&
        r08BudgetResult.analyticSegments.empty() &&
        sameTransactionalState(r08BudgetState, r08BudgetBefore);
    expect(c08FailureMatrix,
        "C08 invalid geometry, ambiguity, budget and recursive failures must all discard private lineage");

    GameState seamShortManual = seamPrefix;
    seamShortManual.balls[0].physicalDomain = PhysicalBallDomain::Airborne;
    const float seamShortWholeDuration = 0.03f;
    const TableDepartureStepResult seamShortTail =
        advanceTableDeparturePhysics(seamShortManual,
            seamShortWholeDuration - seamPrefixDuration, seamProfile,
            PhysicsBoundaryMode::ProductionTable,
            seamProfile.solver.maximumEventsPerTick - 1);
    expect(seamShortTail.ok && seamShortTail.events.empty() &&
        seamShortManual.balls[0].physicalDomain ==
            PhysicalBallDomain::Airborne &&
        seamShortManual.balls[0].position.y > geometry.clothSupportedCenterYCm,
        "C02 exact post-island tail must retain the independently airborne release before landing");

    PhysicsPlaneObservationRequest seamPlane;
    seamPlane.observationId = "d6a_same_tick_seam_x";
    seamPlane.ballIndex = 0;
    seamPlane.axis = PhysicsPlaneAxis::Y;
    seamPlane.planeCoordinateCm =
        seamPrefix.balls[0].position.y + 0.1;
    seamPlane.direction = PhysicsPlaneCrossingDirection::NegativeToPositive;
    const double seamPlaneTailRoot = independentEarliestPositivePlaneRoot(
        seamPrefix.balls[0].position.y, seamPrefix.balls[0].velocity.y,
        seamPlane.planeCoordinateCm, seamTailDuration,
        geometry.gravityYCmS2);
    expect(std::isfinite(seamPlaneTailRoot) && seamPlaneTailRoot > 0.0 &&
        seamPlaneTailRoot < seamReturnRoot,
        "C08 independent ballistic root must place the requested crossing after handoff and before return");
    GameState seamWhole = seamInitial;
    const PhysicsStepTelemetry seamWholeTelemetry = updatePhysics(seamWhole,
        seamWholeDuration, seamProfile, PhysicsBoundaryMode::ProductionTable,
        {seamPlane});
    GameState seamTelemetryOracleState = seamInitial;
    const PhysicsStepTelemetry seamTelemetryOracle = updatePhysics(
        seamTelemetryOracleState, seamWholeDuration, seamProfile,
        PhysicsBoundaryMode::ProductionTable, {seamPlane});
    const auto contactOfKind = [](const PhysicsStepTelemetry& telemetry,
                                   PhysicsContactKind kind) {
        return std::find_if(telemetry.contacts.begin(), telemetry.contacts.end(),
            [kind](const PhysicsContactRecord& contact) {
                return contact.kind == kind;
            });
    };
    const auto ballContact = contactOfKind(seamWholeTelemetry,
        PhysicsContactKind::BallBall);
    const auto ballContactOracle = contactOfKind(seamTelemetryOracle,
        PhysicsContactKind::BallBall);
    const auto supportContact = contactOfKind(seamWholeTelemetry,
        PhysicsContactKind::Support);
    const auto supportContactOracle = contactOfKind(seamTelemetryOracle,
        PhysicsContactKind::Support);
    const bool seamBallContactSame =
        ballContact != seamWholeTelemetry.contacts.end() &&
        ballContactOracle != seamTelemetryOracle.contacts.end() &&
        sameLegacyContact(*ballContact, *ballContactOracle);
    const bool seamSupportContactSame =
        supportContact != seamWholeTelemetry.contacts.end() &&
        supportContactOracle != seamTelemetryOracle.contacts.end() &&
        sameLegacyContact(*supportContact, *supportContactOracle);
    bool seamAllContactsSame = seamWholeTelemetry.contacts.size() ==
        seamTelemetryOracle.contacts.size();
    for (std::size_t index = 0;
            seamAllContactsSame && index < seamWholeTelemetry.contacts.size();
            ++index) {
        seamAllContactsSame = sameLegacyContact(
            seamWholeTelemetry.contacts[index],
            seamTelemetryOracle.contacts[index]);
    }
    bool seamSolverSame = seamWholeTelemetry.solverEvents.size() ==
        seamTelemetryOracle.solverEvents.size();
    for (std::size_t index = 0;
            seamSolverSame && index < seamWholeTelemetry.solverEvents.size();
            ++index) {
        seamSolverSame = sameLegacySolverEvent(
            seamWholeTelemetry.solverEvents[index],
            seamTelemetryOracle.solverEvents[index]);
    }
    const int sharedEventId = ballContact != seamWholeTelemetry.contacts.end()
        ? ballContact->solverEventId : -1;
    const int sharedIslandId = ballContact != seamWholeTelemetry.contacts.end()
        ? ballContact->solverIslandId : -1;
    const double sharedTime = ballContact != seamWholeTelemetry.contacts.end()
        ? ballContact->wholeTickTimeOfImpactSeconds : -1.0;
    int sharedSupportCount = 0;
    int sharedPositiveSupportCount = 0;
    for (const auto& contact : seamWholeTelemetry.contacts) {
        if (contact.kind != PhysicsContactKind::Support ||
            contact.solverEventId != sharedEventId ||
            contact.solverIslandId != sharedIslandId ||
            !close(contact.wholeTickTimeOfImpactSeconds, sharedTime,
                seamProfile.solver.toiToleranceSeconds))
            continue;
        ++sharedSupportCount;
        if (contact.normalImpulseNs > 0.0) ++sharedPositiveSupportCount;
    }
    const auto sharedSolverEvent = std::find_if(
        seamWholeTelemetry.solverEvents.begin(),
        seamWholeTelemetry.solverEvents.end(),
        [sharedEventId, sharedIslandId](const auto& event) {
            return event.eventId == sharedEventId &&
                event.islandId == sharedIslandId;
        });
    const bool seamSupportIslandContract =
        seamWholeTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        ballContact != seamWholeTelemetry.contacts.end() &&
        ballContactOracle != seamTelemetryOracle.contacts.end() &&
        supportContact != seamWholeTelemetry.contacts.end() &&
        supportContactOracle != seamTelemetryOracle.contacts.end() &&
        std::count_if(seamWholeTelemetry.contacts.begin(),
            seamWholeTelemetry.contacts.end(), [](const auto& contact) {
                return contact.kind == PhysicsContactKind::BallBall;
            }) == 1 &&
        sharedSupportCount == 2 && sharedPositiveSupportCount == 1 &&
        ballContact->firstBall == 0 &&
        ballContact->secondBall == 1 &&
        ballContact->velocityImpulseApplied &&
        close(ballContact->wholeTickTimeOfImpactSeconds,
            seamRoot.timeSeconds, seamProfile.solver.toiToleranceSeconds) &&
        sharedSolverEvent != seamWholeTelemetry.solverEvents.end() &&
        sharedSolverEvent->contactCount == 3 &&
        std::string(sharedSolverEvent->failureCode) == "converged" &&
        seamBallContactSame && seamSupportContactSame &&
        seamAllContactsSame && seamSolverSame &&
        sameTransactionalState(seamWhole, seamTelemetryOracleState);
    if (!seamSupportIslandContract) {
        std::cerr << "C07 support-island status="
                  << static_cast<int>(seamWholeTelemetry.stepStatus)
                  << " contacts=" << seamWholeTelemetry.contacts.size()
                  << '/' << seamTelemetryOracle.contacts.size()
                  << " events=" << seamWholeTelemetry.solverEvents.size()
                  << '/' << seamTelemetryOracle.solverEvents.size()
                  << " ballSame=" << seamBallContactSame
                  << " supportSame=" << seamSupportContactSame
                  << " solverSame=" << seamSolverSame
                  << " stateSame="
                  << sameTransactionalState(seamWhole,
                         seamTelemetryOracleState)
                  << '\n';
        for (const auto& contact : seamWholeTelemetry.contacts) {
            std::cerr << " C07 contact kind=" << static_cast<int>(contact.kind)
                      << " pair=" << contact.firstBall << ':'
                      << contact.secondBall << " event="
                      << contact.solverEventId << " island="
                      << contact.solverIslandId << " toi="
                      << contact.wholeTickTimeOfImpactSeconds << " impulse="
                      << contact.normalImpulseNs << '\n';
        }
        for (const auto& event : seamWholeTelemetry.solverEvents) {
            std::cerr << " C07 solver event=" << event.eventId
                      << " island=" << event.islandId
                      << " contacts=" << event.contactCount
                      << " code=" << event.failureCode << '\n';
        }
    }
    expect(seamSupportIslandContract,
        "C07 handoff must retain one exact ball contact plus its explicit support constraint");
    bool seamPlaneContinuous =
        seamWholeTelemetry.planeAttestations.size() == 1 &&
        seamWholeTelemetry.planeAttestations[0].complete &&
        seamWholeTelemetry.planeCrossings.size() == 1 &&
        seamWholeTelemetry.analyticSurfaceMotion.size() >= 4;
    bool seamSegmentLedgerContinuous = true;
    int seamObservedBallSegmentCount = -1;
    for (std::size_t index = 0;
            index < seamWholeTelemetry.analyticSurfaceMotion.size(); ++index) {
        seamSegmentLedgerContinuous = seamSegmentLedgerContinuous &&
            seamWholeTelemetry.analyticSurfaceMotion[index].segmentOrdinal ==
                static_cast<int>(index);
    }
    for (int ballIndex : {0, 1}) {
        double coveredUntil = 0.0;
        int segmentCount = 0;
        Point3 previousEnd;
        bool havePreviousEnd = false;
        for (const SurfaceMotionAnalyticSegment& segment :
             seamWholeTelemetry.analyticSurfaceMotion) {
            if (segment.ballIndex != ballIndex) continue;
            const Point3 integratedEnd{
                static_cast<float>(segment.startPositionCm.x +
                    segment.startVelocityCmS.x * segment.durationSeconds +
                    0.5 * segment.constantAccelerationCmS2.x *
                        segment.durationSeconds * segment.durationSeconds),
                static_cast<float>(segment.startPositionCm.y +
                    segment.startVelocityCmS.y * segment.durationSeconds +
                    0.5 * segment.constantAccelerationCmS2.y *
                        segment.durationSeconds * segment.durationSeconds),
                static_cast<float>(segment.startPositionCm.z +
                    segment.startVelocityCmS.z * segment.durationSeconds +
                    0.5 * segment.constantAccelerationCmS2.z *
                        segment.durationSeconds * segment.durationSeconds)};
            seamSegmentLedgerContinuous = seamSegmentLedgerContinuous &&
                segment.analyticPathComplete && segment.durationSeconds > 0.0 &&
                std::fabs(segment.wholeTickStartOffsetSeconds - coveredUntil) <=
                    seamProfile.solver.toiToleranceSeconds &&
                close(segment.endPositionCm.x, integratedEnd.x,
                    seamProfile.solver.penetrationSlopCm) &&
                close(segment.endPositionCm.y, integratedEnd.y,
                    seamProfile.solver.penetrationSlopCm) &&
                close(segment.endPositionCm.z, integratedEnd.z,
                    seamProfile.solver.penetrationSlopCm) &&
                (!havePreviousEnd ||
                    (close(segment.startPositionCm.x, previousEnd.x,
                         seamProfile.solver.penetrationSlopCm) &&
                     close(segment.startPositionCm.y, previousEnd.y,
                         seamProfile.solver.penetrationSlopCm) &&
                     close(segment.startPositionCm.z, previousEnd.z,
                         seamProfile.solver.penetrationSlopCm)));
            coveredUntil = segment.wholeTickStartOffsetSeconds +
                segment.durationSeconds;
            previousEnd = segment.endPositionCm;
            havePreviousEnd = true;
            ++segmentCount;
        }
        seamSegmentLedgerContinuous = seamSegmentLedgerContinuous &&
            segmentCount >= 2 &&
            std::fabs(coveredUntil - seamWholeDuration) <=
                seamProfile.solver.toiToleranceSeconds;
        if (ballIndex == seamPlane.ballIndex)
            seamObservedBallSegmentCount = segmentCount;
    }
    bool crossingOwnedByExactlyOneSegment = false;
    double seamIndependentCrossingTime =
        std::numeric_limits<double>::quiet_NaN();
    if (seamWholeTelemetry.planeCrossings.size() == 1) {
        int containingSegments = 0;
        int containingOrdinal = -1;
        const double crossingTime =
            seamWholeTelemetry.planeCrossings[0].wholeTickTimeOfImpactSeconds;
        for (const SurfaceMotionAnalyticSegment& segment :
             seamWholeTelemetry.analyticSurfaceMotion) {
            if (segment.ballIndex != seamPlane.ballIndex) continue;
            const double end = segment.wholeTickStartOffsetSeconds +
                segment.durationSeconds;
            const bool finalSegment =
                std::fabs(end - seamWholeDuration) <=
                    seamProfile.solver.toiToleranceSeconds;
            if (crossingTime >= segment.wholeTickStartOffsetSeconds &&
                (crossingTime < end || (finalSegment && crossingTime <= end))) {
                ++containingSegments;
                containingOrdinal = segment.segmentOrdinal;
                const double localRoot = independentEarliestPositivePlaneRoot(
                    segment.startPositionCm.y,
                    segment.startVelocityCmS.y,
                    seamPlane.planeCoordinateCm,
                    segment.durationSeconds,
                    segment.constantAccelerationCmS2.y);
                if (std::isfinite(localRoot)) {
                    seamIndependentCrossingTime =
                        segment.wholeTickStartOffsetSeconds + localRoot;
                }
            }
        }
        crossingOwnedByExactlyOneSegment = containingSegments == 1 &&
            std::isfinite(seamIndependentCrossingTime) &&
            close(crossingTime, seamIndependentCrossingTime,
                seamProfile.solver.toiToleranceSeconds) &&
            seamWholeTelemetry.planeCrossings[0].segmentOrdinal ==
                containingOrdinal &&
            seamWholeTelemetry.planeCrossings[0].ballIndex ==
                seamPlane.ballIndex &&
            seamWholeTelemetry.planeCrossings[0].observationId ==
                seamPlane.observationId &&
            seamWholeTelemetry.planeCrossings[0].complete &&
            std::string(seamWholeTelemetry.planeCrossings[0].origin) ==
                "AUTHORITATIVE_CONTINUOUS_SURFACE_INTEGRATOR";
    }
    seamPlaneContinuous = seamPlaneContinuous &&
        crossingOwnedByExactlyOneSegment;
    const auto seamAttestationHasExactCount = [&](int reportedCount) {
        return seamObservedBallSegmentCount >= 2 &&
            reportedCount == seamObservedBallSegmentCount;
    };
    const bool seamAttestationExact =
        seamWholeTelemetry.planeAttestations.size() == 1 &&
        seamWholeTelemetry.planeAttestations[0].observationId ==
            seamPlane.observationId &&
        seamWholeTelemetry.planeAttestations[0].ballIndex ==
            seamPlane.ballIndex &&
        close(seamWholeTelemetry.planeAttestations[0].wholeTickDurationSeconds,
            seamWholeDuration, seamProfile.solver.toiToleranceSeconds) &&
        seamAttestationHasExactCount(
            seamWholeTelemetry.planeAttestations[0].coveredSegmentCount) &&
        seamWholeTelemetry.planeAttestations[0].crossingRecordCount == 1 &&
        seamWholeTelemetry.planeAttestations[0].complete;
    expect(!seamAttestationHasExactCount(seamObservedBallSegmentCount - 1) &&
        !seamAttestationHasExactCount(seamObservedBallSegmentCount + 1),
        "C08 attestation count checker must reject exact-ledger ±1 mutations");
    (void)seamPlaneContinuous;
    expect(seamWholeTelemetry.planeAttestations.size() == 1 &&
        seamWholeTelemetry.planeAttestations[0].complete &&
        seamWholeTelemetry.analyticSurfaceMotion.size() >= 4,
        "C08 handoff plane evidence must cover prefix/tail in absolute time without gaps");
    const bool seamWholeSplitEquivalent = seamPlaneContinuous &&
        seamAttestationExact &&
        seamSegmentLedgerContinuous && crossingOwnedByExactlyOneSegment &&
        seamAllContactsSame && seamSolverSame &&
        sameTransactionalState(seamWhole, seamTelemetryOracleState);
    if (!seamWholeSplitEquivalent) {
        std::cerr << "C08 seam diagnostics: plane=" << seamPlaneContinuous
                  << " attestation=" << seamAttestationExact
                  << " ledger=" << seamSegmentLedgerContinuous
                  << " owner=" << crossingOwnedByExactlyOneSegment
                  << " crossings="
                  << seamWholeTelemetry.planeCrossings.size()
                  << " attestations="
                  << seamWholeTelemetry.planeAttestations.size()
                  << " segments="
                  << seamWholeTelemetry.analyticSurfaceMotion.size();
        if (!seamWholeTelemetry.planeCrossings.empty()) {
            const auto& crossing = seamWholeTelemetry.planeCrossings[0];
            std::cerr << " crossing_time="
                      << crossing.wholeTickTimeOfImpactSeconds
                      << " crossing_ordinal=" << crossing.segmentOrdinal
                      << " expected_crossing_time="
                      << seamIndependentCrossingTime;
        }
        if (!seamWholeTelemetry.planeAttestations.empty()) {
            const auto& attestation = seamWholeTelemetry.planeAttestations[0];
            std::cerr << " covered=" << attestation.coveredSegmentCount
                      << " observed=" << seamObservedBallSegmentCount
                      << " reported_crossings="
                      << attestation.crossingRecordCount
                      << " complete=" << attestation.complete;
        }
        std::cerr << " whole_b0_y=" << seamWhole.balls[0].position.y
                  << " manual_b0_y=" << seamManual.balls[0].position.y
                  << " whole_b0_vy=" << seamWhole.balls[0].velocity.y
                  << " manual_b0_vy=" << seamManual.balls[0].velocity.y
                  << " whole_b0_domain="
                  << static_cast<int>(seamWhole.balls[0].physicalDomain)
                  << " manual_b0_domain="
                  << static_cast<int>(seamManual.balls[0].physicalDomain)
                  << " whole_b1_y=" << seamWhole.balls[1].position.y
                  << " manual_b1_y=" << seamManual.balls[1].position.y
                  << " whole_b1_vy=" << seamWhole.balls[1].velocity.y
                  << " manual_b1_vy=" << seamManual.balls[1].velocity.y
                  << " whole_b1_domain="
                  << static_cast<int>(seamWhole.balls[1].physicalDomain)
                  << " manual_b1_domain="
                  << static_cast<int>(seamManual.balls[1].physicalDomain)
                  << '\n';
    }
    // C03: planar/no-contact controls retain the legacy path exactly.
    GameState seamNoContact = isolatedState(ballAt(-20.0f,
        geometry.clothSupportedCenterYCm, 20.0f,
        10.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported));
    GameState seamNoContactOracle = seamNoContact;
    const PhysicsStepTelemetry seamNoContactTelemetry = updatePhysics(
        seamNoContact, seamWholeDuration, seamProfile,
        PhysicsBoundaryMode::Unbounded);
    const PhysicsStepTelemetry seamNoContactOracleTelemetry = updatePhysics(
        seamNoContactOracle, seamWholeDuration, seamProfile,
        PhysicsBoundaryMode::Unbounded);
    expect(sameTransactionalState(seamNoContact, seamNoContactOracle) &&
        seamNoContactTelemetry.contacts.empty() &&
        seamNoContactTelemetry.solverEvents.empty() &&
        seamNoContactTelemetry.analyticSurfaceMotion.size() ==
            seamNoContactOracleTelemetry.analyticSurfaceMotion.size(),
        "C03 no-handoff legacy control must remain transactionally and record equivalent");
    GameState seamHorizontal = seamInitial;
    seamHorizontal.balls[0].position.y = geometry.clothSupportedCenterYCm;
    seamHorizontal.balls[1].position.y = geometry.clothSupportedCenterYCm;
    GameState seamHorizontalRepeat = seamHorizontal;
    const PhysicsStepTelemetry seamHorizontalTelemetry = updatePhysics(
        seamHorizontal, seamWholeDuration, seamProfile,
        PhysicsBoundaryMode::Unbounded);
    const PhysicsStepTelemetry seamHorizontalRepeatTelemetry = updatePhysics(
        seamHorizontalRepeat, seamWholeDuration, seamProfile,
        PhysicsBoundaryMode::Unbounded);
    expect(sameTransactionalState(seamHorizontal, seamHorizontalRepeat) &&
        seamHorizontalTelemetry.contacts.size() == 1 &&
        seamHorizontalRepeatTelemetry.contacts.size() == 1 &&
        seamHorizontal.balls[0].velocity.y == 0.0f &&
        seamHorizontal.balls[1].velocity.y == 0.0f &&
        seamHorizontal.balls[0].physicalDomain ==
            PhysicalBallDomain::ClothSupported &&
        seamHorizontal.balls[1].physicalDomain ==
            PhysicalBallDomain::ClothSupported,
        "C03 horizontal legacy island must remain deterministic and never hand off");

    // C05/C06/C09/C10: direct invalid input and too-small shared budget must
    // remain atomic.  They are controls against repair/clamp/replay patches.
    PhysicsProfile seamBudgetProfile = seamProfile;
    seamBudgetProfile.solver.maximumEventsPerTick = 1;
    GameState seamBudgetPrefix = seamInitial;
    const PhysicsStepTelemetry seamBudgetPrefixTelemetry = updatePhysics(
        seamBudgetPrefix, seamPrefixDuration, seamBudgetProfile,
        PhysicsBoundaryMode::Unbounded);
    const bool seamBudgetHasRealPrefix =
        seamBudgetPrefixTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        seamBudgetPrefixTelemetry.contacts.size() == 1 &&
        seamBudgetPrefixTelemetry.solverEvents.size() == 1 &&
        sameLegacyContact(seamBudgetPrefixTelemetry.contacts[0],
            seamPrefixTelemetry.contacts[0]) &&
        sameLegacySolverEvent(seamBudgetPrefixTelemetry.solverEvents[0],
            seamPrefixTelemetry.solverEvents[0]);
    expect(seamBudgetHasRealPrefix,
        "C06 budget control must execute the same exact real legacy prefix");
    GameState seamBudget = seamInitial;
    const GameState seamBudgetBefore = seamBudget;
    const PhysicsStepTelemetry seamBudgetTelemetry = updatePhysics(seamBudget,
        seamWholeDuration, seamBudgetProfile,
        PhysicsBoundaryMode::ProductionTable);
    expect(seamBudgetTelemetry.stepStatus == PhysicsStepStatus::Failed &&
        seamBudgetTelemetry.failureCode == PhysicsFailureCode::EventBudget &&
        sameTransactionalState(seamBudget, seamBudgetBefore),
        "C06 shared prefix/tail event budget must fail with whole rollback");
    PhysicsProfile seamChildInvalidProfile = seamProfile;
    seamChildInvalidProfile.tableDeparture.clothSurfaceYCm =
        geometry.clothSurfaceYCm + 20.0f;
    seamChildInvalidProfile.tableDeparture.railTopSurfaceYCm += 20.0f;
    seamChildInvalidProfile.tableDeparture.outsideLandingSurfaceYCm += 20.0f;
    GameState seamChildInvalidPrefix = seamInitial;
    const PhysicsStepTelemetry seamChildInvalidPrefixTelemetry = updatePhysics(
        seamChildInvalidPrefix, seamPrefixDuration, seamChildInvalidProfile,
        PhysicsBoundaryMode::Unbounded);
    const bool seamChildInvalidHasRealPrefix =
        seamChildInvalidPrefixTelemetry.stepStatus ==
            PhysicsStepStatus::Succeeded &&
        seamChildInvalidPrefixTelemetry.contacts.size() == 1 &&
        seamChildInvalidPrefixTelemetry.solverEvents.size() == 1 &&
        seamChildInvalidPrefixTelemetry.contacts[0].firstBall == 0 &&
        seamChildInvalidPrefixTelemetry.contacts[0].secondBall == 1 &&
        seamChildInvalidPrefixTelemetry.contacts[0].velocityImpulseApplied &&
        seamChildInvalidPrefix.balls[0].velocity.y > 0.0f &&
        seamChildInvalidPrefix.balls[1].velocity.y < 0.0f;
    expect(seamChildInvalidHasRealPrefix,
        "C06 child InvalidState fixture must first execute the same real legacy island");
    GameState seamChildInvalid = seamInitial;
    const GameState seamChildInvalidBefore = seamChildInvalid;
    const PhysicsStepTelemetry seamChildInvalidTelemetry = updatePhysics(
        seamChildInvalid, seamWholeDuration, seamChildInvalidProfile,
        PhysicsBoundaryMode::ProductionTable);
    const bool seamChildInvalidPropagated = seamChildInvalidHasRealPrefix &&
        seamChildInvalidTelemetry.stepStatus == PhysicsStepStatus::Failed &&
        seamChildInvalidTelemetry.failureCode ==
            PhysicsFailureCode::PenetrationLimit &&
        seamChildInvalidTelemetry.failureCode != PhysicsFailureCode::EventBudget &&
        seamChildInvalidTelemetry.contacts.empty() &&
        seamChildInvalidTelemetry.solverEvents.empty() &&
        seamChildInvalidTelemetry.surfaceMotion.empty() &&
        seamChildInvalidTelemetry.analyticSurfaceMotion.empty() &&
        sameTransactionalState(seamChildInvalid, seamChildInvalidBefore);
    (void)seamChildInvalidPropagated;
    expect(validatePhysicsProfile(seamChildInvalidProfile).ok &&
        seamChildInvalidTelemetry.failureCode != PhysicsFailureCode::EventBudget,
        "C06 real post-prefix child validation failure must not map broadly to EventBudget");
    GameState seamInvalid = seamInitial;
    seamInvalid.balls[0].physicalDomain = PhysicalBallDomain::Airborne;
    seamInvalid.balls[0].position.y = geometry.clothSupportedCenterYCm - 0.01f;
    const GameState seamInvalidBefore = seamInvalid;
    const PhysicsStepTelemetry seamInvalidTelemetry = updatePhysics(
        seamInvalid, seamWholeDuration, seamProfile,
        PhysicsBoundaryMode::ProductionTable);
    expect(seamInvalidTelemetry.stepStatus == PhysicsStepStatus::Failed &&
        seamInvalidTelemetry.failureCode == PhysicsFailureCode::InvalidControls &&
        seamInvalidTelemetry.contacts.empty() &&
        seamInvalidTelemetry.solverEvents.empty() &&
        sameTransactionalState(seamInvalid, seamInvalidBefore),
        "C09/C10 genuine below-cloth public input must remain exact rollback, never repair");
    // C05: a second real legacy pair is independently timed to the first
    // D6 ClothReturn after the real handoff.  The two candidate roots share
    // one absolute whole-tick slot; ±1.01 tolerance controls prove this is an
    // equality/ordering contract rather than a broad failure fixture.
    auto c05StateForOffset = [&](double offsetSeconds) {
        GameState state = seamInitial;
        const float pairSpeed = 40.0f;
        const double pairRoot = seamRoot.timeSeconds + seamReturnRoot +
            offsetSeconds;
        state.balls[2] = ballAt(-30.0f,
            geometry.clothSupportedCenterYCm, -20.0f,
            pairSpeed, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
        state.balls[3] = ballAt(static_cast<float>(-30.0 +
                pairSpeed * pairRoot + 2.0 * seamProfile.ball.radiusCm),
            geometry.clothSupportedCenterYCm, -20.0f,
            0.0f, 0.0f, 0.0f, PhysicalBallDomain::ClothSupported);
        state.balls[2].pocketed = false;
        state.balls[3].pocketed = false;
        return state;
    };
    const IndependentSweptSphereRoot c05EqualLegacyRoot =
        independentEarliestSweptSphereRoot(
            c05StateForOffset(0.0).balls[2],
            c05StateForOffset(0.0).balls[3], seamWholeDuration,
            0.0, 2.0 * seamProfile.ball.radiusCm);
    const double c05AbsoluteD6Root = seamRoot.timeSeconds + seamReturnRoot;
    expect(c05EqualLegacyRoot.hit &&
        std::fabs(c05EqualLegacyRoot.timeSeconds - c05AbsoluteD6Root) <=
            seamProfile.solver.toiToleranceSeconds &&
        c05EqualLegacyRoot.approachDerivative < 0.0,
        "C05 independent real legacy and D6 roots must share one absolute TOI");
    GameState c05Equal = c05StateForOffset(0.0);
    const PhysicsStepTelemetry c05EqualTelemetry = updatePhysics(c05Equal,
        seamWholeDuration, seamProfile,
        PhysicsBoundaryMode::ProductionTable);
    const double c05PairSpeed = 40.0;
    const double c05PositionUlp = floatUlp(
        c05StateForOffset(0.0).balls[3].position.x);
    const double c05RootMargin = 2.0 * std::max(
        1.25 * seamProfile.solver.toiToleranceSeconds,
        8.0 * c05PositionUlp / c05PairSpeed);
    GameState c05Early = c05StateForOffset(-c05RootMargin);
    GameState c05Late = c05StateForOffset(c05RootMargin);
    const IndependentSweptSphereRoot c05EarlyRoot =
        independentEarliestSweptSphereRoot(c05Early.balls[2],
            c05Early.balls[3], seamWholeDuration, 0.0,
            2.0 * seamProfile.ball.radiusCm);
    const IndependentSweptSphereRoot c05LateRoot =
        independentEarliestSweptSphereRoot(c05Late.balls[2],
            c05Late.balls[3], seamWholeDuration, 0.0,
            2.0 * seamProfile.ball.radiusCm);
    expect(c05EarlyRoot.hit && c05LateRoot.hit &&
        c05EarlyRoot.timeSeconds < c05AbsoluteD6Root -
            seamProfile.solver.toiToleranceSeconds &&
        c05LateRoot.timeSeconds > c05AbsoluteD6Root +
            seamProfile.solver.toiToleranceSeconds &&
        c05AbsoluteD6Root - c05EarlyRoot.timeSeconds >=
            1.25 * seamProfile.solver.toiToleranceSeconds &&
        c05LateRoot.timeSeconds - c05AbsoluteD6Root >=
            1.25 * seamProfile.solver.toiToleranceSeconds &&
        (c05AbsoluteD6Root - c05EarlyRoot.timeSeconds) * c05PairSpeed >=
            8.0 * c05PositionUlp &&
        (c05LateRoot.timeSeconds - c05AbsoluteD6Root) * c05PairSpeed >=
            8.0 * c05PositionUlp,
        "C05 float-realized ±controls must retain sealed TOI and ULP margins");
    const PhysicsStepTelemetry c05EarlyTelemetry = updatePhysics(c05Early,
        seamWholeDuration, seamProfile,
        PhysicsBoundaryMode::ProductionTable);
    const PhysicsStepTelemetry c05LateTelemetry = updatePhysics(c05Late,
        seamWholeDuration, seamProfile,
        PhysicsBoundaryMode::ProductionTable);
    auto c05ContactCounts = [](const PhysicsStepTelemetry& telemetry) {
        int primary = 0;
        int control = 0;
        int primaryIsland = -1;
        int controlIsland = -1;
        double primaryTime = std::numeric_limits<double>::quiet_NaN();
        double controlTime = std::numeric_limits<double>::quiet_NaN();
        for (const PhysicsContactRecord& contact : telemetry.contacts) {
            if (contact.kind != PhysicsContactKind::BallBall) continue;
            if (contact.firstBall == 0 && contact.secondBall == 1) {
                ++primary;
                primaryIsland = contact.solverIslandId;
                primaryTime = contact.wholeTickTimeOfImpactSeconds;
            }
            if (contact.firstBall == 2 && contact.secondBall == 3) {
                ++control;
                controlIsland = contact.solverIslandId;
                controlTime = contact.wholeTickTimeOfImpactSeconds;
            }
        }
        return std::tuple<int, int, int, int, double, double>{primary,
            control, primaryIsland, controlIsland, primaryTime, controlTime};
    };
    const auto [earlyPrimaryCount, earlyControlCount, earlyPrimaryIsland,
        earlyControlIsland, earlyPrimaryTime, earlyControlTime] =
            c05ContactCounts(c05EarlyTelemetry);
    const auto [latePrimaryCount, lateControlCount, latePrimaryIsland,
        lateControlIsland, latePrimaryTime, lateControlTime] =
            c05ContactCounts(c05LateTelemetry);
    const auto [equalPrimaryCount, equalControlCount, equalPrimaryIsland,
        equalControlIsland, equalPrimaryTime, equalControlTime] =
            c05ContactCounts(c05EqualTelemetry);
    const bool c05ExactSimultaneousComposition =
        c05EqualTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        equalPrimaryCount == 1 && equalControlCount == 1 &&
        equalPrimaryIsland >= 0 && equalControlIsland >= 0 &&
        close(equalPrimaryTime, seamRoot.timeSeconds,
            seamProfile.solver.toiToleranceSeconds) &&
        close(equalControlTime, c05EqualLegacyRoot.timeSeconds,
            seamProfile.solver.toiToleranceSeconds) &&
        c05Equal.balls[0].physicalDomain ==
            PhysicalBallDomain::ClothSupported;
    expect(c05EarlyTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        c05LateTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        earlyPrimaryCount == 1 && earlyControlCount == 1 &&
        latePrimaryCount == 1 && lateControlCount == 1 &&
        earlyPrimaryIsland >= 0 && earlyControlIsland >= 0 &&
        latePrimaryIsland >= 0 && lateControlIsland >= 0 &&
        close(earlyPrimaryTime, seamRoot.timeSeconds,
            seamProfile.solver.toiToleranceSeconds) &&
        close(latePrimaryTime, seamRoot.timeSeconds,
            seamProfile.solver.toiToleranceSeconds) &&
        std::fabs(earlyControlTime - c05EarlyRoot.timeSeconds) <=
            1.25 * seamProfile.solver.toiToleranceSeconds &&
        std::fabs(lateControlTime - c05LateRoot.timeSeconds) <=
            1.25 * seamProfile.solver.toiToleranceSeconds &&
        earlyControlTime < c05AbsoluteD6Root &&
        lateControlTime > c05AbsoluteD6Root &&
        c05Early.balls[0].physicalDomain ==
            PhysicalBallDomain::ClothSupported &&
        c05Late.balls[0].physicalDomain ==
            PhysicalBallDomain::ClothSupported &&
        c05Early.balls[0].position.y >= geometry.clothSupportedCenterYCm &&
        c05Late.balls[0].position.y >= geometry.clothSupportedCenterYCm,
        "C05 ±1.01 TOI-tolerance controls must remain distinctly ordered successes");
    const auto emptyPublishedEvidence = [](const PhysicsStepTelemetry& telemetry) {
        return telemetry.contacts.empty() && telemetry.solverEvents.empty() &&
            telemetry.surfaceMotion.empty() &&
            telemetry.analyticSurfaceMotion.empty() &&
            telemetry.planeCrossings.empty() &&
            telemetry.planeAttestations.empty();
    };
    const bool c09BeforePrefix =
        seamInvalidTelemetry.stepStatus == PhysicsStepStatus::Failed &&
        seamInvalidTelemetry.failureCode == PhysicsFailureCode::InvalidControls &&
        emptyPublishedEvidence(seamInvalidTelemetry) &&
        sameTransactionalState(seamInvalid, seamInvalidBefore);
    const bool c09HandoffValidation = seamChildInvalidPropagated &&
        emptyPublishedEvidence(seamChildInvalidTelemetry);
    const bool c09D6Tail = c05ExactSimultaneousComposition;

    PhysicsProfile c09LaterLegacyProfile = seamProfile;
    c09LaterLegacyProfile.solver.maximumEventsPerTick = 2;
    GameState c09LaterLegacyState = c05StateForOffset(-c05RootMargin);
    const GameState c09LaterLegacyBefore = c09LaterLegacyState;
    const PhysicsStepTelemetry c09LaterLegacyTelemetry = updatePhysics(
        c09LaterLegacyState, seamWholeDuration, c09LaterLegacyProfile,
        PhysicsBoundaryMode::ProductionTable);
    const bool c09LaterLegacy =
        c09LaterLegacyTelemetry.stepStatus == PhysicsStepStatus::Failed &&
        c09LaterLegacyTelemetry.failureCode == PhysicsFailureCode::EventBudget &&
        emptyPublishedEvidence(c09LaterLegacyTelemetry) &&
        sameTransactionalState(c09LaterLegacyState, c09LaterLegacyBefore);
    expect(c09LaterLegacyTelemetry.stepStatus == PhysicsStepStatus::Failed &&
        c09LaterLegacyTelemetry.failureCode == PhysicsFailureCode::EventBudget &&
        sameTransactionalState(c09LaterLegacyState, c09LaterLegacyBefore),
        "C09 later legacy event exhaustion must be a reachable atomic EventBudget failure");

    GameState c09FinalValidation = seamInitial;
    c09FinalValidation.balls[4] = ballAt(-40.0f,
        geometry.clothSupportedCenterYCm + 100.0f, -40.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne);
    c09FinalValidation.balls[4].pocketed = false;
    c09FinalValidation.ballsMoving = true;
    const auto c09TotalEnergy = [&](const GameState& state) {
        double energy = translationalKineticEnergyJ(
            state, seamProfile.ball.massKg);
        for (const BallState& ball : state.balls) {
            if (!ball.pocketed)
                energy += rotationalKineticEnergyJ(ball, seamProfile.ball);
        }
        return energy;
    };
    const double c09InitialEnergy = c09TotalEnergy(c09FinalValidation);
    expect(std::isfinite(c09InitialEnergy),
        "C09 composed final-gate fixture must enter with finite state and energy");

    GameState c09FinalPrefix = c09FinalValidation;
    c09FinalPrefix.balls[4].pocketed = true;
    GameState c09FinalPrefixRepeat = c09FinalPrefix;
    const PhysicsStepTelemetry c09FinalPrefixTelemetry = updatePhysics(
        c09FinalPrefix, seamPrefixDuration, seamProfile,
        PhysicsBoundaryMode::ProductionTable);
    const PhysicsStepTelemetry c09FinalPrefixRepeatTelemetry = updatePhysics(
        c09FinalPrefixRepeat, seamPrefixDuration, seamProfile,
        PhysicsBoundaryMode::ProductionTable);
    const auto c09PrefixBall = contactOfKind(c09FinalPrefixTelemetry,
        PhysicsContactKind::BallBall);
    const auto c09PrefixSupport = contactOfKind(c09FinalPrefixTelemetry,
        PhysicsContactKind::Support);
    bool c09PrefixContactsExact = c09FinalPrefixTelemetry.contacts.size() ==
        c09FinalPrefixRepeatTelemetry.contacts.size();
    for (std::size_t index = 0; c09PrefixContactsExact &&
            index < c09FinalPrefixTelemetry.contacts.size(); ++index) {
        c09PrefixContactsExact = sameLegacyContact(
            c09FinalPrefixTelemetry.contacts[index],
            c09FinalPrefixRepeatTelemetry.contacts[index]);
    }
    bool c09PrefixEventsExact =
        c09FinalPrefixTelemetry.solverEvents.size() ==
            c09FinalPrefixRepeatTelemetry.solverEvents.size();
    for (std::size_t index = 0; c09PrefixEventsExact &&
            index < c09FinalPrefixTelemetry.solverEvents.size(); ++index) {
        c09PrefixEventsExact = sameLegacySolverEvent(
            c09FinalPrefixTelemetry.solverEvents[index],
            c09FinalPrefixRepeatTelemetry.solverEvents[index]);
    }
    expect(c09FinalPrefixTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        c09PrefixBall != c09FinalPrefixTelemetry.contacts.end() &&
        c09PrefixSupport != c09FinalPrefixTelemetry.contacts.end() &&
        c09PrefixBall->solverEventId == c09PrefixSupport->solverEventId &&
        c09PrefixBall->solverIslandId == c09PrefixSupport->solverIslandId &&
        close(c09PrefixBall->wholeTickTimeOfImpactSeconds,
            c09PrefixSupport->wholeTickTimeOfImpactSeconds,
            seamProfile.solver.toiToleranceSeconds) &&
        c09PrefixContactsExact && c09PrefixEventsExact &&
        sameTransactionalState(c09FinalPrefix, c09FinalPrefixRepeat),
        "C09 composed request must execute one deterministic public joint prefix and support island");

    GameState c09FinalLegacyWhole = c09FinalValidation;
    c09FinalLegacyWhole.balls[4].pocketed = true;
    const PhysicsStepTelemetry c09FinalLegacyWholeTelemetry = updatePhysics(
        c09FinalLegacyWhole, seamWholeDuration, seamProfile,
        PhysicsBoundaryMode::ProductionTable);
    expect(c09FinalLegacyWholeTelemetry.stepStatus ==
            PhysicsStepStatus::Succeeded &&
        contactOfKind(c09FinalLegacyWholeTelemetry,
            PhysicsContactKind::BallBall) !=
                c09FinalLegacyWholeTelemetry.contacts.end() &&
        contactOfKind(c09FinalLegacyWholeTelemetry,
            PhysicsContactKind::Support) !=
                c09FinalLegacyWholeTelemetry.contacts.end() &&
        c09FinalLegacyWholeTelemetry.analyticSurfaceMotion.size() >= 4,
        "C09 same-request public scheduler must complete its joint prefix and later support work");
    const GameState c09FinalValidationBefore = c09FinalValidation;
    GameState c09FinalValidationRepeat = c09FinalValidation;
    const PhysicsStepTelemetry c09FinalValidationTelemetry = updatePhysics(
        c09FinalValidation, seamWholeDuration, seamProfile,
        PhysicsBoundaryMode::ProductionTable);
    const PhysicsStepTelemetry c09FinalValidationRepeatTelemetry = updatePhysics(
        c09FinalValidationRepeat, seamWholeDuration, seamProfile,
        PhysicsBoundaryMode::ProductionTable);
    bool c09WholeContactsExact =
        c09FinalValidationTelemetry.contacts.size() ==
            c09FinalValidationRepeatTelemetry.contacts.size();
    for (std::size_t index = 0; c09WholeContactsExact &&
            index < c09FinalValidationTelemetry.contacts.size(); ++index) {
        c09WholeContactsExact = sameLegacyContact(
            c09FinalValidationTelemetry.contacts[index],
            c09FinalValidationRepeatTelemetry.contacts[index]);
    }
    bool c09WholeEventsExact =
        c09FinalValidationTelemetry.solverEvents.size() ==
            c09FinalValidationRepeatTelemetry.solverEvents.size();
    for (std::size_t index = 0; c09WholeEventsExact &&
            index < c09FinalValidationTelemetry.solverEvents.size(); ++index) {
        c09WholeEventsExact = sameLegacySolverEvent(
            c09FinalValidationTelemetry.solverEvents[index],
            c09FinalValidationRepeatTelemetry.solverEvents[index]);
    }
    bool c09BallisticTailObserved = false;
    bool c09SupportedTailObserved = false;
    for (const auto& segment :
            c09FinalValidationTelemetry.analyticSurfaceMotion) {
        if (segment.ballIndex != 0) continue;
        c09BallisticTailObserved = c09BallisticTailObserved ||
            segment.constantAccelerationCmS2.y == geometry.gravityYCmS2;
        c09SupportedTailObserved = c09SupportedTailObserved ||
            segment.constantAccelerationCmS2.y == 0.0f;
    }
    expect(c09FinalValidationTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        !sameTransactionalState(c09FinalValidation,
            c09FinalValidationBefore) &&
        sameTransactionalState(c09FinalValidation,
            c09FinalValidationRepeat) &&
        c09WholeContactsExact && c09WholeEventsExact &&
        c09BallisticTailObserved && c09SupportedTailObserved,
        "C09 same ProductionTable request must deterministically complete joint prefix, D6 tail, later support work and final commit");

    GameState c09Gravity = isolatedState(ballAt(-40.0f,
        geometry.clothSupportedCenterYCm + 5.0f, -40.0f,
        0.0f, 0.0f, 0.0f, PhysicalBallDomain::Airborne));
    const GameState c09GravityBefore = c09Gravity;
    const auto gravitationalPotentialJ = [&](const GameState& state) {
        double potential = 0.0;
        for (const BallState& ball : state.balls) {
            if (!ball.pocketed) potential +=
                -static_cast<double>(seamProfile.ball.massKg) *
                static_cast<double>(geometry.gravityYCmS2) *
                static_cast<double>(ball.position.y) / 10000.0;
        }
        return potential;
    };
    const double c09GravityKBefore = c09TotalEnergy(c09GravityBefore);
    const double c09GravityMechanicalBefore = c09GravityKBefore +
        gravitationalPotentialJ(c09GravityBefore);
    const PhysicsStepTelemetry c09GravityTelemetry = updatePhysics(
        c09Gravity, 0.05f, seamProfile,
        PhysicsBoundaryMode::ProductionTable);
    const double c09GravityKAfter = c09TotalEnergy(c09Gravity);
    const double c09GravityMechanicalAfter = c09GravityKAfter +
        gravitationalPotentialJ(c09Gravity);
    const bool c09GravityMechanicalControl =
        c09GravityTelemetry.stepStatus == PhysicsStepStatus::Succeeded &&
        c09GravityKAfter > c09GravityKBefore &&
        c09GravityMechanicalAfter <= c09GravityMechanicalBefore +
            seamProfile.solver.passiveEnergyToleranceJ +
            2.0 * static_cast<double>(seamProfile.ball.massKg) *
                std::fabs(static_cast<double>(geometry.gravityYCmS2)) *
                static_cast<double>(std::numeric_limits<float>::epsilon()) *
                std::max(std::fabs(static_cast<double>(
                    c09GravityBefore.balls[0].position.y)),
                    std::fabs(static_cast<double>(
                        c09Gravity.balls[0].position.y))) / 10000.0 &&
        c09Gravity.balls[0].physicalDomain ==
            PhysicalBallDomain::Airborne;
    expect(c09GravityMechanicalControl,
        "C09 gravity control must increase K while K+U stays within the unchanged public tolerance");

    std::ifstream c09PhysicsSource(std::string(BILLIARDGL_SOURCE_ROOT) +
        "/src/Billiards/physics.cpp");
    std::ostringstream c09PhysicsStream;
    c09PhysicsStream << c09PhysicsSource.rdbuf();
    const std::string c09CompositionFunction = extractCompleteFunction(
        c09PhysicsStream.str(), "PhysicsStepTelemetry updatePhysics(\n"
        "    GameState& state, float timeStep, const PhysicsProfile& profile,\n"
        "    PhysicsBoundaryMode boundaryMode,\n"
        "    const std::vector<PhysicsPlaneObservationRequest>& planeObservations)");
    const std::string c09ClearHelper = extractCompleteFunction(
        c09PhysicsStream.str(),
        "void clearCanonicalTelemetry(PhysicsStepTelemetry& telemetry)");
    const std::string c09CanonicalSpecimen =
        "PhysicsStepTelemetry composed(){"
        "auto telemetry=runComposedEventScheduler();"
        "if (!validateComposedFinalState(totalMechanicalEnergyIncludingGravityJ())) {"
        "state = snapshot;clearCanonicalTelemetry(telemetry);return telemetry;}"
        "if (!verifyComposedAnalyticPartition()) {state = snapshot;"
        "clearCanonicalTelemetry(telemetry);return telemetry;}"
        "appendPlaneObservationEvidence();"
        "for(auto attestation:planeAttestations){"
        "if (!attestation.complete) {state = snapshot;"
        "clearCanonicalTelemetry(telemetry);return telemetry;}}"
        "state = staged;return telemetry;}";
    const std::string c09CanonicalClearHelper =
        "void clearCanonicalTelemetry(PhysicsStepTelemetry& telemetry){"
        "telemetry.contacts.clear();telemetry.solverEvents.clear();"
        "telemetry.surfaceMotion.clear();"
        "telemetry.analyticSurfaceMotion.clear();"
        "telemetry.planeCrossings.clear();"
        "telemetry.planeAttestations.clear();}";
    const std::string c09CanonicalValidator =
        "bool validateComposedFinalState(){"
        "auto before=totalMechanicalEnergyIncludingGravityJ(snapshot,profile);"
        "auto after=totalMechanicalEnergyIncludingGravityJ(staged,profile);"
        "double representationBound=0;"
        "if(!finite){failStep(PhysicsFailureCode::NonfiniteState);"
        "failStep(PhysicsFailureCode::NonfiniteEnergy);return false;}"
        "if(after>before+profile.solver.passiveEnergyToleranceJ+"
        "representationBound){failStep(PhysicsFailureCode::PassiveEnergyCreation);"
        "return false;}return true;}";
    const auto replaceFirst = [](std::string text, const std::string& from,
                                  const std::string& to) {
        const std::size_t at = text.find(from);
        if (at != std::string::npos) text.replace(at, from.size(), to);
        return text;
    };
    const std::array<std::string, 7> c09FunctionMutations{{
        replaceFirst(c09CanonicalSpecimen,
            "if (!validateComposedFinalState(totalMechanicalEnergyIncludingGravityJ())) {state = snapshot;clearCanonicalTelemetry(telemetry);return telemetry;}", ""),
        replaceFirst(c09CanonicalSpecimen, "state = staged;",
            "state = staged;validateComposedFinalState(totalMechanicalEnergyIncludingGravityJ());"),
        replaceFirst(c09CanonicalSpecimen, "auto telemetry=runComposedEventScheduler();",
            "validateComposedFinalState(totalMechanicalEnergyIncludingGravityJ());auto telemetry=runComposedEventScheduler();"),
        replaceFirst(c09CanonicalSpecimen, "state = snapshot;", ""),
        replaceFirst(c09CanonicalSpecimen, "verifyComposedAnalyticPartition()",
            "injectFinalFailureForTest() || verifyComposedAnalyticPartition()"),
        replaceFirst(c09CanonicalSpecimen,
            "appendPlaneObservationEvidence();", "state = staged;appendPlaneObservationEvidence();"),
        replaceFirst(c09CanonicalSpecimen,
            "if (!attestation.complete)", "if (false && !attestation.complete)")
    }};
    bool c09MutationsRejected = hasComposedFinalGateContract(
        c09CanonicalSpecimen, c09CanonicalClearHelper,
        c09CanonicalValidator);
    for (const std::string& mutation : c09FunctionMutations)
        c09MutationsRejected = c09MutationsRejected &&
            mutation != c09CanonicalSpecimen &&
            !hasComposedFinalGateContract(mutation,
                c09CanonicalClearHelper, c09CanonicalValidator);
    const std::array<std::string, 3> c09ValidatorMutations{{
        replaceFirst(c09CanonicalValidator,
            "totalMechanicalEnergyIncludingGravityJ(",
            "translationalKineticEnergyJ("),
        replaceFirst(c09CanonicalValidator,
            "profile.solver.passiveEnergyToleranceJ", "0.0"),
        replaceFirst(c09CanonicalValidator,
            "PhysicsFailureCode::PassiveEnergyCreation",
            "PhysicsFailureCode::None")
    }};
    for (const std::string& mutation : c09ValidatorMutations)
        c09MutationsRejected = c09MutationsRejected &&
            mutation != c09CanonicalValidator &&
            !hasComposedFinalGateContract(c09CanonicalSpecimen,
                c09CanonicalClearHelper, mutation);
    for (const char* ledger : kCanonicalTelemetryLedgers) {
        const std::string clear = std::string("telemetry.") + ledger +
            ".clear();";
        const std::string retained = replaceFirst(c09CanonicalClearHelper,
            clear, "");
        c09MutationsRejected = c09MutationsRejected &&
            retained != c09CanonicalClearHelper &&
            !hasComposedFinalGateContract(c09CanonicalSpecimen, retained,
                c09CanonicalValidator);
    }
    const std::string c09ActualValidator = extractCompleteFunction(
        c09PhysicsStream.str(),
        "bool validateComposedFinalState(const GameState& snapshot");
    const std::array<std::pair<std::string, std::string>, 6>
        c09ActualFunctionMutationTokens{{
            {"if (!validateComposedFinalState(",
                "if (false && !validateComposedFinalState("},
            {"state = staged;", "state = staged; validateComposedFinalState("},
            {"telemetry = runComposedEventScheduler(",
                "validateComposedFinalState(); telemetry = runComposedEventScheduler("},
            {"verifyComposedAnalyticPartition(",
                "injectFinalFailureForTest() || verifyComposedAnalyticPartition("},
            {"appendPlaneObservationEvidence(telemetry, planeObservations, profile);",
                "state = staged; appendPlaneObservationEvidence(telemetry, planeObservations, profile);"},
            {"if (!attestation.complete)",
                "if (false && !attestation.complete)"}
        }};
    if (!c09CompositionFunction.empty() && !c09ClearHelper.empty() &&
        hasComposedFinalGateContract(c09CompositionFunction,
            c09ClearHelper, c09ActualValidator)) {
        for (const auto& [from, to] : c09ActualFunctionMutationTokens) {
            const std::string mutated = replaceFirst(c09CompositionFunction,
                from, to);
            c09MutationsRejected = c09MutationsRejected &&
                mutated != c09CompositionFunction &&
                !hasComposedFinalGateContract(mutated, c09ClearHelper,
                    c09ActualValidator);
        }
        for (const char* ledger : kCanonicalTelemetryLedgers) {
            const std::string clear = std::string("telemetry.") + ledger +
                ".clear();";
            const std::string retained = replaceFirst(c09ClearHelper,
                clear, "");
            c09MutationsRejected = c09MutationsRejected &&
                retained != c09ClearHelper &&
                !hasComposedFinalGateContract(c09CompositionFunction,
                    retained, c09ActualValidator);
        }
        const std::array<std::pair<std::string, std::string>, 3>
            actualValidatorMutations{{
                {"totalMechanicalEnergyIncludingGravityJ(",
                    "translationalKineticEnergyJ("},
                {"profile.solver.passiveEnergyToleranceJ", "0.0"},
                {"PhysicsFailureCode::PassiveEnergyCreation",
                    "PhysicsFailureCode::None"}
            }};
        for (const auto& [from, to] : actualValidatorMutations) {
            const std::string mutated = replaceFirst(c09ActualValidator,
                from, to);
            c09MutationsRejected = c09MutationsRejected &&
                mutated != c09ActualValidator &&
                !hasComposedFinalGateContract(c09CompositionFunction,
                    c09ClearHelper, mutated);
        }
    }
    expect(c09MutationsRejected,
        "C09 static final-gate audit must reject all eight exact source mutations");
    const bool c09FinalFiniteEnergy = c09PhysicsSource.good() &&
        hasComposedFinalGateContract(c09CompositionFunction,
            c09ClearHelper, c09ActualValidator) &&
        c09GravityMechanicalControl;
    const std::string c08ConsumerSpecimen =
        "void scheduler(){verifyPrivateAnalyticLineage();"
        "appendRebasedPrivateAnalyticSegments();"
        "}void composed(){"
        "validateComposedFinalState();appendPlaneObservationEvidence();"
        "for(auto attestation:planeAttestations){"
        "if (!attestation.complete){state = snapshot;"
        "clearCanonicalTelemetry(telemetry);return telemetry;}}"
        "state = staged;}";
    const std::string c08VerifierSpecimen =
        "bool verifyPrivateAnalyticLineage(){result.analyticSegments;"
        "result.events;before.balls;after.balls;duration;"
        "profile.solver.toiToleranceSeconds;"
        "profile.solver.penetrationSlopCm;stableIdentity;"
        "segment.motionState;segment.startPositionCm;segment.endPositionCm;"
        "segment.startVelocityCmS;segment.constantAccelerationCmS2;}";
    const std::string c08AppenderSpecimen =
        "void appendRebasedPrivateAnalyticSegments(){for(auto segment:"
        "result.analyticSegments){"
        "segment.wholeTickStartOffsetSeconds += prefixDuration;"
        "segment.segmentOrdinal = static_cast<int>(telemetry.analyticSurfaceMotion.size());"
        "telemetry.analyticSurfaceMotion.push_back(segment);}}";
    const std::array<std::pair<std::string, std::string>, 9>
        c08ConsumerMutations{{
            {"verifyPrivateAnalyticLineage();", ""},
            {"appendRebasedPrivateAnalyticSegments();", ""},
            {"validateComposedFinalState();", ""},
            {"validateComposedFinalState();appendPlaneObservationEvidence();",
                "state = staged;validateComposedFinalState();appendPlaneObservationEvidence();"},
            {"appendPlaneObservationEvidence();", ""},
            {"appendPlaneObservationEvidence();",
                "appendPlaneObservationEvidence(); contacts.push_back(privateEvent);"},
            {"appendPlaneObservationEvidence();",
                "appendPlaneObservationEvidence(); tail.events.size();"},
            {"if (!attestation.complete)",
                "if (false && !attestation.complete)"},
            {"clearCanonicalTelemetry(telemetry);", ""}
        }};
    bool c08ConsumerMutationsRejected = hasPrivateLineageConsumerContract(
        c08ConsumerSpecimen, c08VerifierSpecimen, c08AppenderSpecimen);
    for (const auto& [from, to] : c08ConsumerMutations) {
        const std::string mutation = replaceFirst(c08ConsumerSpecimen,
            from, to);
        c08ConsumerMutationsRejected = c08ConsumerMutationsRejected &&
            mutation != c08ConsumerSpecimen &&
            !hasPrivateLineageConsumerContract(mutation,
                c08VerifierSpecimen, c08AppenderSpecimen);
    }
    const std::array<std::pair<std::string, std::string>, 5>
        c08AppenderMutations{{
            {"+= prefixDuration", "+= 2 * prefixDuration"},
            {"static_cast<int>(telemetry.analyticSurfaceMotion.size())",
                "segment.segmentOrdinal"},
            {"push_back(segment)", "push_back(recomputedSegment)"},
            {"push_back(segment)", "push_back(segment); contacts.push_back(privateEvent)"},
            {"push_back(segment)", "push_back(segment); endPositionCm = recompute()"}
        }};
    for (const auto& [from, to] : c08AppenderMutations) {
        const std::string mutation = replaceFirst(c08AppenderSpecimen,
            from, to);
        c08ConsumerMutationsRejected = c08ConsumerMutationsRejected &&
            mutation != c08AppenderSpecimen &&
            !hasPrivateLineageConsumerContract(c08ConsumerSpecimen,
                c08VerifierSpecimen, mutation);
    }
    expect(c08ConsumerMutationsRejected,
        "C08 physics consumer checker must reject rebase, inference, denominator and private-event mutations");
    const std::string c08ActualVerifier = extractCompleteFunction(
        c09PhysicsStream.str(),
        "bool verifyPrivateAnalyticLineage(const TableDepartureStepResult& result");
    const std::string c08ActualAppender = extractCompleteFunction(
        c09PhysicsStream.str(),
        "void appendRebasedPrivateAnalyticSegments(PhysicsStepTelemetry& telemetry");
    const std::string c08ActualScheduler = extractCompleteFunction(
        c09PhysicsStream.str(),
        "PhysicsStepTelemetry runComposedEventScheduler(GameState& staged");
    const bool c08ActualConsumerContract =
        hasPrivateLineageConsumerContract(c08ActualScheduler +
            c09CompositionFunction,
            c08ActualVerifier, c08ActualAppender);
    const bool seamBudgetAtomic = seamBudgetHasRealPrefix &&
        seamBudgetTelemetry.stepStatus == PhysicsStepStatus::Failed &&
        seamBudgetTelemetry.failureCode == PhysicsFailureCode::EventBudget &&
        emptyPublishedEvidence(seamBudgetTelemetry) &&
        sameTransactionalState(seamBudget, seamBudgetBefore);
    const bool c09FiveStageAtomicMatrix = c09BeforePrefix &&
        c09HandoffValidation && c09D6Tail && c09LaterLegacy &&
        c09FinalFiniteEnergy;
    if (!c09FiveStageAtomicMatrix) {
        std::cerr << "C09 stage diagnostics: before_prefix="
                  << c09BeforePrefix << " handoff_validation="
                  << c09HandoffValidation << " d6_tail=" << c09D6Tail
                  << " later_legacy=" << c09LaterLegacy
                  << " final_finite_energy=" << c09FinalFiniteEnergy
                  << " later_status="
                  << static_cast<int>(c09LaterLegacyTelemetry.stepStatus)
                  << " later_code="
                  << static_cast<int>(c09LaterLegacyTelemetry.failureCode)
                  << " later_empty="
                  << emptyPublishedEvidence(c09LaterLegacyTelemetry)
                  << " later_rollback="
                  << sameTransactionalState(c09LaterLegacyState,
                         c09LaterLegacyBefore)
                  << " child_status="
                  << static_cast<int>(seamChildInvalidTelemetry.stepStatus)
                  << " child_code="
                  << static_cast<int>(seamChildInvalidTelemetry.failureCode)
                  << " child_empty="
                  << emptyPublishedEvidence(seamChildInvalidTelemetry)
                  << " child_rollback="
                  << sameTransactionalState(seamChildInvalid,
                         seamChildInvalidBefore)
                  << '\n';
    }

    // This final assertion is the honest tests-first RED: all independent
    // prefix, D6 tail, controls and rollback contracts above execute first.
    expect(seamWholeSplitEquivalent,
        "C08 composed prefix/tail plane lineage must be continuous, owned and state-equivalent");
    expect(c05ExactSimultaneousComposition,
        "C05 disjoint legacy contact and D6 support landing at one TOI must compose successfully and exactly once");
    expect(seamChildInvalidPropagated,
        "C06 composed child penetration failure must propagate without EventBudget translation");
    expect(seamBudgetAtomic,
        "C06 composed event-budget failure must clear evidence and rollback");
    expect(c09FiveStageAtomicMatrix,
        "C09 all five composed failure stages must remain atomic");
    expect(c08ActualConsumerContract,
        "C08 actual consumer must verify, rebase, append, validate, commit and attest in order");

    return EXIT_SUCCESS;
}
