#pragma once

#include "godot_cpp/classes/engine.hpp"

using namespace godot;

namespace ChunkMath{

    // Just the signatures, no function bodies
    int32_t get_parent_from_child(int32_t coord, int &lod);
    int32_t get_parent_from_child(int32_t coord);

}
