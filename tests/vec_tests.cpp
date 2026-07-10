#include "vec.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

bool closeEnough(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

}  // namespace

int main()
{
    vec xAxis(1.0f, 0.0f, 0.0f);
    vec yAxis(0.0f, 1.0f, 0.0f);
    vec zAxis = xAxis.CrossProduct(yAxis);
    if (!closeEnough(zAxis.x, 0.0f) || !closeEnough(zAxis.y, 0.0f) || !closeEnough(zAxis.z, 1.0f)) {
        return fail("CrossProduct should return a stable value");
    }

    vec zero(0.0f, 0.0f, 0.0f);
    zero.Normalize();
    if (!closeEnough(zero.x, 0.0f) || !closeEnough(zero.y, 0.0f) || !closeEnough(zero.z, 0.0f)) {
        return fail("Normalize should leave a zero vector unchanged");
    }
    if (!std::isfinite(zero.x) || !std::isfinite(zero.y) || !std::isfinite(zero.z)) {
        return fail("Normalize should not produce NaN or infinity");
    }

    vec value(2.0f, 3.0f, 4.0f);
    std::unique_ptr<float[]> components(value.toFloat());
    if (!components || !closeEnough(components[0], 2.0f) || !closeEnough(components[1], 3.0f) || !closeEnough(components[2], 4.0f)) {
        return fail("toFloat should return initialized vector components");
    }

    return EXIT_SUCCESS;
}
