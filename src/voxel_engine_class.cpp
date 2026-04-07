#include "voxel_engine_class.h"
#include "voxel_constant.h"

#include "sdf_dummy.h"

#include "chunk_math.hpp"



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

    if (lod_distances.is_empty()) {
        //hard coded base distances for LOD levels. Can be modified later to be more dynamic or to be set in the editor.
        const float phi = 2.0f;
        const float alpha = 0.9f;
        const int lod_levels = 15;

        PackedFloat32Array generated_distances;
        generated_distances.resize(lod_levels+1);

        float value = alpha;
        for (int i = 0; i < lod_levels+1; ++i) {
            generated_distances.set(i, value);
            value *= phi;
        }

        set_lod_distances(generated_distances);
    } else {
        chunks_to_load_by_lod.resize(lod_distances.size()-1);
    }

    print_line("LOD Distances: ");
    for (int i = 0; i < lod_distances.size()-1; ++i) {
        print_line(vformat("LOD %d: %f", i, lod_distances[i]*VoxelEngineConstants::CHUNK_DIMENSION)); // Convert from chunk distance to world distance for easier understanding.
    }
    print_line(vformat("Render Distance : %f", lod_distances[lod_distances.size()-1]*VoxelEngineConstants::CHUNK_DIMENSION));
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

            const float chunk_outer_radius = ChunkMath::world_chunk_size(chunk_pos) * 0.8660254f; // radius that contain the cube
            if (abs(sdf->evaluate(ChunkMath::chunk_to_world(chunk_pos))) > chunk_outer_radius) {
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

    if (distance_to_player - (2<<current_lod)*1.4f < lod_distances[current_lod-1]) {

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
                chunk->initialize_debug(chunk_pos,sdf);
                add_child(chunk);

                chunks.insert(chunk_pos, chunk);
                loaded_chunks.insert(chunk_pos);
            }
        }
        chunks_to_load_by_lod[lod].clear();
    }

    print_line(vformat("Loaded chunks: %d", loaded_chunks.size()));
}

void VoxelEngineClass::run_chunk_pipeline() {
    scan_chunks_to_load();
    prepare_chunks_to_load();
    load_chunks();
}