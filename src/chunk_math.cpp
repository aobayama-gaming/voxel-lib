
#include "chunk_math.hpp"

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