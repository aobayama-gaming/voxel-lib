#pragma once

namespace VoxelEngineConstants {
    // Can be modified later, define the chunk size.
    inline constexpr int CHUNK_SIZE = 32; // 8x8x8 voxels per chunk.
    inline constexpr float VOXEL_SIZE = 0.5f; // Each voxel is 10cm in size.

    inline constexpr int SKIRT_SIZE = 2;

    // Chunk dimension in world space
    inline constexpr float CHUNK_DIMENSION = (CHUNK_SIZE-SKIRT_SIZE*2) * VOXEL_SIZE; // World size of a chunk, Offset by one
    
}