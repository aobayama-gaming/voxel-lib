#pragma once

#include "sdf_base.h"

class SDFDummy : public SDFBase {
public:
    SDFDummy() = default;
    ~SDFDummy() override = default;
    
    // plane
    // float evaluate(const Vector3 &p_world_pos) const override {
    //     return p_world_pos.y + 1.0f;
    // }

    // GRAD OF 1 IS IMPORTANT !!!!
    float evaluate(const Vector3 &p_world_pos) const override {
        return sqrt(p_world_pos.y*p_world_pos.y + p_world_pos.x*p_world_pos.x + p_world_pos.z*p_world_pos.z)  - 10.0f;
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
