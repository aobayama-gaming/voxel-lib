#include "chunk_class.h"

#include "godot_cpp/classes/standard_material3d.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"

#include <cmath>
#include <cstdint>

namespace {
uint32_t hash_u32(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

uint32_t chunk_pos_seed(const Vector3i &chunk_pos) {
    uint32_t seed = 2166136261u;
    seed = (seed ^ static_cast<uint32_t>(chunk_pos.x)) * 16777619u;
    seed = (seed ^ static_cast<uint32_t>(chunk_pos.y)) * 16777619u;
    seed = (seed ^ static_cast<uint32_t>(chunk_pos.z)) * 16777619u;
    return hash_u32(seed);
}

float rand01(uint32_t &state) {
    state = hash_u32(state + 0x9e3779b9u);
    return static_cast<float>(state & 0x00ffffffu) / 16777215.0f;
}

float rand_signed(uint32_t &state) {
    return rand01(state) * 2.0f - 1.0f;
}

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
    //_build_debug_mesh_edge_points();
}

void ChunkClass::initialize(const Vector3i &p_chunk_pos, SDFBase *p_sdf) {
    chunk_pos = p_chunk_pos;
    sdf = p_sdf;

    set_position(ChunkMath::chunk_to_world(chunk_pos));

    mesh_info.initialize(chunk_pos, sdf);
    mesh_info.execute_on_self();
    _build_chunk_mesh();
    //_build_debug_mesh_point();

    state = ChunkState::OUTER_MESH;
}

void ChunkClass::rebuild_mesh() {
    _build_chunk_mesh();
}

void ChunkClass::_build_chunk_mesh() {
    if (mesh_instance == nullptr) {
        mesh_instance = memnew(MeshInstance3D);
        add_child(mesh_instance);
    }

    const auto &points = mesh_info.vertices_data.points;
    const auto &raw_indices = mesh_info.vertices_data.vertices;
    const auto &outer_points = mesh_info.vertices_data.outer_point;
    const auto &outer_raw_indices = mesh_info.vertices_data.outer_vertices;

    if ((points.empty() || raw_indices.empty()) && (outer_points.is_empty() || outer_raw_indices.is_empty())) {
        mesh_instance->set_mesh(Ref<ArrayMesh>());
        return;
    }

    const Vector3 chunk_origin = ChunkMath::chunk_to_world(chunk_pos);

    // 1. Gather all shared positions first (in local chunk space)
    PackedVector3Array shared_positions;
    const int base_vertex_count = static_cast<int>(points.size());
    const int outer_vertex_count = outer_points.size();
    shared_positions.resize(base_vertex_count + outer_vertex_count);

    for (size_t i = 0; i < points.size(); ++i) {
        const Vector3 world_pos = ChunkMath::vertices_to_world(chunk_pos, points[i]);
        shared_positions.set(static_cast<int>(i), world_pos - chunk_origin);
    }

    for (int i = 0; i < outer_vertex_count; ++i) {
        const Vector3 world_pos = outer_points[i];
        shared_positions.set(base_vertex_count + i, world_pos - chunk_origin);
    }

    // 2. Prepare flat arrays (Maximum possible size = number of raw indices)
    const size_t max_indices = raw_indices.size() + outer_raw_indices.size();
    PackedVector3Array final_vertices;
    PackedVector3Array final_normals;
    PackedInt32Array final_indices;
    
    final_vertices.resize(max_indices);
    final_normals.resize(max_indices);
    final_indices.resize(max_indices);
    
    int current_vertex_idx = 0;
    constexpr float min_area_sq = 1e-12f;
    const float normal_inset = 0.01f; // 5% inset toward triangle midpoint

    // Lambda to process a single triangle, duplicate points, and calculate inset normals
    auto process_triangle = [&](int idx_a, int idx_b, int idx_c) {
        // if (idx_a < 0 || idx_b < 0 || idx_c < 0 || 
        //     idx_a >= shared_positions.size() || idx_b >= shared_positions.size() || idx_c >= shared_positions.size()) {
        //     return;
        // }
        // if (idx_a == idx_b || idx_b == idx_c || idx_c == idx_a) {
        //     return;
        // }

        const Vector3 va = shared_positions[idx_a];
        const Vector3 vb = shared_positions[idx_b];
        const Vector3 vc = shared_positions[idx_c];
        
        // Calculate raw geometric face normal
        const Vector3 face_normal_cross = (vb - va).cross(vc - va);
        // if (face_normal_cross.length_squared() <= min_area_sq) {
        //     return; // Degenerate triangle
        // }
        const Vector3 geometric_normal = face_normal_cross.normalized();

        // Calculate triangle midpoint (centroid) in local space
        const Vector3 centroid = (va + vb + vc) / 3.0f;

        const Vector3 tri_verts[3] = {va, vb, vc};

        // Output 3 brand new, distinct vertices for this triangle
        for (int j = 0; j < 3; ++j) {
            const Vector3 v = tri_verts[j];
            Vector3 n;

            if (sdf != nullptr) {
                // Lerp 5% toward the centroid, then shift to world space to evaluate SDF
                const Vector3 sample_point = v.lerp(centroid, normal_inset) + chunk_origin;
                n = -sdf->evaluate_normal(sample_point);
                
                const float len = n.length();
                if (len > 1e-8f) {
                    n /= len;
                } else {
                    n = geometric_normal; // Fallback if SDF gradient is zero
                }
            } else {
                n = geometric_normal;
            }

            final_vertices.set(current_vertex_idx, v);
            final_normals.set(current_vertex_idx, n);
            final_indices.set(current_vertex_idx, current_vertex_idx); // 1:1 mapping
            
            current_vertex_idx++;
        }
    };

    // 3. Process Base chunk triangles
    for (size_t i = 0; i + 2 < raw_indices.size(); i += 3) {
        process_triangle(
            static_cast<int>(raw_indices[i]),
            static_cast<int>(raw_indices[i + 1]),
            static_cast<int>(raw_indices[i + 2])
        );
    }

    // 4. Process Outer/Patch triangles (offset by base_vertex_count)
    for (int i = 0; i + 2 < outer_raw_indices.size(); i += 3) {
        process_triangle(
            base_vertex_count + static_cast<int>(outer_raw_indices[i]),
            base_vertex_count + static_cast<int>(outer_raw_indices[i + 1]),
            base_vertex_count + static_cast<int>(outer_raw_indices[i + 2])
        );
    }

    // 5. Shrink arrays to fit exactly the number of valid vertices produced
    if (current_vertex_idx == 0) {
        mesh_instance->set_mesh(Ref<ArrayMesh>());
        return;
    }

    final_vertices.resize(current_vertex_idx);
    final_normals.resize(current_vertex_idx);
    final_indices.resize(current_vertex_idx);

    // 6. Build the Godot ArrayMesh
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = final_vertices;
    arrays[Mesh::ARRAY_NORMAL] = final_normals;
    arrays[Mesh::ARRAY_INDEX] = final_indices;

    Ref<ArrayMesh> chunk_mesh;
    chunk_mesh.instantiate();
    chunk_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    mesh_instance->set_mesh(chunk_mesh);

    Ref<StandardMaterial3D> surface_material;
    surface_material.instantiate();
    surface_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_PER_PIXEL);
    surface_material->set_roughness(1.0f);
    surface_material->set_metallic(0.0f);
    mesh_instance->set_material_override(surface_material);
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
    PackedColorArray colors;
    vertices.resize(static_cast<int>(points.size()));
    colors.resize(static_cast<int>(points.size()));

