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

#include <vector>

using namespace godot;



class LODHashSet : public HashSet<Vector3i> {
// HashSet of chunk position. The position is stored as the chunk center (LOD 0 : 2 wide),
// It enables fast LOD calculation and chunk management.

public:

    int get_max_lod(const Vector3i &p_chunk_pos,int max_lod) const {
        // Get if a parent of the chunk is already in the HashSet, and return the LOD of the parent chunk. If no parent is found, return -1.
        
        int actual_lod = 0;

        while (actual_lod < max_lod) {

            Vector3i parent_pos = ChunkMath::get_parent_from_child(p_chunk_pos, actual_lod);

            if (has(parent_pos)) {
                return actual_lod;
            }
            actual_lod++;
        };

        return -1; // Not found
    };
};

class ChunkHashMap : public HashMap<Vector3i, ChunkClass*>{

public:

    uint32_t max_lod;

    int get_max_lod(const Vector3i &p_chunk_pos) const {
        // Get if a parent of the chunk is already in the HashSet, and return the LOD of the parent chunk. If no parent is found, return -1.
        
        int actual_lod = 0;

        while (actual_lod < max_lod) {

            Vector3i parent_pos = ChunkMath::get_parent_from_child(p_chunk_pos, actual_lod);

            if (has(parent_pos)) {
                return actual_lod;
            }
            actual_lod++;
        };

        return -1; // Not found
    };

    int get_chunk(const Vector3i &p_chunk_pos,ChunkClass *chunk){
        // retrieve the chunk or its parent.

        int actual_lod = 0;
        while (actual_lod < max_lod) {

            Vector3i parent_pos = ChunkMath::get_parent_from_child(p_chunk_pos, actual_lod);

            if (has(parent_pos)) {
                chunk = get(parent_pos);
                return actual_lod;
            }
            actual_lod++;
        };

        return -1; // Not found


    };

};

namespace {
    // Scan the area around a chunk recursively to choose the proper LOD leaves/parents.
    void _recursive_chunk_scan(const Vector3i &player_chunk, const Vector3i &parent_chunk, PackedFloat32Array &lod_distances, LODHashSet &scanned_chunks);
};

class VoxelEngineClass : public Node3D {
	GDCLASS(VoxelEngineClass, Node3D)

protected:
	static void _bind_methods();
    
private:
    Camera3D *camera;
    ChunkHashMap chunks; // chunks loaded (maybe duplicate of the loaded_chunks set)

    LODHashSet scanned_chunks; // Chunks that have been scanned for loading. Used to avoid scanning the same chunk multiple times in a row when the camera is still.
    
    //LODHashSet dirty_chunks; // Chunks that need to be updated/treated. (not used)
    LODHashSet loaded_chunks; // Chunks that are currently loaded in the scene.
    LODHashSet empty_chunks; // Chunks that are empty. Caching these avoid over generation.

    Vector3i center_chunk; // The chunk the camera is currently in. Used to determine which chunks to load/unload. 

    PackedFloat32Array lod_distances; // The distances at which to switch LOD. The index of the distance in the array is the LOD level.

    SDFBase *sdf =nullptr; // The SDF used to generate the chunks. Can be modified later to support multiple SDFs or a more complex SDF system.

    std::vector<LODHashSet> chunks_to_load_by_lod;

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

    void chunk_patching( ChunkClass &chunk);
    
    void scan_chunks_to_load(); //Scan the area arount the camera and determine which chunks to load and their LOD.
    
    void prepare_chunks_to_load();//Prepare the chunks that need to be loaded. perform erase of loaded useless chunk and fill the queue of chunk to laod.
    
    void load_chunks(); // Load the chunks in the queue. This is separated from the scan function to avoid doing heavy operations in the scan function, which is called every frame.

    void run_chunk_pipeline(); // Execute full update pipeline: scan, prepare and load.

    void center_on_camera(); // Center the camera on the current chunk. Used when the camera is moved manually to avoid having the engine load/unload chunks unnecessarily.
};