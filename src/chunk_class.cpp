#include "chunk_class.h"

#include "godot_cpp/classes/standard_material3d.hpp"

#include <cmath>

namespace {
Color hsv_to_rgb(float h, float s, float v) {
    h = h - std::floor(h);
    const float c = v * s;
    const float x = c * (1.0f - std::abs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    const float m = v - c;

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;

    const int sector = static_cast<int>(std::floor(h * 6.0f));
    switch (sector) {
        case 0: r = c; g = x; b = 0.0f; break;
        case 1: r = x; g = c; b = 0.0f; break;
        case 2: r = 0.0f; g = c; b = x; break;
        case 3: r = 0.0f; g = x; b = c; break;
        case 4: r = x; g = 0.0f; b = c; break;
        default: r = c; g = 0.0f; b = x; break;
    }

    return Color(r + m, g + m, b + m, 1.0f);
}

Color lod_to_color(int lod) {
    // Golden-angle hue progression gives stable, well-distributed colors for any LOD count.
    const float hue = std::fmod(static_cast<float>(lod) * 0.61803398875f, 1.0f);
    return hsv_to_rgb(hue, 0.62f, 0.92f);
}
}


void ChunkClass::_bind_methods() {
	//godot::ClassDB::bind_method(D_METHOD("print_type", "variant"), &ChunkClass::print_type);
}

void ChunkClass::initialize_debug(const Vector3i &p_chunk_pos) {
    chunk_pos = p_chunk_pos;

    set_position(ChunkMath::chunk_to_world(chunk_pos));
    _build_debug_mesh();
}

void ChunkClass::_build_debug_mesh() {
    const float chunk_size = ChunkMath::world_chunk_size(chunk_pos);
    const int lod = ChunkMath::get_lod(chunk_pos);
    const float fill_scale = 0.996f;
    const Color base_color = lod_to_color(lod);
    const Color contour_color = Color(base_color.r * 0.22f, base_color.g * 0.22f, base_color.b * 0.22f, 1.0f);

    if (mesh_instance == nullptr) {
        mesh_instance = memnew(MeshInstance3D);
        add_child(mesh_instance);
    }

    if (outline_mesh_instance == nullptr) {
        outline_mesh_instance = memnew(MeshInstance3D);
        add_child(outline_mesh_instance);
    }

    debug_box_mesh.instantiate();
    debug_box_mesh->set_size(Vector3(chunk_size * fill_scale, chunk_size * fill_scale, chunk_size * fill_scale));
    mesh_instance->set_mesh(debug_box_mesh);
    debug_material.instantiate();
    debug_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    debug_material->set_albedo(base_color);
    mesh_instance->set_material_override(debug_material);

    // Very thin inflated shell + slightly shrunken fill to approximate a 1px contour.
    const float outline_scale = 1.004f;
    outline_box_mesh.instantiate();
    outline_box_mesh->set_size(Vector3(chunk_size * outline_scale, chunk_size * outline_scale, chunk_size * outline_scale));
    outline_mesh_instance->set_mesh(outline_box_mesh);

    outline_material.instantiate();
    outline_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    outline_material->set_albedo(contour_color);
    outline_material->set_cull_mode(BaseMaterial3D::CULL_FRONT);
    outline_mesh_instance->set_material_override(outline_material);
}

void ChunkClass::_ready() {
    if (mesh_instance == nullptr) {
        _build_debug_mesh();
    }
}