    const uint32_t chunk_seed = chunk_pos_seed(chunk_pos);
    const float jitter_strength = ChunkMath::world_chunk_size(chunk_pos) * 0.01f;
    uint32_t chunk_color_seed = hash_u32(chunk_seed ^ 0xa511e9b3u);
    const Color chunk_color = hsv_to_rgb(rand01(chunk_color_seed), 0.75f, 0.95f);
    uint32_t chunk_offset_seed = hash_u32(chunk_seed ^ 0x27d4eb2fu);
    const Vector3 chunk_offset(
        rand_signed(chunk_offset_seed) * jitter_strength,
        rand_signed(chunk_offset_seed) * jitter_strength,
        rand_signed(chunk_offset_seed) * jitter_strength
    );
    for (size_t i = 0; i < points.size(); ++i) {
        const Vector3 global_pos = ChunkMath::vertices_to_world(chunk_pos, points[i]);

        vertices.set(static_cast<int>(i), global_pos - chunk_center + chunk_offset);
        colors.set(static_cast<int>(i), chunk_color);
    }

    // Create arrays
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;
    arrays[Mesh::ARRAY_COLOR] = colors;

    // Add surface as point cloud
    point_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_POINTS, arrays);

    // Set mesh
    point_mesh_instance->set_mesh(point_mesh);

    // Create and set material
    Ref<StandardMaterial3D> point_material;
    point_material.instantiate();
    point_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    point_material->set_albedo(Color(1.0f, 1.0f, 1.0f, 1.0f));
    point_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
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