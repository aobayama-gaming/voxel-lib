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


using namespace godot;

// The key for identifying a chunk by its position and LOD.
struct ChunkKey {
    Vector3i position;
    int lod = 0;

    // Equality check, which HashSet uses directly.
    bool operator==(const ChunkKey& other) const {
        return position == other.position && lod == other.lod;
    }
};

struct ChunkKeyHasher {
    // Hash function for ChunkKey, which HashSet uses directly.
    static _FORCE_INLINE_ uint32_t hash(const ChunkKey& key) {
        // Combine the hash of the position and LOD. This is a simple combination; you can use a more complex one if needed.
        uint32_t h1 = HashMapHasherDefault::hash(key.position);
        uint32_t h2 = HashMapHasherDefault::hash(key.lod);
        return hash_murmur3_one_32(h1,h2); // Example of combining two hashes.
    }
};



class LODHashSet : public HashSet<ChunkKey,ChunkKeyHasher> {
public:
    bool has_lod(const Vector3i &p_chunk_pos, int lod) const {
        // Check if the chunk at the given position and LOD is in the set.
        // This is a placeholder implementation. You can modify it to suit your needs.
        ChunkKey key;
        key.position = p_chunk_pos;
        key.lod = lod;
        return has(key);
    }
    int get_lod(const Vector3i &p_chunk_pos,int max_lod) const {
        // Get the LOD of the chunk at the given position. This is a placeholder implementation.
        for (int lod = 0; lod < max_lod; lod++) {
            ChunkKey key;
            key.position = p_chunk_pos/(1 << lod);
            key.lod = lod;
            if (has(key)) {
                return lod;
            }
        }
        return -1; // Not found
    }
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

    void _recursive_chunk_scan(const Vector3i &player_pos, con)

public:

    
    void set_lod_distances(const PackedFloat32Array &p_distances);
    PackedFloat32Array get_lod_distances() const;

    void set_sdf(SDFBase *p_sdf);
    SDFBase* get_sdf() const;

    void _ready() override;
    void _process(double delta) override;
	VoxelEngineClass() = default;
	~VoxelEngineClass() override = default;

    void scan_chunks_to_load(); 
    // scan the area around the camera determine which chunk to load and their LOD.
    
    void center_on_camera(); 
    // Center the camera on the current chunk. Used when the camera is moved manually to avoid having the engine load/unload chunks unnecessarily.
};