#include "voxel_engine_class.h"
#include "voxel_constant.h"

#include "sdf_dummy.h"

#include "chunk_math.hpp"

void VoxelEngineClass::_bind_methods() {
    // Bind properties so they appear in editor

    ClassDB::bind_method(D_METHOD("set_lod_distances", "distances"), &VoxelEngineClass::set_lod_distances);
    ClassDB::bind_method(D_METHOD("get_lod_distances"), &VoxelEngineClass::get_lod_distances);
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "lod_distances"), "set_lod_distances", "get_lod_distances");


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

    const int load_radius = lod_distances[lod_distances.size() - 1]; // The last LOD distance is the maximum distance at which to load chunks.

    const int lod_count = lod_distances.size()-1; 
    // last index is the max distance, so the number of LODs is size - 1.

    Vector3i scan = Vector3i(-load_radius, -load_radius, -load_radius) - center_chunk;

    Vector3i scan_end= Vector3i(load_radius, load_radius, load_radius) - center_chunk;

    int scan_lod = lod_count-1; // Start scanning from the lowest LOD, which is the one with the largest distance.

    while (scan.x <= scan_end.x) {
        while (scan.y <= scan_end.y) {
            while (scan.z <= scan_end.z) {
                const int is_empty = empty_chunks.get_max_lod(scan,lod_count-1);
                const int is_loaded = loaded_chunks.get_max_lod(scan,lod_count-1);
                // print_line(vformat("Scanning chunk: %s", chunk_pos));
                //scan_x += VoxelEngineConstants::CHUNK_SIZE;
            }
            scan.x = -load_radius;
            //scan.y += VoxelEngineConstants::CHUNK_SIZE;
        }
        scan.y = -load_radius;
        //scan.z += VoxelEngineConstants::CHUNK_SIZE;
    }
}
