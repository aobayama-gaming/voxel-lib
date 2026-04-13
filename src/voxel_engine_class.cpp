#include "voxel_engine_class.h"
#include "voxel_constant.h"

#include "sdf_dummy.h"

#include "chunk_math.hpp"

#include <algorithm>
#include <cmath>

namespace {
bool _chunk_is_outside_sdf(const SDFBase &sdf, const Vector3i &chunk_pos) {
    const float chunk_size = ChunkMath::world_chunk_size(chunk_pos);
    const float half_size = chunk_size * 0.25f;
    const float chunk_outer_radius = chunk_size ; //0.8660254f
    const Vector3 chunk_center = ChunkMath::chunk_to_world(chunk_pos);

    for (int corner = 0; corner < 8; ++corner) {
        const Vector3 corner_offset(
            (corner & 1) ? half_size : -half_size,
            (corner & 2) ? half_size : -half_size,
            (corner & 4) ? half_size : -half_size
        );

        const float corner_distance = std::abs(sdf.evaluate(chunk_center + corner_offset));
        if (corner_distance <= chunk_outer_radius) {
            return false;
        }
    }

    return true;
}
}



void VoxelEngineClass::_bind_methods() {
    // Bind properties so they appear in editor

    ClassDB::bind_method(D_METHOD("set_lod_distances", "distances"), &VoxelEngineClass::set_lod_distances);
    ClassDB::bind_method(D_METHOD("get_lod_distances"), &VoxelEngineClass::get_lod_distances);
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "lod_distances"), "set_lod_distances", "get_lod_distances");


    // Debug method to test parent chunk calculation
    ClassDB::bind_method(D_METHOD("debug_get_parent_chunk", "child_pos"), &VoxelEngineClass::debug_get_parent_chunk);

}


void VoxelEngineClass::set_lod_distances(const PackedFloat32Array &p_distances) {
    lod_distances = p_distances;
    chunks_to_load_by_lod.resize(lod_distances.size());
}

PackedFloat32Array VoxelEngineClass::get_lod_distances() const {
    return lod_distances;
}


void VoxelEngineClass::set_sdf(SDFBase *p_sdf) {
    sdf = p_sdf;
}
SDFBase *VoxelEngineClass::get_sdf() const {
    return sdf;
}


Vector3i VoxelEngineClass::debug_get_parent_chunk(const Vector3i &child_pos) const {
    // Debug function to test the parent chunk calculation.
    return Vector3i(
        ChunkMath::get_parent_from_child(child_pos.x), 
        ChunkMath::get_parent_from_child(child_pos.y), 
        ChunkMath::get_parent_from_child(child_pos.z)
    );
}


