#include "chunk_class.h"


void ChunkClass::_bind_methods() {
	//godot::ClassDB::bind_method(D_METHOD("print_type", "variant"), &ChunkClass::print_type);
}

void ChunkClass::initialize_debug(const Vector3i &p_chunk_pos) {
    chunk_pos = p_chunk_pos;

    set_position(ChunkMath::chunk_to_world(chunk_pos));
    _build_debug_mesh();
}

void ChunkClass::_build_debug_mesh() {
    const float chunk_size = ChunkMath::world_chunk_size(chunk_pos);

    if (mesh_instance == nullptr) {
        mesh_instance = memnew(MeshInstance3D);
        add_child(mesh_instance);
    }

    debug_box_mesh.instantiate();
    debug_box_mesh->set_size(Vector3(chunk_size, chunk_size, chunk_size));
    mesh_instance->set_mesh(debug_box_mesh);
}

void ChunkClass::_ready() {
    if (mesh_instance == nullptr) {
        _build_debug_mesh();
    }
}