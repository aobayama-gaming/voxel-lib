#pragma once

#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/vector3.hpp"

using namespace godot;

class SDFBase {
public:
    virtual ~SDFBase() = default;
    
    // CPU evaluation: given a world position, return signed distance
    virtual float evaluate(const Vector3 &p_world_pos) const = 0;
    
    // GPU code: GLSL function that evaluates SDF
    // Should return a function named "evaluate_sdf(vec3 pos) -> float"
    virtual String get_glsl_code() const = 0;
};
