#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/node.hpp"
#include "voxel_constant.h"

#include "sdf_base.h"

using namespace godot;

struct VerticesData {



    struct EdgeGrid3D {
        int32_t width = 0;
        int32_t height = 0;
        int32_t depth = 0;
        std::vector<uint32_t> values;

        void resize(int32_t p_width, int32_t p_height, int32_t p_depth) {
            width = p_width;
            height = p_height;
            depth = p_depth;
            values.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth), 0);
        }

        int32_t index(int32_t x, int32_t y, int32_t z) const {
            return x + width * (y + height * z);
        }

        std::vector<uint32_t>::reference operator()(int32_t x, int32_t y, int32_t z) {
            return values[static_cast<size_t>(index(x, y, z))];
        }

        std::vector<uint32_t>::reference operator()(const Vector3i &coord) {
            return (*this)(coord.x, coord.y, coord.z);
        }

        uint32_t operator()(int32_t x, int32_t y, int32_t z) const {
            return values[static_cast<size_t>(index(x, y, z))];
        }

        uint32_t operator()(const Vector3i &coord) const {
            return (*this)(coord.x, coord.y, coord.z);
        }


        // I don't really like this lol.
        // std::vector<bool>::reference at(int32_t x, int32_t y, int32_t z) {
        //     return (*this)(x, y, z);
        // }

        // std::vector<bool>::reference at(const Vector3i &coord) {
        //     return (*this)(coord);
        // }

        // bool at(int32_t x, int32_t y, int32_t z) const {
        //     return (*this)(x, y, z);
        // }

        // bool at(const Vector3i &coord) const {
        //     return (*this)(coord);
        // }
    };

    // Metadata grid of size (chunk_size +1)^2
    struct Metadata {
        int32_t start_trim = 0;
        int32_t end_trim = 0;
        struct Counts {
            int32_t x_edge = 0;
            int32_t y_edge = 0;
            int32_t z_edge = 0;
            int32_t point = 0;
        } counts;
    };

    struct MetadataGrid {
        int32_t height = 0;
        int32_t depth = 0;
        std::vector<Metadata> values;
        std::vector<Metadata::Counts> cumulative_counts;
        bool cache_valid = false;

        void resize(int32_t p_height, int32_t p_depth) {
            height = p_height;
            depth = p_depth;
            values.assign(static_cast<size_t>(height) * static_cast<size_t>(depth), Metadata {});
            cumulative_counts.assign(values.size(), Metadata::Counts {});
            cache_valid = false;
        }

        int32_t index(int32_t y, int32_t z) const {
            return z * height + y;
        }

        Metadata &operator()(int32_t y, int32_t z) {
            return values[static_cast<size_t>(index(y, z))];
        }

        const Metadata &operator()(int32_t y, int32_t z) const {
            return values[static_cast<size_t>(index(y, z))];
        }

        const Metadata::Counts &cum(int32_t y, int32_t z) const {
            return cumulative_counts[static_cast<size_t>(index(y, z))];
        }

        void cache() {
            cumulative_counts.resize(values.size());

            if (depth <= 0 || height <= 0 || values.empty()) {
                cache_valid = true;
                return;
            }

            Metadata::Counts row_accumulator {};
            
            for (int32_t z = 0; z < depth; ++z) {
                

                for (int32_t y = 0; y < height; ++y) {
                    const size_t i = static_cast<size_t>(index(y, z));
                    const Metadata &source = values[i];

                    row_accumulator.x_edge += source.counts.x_edge;
                    row_accumulator.y_edge += source.counts.y_edge;
                    row_accumulator.z_edge += source.counts.z_edge;
                    row_accumulator.point += source.counts.point;

                    cumulative_counts[i] = row_accumulator;
                }
            }

            cache_valid = true;
        }

        bool is_cached() const {
            return cache_valid;
        }
    };

    EdgeGrid3D x_edge_cases;                 // size = all cell corner (chunk_size +1)^3

    int32_t width;
    int32_t height;
    int32_t depth;

    MetadataGrid metadata;

    //result vector
    std::vector<float> x_edge_position; 
    std::vector<Vector3> x_edge_normals;

    std::vector<float> y_edge_position; 
    std::vector<Vector3> y_edge_normals;

    std::vector<float> z_edge_position; 
    std::vector<Vector3> z_edge_normals;

    std::vector<Vector3> points;
    std::vector<int32_t> vertices;

    void configure(int32_t p_width, int32_t p_height, int32_t p_depth) { // The size of this should one unit bigger (difference between center and corner)
        width = p_width;
        height = p_height;
        depth = p_depth;
        x_edge_cases.resize(p_width, p_height, p_depth);
        metadata.resize(p_height, p_depth);
    }

    void configure_x_edge(int32_t p_count) {
        x_edge_position.resize(static_cast<size_t>(p_count));
        x_edge_normals.resize(static_cast<size_t>(p_count));
    }

    void configure_y_edge(int32_t p_count) {
        y_edge_position.resize(static_cast<size_t>(p_count));
        y_edge_normals.resize(static_cast<size_t>(p_count));
    }

    void configure_z_edge(int32_t p_count) {
        z_edge_position.resize(static_cast<size_t>(p_count));
        z_edge_normals.resize(static_cast<size_t>(p_count));
    }

    void configure_points(int32_t p_count) {
        points.resize(static_cast<size_t>(p_count));
    }

    void configure_vertices(int32_t p_count) {
        vertices.resize(static_cast<size_t>(p_count));
    }

    
};