void VoxelEngineClass::_ready() {

    if(Engine::get_singleton()->is_editor_hint()) {
        camera = EditorInterface::get_singleton()->get_editor_viewport_3d()->get_camera_3d();
    }

    //if (lod_distances.is_empty()) {
    if (true) {
        //hard coded base distances for LOD levels. Can be modified later to be more dynamic or to be set in the editor.
        // const float phi = 2.0f;
        // const float alpha = 2.0f;
        // const int lod_levels = 15;

        PackedFloat32Array generated_distances;
        // generated_distances.resize(lod_levels+1);

        // float value = alpha;
        // for (int i = 0; i < lod_levels+1; ++i) {
        //     generated_distances.set(i, value);
        //     value *= phi;
        // }
        // Configuration
        const int lod_levels = 4;

        // LOD thresholds are stored in chunk-coordinate units because the scan
        // logic compares them against chunk-space distances.
        // Keep LOD0 until one base voxel is about ~1 pixel on screen.
        const float target_pixel_error = 5.0f; // acceptable projected geometric error in pixels
        const float viewport_height = 1080.0f;
        const float fov_rad = 75.0f * (3.14159265f / 180.0f);
        const float focal_length = viewport_height / (2.0f * std::tan(fov_rad * 0.5f));

        generated_distances.resize(lod_levels + 1);

        float previous_distance = 0.0f;
        for (int i = 0; i < lod_levels + 1; ++i) {
            // Geometric error follows voxel size per LOD (voxel doubles each level).
            const float lod_voxel_size_world = VoxelEngineConstants::VOXEL_SIZE * static_cast<float>(1 << i);
            const float geometric_error_world = lod_voxel_size_world * 0.5f;

            // Compute threshold in world units, then convert to chunk-coordinate units.
            // One chunk coordinate step equals CHUNK_DIMENSION / 2 in world units.
            float dist_world = (geometric_error_world * focal_length) / target_pixel_error;
            float dist_chunk = dist_world / (VoxelEngineConstants::CHUNK_DIMENSION * 0.5f);

            // Keep thresholds strictly increasing and avoid near-zero first level.
            if (i == 0 && dist_chunk < 1.0f) {
                dist_chunk = 1.0f;
            }
            if (dist_chunk <= previous_distance) {
                dist_chunk = previous_distance + 1.0f;
            }

            generated_distances.set(i, dist_chunk);
            previous_distance = dist_chunk;
        }
        
        set_lod_distances(generated_distances);

    } else {
        chunks_to_load_by_lod.resize(lod_distances.size()-1);
    }

    print_line("LOD Distances: ");
    for (int i = 0; i < lod_distances.size()-1; ++i) {
        print_line(vformat("LOD %d: %f", i, lod_distances[i] * (VoxelEngineConstants::CHUNK_DIMENSION * 0.5f))); // Convert from chunk-coord distance to world distance.
    }
    print_line(vformat("Render Distance : %f", lod_distances[lod_distances.size()-1] * (VoxelEngineConstants::CHUNK_DIMENSION * 0.5f)));
    chunks.max_lod=lod_distances.size()-2;

    //DEBUG
    SDFBase *sdf = memnew(SDFDummy);
    set_sdf(sdf);
}

void VoxelEngineClass::_process(double delta) {
    if (!camera) {
        return;
    }

    const Vector3i new_center_chunk = ChunkMath::world_to_chunk(camera->get_global_position());
    if (new_center_chunk != center_chunk) {
        center_chunk = new_center_chunk;
        run_chunk_pipeline();
        //print_line(vformat("New center chunk: %s", center_chunk));
    }
}

void VoxelEngineClass::center_on_camera() {
    if (!camera) {
        return;
    }

    const Vector3i new_center_chunk = ChunkMath::world_to_chunk(camera->get_global_position());
    if (new_center_chunk != center_chunk) {
        center_chunk = new_center_chunk;
        print_line(vformat("New center chunk: %s", center_chunk));
    }


}

void VoxelEngineClass::scan_chunks_to_load() {

    const int lod_count = lod_distances.size()-1; 

    const int max_lod = lod_count-1; // The maximum LOD level to consider for loading. This is determined by the size of the lod_distances array, which should be set up so that each LOD level corresponds to a distance threshold.

    const int max_lod_radius = 2 << (max_lod); // The radius of the largest LOD. This is the maximum distance at which chunks will be loaded, so it determines the size of the area to scan.

    const int max_lod_number = ceil( lod_distances[lod_count-1] /max_lod_radius)+2; 

    const Vector3i ancestor_chunk = ChunkMath::get_parent_from_child_until(center_chunk, max_lod);

    const Vector3i search_space = Vector3i(max_lod_number, max_lod_number, max_lod_number) * max_lod_radius;

    const Vector3i start_scan = ancestor_chunk - search_space;

    const Vector3i end_scan = ancestor_chunk + search_space;

    for (int x = start_scan.x; x <= end_scan.x; x += max_lod_radius) {
        for (int y = start_scan.y; y <= end_scan.y; y += max_lod_radius) {
            for (int z = start_scan.z; z <= end_scan.z; z += max_lod_radius) {
                Vector3i scan(x, y, z);
                _recursive_chunk_scan(center_chunk, scan, lod_distances, scanned_chunks);
            }
        }
    }
}

