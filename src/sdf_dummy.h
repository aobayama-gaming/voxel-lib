#pragma once

#include "sdf_base.h"

class SDFDummy : public SDFBase {
public:
    SDFDummy() = default;
    ~SDFDummy() override = default;
    
    // Always return 1.0 (solid everywhere)
    float evaluate(const Vector3 &p_world_pos) const override {
        return p_world_pos.y + 0.5f;
    }
    
    // GLSL implementation
    String get_glsl_code() const override {
        return R"(
float evaluate_sdf(vec3 pos) {
    return 1.0;  // Dummy: always solid
}
        )";
    }
};
