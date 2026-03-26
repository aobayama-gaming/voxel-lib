#pragma once

namespace VoxelEngineConstants {
    // Can be modified later, define the chunk size.
    inline constexpr int CHUNK_SIZE = 8; // 8x8x8 voxels per chunk.
    inline constexpr float VOXEL_SIZE = 0.1f; // Each voxel is 10cm in size.

    // Chunk dimension in world space
    inline constexpr float CHUNK_DIMENSION = CHUNK_SIZE * VOXEL_SIZE; // World size of a chunk
    
}