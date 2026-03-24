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

    //DEBUG
    SDFBase *sdf = memnew(SDFDummy);
    set_sdf(sdf);
}

void VoxelEngineClass::_process(double delta) {

    // if (camera) {
    //     Vector3 cam_pos = camera->get_global_position();
    //     print_line(vformat("Camera position: %s", cam_pos));
    // }
}

void VoxelEngineClass::center_on_camera() {
    if (!camera) {
        return;
    }

    Vector3 cam_pos = camera->get_global_position();

    Vector3i new_center_chunk = Vector3i(
        Math::floor(cam_pos.x / VoxelEngineConstants::CHUNK_SIZE), 
        Math::floor(cam_pos.y / VoxelEngineConstants::CHUNK_SIZE), 
        Math::floor(cam_pos.z / VoxelEngineConstants::CHUNK_SIZE)
    );
    if (new_center_chunk != center_chunk) {
        center_chunk = new_center_chunk;
        print_line(vformat("New center chunk: %s", center_chunk));
    }


}

void VoxelEngineClass::scan_chunks_to_load() {

    const int lod_count = lod_distances.size()-1; 

    const int max_lod = lod_count-1; // The maximum LOD level to consider for loading. This is determined by the size of the lod_distances array, which should be set up so that each LOD level corresponds to a distance threshold.

    const int max_lod_radius = 1 << (max_lod); // The radius of the largest LOD. This is the maximum distance at which chunks will be loaded, so it determines the size of the area to scan.

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

    for (const Vector3i &chunk_pos : scanned_chunks) {
        if (!loaded_chunks.has(chunk_pos) && !empty_chunks.has(chunk_pos)) {
            
            const int lod = ChunkMath::get_lod(chunk_pos);
            
            if (abs(sdf->evaluate(ChunkMath::chunk_to_world(chunk_pos))) > ChunkMath::world_chunk_spherical_radius(chunk_pos)) {
                empty_chunks.insert(chunk_pos);
            }
            else {
                chunks_to_load_by_lod[lod].insert(chunk_pos);
            }

        }
    }

    // Erase loaded chunks that are not in the scanned chunks anymore.
    for (const Vector3i &chunk_pos : loaded_chunks) {
        if (!scanned_chunks.has(chunk_pos)) {
            loaded_chunks.erase(chunk_pos);
        }
    }
}

 void _recursive_chunk_scan(const Vector3i &player_chunk,const Vector3i &parent_chunk, PackedFloat32Array &lod_distances, LODHashSet &scanned_chunks)
 {
    const int current_lod = ChunkMath::get_lod(parent_chunk);

    if (current_lod ==0){
        scanned_chunks.insert(parent_chunk);
        return;
    }

    const double distance_to_player = player_chunk.distance_to(parent_chunk);

    if (distance_to_player - (1<<current_lod)*0.71f < lod_distances[current_lod-1]) {

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


void VoxelEngineClass::load_chunks() {
    // Load the chunks in the queue. This is separated from the scan function to avoid doing heavy operations in the scan function, which is called every frame.

    for (int lod = 0; lod < chunks_to_load_by_lod.size(); ++lod) {
        for (const Vector3i &chunk_pos : chunks_to_load_by_lod[lod]) {
            if (!loaded_chunks.has(chunk_pos) && !empty_chunks.has(chunk_pos)) {
                
                // Dummy load function
                ChunkClass *chunk = memnew(ChunkClass);
                chunk->initialize_debug(chunk_pos);
                add_child(chunk);

                chunks.insert(chunk_pos, chunk);
                loaded_chunks.insert(chunk_pos);
            }
        }
        chunks_to_load_by_lod[lod].clear();
    }
}