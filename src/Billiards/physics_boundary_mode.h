#pragma once

namespace billiardgl {

enum class PhysicsBoundaryMode {
    ProductionTable,
    Unbounded
};

inline const char* physicsBoundaryModeName(PhysicsBoundaryMode mode)
{
    return mode == PhysicsBoundaryMode::Unbounded ?
        "unbounded" : "production_table";
}

}  // namespace billiardgl