void VoxelEngineClass::prepare_chunks_to_load() {
    // Prepare the chunks that need to be loaded. perform erase of loaded useless chunk and fill the queue of chunk to laod.

    // Erase loaded chunks that are not in the scanned chunks anymore.
    std::vector<Vector3i> chunks_to_unload;
    for (const Vector3i &chunk_pos : loaded_chunks) {
        if (!scanned_chunks.has(chunk_pos)) {
            chunks_to_unload.push_back(chunk_pos);
        }
    }

    for (const Vector3i &chunk_pos : chunks_to_unload) {
        ChunkClass **chunk_ptr = chunks.getptr(chunk_pos);
        if (chunk_ptr && *chunk_ptr) {
            (*chunk_ptr)->queue_free();
        }
        //Should add here the decimate/augment logic for the chunk if a child/parent need to be loaded

        chunks.erase(chunk_pos);
        loaded_chunks.erase(chunk_pos);
    }

    

    for (const Vector3i &chunk_pos : scanned_chunks) {
        if (!loaded_chunks.has(chunk_pos) && !empty_chunks.has(chunk_pos)) {
            
            const int lod = ChunkMath::get_lod(chunk_pos);

            if (_chunk_is_outside_sdf(*sdf, chunk_pos)) {
                empty_chunks.insert(chunk_pos);
            }
            else {
                //print_line(vformat("Chunk to load: %s (LOD %d)", chunk_pos, lod));
                chunks_to_load_by_lod[lod].insert(chunk_pos);
            }
        }
    }



    scanned_chunks.clear();
}

namespace {
void _recursive_chunk_scan(const Vector3i &player_chunk,const Vector3i &parent_chunk, PackedFloat32Array &lod_distances, LODHashSet &scanned_chunks)
{
    const int current_lod = ChunkMath::get_lod(parent_chunk);

    if (current_lod ==0){
        scanned_chunks.insert(parent_chunk);
        return;
    }

    const double distance_to_player = player_chunk.distance_to(parent_chunk);

    // Work fully in chunk-coordinate units.
    // Half-extent of current LOD chunk in chunk-coord units is 2^lod.
    const float half_extent = static_cast<float>(1 << current_lod);
    // Bounding sphere radius of the chunk cube in chunk-coord units.
    const float chunk_outer_radius = std::sqrt(3.0f) * half_extent;

    if (distance_to_player - chunk_outer_radius < lod_distances[current_lod-1]) {

        const int child_lod = current_lod - 1;
        const int child_size = 1 << child_lod;

        for (int i = 0; i < 8; ++i) {
            Vector3i child_offset(
                (i & 1) ? child_size : -child_size,
                (i & 2) ? child_size : -child_size,
                (i & 4) ? child_size : -child_size
            );

            _recursive_chunk_scan(
                player_chunk,
                parent_chunk + child_offset,
                lod_distances,
                scanned_chunks
            );
        }
    }
    else {
        scanned_chunks.insert(parent_chunk);
    }
}
}


