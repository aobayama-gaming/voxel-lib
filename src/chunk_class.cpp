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

void ChunkClass::initialize_debug(const Vector3i &p_chunk_pos, SDFBase *p_sdf) {
    chunk_pos = p_chunk_pos;
    sdf = p_sdf;

    set_position(ChunkMath::chunk_to_world(chunk_pos));
    //_build_debug_mesh();

    mesh_info.initialize(chunk_pos,sdf);
    mesh_info.execute_on_self();
    _build_debug_mesh_point();
    _build_debug_mesh_edge_points();
}

void ChunkClass::_build_debug_mesh() {
    const float chunk_size = ChunkMath::world_chunk_size(chunk_pos);
    const int lod = ChunkMath::get_lod(chunk_pos);
    const float fill_scale = 0.9f;
    const float fill_alpha = 0.1f;
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
    debug_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS);
    debug_material->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_ALWAYS);
    debug_material->set_albedo(Color(base_color.r, base_color.g, base_color.b, fill_alpha));
    debug_material->set_render_priority(0);
    mesh_instance->set_material_override(debug_material);

    // Very thin inflated shell + slightly shrunken fill to approximate a 1px contour.
    const float outline_scale = 1.004f;
    outline_box_mesh.instantiate();
    outline_box_mesh->set_size(Vector3(chunk_size * outline_scale, chunk_size * outline_scale, chunk_size * outline_scale));
    outline_mesh_instance->set_mesh(outline_box_mesh);

    outline_material.instantiate();
    outline_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    outline_material->set_transparency(BaseMaterial3D::TRANSPARENCY_DISABLED);
    outline_material->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_OPAQUE_ONLY);
    outline_material->set_albedo(contour_color);
    outline_material->set_cull_mode(BaseMaterial3D::CULL_FRONT);
    outline_material->set_render_priority(1);
    outline_mesh_instance->set_material_override(outline_material);
}

void ChunkClass::_build_debug_mesh_point() {
    // Create mesh instance for points if it doesn't exist
    if (point_mesh_instance == nullptr) {
        point_mesh_instance = memnew(MeshInstance3D);
        add_child(point_mesh_instance);
    }

    // Get points from vertices_data
    const auto& points = mesh_info.vertices_data.points;
    const Vector3 chunk_center = ChunkMath::chunk_to_world(chunk_pos);

    if (points.size() == 0) {
        return; // No points to display
    }

    // Create array mesh
    Ref<ArrayMesh> point_mesh;
    point_mesh.instantiate();

    // Create vertex array
    PackedVector3Array vertices;
    vertices.resize(static_cast<int>(points.size()));
    for (size_t i = 0; i < points.size(); ++i) {
        const Vector3 global_pos = ChunkMath::vertices_to_world(chunk_pos, points[i]);
        vertices[i] = global_pos - chunk_center;
    }

    // Create arrays
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;

    // Add surface as point cloud
    point_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_POINTS, arrays);

    // Set mesh
    point_mesh_instance->set_mesh(point_mesh);

    // Create and set material
    Ref<StandardMaterial3D> point_material;
    point_material.instantiate();
    point_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    point_material->set_albedo(Color(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow for visibility
    point_material->set_flag(BaseMaterial3D::FLAG_USE_POINT_SIZE,true );
    point_material->set_point_size(15.0f); // Make points visible
    point_mesh_instance->set_material_override(point_material);
}

void ChunkClass::_build_debug_mesh_edge_points() {
    if (x_edge_point_mesh_instance == nullptr) {
        x_edge_point_mesh_instance = memnew(MeshInstance3D);
        add_child(x_edge_point_mesh_instance);
    }
    if (y_edge_point_mesh_instance == nullptr) {
        y_edge_point_mesh_instance = memnew(MeshInstance3D);
        add_child(y_edge_point_mesh_instance);
    }
    if (z_edge_point_mesh_instance == nullptr) {
        z_edge_point_mesh_instance = memnew(MeshInstance3D);
        add_child(z_edge_point_mesh_instance);
    }

    const Vector3 chunk_center = ChunkMath::chunk_to_world(chunk_pos);

    auto build_edge_cloud = [&](MeshInstance3D *instance, const VerticesData::EdgeCompute &edge, const Color &color) {
        if (instance == nullptr || edge.local_positions.empty()) {
            return;
        }

        Ref<ArrayMesh> edge_mesh;
        edge_mesh.instantiate();

        PackedVector3Array vertices;
        vertices.resize(static_cast<int>(edge.local_positions.size()));
        for (size_t i = 0; i < edge.local_positions.size(); ++i) {
            const Vector3 global_pos = ChunkMath::vertices_to_world(chunk_pos, edge.local_positions[i]);
            vertices.set(static_cast<int>(i), global_pos - chunk_center);
        }

        Array arrays;
        arrays.resize(Mesh::ARRAY_MAX);
        arrays[Mesh::ARRAY_VERTEX] = vertices;

        edge_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_POINTS, arrays);
        instance->set_mesh(edge_mesh);

        Ref<StandardMaterial3D> edge_material;
        edge_material.instantiate();
        edge_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
        edge_material->set_albedo(color);
        edge_material->set_flag(BaseMaterial3D::FLAG_USE_POINT_SIZE,true );
        edge_material->set_point_size(10.0f);
        instance->set_material_override(edge_material);
    };

    build_edge_cloud(x_edge_point_mesh_instance, mesh_info.vertices_data.x_edge, Color(1.0f, 0.2f, 0.2f, 1.0f));
    build_edge_cloud(y_edge_point_mesh_instance, mesh_info.vertices_data.y_edge, Color(0.2f, 1.0f, 0.2f, 1.0f));
    build_edge_cloud(z_edge_point_mesh_instance, mesh_info.vertices_data.z_edge, Color(0.2f, 0.5f, 1.0f, 1.0f));
}

void ChunkClass::_ready() {

    //first debug of the big square
    //_build_debug_mesh();

    //second debug serialised treatment    
    // mesh_info.initialize(chunk_pos,sdf);
    // mesh_info.execute_on_self();
    // _build_debug_mesh_point();

}