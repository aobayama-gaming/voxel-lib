#pragma once

// Use to get the editor camera.
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/editor_interface.hpp"
#include "godot_cpp/classes/sub_viewport.hpp"

#include "godot_cpp/classes/camera3d.hpp"

#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/variant.hpp"

#include "godot_cpp/templates/hash_map.hpp"
#include "godot_cpp/templates/hash_set.hpp"
#include "godot_cpp/templates/hashfuncs.hpp" // To overload the hash function for our custom ChunkKey type.

#include "chunk_class.h"

#include "sdf_base.h"

#include "chunk_math.hpp"


using namespace godot;

class LODHashSet : public HashSet<Vector3i> {
// HashSet of chunk position. The position is stored as the chunk center (LOD 0 : 2 wide),
// It enables fast LOD calculation and chunk management.

public:

    int get_max_lod(const Vector3i &p_chunk_pos,int max_lod) const {
        // Get if a parent of the chunk is already in the HashSet, and return the LOD of the parent chunk. If no parent is found, return -1.
        
        int actual_lod = 0;

        while (actual_lod < max_lod) {
            int32_t parent_x = ChunkMath::get_parent_from_child(p_chunk_pos.x, actual_lod);
            int32_t parent_y = ChunkMath::get_parent_from_child(p_chunk_pos.y);
            int32_t parent_z = ChunkMath::get_parent_from_child(p_chunk_pos.z);

            Vector3i parent_pos(parent_x, parent_y, parent_z);

            if (has(parent_pos)) {
                return actual_lod;
            }
            actual_lod++;
        };

        return -1; // Not found
    };
};

class VoxelEngineClass : public Node3D {
	GDCLASS(VoxelEngineClass, Node3D)

protected:
	static void _bind_methods();
    
private:
    Camera3D *camera;
    HashMap<Vector3i, ChunkClass*> chunks; // chunks loaded (maybe duplicate of the loaded_chunks set)
    LODHashSet dirty_chunks; // Chunks that need to be updated/treated. (not used)
    LODHashSet loaded_chunks; // Chunks that are currently loaded in the scene.
    LODHashSet empty_chunks; // Chunks that are empty. Caching these avoid over generation.

    Vector3i center_chunk; // The chunk the camera is currently in. Used to determine which chunks to load/unload. 

    PackedFloat32Array lod_distances = {32.0f, 64.0f, 128.0f,256.0f}; // The distances at which to switch LOD. The index of the distance in the array is the LOD level.

    SDFBase *sdf =nullptr; // The SDF used to generate the chunks. Can be modified later to support multiple SDFs or a more complex SDF system.

    //void _recursive_chunk_scan(const Vector3i &player_pos, con)

public:

    
    void set_lod_distances(const PackedFloat32Array &p_distances);
    PackedFloat32Array get_lod_distances() const;

    void set_sdf(SDFBase *p_sdf);
    SDFBase* get_sdf() const;

    void _ready() override;
    void _process(double delta) override;
	VoxelEngineClass() = default;
	~VoxelEngineClass() override = default;

    Vector3i debug_get_parent_chunk(const Vector3i &child_pos) const; // Debug function to test the parent chunk calculation.

    void scan_chunks_to_load(); 
    // scan the area around the camera determine which chunk to load and their LOD.
    
    void center_on_camera(); 
    // Center the camera on the current chunk. Used when the camera is moved manually to avoid having the engine load/unload chunks unnecessarily.
};