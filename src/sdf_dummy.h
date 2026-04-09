#pragma once

#include "sdf_base.h"
#include <cmath>

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
        return -sqrt(p_world_pos.y*p_world_pos.y + p_world_pos.x*p_world_pos.x + p_world_pos.z*p_world_pos.z)  + 10.0f;
    }

    Vector3 evaluate_normal(const Vector3 &p_world_pos) const override {
        const float len_sq = p_world_pos.length_squared();
        if (len_sq <= 1e-12f) {
            return Vector3(0.0f, 1.0f, 0.0f);
        }
        return p_world_pos / std::sqrt(len_sq);
    }

    // Periodic sphere SDF using modulo-like coordinate wrapping.
    
    // float evaluate(const Vector3 &p_world_pos) const override {
    //     const float period = 3.0f;
    //     const float radius = 0.5f;

    //     const auto repeat_axis = [period](float v) {
    //         // Wrap to [-period/2, period/2] to repeat the same cell in space.
    //         return v - period * std::floor(v / period + 0.5f);
    //     };

    //     const Vector3 q(
    //         repeat_axis(p_world_pos.x),
    //         repeat_axis(p_world_pos.y),
    //         repeat_axis(p_world_pos.z)
    //     );

    //     return -q.length() + radius;
    // }

    // GLSL implementation
    String get_glsl_code() const override {
        return R"(
float evaluate_sdf(vec3 pos) {
    return 1.0;  // Dummy: always solid
}
        )";
    }
};
