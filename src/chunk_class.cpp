#include "chunk_class.h"

void ChunkClass::_bind_methods() {
	//godot::ClassDB::bind_method(D_METHOD("print_type", "variant"), &ChunkClass::print_type);
}

void ChunkClass::_ready() {
    mesh = memnew(ArrayMesh);

    // Create random vertices
    PackedVector3Array vertices;
    vertices.push_back(Vector3(0, 0, 0));
    vertices.push_back(Vector3(1, 0, 0));
    vertices.push_back(Vector3(0, 1, 0));
    vertices.push_back(Vector3(1, 1, 0));
    vertices.push_back(Vector3(0, 0, 1));
    vertices.push_back(Vector3(1, 0, 1));

    // Create indices (which vertices form triangles)
    PackedInt32Array indices;
    indices.append(0); indices.append(1); indices.append(2);  // Triangle 1
    indices.append(1); indices.append(3); indices.append(2);  // Triangle 2
    indices.append(4); indices.append(6); indices.append(5);  // Triangle 3 (back)
    indices.append(5); indices.append(6); indices.append(7);  // Triangle 4 (back)

    // Create surface
    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;
    arrays[Mesh::ARRAY_INDEX] = indices;

    mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

    // Create and display
    mesh_instance = memnew(MeshInstance3D);
    mesh_instance->set_mesh(mesh);
    add_child(mesh_instance);
}