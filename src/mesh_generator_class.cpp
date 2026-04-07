#include "mesh_generator_class.hpp"
#include "sdf_dummy.h"
#include "chunk_math.hpp"

inline constexpr int BINARY_SEARCH_STEP = 4;

void MeshBufferClass::initialize(const Vector3i &p_chunk_id, SDFBase *p_sdf) {

    sdf = p_sdf;
	chunk_id = p_chunk_id;

	const int32_t size = VoxelEngineConstants::CHUNK_SIZE + 1; //Vertices
	vertices_data.configure(size-1, size, size); // since it stores the vertices ID it is one less big in the x direction.
}

void MeshBufferClass::first_pass(const int32_t p_y_edge,const int32_t p_z_edge){

    float_t last_value = 0.0f;

    bool first_change = true;

    for(int32_t x=0;x<vertices_data.width;x++){

        const Vector3i vertices_coordinates =  Vector3i(x,p_y_edge,p_z_edge);

        const float_t actual_value = sdf->evaluate(ChunkMath::vertices_to_world(chunk_id , vertices_coordinates));

        if(x>0){

            const uint32_t edge_case = (last_value < 0.0f ? 1 : 0) | (actual_value < 0.0f ? 2 : 0);

            vertices_data.x_edge_cases(vertices_coordinates) = edge_case;

            if(_edge_change(edge_case)){ // sign change

                if(first_change){
                    first_change=false;
                    vertices_data.metadata(p_y_edge,p_z_edge).start_trim = x;
                }
                vertices_data.metadata(p_y_edge,p_z_edge).counts.x_edge++;

                vertices_data.metadata(p_y_edge,p_z_edge).end_trim = x+1;
            }

        }

        last_value=actual_value;

    }

}

void MeshBufferClass::second_pass(const int32_t p_y_cell,const int32_t p_z_cell){


    if( p_y_cell>=vertices_data.height || p_z_cell>=vertices_data.depth ){
        //in case we run an extra row colum, this pass is iterate on CELL.
        return ;
    }

    for( int32_t x=vertices_data.metadata(p_y_cell,p_z_cell).start_trim ; x<vertices_data.metadata(p_y_cell,p_z_cell).end_trim ; x++){

        const uint32_t top_left_case = vertices_data.x_edge_cases(x,p_y_cell,p_z_cell);
        const uint32_t top_right_case = vertices_data.x_edge_cases(x,p_y_cell+1,p_z_cell);
        const uint32_t bottom_left_case = vertices_data.x_edge_cases(x,p_y_cell,p_z_cell+1);
        const uint32_t bottom_right_case = vertices_data.x_edge_cases(x,p_y_cell+1,p_z_cell+1);

        const bool front_top_changed = _transversal_change(top_left_case, top_right_case, true); //y
        const bool front_bottom_changed = _transversal_change(bottom_left_case, bottom_right_case, true); //y

        const bool rear_top_changed = _transversal_change(top_left_case, top_right_case, false); //y
        const bool rear_bottom_changed = _transversal_change(bottom_left_case, bottom_right_case, false); //y


        const bool front_left_changed = _transversal_change(top_left_case, bottom_left_case, true); //z
        const bool front_right_changed = _transversal_change(top_right_case, bottom_right_case, true); //z

        const bool rear_left_changed = _transversal_change(top_left_case, bottom_left_case, false); //z
        const bool rear_right_changed = _transversal_change(top_right_case, bottom_right_case, false); //z
        
        bool changed = front_top_changed || front_left_changed;

        const bool y_max = p_y_cell==vertices_data.height-1;
        const bool z_max = p_z_cell==vertices_data.depth-1;

        auto& meta = vertices_data.metadata(p_y_cell, p_z_cell);

        if(y_max){ // Edge case for max height
            changed |= front_right_changed;
        }
        if(z_max){ // Edge case for max depth
            changed |= front_bottom_changed;
        }


        if(x>0 && x ==meta.start_trim && changed )
        {
            meta.start_trim = 0;
            x=-1;
            continue;
        }

        if(y_max){
            vertices_data.metadata(p_y_cell+1,p_z_cell).counts.z_edge += front_right_changed;
        }
        if(z_max){ // Edge case for max depth
            vertices_data.metadata(p_y_cell,p_z_cell+1).counts.y_edge += front_bottom_changed;
        }
        meta.counts.y_edge += front_top_changed;
        meta.counts.z_edge += front_left_changed;


        if(x == meta.end_trim -1){


            if(x==vertices_data.width-1){ // In case sdf cross on the futhermost 
                meta.counts.y_edge += rear_top_changed;
                meta.counts.z_edge += rear_left_changed;

                if(y_max){ // Edge case for max height
                    vertices_data.metadata(p_y_cell+1,p_z_cell).counts.z_edge += rear_right_changed;
                }
                if(z_max){ // Edge case for max depth
                    vertices_data.metadata(p_y_cell,p_z_cell+1).counts.y_edge += rear_bottom_changed;
                }
            }
            else
            {
                bool far_changed = rear_top_changed || rear_left_changed;
                
                if(y_max){ // Edge case for max height
                    far_changed |= rear_right_changed;
                }
                if(z_max){ // Edge case for max depth
                    far_changed |= rear_bottom_changed;
                }
                if(far_changed){
                    meta.end_trim = vertices_data.width;
                }
            }
            
        }

    }
}