void VoxelEngineClass::load_chunks() {
    // Load the chunks in the queue. This is separated from the scan function to avoid doing heavy operations in the scan function, which is called every frame.

    // DEBUG: Count total chunks to load
    int total_chunks_to_load = 0;
    for (int lod = 0; lod < chunks_to_load_by_lod.size(); ++lod) {
        total_chunks_to_load += chunks_to_load_by_lod[lod].size();
    }
    print_line(vformat("Total chunks to load: %d", total_chunks_to_load));

    for (int lod = 0; lod < chunks_to_load_by_lod.size(); ++lod) {
        for (const Vector3i &chunk_pos : chunks_to_load_by_lod[lod]) {
            if (!loaded_chunks.has(chunk_pos) && !empty_chunks.has(chunk_pos)) {
                
                // Dummy load function
                ChunkClass *chunk = memnew(ChunkClass);
                chunk->initialize(chunk_pos,sdf);
                add_child(chunk);

                chunks.insert(chunk_pos, chunk);
                loaded_chunks.insert(chunk_pos);
            }
        }
        chunks_to_load_by_lod[lod].clear();
    }

    // // Patch all currently loaded chunks, then rebuild their mesh with outer patch data.
    // for (const Vector3i &chunk_pos : loaded_chunks) {
    //     ChunkClass **chunk_ptr = chunks.getptr(chunk_pos);
    //     if (!chunk_ptr || !*chunk_ptr) {
    //         continue;
    //     }

    //     chunk_patching(**chunk_ptr);
    // }

    // for (const Vector3i &chunk_pos : loaded_chunks) {
    //     ChunkClass **chunk_ptr = chunks.getptr(chunk_pos);
    //     if (!chunk_ptr || !*chunk_ptr) {
    //         continue;
    //     }

    //     (*chunk_ptr)->rebuild_mesh();
    // }

    // print_line(vformat("Loaded chunks: %d", loaded_chunks.size()));

    //Debug dummy patching 
}

void VoxelEngineClass::run_chunk_pipeline() {
    //dummy implementation without any queue.
    scan_chunks_to_load();
    prepare_chunks_to_load();
    load_chunks();
}

