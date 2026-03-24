#pragma once

#include "godot_cpp/classes/array_mesh.hpp"
#include "godot_cpp/classes/box_mesh.hpp"
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/variant.hpp"

#include "chunk_math.hpp"

using namespace godot;

class ChunkClass : public Node3D {
	GDCLASS(ChunkClass, Node3D)

protected:
	static void _bind_methods();
    
private:
    Ref<BoxMesh> debug_box_mesh;
    MeshInstance3D *mesh_instance = nullptr;
	Vector3i chunk_pos; // The position of the chunk in chunk coordinates (not world coordinates). This is used to calculate the LOD and the position of the chunk in the world.


	void _build_debug_mesh();

public:
    void _ready() override;
	void initialize_debug(const Vector3i &p_chunk_pos);
	ChunkClass() = default;
	~ChunkClass() override = default;

};