bool MeshBufferClass::_edge_change(uint32_t edge_case)
{
    return edge_case == 1 || edge_case == 2;
}

bool MeshBufferClass::_transversal_change(uint32_t origin_edge, uint32_t paired_edge, bool front)
{
    const uint32_t mask = front ? 0b01u : 0b10u;
    return ((origin_edge ^ paired_edge) & mask) != 0;
}

uint32_t MeshBufferClass::_transversal_combination(uint32_t origin_edge, uint32_t paired_edge, bool front)
{
    const uint32_t origin_bit = front ? (origin_edge & 0b01u) : ((origin_edge & 0b10u) >> 1);
    const uint32_t paired_bit = front ? (paired_edge & 0b01u) : ((paired_edge & 0b10u) >> 1);
    return (origin_bit << 1) | paired_bit;
}

void MeshBufferClass::_find_edge_intersection(const Vector3i &start_point, const Vector3i &end_point, const uint32_t edge_case, VerticesData::EdgeCompute &edge_data, uint32_t &index)
{
    if(edge_case==1 || edge_case==2){
        //dummy implementation

        bool left = edge_case & 0b01u;
        bool right = (edge_case & 0b10u) >> 1;

        Vector3 left_vector = start_point;
        Vector3 right_vector = end_point;

        const Vector3 mid_vector = (right_vector+left_vector)/2;

        for(int i=0;i<BINARY_SEARCH_STEP;i++){

            const bool mid = sdf->evaluate(ChunkMath::vertices_to_world(chunk_id,mid_vector))>0;

            if(left==mid){
                left_vector=mid_vector;
            }
            else{
                right_vector=mid_vector;
            }


        }

        edge_data.local_positions[index] = mid_vector;
        edge_data.normals[index] = sdf->evaluate_normal(mid_vector);

        index--;
    }
}

void MeshBufferClass::third_pass(const int32_t p_y_edge,const int32_t p_z_edge)
{

    uint32_t x_edge_counter = vertices_data.metadata.cum(p_y_edge,p_z_edge).x_edge -1 ; // Init the counter at the end of array
    uint32_t y_edge_counter = vertices_data.metadata.cum(p_y_edge,p_z_edge).y_edge -1; // Init the counter at the end of array
    uint32_t z_edge_counter = vertices_data.metadata.cum(p_y_edge,p_z_edge).z_edge -1; // Init the counter at the end of array

    const bool y_max = p_y_edge==vertices_data.height-1;
    const bool z_max = p_z_edge==vertices_data.depth-1;

    for( int32_t x=vertices_data.metadata(p_y_edge,p_z_edge).start_trim ; x<vertices_data.metadata(p_y_edge,p_z_edge).end_trim ; x++){

        const uint32_t x_edge = vertices_data.x_edge_cases(x,p_y_edge,p_z_edge);
        const Vector3i start_position = Vector3i(x,p_y_edge,p_z_edge);

        _find_edge_intersection(start_position,Vector3i(x+1,p_y_edge,p_z_edge),x_edge,vertices_data.x_edge,x_edge_counter);

        if(!y_max){

            const uint32_t neighbor_edge =  vertices_data.x_edge_cases(x,p_y_edge+1,p_z_edge);
            const uint32_t y_edge = _transversal_combination(x_edge, neighbor_edge, true);

            _find_edge_intersection(start_position,Vector3i(x,p_y_edge+1,p_z_edge),y_edge,vertices_data.y_edge,y_edge_counter);
        }

        if(!z_max){

            const uint32_t neighbor_edge =  vertices_data.x_edge_cases(x,p_y_edge,p_z_edge+1);
            const uint32_t z_edge = _transversal_combination(x_edge, neighbor_edge, true);

            _find_edge_intersection(start_position,Vector3i(x,p_y_edge,p_z_edge+1),z_edge,vertices_data.z_edge,z_edge_counter);
        }

    }

    //escaping edge case (not symetric with entry case because we check by default the entry y,z edge with the first x edge)
    if(vertices_data.metadata(p_y_edge,p_z_edge).end_trim == vertices_data.width){

        const uint32_t x = vertices_data.width-1;
        const Vector3i start_position = Vector3i(x+1,p_y_edge,p_z_edge);

        const uint32_t x_edge = vertices_data.x_edge_cases(x,p_y_edge,p_z_edge);
        if(!y_max){

            const uint32_t neighbor_edge =  vertices_data.x_edge_cases(x,p_y_edge+1,p_z_edge);
            const uint32_t y_edge = _transversal_combination(x_edge, neighbor_edge, false);

            _find_edge_intersection(start_position,Vector3i(x+1,p_y_edge+1,p_z_edge),y_edge,vertices_data.y_edge,y_edge_counter);
        }

        if(!z_max){

            const uint32_t neighbor_edge =  vertices_data.x_edge_cases(x,p_y_edge,p_z_edge+1);
            const uint32_t z_edge = _transversal_combination(x_edge, neighbor_edge, false);

            _find_edge_intersection(start_position,Vector3i(x+1,p_y_edge,p_z_edge+1),z_edge,vertices_data.z_edge,z_edge_counter);
        }
    }

}

