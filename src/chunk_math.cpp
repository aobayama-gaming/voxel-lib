
#include "chunk_math.hpp"
#include "voxel_constant.h"

int32_t ChunkMath::get_parent_from_child(int32_t coord, int &lod) {
    // Get the center coordinate of the parent chunk.

    // 1. Find the index of the least significant bit (LSB).
    //    For 5 (101), the LSB is at index 0.
    //    For 6 (110), the LSB is at index 1.
    unsigned long lsb_index;
    if (_BitScanForward(&lsb_index, coord) == 0) {
        return 0; // If coord is 0, its parent is 0.
    }

    lod = lsb_index; // The LOD is the index of the LSB.

    // 2. "set to 0 the least importance"
    //    Create a mask to clear that one bit.
    int32_t cleared_coord = coord & ~(1 << lsb_index);

    // 3. "flip to 1 the one after"
    //    Set the bit at the next higher index.
    int32_t parent_coord = cleared_coord | (1 << (lsb_index + 1));

    return parent_coord;
}

int32_t ChunkMath::get_parent_from_child(int32_t coord ) {

    int dummy_lod;
    return get_parent_from_child(coord, dummy_lod);
}

Vector3i ChunkMath::get_parent_from_child(Vector3i coord,int &lod) {
    return Vector3i(
        get_parent_from_child(coord.x, lod),
        get_parent_from_child(coord.y),
        get_parent_from_child(coord.z)
    );
}

Vector3i ChunkMath::get_parent_from_child(Vector3i coord) {
    return Vector3i(
        get_parent_from_child(coord.x),
        get_parent_from_child(coord.y),
        get_parent_from_child(coord.z)
    );
}

int32_t ChunkMath::get_lod(Vector3i coord) {
    // Get the LOD of the chunk at the given coordinate. The LOD is determined by the largest power of 2 from any coordinate.

    unsigned long lod_x = 0;
    if (_BitScanForward(&lod_x, coord.x) == 0) {
        return 0; // If coord.x is 0, it contributes no LOD.
    }

    return lod_x;
}

Vector3i ChunkMath::get_parent_from_child_until(Vector3i coord, int target_lod) {
    // Get the parent chunk at the target LOD. This is done by repeatedly getting the parent until the desired LOD is reached.

    Vector3i parent = coord;
    int current_lod = get_lod(parent);

    while (current_lod > target_lod) {
        parent = get_parent_from_child(parent);
        current_lod++;
    }

    return parent;
}

Vector3 ChunkMath::chunk_to_world(Vector3i chunk_coord) {
    // Convert chunk coordinates to world coordinates. This is done by multiplying the chunk coordinates by the chunk size and voxel size.

    return Vector3(
        (chunk_coord.x>>1) * VoxelEngineConstants::CHUNK_SIZE * VoxelEngineConstants::VOXEL_SIZE,
        (chunk_coord.y>>1) * VoxelEngineConstants::CHUNK_SIZE * VoxelEngineConstants::VOXEL_SIZE,
        (chunk_coord.z>>1) * VoxelEngineConstants::CHUNK_SIZE * VoxelEngineConstants::VOXEL_SIZE
    );
}

float ChunkMath::world_chunk_size(Vector3i chunk_coord) {
    // Get the world size of the chunk at the given coordinate. The world size is determined by the chunk size multiplied by the LOD.
    int lod = ChunkMath::get_lod(chunk_coord);
    return (1 << lod) * VoxelEngineConstants::CHUNK_SIZE * VoxelEngineConstants::VOXEL_SIZE;
}

float ChunkMath::world_chunk_spherical_radius(Vector3i chunk_coord) {
    // Get the world spherical radius of the chunk at the given coordinate. This is used for distance calculations when determining which chunks to load.
    // The spherical radius is the distance from the center of the chunk to any of its corners, which can be calculated using the Pythagorean theorem in 3D.

    float half_size = world_chunk_size(chunk_coord) * 0.5f;
    return sqrt(half_size * half_size * 3); // sqrt(x^2 + y^2 + z^2) where x=y=z=half_size
}