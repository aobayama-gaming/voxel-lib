#pragma once

#include "godot_cpp/classes/array_mesh.hpp"
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/variant.hpp"

using namespace godot;

class ChunkClass : public Node3D {
	GDCLASS(ChunkClass, Node3D)

protected:
	static void _bind_methods();
    
private:
    ArrayMesh *mesh;
    MeshInstance3D *mesh_instance;

public:
    void _ready() override;
	ChunkClass() = default;
	~ChunkClass() override = default;

};