void VoxelEngineClass::chunk_patching(ChunkClass &chunk){
    /*
    Chunk patching evaluates external edges of the chunk. 
    If the edge is changed, we grab the 4 surrounding dual points (from the chunk and its neighbors).
    - If any neighbor has a larger LOD, we patch toward them to seal the gap.
    - If the neighbors are equal LOD, we only patch on the positive faces to prevent duplicating quads.
    - An outer_point_cache maps world coordinates to indices to reuse dual points cleanly.
    */
    
    HashMap<Vector3, uint32_t> outer_point_cache;
    std::vector<uint32_t> patch_indices;
    std::vector<Vector3> patch_vertices; 



    const Vector3i chunk_id = chunk.chunk_pos;
    const int32_t current_lod = ChunkMath::get_lod(chunk_id);
    auto& vertices_data = chunk.mesh_info.vertices_data;

    auto get_idx = [&](Vector3i vertices_cord) -> int {
        const int& x = vertices_cord.x;
        const int& y = vertices_cord.y;
        const int& z = vertices_cord.z;

        Vector3* point;

        if(x<0||x>vertices_data.width-1 ||y<0||y>vertices_data.height-1 ||z<0||z>vertices_data.depth-1 ){
            
            const Vector3 global_vertices_position = ChunkMath::vertices_to_world(chunk.chunk_pos,vertices_cord+Vector3(0.5f,0.5f,0.5f));

            const Vector3i lod0_chunk = ChunkMath::world_to_chunk(global_vertices_position);
            
            ChunkClass* neighbor_chunk;

            const int neighbor_lod = chunks.get_chunk(lod0_chunk,neighbor_chunk);

            if(neighbor_lod<ChunkMath::get_lod(chunk.chunk_pos)){
                return -1;
            }

            const Vector3 local_neighbor_vertices = ChunkMath::world_to_vertices(neighbor_chunk->chunk_pos,global_vertices_position);
            const Vector3i local_neighbor_cell = local_neighbor_vertices.floor();

            if(!neighbor_chunk->mesh_info.vertices_data.edge_cache.has(local_neighbor_cell)){
                return -1;
            }

            point = neighbor_chunk->mesh_info.vertices_data.edge_cache.get(local_neighbor_cell);

        }
        else{

            point = chunk.mesh_info.vertices_data.edge_cache.get(vertices_cord);

        }

        if(!outer_point_cache.has(*point)){
            patch_vertices.push_back(*point);
            outer_point_cache.insert(*point,patch_vertices.size()-1);
        }

        return outer_point_cache[*point];
    };

    // Define the boundaries based on vertices width/height/depth
    const int32_t x_max = vertices_data.width ;
    const int32_t y_max = vertices_data.height - 1;
    const int32_t z_max = vertices_data.depth - 1;

    const int32_t x_min = 0; // we don t care about outer x edge
    const int32_t y_min = - 1;
    const int32_t z_min = - 1;


    for (int32_t z = z_min; z < z_max; ++z) {
        for (int32_t y = y_min; y < y_max; ++y) {

            const bool y_boundary = y ==y_min || y==y_max-1;
            const bool z_boundary = z ==z_min || z==z_max-1;

            for (int32_t x = x_min; x < x_max; x++) {

                // X case
                if(x<vertices_data.width){
                    if(y_boundary||z_boundary){
                        uint32_t case_x = vertices_data.x_edge_cases(x, y + 1, z + 1);
                        bool x_changed = MeshEdgeUtils::edge_change(case_x);
                    }
                }


            }

        }
    }

    // // Helper: Finds the loaded chunk owning a specific cell and retrieves its cached point
    // auto get_cell_vertex = [&](Vector3i C, Vector3& out_pos, int& out_lod) -> bool {
    //     // Find midpoint of cell in world space
    //     Vector3 mid_world = ChunkMath::vertices_to_world(chunk_id, Vector3(C.x + 0.5f, C.y + 0.5f, C.z + 0.5f));
    //     Vector3i lod0_pos = ChunkMath::world_to_chunk(mid_world);

    //     ChunkClass* owner = nullptr;
    //     out_lod = chunks.get_chunk(lod0_pos, owner);
    //     if (out_lod < 0 || owner == nullptr) {
    //         return false;
    //     }
        
    //     // Convert world position back to local vertices relative to the owner chunk
    //     Vector3 local_world = ChunkMath::world_to_vertices(owner->chunk_pos, mid_world);
    //     Vector3i local_C(std::floor(local_world.x), std::floor(local_world.y), std::floor(local_world.z));
        
    //     // Fetch from that specific neighbor's precomputed boundary edge cache
    //     Vector3 **pt = owner->mesh_info.vertices_data.edge_cache.getptr(local_C);
    //     if (pt && *pt) {
    //         out_pos = ChunkMath::vertices_to_world(owner->chunk_pos, **pt);
    //         return true;
    //     }
    //     return false;
    // };

    // // Helper: Uses cache to prevent duplicating the newly generated patch points
    // auto get_or_add_vertex = [&](const Vector3& pos) -> uint32_t {
    //     if (outer_point_cache.has(pos)) return outer_point_cache[pos];
    //     uint32_t idx = patch_vertices.size();
    //     outer_point_cache[pos] = idx;
    //     patch_vertices.push_back(pos);
    //     return idx;
    // };

    // // Edge Processing
    // auto process_edge = [&](int axis, int x, int y, int z) {
    //     // Check if this edge is sitting exactly on the positive boundary limits
    //     bool is_positive_face = false;
    //     if (axis == 0) is_positive_face = (y == y_max || z == z_max);
    //     if (axis == 1) is_positive_face = (x == x_max || z == z_max);
    //     if (axis == 2) is_positive_face = (x == x_max || y == y_max);

    //     // Check for an intersection on this edge
    //     Vector3 P0 = ChunkMath::vertices_to_world(chunk_id, Vector3(x, y, z));
    //     Vector3 P1 = P0;
    //     if (axis == 0) P1 = ChunkMath::vertices_to_world(chunk_id, Vector3(x + 1, y, z));
    //     else if (axis == 1) P1 = ChunkMath::vertices_to_world(chunk_id, Vector3(x, y + 1, z));
    //     else if (axis == 2) P1 = ChunkMath::vertices_to_world(chunk_id, Vector3(x, y, z + 1));

    //     float v0 = sdf->evaluate(P0);
    //     float v1 = sdf->evaluate(P1);
    //     if ((v0 < 0.0f) == (v1 < 0.0f)) return; // No change in sign, no boundary intersection here

    //     bool positive = (v0 < 0.0f);

    //     // Get relative coordinates for the 4 surrounding cells
    //     Vector3i C[4];
    //     if (axis == 0) {
    //         C[0] = Vector3i(x, y-1, z-1); C[1] = Vector3i(x, y, z-1);
    //         C[2] = Vector3i(x, y, z);     C[3] = Vector3i(x, y-1, z);
    //     } else if (axis == 1) {
    //         C[0] = Vector3i(x-1, y, z-1); C[1] = Vector3i(x, y, z-1);
    //         C[2] = Vector3i(x, y, z);     C[3] = Vector3i(x-1, y, z);
    //     } else {
    //         C[0] = Vector3i(x-1, y-1, z); C[1] = Vector3i(x, y-1, z);
    //         C[2] = Vector3i(x, y, z);     C[3] = Vector3i(x-1, y, z);
    //     }

    //     Vector3 pts[4];
    //     int max_lod_found = -1;
        
    //     // Grab caching points. Quit forming the quad if boundary data limits are missing.
    //     for(int i = 0; i < 4; ++i) {
    //         int lod = -1;
    //         if (!get_cell_vertex(C[i], pts[i], lod)) return;
    //         if (lod > max_lod_found) max_lod_found = lod;
    //     }

    //     // Apply rules for quad creation:
    //     if (max_lod_found < current_lod) return; // Wait to be patched *by* the finer level chunk
    //     if (max_lod_found == current_lod && !is_positive_face) return; // Let equal LOD positive face chunk handle this

    //     // Quad indices Generation
    //     uint32_t i0 = get_or_add_vertex(pts[0]);
    //     uint32_t i1 = get_or_add_vertex(pts[1]);
    //     uint32_t i2 = get_or_add_vertex(pts[2]);
    //     uint32_t i3 = get_or_add_vertex(pts[3]);

    //     if (positive) {
    //         patch_indices.push_back(i0); patch_indices.push_back(i1); patch_indices.push_back(i2);
    //         patch_indices.push_back(i0); patch_indices.push_back(i2); patch_indices.push_back(i3);
    //     } else {
    //         patch_indices.push_back(i0); patch_indices.push_back(i3); patch_indices.push_back(i2);
    //         patch_indices.push_back(i0); patch_indices.push_back(i2); patch_indices.push_back(i1);
    //     }
    // };

    // // 1. X-edge family sweeps X, keeps Y,Z on boundaries.
    // for (int32_t x = 0; x < x_max; ++x) {
    //     for (int32_t y = 0; y <= y_max; ++y) {
    //         for (int32_t z = 0; z <= z_max; ++z) {
    //             if (y == 0 || y == y_max || z == 0 || z == z_max) {
    //                 process_edge(0, x, y, z);
    //             }
    //         }
    //     }
    // }

    // // 2. Y-edge family sweeps Y, keeps X,Z on boundaries.
    // for (int32_t y = 0; y < y_max; ++y) {
    //     for (int32_t x = 0; x <= x_max; ++x) {
    //         for (int32_t z = 0; z <= z_max; ++z) {
    //             if (x == 0 || x == x_max || z == 0 || z == z_max) {
    //                 process_edge(1, x, y, z);
    //             }
    //         }
    //     }
    // }

    // // 3. Z-edge family sweeps Z, keeps X,Y on boundaries.
    // for (int32_t z = 0; z < z_max; ++z) {
    //     for (int32_t x = 0; x <= x_max; ++x) {
    //         for (int32_t y = 0; y <= y_max; ++y) {
    //             if (x == 0 || x == x_max || y == 0 || y == y_max) {
    //                 process_edge(2, x, y, z);
    //             }
    //         }
    //     }
    // }
    
    // // Export patch buffers to the chunk mesh side-channel arrays.
    // vertices_data.outer_point.clear();
    // vertices_data.outer_vertices.clear();

    // for (const Vector3 &v : patch_vertices) {
    //     vertices_data.outer_point.push_back(v);
    // }

    // for (uint32_t idx : patch_indices) {
    //     vertices_data.outer_vertices.push_back(static_cast<int64_t>(idx));
    // }
}