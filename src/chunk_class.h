#pragma once

#include "godot_cpp/classes/array_mesh.hpp"
#include "godot_cpp/classes/box_mesh.hpp"
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/standard_material3d.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/variant.hpp"

#include "chunk_math.hpp"
#include "mesh_generator_class.hpp"

using namespace godot;

enum class ChunkState {
	DIRTY =0, // The chunk needs to be generated or updated.
	EVALUATED, // The chunk voxel data has been evaluated, but the mesh has not been generated yet.
	POINT_CLOUD, // The chunk used the voxel data to generate the point cloud.
	INNER_MESH, // The chunk generated the inner mesh, but not the outer mesh.
	OUTER_MESH, // The chunk generated the outer mesh through patch.
};

class ChunkClass : public Node3D {
	GDCLASS(ChunkClass, Node3D)

protected:
	static void _bind_methods();
    
private:
    Ref<BoxMesh> debug_box_mesh;
	Ref<BoxMesh> outline_box_mesh;
	Ref<StandardMaterial3D> debug_material;
	Ref<StandardMaterial3D> outline_material;
    MeshInstance3D *mesh_instance = nullptr;
	MeshInstance3D *outline_mesh_instance = nullptr;
	MeshInstance3D *point_mesh_instance = nullptr;
	Vector3i chunk_pos; // The position of the chunk in chunk coordinates (not world coordinates). This is used to calculate the LOD and the position of the chunk in the world.

	ChunkState state = ChunkState::DIRTY;

	void _build_debug_mesh();
	void _build_debug_mesh_point();

public:

	MeshBufferClass mesh_info;

    void _ready() override;
	void initialize_debug(const Vector3i &p_chunk_pos);
	ChunkClass() = default;
	~ChunkClass() override = default;

};