/*
    Memory optimised way (for fast search)

TODO : add edge case and store this as int byte detection on the four to detect wether or not a point should be generated
chunk wise explanation : 

Need to allocate all grid vertices as boolean.

1. First pass, iterate x-edge. along the x-edges to fill the boolean grid, count the number of time sign change happens, mark end and start trimming in metadata.
2. Second pass, iterate ~~edges~~ cell. Using the boolean grid, count the number of time sign change in y and z axis. Use the trimming but be carefull on wether or not we have parallel traversal. Count also the number of point (by using the fact that any sign change equal to a point)

At the end of the second step we have the memory footprint of everything, count of traversal give the number of quad and number of point.
Allocate this memory.

3. Third pass, iterate edges. fill in the point intersection and the normal, to not waste resources we do a binary search from the center of edge. We store the normal also in this pass.

4. Fourth pass, iterate by cell, using all the information before to create the points.

5. Fifth pass, iterate by group of 8-cell to add the vertices in the mesh buffer.

Hopefully this way we can generate the mesh without any dynamic allocation, fully parallel and without computing the same thing twice. By smartly stacking this row wise (to not fuck up the x-metadata) we should be able to expand this for chunk of chunk.

This algorithm is deeply inspired of flying edge. 
some metadata should be added later for streamlining the sewing between LOD

*/

class MeshBufferClass {

public:

    VerticesData vertices_data;
    Vector3i chunk_id;
    SDFBase* sdf;

    void initialize(const Vector3i &p_chunk_id, SDFBase *p_sdf);

    void first_pass(const int32_t p_y_edge,const int32_t p_z_edge);
    void second_pass(const int32_t p_y_edge,const int32_t p_z_edge);
    void third_pass(const int32_t p_y_edge,const int32_t p_z_edge);
    void fourth_pass(const int32_t p_y_cell,const int32_t p_z_cell);
    void fifth_pass(const int32_t p_y_qcell,const int32_t p_z_qcell);

    void execute();


};


class MeshGeneratorClass : public Node {
    GDCLASS(MeshGeneratorClass, Node)

public:

    int32_t batch_size = 16; // The number of chunks to generate per thread. This is used to avoid blocking the main thread for too long when generating a large number of chunks.
    int32_t max_threads = 4; // The maximum number of threads to use for chunk generation. This is used to avoid overloading the system with too many threads.

    void _ready() override;
    void _process(double delta) override;
};
