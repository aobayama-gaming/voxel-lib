#pragma once

#include "godot_cpp/classes/engine.hpp"

using namespace godot;

namespace ChunkMath{

    // Just the signatures, no function bodies
    int32_t get_parent_from_child(int32_t coord, int &lod);
    int32_t get_parent_from_child(int32_t coord);

    Vector3i get_parent_from_child(Vector3i coord, int &lod);
    Vector3i get_parent_from_child(Vector3i coord);

    int32_t get_lod(Vector3i coord);

    Vector3i get_parent_from_child_until(Vector3i coord, int target_lod);

    Vector3 chunk_to_world(Vector3i chunk_coord);
    Vector3i world_to_chunk(Vector3 world_coord);

    Vector3 vertices_to_world(Vector3i chunk_coord, Vector3 local_coord);
    Vector3 world_to_vertices(Vector3i chunk_coord, Vector3 world_coord);

    float cell_size(Vector3i chunk_coord);// not implemented
    
    float world_chunk_size(Vector3i chunk_coord);
    float world_chunk_spherical_radius(Vector3i chunk_coord);
}