void MeshBufferClass::_accumulate_qef(Vector3 normal,Vector3 position,Basis& a_matrix,Vector3& b_vector){

    const Vector3 &n = normal;
    const Vector3 &p = position;

    const Basis cp = Basis(
        Vector3(n.x * n.x, n.x * n.y, n.x * n.z),
        Vector3(n.y * n.x, n.y * n.y, n.y * n.z),
        Vector3(n.z * n.x, n.z * n.y, n.z * n.z)
    );

    a_matrix += cp;

    b_vector+= Vector3(cp.tdotx(p),cp.tdoty(p),cp.tdotz(p));
}

void MeshBufferClass::fourth_pass(const int32_t p_y_cell,const int32_t p_z_cell,const float_t alpha=0.5){

    if( p_y_cell>=vertices_data.height || p_z_cell>=vertices_data.depth ){
        //in case we run an extra row colum, this pass is iterate on CELL.
        return ;
    }

    uint32_t top_left_edge_counter = vertices_data.metadata.cum(p_y_cell,p_z_cell).x_edge -1 ; // Init the counter at the end of array
    uint32_t top_right_edge_counter = vertices_data.metadata.cum(p_y_cell+1,p_z_cell).x_edge -1 ; // Init the counter at the end of array
    uint32_t bottom_left_edge_counter = vertices_data.metadata.cum(p_y_cell,p_z_cell+1).x_edge -1 ; // Init the counter at the end of array
    uint32_t bottom_right_edge_counter = vertices_data.metadata.cum(p_y_cell+1,p_z_cell+1).x_edge -1 ; // Init the counter at the end of array

    uint32_t front_top_edge_counter = vertices_data.metadata.cum(p_y_cell,p_z_cell).y_edge -1; // Init the counter at the end of array
    uint32_t front_bottom_edge_counter = vertices_data.metadata.cum(p_y_cell,p_z_cell+1).y_edge -1; // Init the counter at the end of array

    uint32_t front_left_edge_counter = vertices_data.metadata.cum(p_y_cell,p_z_cell).z_edge -1; // Init the counter at the end of array
    uint32_t front_right_edge_counter = vertices_data.metadata.cum(p_y_cell+1,p_z_cell).z_edge -1; // Init the counter at the end of array

    uint32_t vertices_counter = vertices_data.metadata.cum(p_y_cell,p_z_cell).point-1;  


    const bool y_max = p_y_cell==vertices_data.height-1;
    const bool z_max = p_z_cell==vertices_data.depth-1;

    for( int32_t x=vertices_data.metadata(p_y_cell,p_z_cell).start_trim ; x<vertices_data.metadata(p_y_cell,p_z_cell).end_trim ; x++){

        Basis A = Basis();
        Vector3 b = Vector3();

        Vector3 mass_point=Vector3();

        int32_t num_vertices = 0;

        const uint32_t top_left_case = vertices_data.x_edge_cases(x,p_y_cell,p_z_cell);
        const uint32_t top_right_case = vertices_data.x_edge_cases(x,p_y_cell+1,p_z_cell);
        const uint32_t bottom_left_case = vertices_data.x_edge_cases(x,p_y_cell,p_z_cell+1);
        const uint32_t bottom_right_case = vertices_data.x_edge_cases(x,p_y_cell+1,p_z_cell+1);

        const bool top_left_changed = _edge_change(top_left_case);//x
        const bool top_right_changed = _edge_change(top_right_case);//x
        const bool bottom_left_changed = _edge_change(bottom_left_case);//x
        const bool bottom_right_changed = _edge_change(bottom_right_case);//x

        const bool front_top_changed = _transversal_change(top_left_case, top_right_case, true); //y
        const bool front_bottom_changed = _transversal_change(bottom_left_case, bottom_right_case, true); //y

        const bool front_left_changed = _transversal_change(top_left_case, bottom_left_case, true); //z
        const bool front_right_changed = _transversal_change(top_right_case, bottom_right_case, true); //z

        const bool rear_top_changed = _transversal_change(top_left_case, top_right_case, false); //y
        const bool rear_bottom_changed = _transversal_change(bottom_left_case, bottom_right_case, false); //y

        const bool rear_left_changed = _transversal_change(top_left_case, bottom_left_case, false); //z
        const bool rear_right_changed = _transversal_change(top_right_case, bottom_right_case, false); //z

        const uint32_t rear_top_edge_counter = front_top_edge_counter - 1;
        const uint32_t rear_bottom_edge_counter = front_bottom_edge_counter - 1;
        const uint32_t rear_left_edge_counter = front_left_edge_counter - 1;
        const uint32_t rear_right_edge_counter = front_right_edge_counter - 1;

        auto accumulate_from_edge = [&](bool changed, const VerticesData::EdgeCompute &edge, uint32_t edge_index) {
            if (!changed) {
                return;
            }

            const Vector3 global_position = ChunkMath::vertices_to_world(chunk_id, edge.local_positions[edge_index]);

            _accumulate_qef(edge.normals[edge_index], global_position, A, b);
            mass_point += global_position;
            ++num_vertices;
        };

        accumulate_from_edge(top_left_changed, vertices_data.x_edge, top_left_edge_counter);
        accumulate_from_edge(top_right_changed, vertices_data.x_edge, top_right_edge_counter);
        accumulate_from_edge(bottom_left_changed, vertices_data.x_edge, bottom_left_edge_counter);
        accumulate_from_edge(bottom_right_changed, vertices_data.x_edge, bottom_right_edge_counter);

        accumulate_from_edge(front_top_changed, vertices_data.y_edge, front_top_edge_counter);
        accumulate_from_edge(front_bottom_changed, vertices_data.y_edge, front_bottom_edge_counter);
        accumulate_from_edge(rear_top_changed, vertices_data.y_edge, rear_top_edge_counter);
        accumulate_from_edge(rear_bottom_changed, vertices_data.y_edge, rear_bottom_edge_counter);

        accumulate_from_edge(front_left_changed, vertices_data.z_edge, front_left_edge_counter);
        accumulate_from_edge(front_right_changed, vertices_data.z_edge, front_right_edge_counter);
        accumulate_from_edge(rear_left_changed, vertices_data.z_edge, rear_left_edge_counter);
        accumulate_from_edge(rear_right_changed, vertices_data.z_edge, rear_right_edge_counter);

        //Regularization

        A[0][0] +=alpha;
        A[1][1] +=alpha;
        A[2][2] +=alpha;

        mass_point/=num_vertices;

        b+= alpha*mass_point;

        Vector3 final_vertex = A.inverse().xform(b);

        vertices_data.points[vertices_counter]= final_vertex;

        vertices_counter--; // there is always a point thanks to triming

        //decrease the counter for the next pass

        top_left_edge_counter -= top_left_changed;
        top_right_edge_counter -= top_right_changed;
        bottom_left_edge_counter -= bottom_left_changed;
        bottom_right_edge_counter -= bottom_right_changed;

        front_top_edge_counter -= front_top_changed;
        front_bottom_edge_counter -= front_bottom_changed;

        front_left_edge_counter -= front_left_changed;
        front_right_edge_counter -= front_right_changed;

    }

}

void MeshBufferClass::execute_on_self(){

    vertices_data.configure(VoxelEngineConstants::CHUNK_SIZE,VoxelEngineConstants::CHUNK_SIZE,VoxelEngineConstants::CHUNK_SIZE);

    // First and third pass iterate on edges (y, z bounds depend on edge count)
    for (int32_t z = 0; z < vertices_data.depth; ++z) {
        for (int32_t y = 0; y < vertices_data.height; ++y) {
            first_pass(y, z);
        }
    }

    // Second and fourth pass iterate on cells (y, z bounds are height-1, depth-1)
    for (int32_t z = 0; z < vertices_data.depth - 1; ++z) {
        for (int32_t y = 0; y < vertices_data.height - 1; ++y) {
            second_pass(y, z);
        }
    }

    // Cache metadata for cumulative counts
    vertices_data.metadata.cache();

    vertices_data.configure_points(vertices_data.metadata.cum(vertices_data.height-1,vertices_data.depth-1).point);
    // TODO: Allocate edge arrays and point vector based on cumulative counts

    // Third pass: fill edge positions and normals
    for (int32_t z = 0; z < vertices_data.depth; ++z) {
        for (int32_t y = 0; y < vertices_data.height; ++y) {
            third_pass(y, z);
        }
    }

    // Fourth pass: generate vertices using QEF solver
    for (int32_t z = 0; z < vertices_data.depth - 1; ++z) {
        for (int32_t y = 0; y < vertices_data.height - 1; ++y) {
            fourth_pass(y, z);
        }
    }
}