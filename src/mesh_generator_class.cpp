#include "mesh_generator_class.hpp"
#include "sdf_dummy.h"
#include "chunk_math.hpp"

inline constexpr int BINARY_SEARCH_STEP = 8;

void MeshBufferClass::initialize(const Vector3i &p_chunk_id, SDFBase *p_sdf) {

    sdf = p_sdf;
	chunk_id = p_chunk_id;

	const int32_t size = VoxelEngineConstants::CHUNK_SIZE + 1; //Vertices
	vertices_data.configure(size-1, size, size); // since it stores the vertices ID it is one less big in the x direction.
}

void MeshBufferClass::first_pass(const int32_t p_y_edge,const int32_t p_z_edge){

    float_t last_value = 0.0f;

    bool first_change = true;

    for(int32_t x=0;x<=vertices_data.width;x++){

        const Vector3i vertices_coordinates =  Vector3i(x,p_y_edge,p_z_edge);

        const float_t actual_value = sdf->evaluate(ChunkMath::vertices_to_world(chunk_id , vertices_coordinates));

        if(x>0){

            const uint32_t edge_case = (last_value < 0.0f ? 1 : 0) | (actual_value < 0.0f ? 2 : 0);

            vertices_data.x_edge_cases(Vector3i(x-1,p_y_edge,p_z_edge)) = edge_case;

            if(_edge_change(edge_case)){ // sign change

                if(first_change){
                    first_change=false;
                    vertices_data.metadata(p_y_edge,p_z_edge).start_trim = x-1;
                }
                vertices_data.metadata(p_y_edge,p_z_edge).counts.x_edge++;

                vertices_data.metadata(p_y_edge,p_z_edge).end_trim = x;
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

    auto &meta = vertices_data.metadata(p_y_cell, p_z_cell);
    // If x-trim is collapsed, we still need to scan x to catch y/z-only crossings.

    // in some weird surface this case can happens
    meta.start_trim = MIN(meta.start_trim,MIN(vertices_data.metadata(p_y_cell+1, p_z_cell).start_trim,vertices_data.metadata(p_y_cell, p_z_cell+1).start_trim));
    meta.end_trim = MAX(meta.end_trim,MAX(vertices_data.metadata(p_y_cell+1, p_z_cell).end_trim,vertices_data.metadata(p_y_cell, p_z_cell+1).end_trim));


    const bool collapsed_trim = ( meta.end_trim == 0);
    meta.end_trim+=collapsed_trim;

    const bool y_max = p_y_cell==vertices_data.height-2;
    const bool z_max = p_z_cell==vertices_data.depth-2;

    uint32_t end_condition = meta.end_trim;
    uint32_t start_condition = meta.start_trim;

    if (y_max) {
        vertices_data.metadata(p_y_cell+1, p_z_cell).start_trim = MIN(vertices_data.metadata(p_y_cell+1, p_z_cell).start_trim, meta.start_trim);
        vertices_data.metadata(p_y_cell+1, p_z_cell).end_trim = MAX(vertices_data.metadata(p_y_cell+1, p_z_cell).end_trim, meta.end_trim);
    }
    if (z_max) {
        vertices_data.metadata(p_y_cell, p_z_cell+1).start_trim = MIN(vertices_data.metadata(p_y_cell, p_z_cell+1).start_trim, meta.start_trim);
        vertices_data.metadata(p_y_cell, p_z_cell+1).end_trim = MAX(vertices_data.metadata(p_y_cell, p_z_cell+1).end_trim, meta.end_trim);
    }

    for( int32_t x=start_condition ; x<end_condition ; x++){

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

        const bool rear_top_changed = _transversal_change(top_left_case, top_right_case, false); //y
        const bool rear_bottom_changed = _transversal_change(bottom_left_case, bottom_right_case, false); //y


        const bool front_left_changed = _transversal_change(top_left_case, bottom_left_case, true); //z
        const bool front_right_changed = _transversal_change(top_right_case, bottom_right_case, true); //z

        const bool rear_left_changed = _transversal_change(top_left_case, bottom_left_case, false); //z
        const bool rear_right_changed = _transversal_change(top_right_case, bottom_right_case, false); //z
        
        if(x>0 && x ==start_condition )
        {
            if(front_top_changed || front_left_changed){
                meta.start_trim = 0;
                start_condition = 0;
                x=-1;
                continue;
            }
            if(y_max && front_right_changed){
                vertices_data.metadata(p_y_cell+1,p_z_cell).start_trim=0;
                start_condition = 0;
                x=-1;
                continue;
            }
            if(z_max && front_bottom_changed){
                vertices_data.metadata(p_y_cell,p_z_cell+1).start_trim=0;
                start_condition = 0;
                x=-1;
                continue;
            }

        }

        if(y_max){
            vertices_data.metadata(p_y_cell+1,p_z_cell).counts.z_edge += front_right_changed;

        }
        if(z_max){ // Edge case for max depth
            vertices_data.metadata(p_y_cell,p_z_cell+1).counts.y_edge += front_bottom_changed;

        }

        meta.counts.y_edge += front_top_changed;
        meta.counts.z_edge += front_left_changed;

        meta.counts.point+= top_left_changed || top_right_changed || bottom_left_changed || bottom_right_changed || front_top_changed || front_bottom_changed || rear_top_changed || rear_bottom_changed || front_left_changed || front_right_changed || rear_left_changed || rear_right_changed;;


        if(x == end_condition -1){

            if(x==vertices_data.width-1){ // In case sdf cross on the futhermost 
                meta.counts.y_edge += rear_top_changed;
                meta.counts.z_edge += rear_left_changed;

                if(y_max){ // Edge case for max height
                    vertices_data.metadata(p_y_cell+1,p_z_cell).counts.z_edge += rear_right_changed;
                    if(rear_right_changed) vertices_data.metadata(p_y_cell+1,p_z_cell).end_trim = vertices_data.width;

                }
                if(z_max){ // Edge case for max depth
                    vertices_data.metadata(p_y_cell,p_z_cell+1).counts.y_edge += rear_bottom_changed;
                    if(rear_bottom_changed) vertices_data.metadata(p_y_cell,p_z_cell+1).end_trim = vertices_data.width;
                }

            }
            else
            {

                if(y_max && rear_right_changed){ // Edge case for max height
                    end_condition = vertices_data.width;
                    vertices_data.metadata(p_y_cell+1,p_z_cell).end_trim=vertices_data.width;
                }

                if(z_max && rear_bottom_changed){ // Edge case for max depth
                    end_condition = vertices_data.width;
                    vertices_data.metadata(p_y_cell,p_z_cell+1).end_trim=vertices_data.width;
                }

                if(rear_top_changed || rear_left_changed){
                    meta.end_trim = vertices_data.width;
                    end_condition = vertices_data.width;
                }

            }
            
        }

    }

    if( collapsed_trim && meta.end_trim ==1 ){
        meta.end_trim=0;
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
    return (paired_bit << 1) | origin_bit;
}

void MeshBufferClass::_find_edge_intersection(const Vector3i &start_point, const Vector3i &end_point, const uint32_t edge_case, VerticesData::EdgeCompute &edge_data, uint32_t &index)
{
    if(edge_case==1 || edge_case==2){
        //dummy implementation

        bool left = edge_case & 0b01u;
        bool right = (edge_case & 0b10u) >> 1;

        Vector3 left_vector = start_point;
        Vector3 right_vector = end_point;

        Vector3 mid_vector = (right_vector+left_vector)/2;

        for(int i=0;i<BINARY_SEARCH_STEP;i++){

            const bool mid = sdf->evaluate(ChunkMath::vertices_to_world(chunk_id,mid_vector))< 0.0f;

            if(left==mid){
                left_vector=mid_vector;
            }
            else{
                right_vector=mid_vector;
            }

            mid_vector = (right_vector+left_vector)/2;


        }

        edge_data.local_positions[index] = mid_vector;
        edge_data.normals[index] = sdf->evaluate_normal(ChunkMath::vertices_to_world(chunk_id, mid_vector));

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
    //for( int32_t x=0 ; x<vertices_data.width ; x++){

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

// void MeshBufferClass::_accumulate_qef(Vector3 normal,Vector3 position,Basis& a_matrix,Vector3& b_vector){

//     const Vector3 &n = normal;
//     const Vector3 &p = position;

//     const Basis cp = Basis(
//         Vector3(n.x * n.x, n.x * n.y, n.x * n.z),
//         Vector3(n.y * n.x, n.y * n.y, n.y * n.z),
//         Vector3(n.z * n.x, n.z * n.y, n.z * n.z)
//     );

//     a_matrix += cp;

//     b_vector+= Vector3(cp.tdotx(p),cp.tdoty(p),cp.tdotz(p));
// }

// void MeshBufferClass::_accumulate_qef(Vector3 normal, Vector3 position, Basis& a_matrix, Vector3& b_vector){
//     const Vector3 &n = normal;

//     // Outer Product A = n * n^T
//     a_matrix[0][0] += n.x * n.x;
//     a_matrix[0][1] += n.x * n.y;
//     a_matrix[0][2] += n.x * n.z;

//     a_matrix[1][0] += n.y * n.x;
//     a_matrix[1][1] += n.y * n.y;
//     a_matrix[1][2] += n.y * n.z;

//     a_matrix[2][0] += n.z * n.x;
//     a_matrix[2][1] += n.z * n.y;
//     a_matrix[2][2] += n.z * n.z;

//     // Simple, foolproof b accumulation: b += (n . p) * n
//     float dot_product = n.dot(position);
//     b_vector += n * dot_product;
// }

void MeshBufferClass::_accumulate_qef(Vector3 normal, Vector3 position, Basis& a_total, Vector3& b_total) {
    const Vector3 &n = normal;
    const Vector3 &p = position;

    // --- According to the paper (Section 3.1, Eq. 4) ---

    // 1. Calculate the A matrix for this single plane: A = n * n^T
    const Basis a_plane = Basis(
        Vector3(n.x * n.x, n.x * n.y, n.x * n.z),
        Vector3(n.y * n.x, n.y * n.y, n.y * n.z),
        Vector3(n.z * n.x, n.z * n.y, n.z * n.z)
    );

    // 2. Calculate the b vector for this single plane: b = A * p
    const Vector3 b_plane = a_plane.xform(p);
    
    // 3. Add the results for this plane to the running totals for the cell
    a_total += a_plane;
    b_total += b_plane;
}

Vector3 MeshBufferClass::_surface_net_vertex(const Vector3 &mass_point_sum, int32_t num_vertices) const {
    if (num_vertices <= 0) {
        return Vector3();
    }
    return mass_point_sum / static_cast<float>(num_vertices);
}

void MeshBufferClass::fourth_pass(const int32_t p_y_cell,const int32_t p_z_cell,const float_t alpha=0.1){

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

    const int32_t x_start = MIN(
        vertices_data.metadata(p_y_cell, p_z_cell).start_trim,
        MIN(vertices_data.metadata(p_y_cell + 1, p_z_cell).start_trim,
            MIN(vertices_data.metadata(p_y_cell, p_z_cell + 1).start_trim,
                vertices_data.metadata(p_y_cell + 1, p_z_cell + 1).start_trim))
    );

    const int32_t x_end = MAX(
        vertices_data.metadata(p_y_cell, p_z_cell).end_trim,
        MAX(vertices_data.metadata(p_y_cell + 1, p_z_cell).end_trim,
            MAX(vertices_data.metadata(p_y_cell, p_z_cell + 1).end_trim,
                vertices_data.metadata(p_y_cell + 1, p_z_cell + 1).end_trim))
    );

    for( int32_t x=x_start ; x<x_end ; x++){

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

        const uint32_t rear_top_edge_counter = front_top_edge_counter - front_top_changed;
        const uint32_t rear_bottom_edge_counter = front_bottom_edge_counter - front_bottom_changed;
        const uint32_t rear_left_edge_counter = front_left_edge_counter - front_left_changed;
        const uint32_t rear_right_edge_counter = front_right_edge_counter - front_right_changed;

        auto accumulate_from_edge = [&](bool changed, const VerticesData::EdgeCompute &edge, uint32_t edge_index) {
            if (!changed) {
                return;
            }

            const Vector3 &local_position = edge.local_positions[edge_index];

            _accumulate_qef(edge.normals[edge_index], local_position, A, b);
            mass_point += local_position;
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

        if(num_vertices > 0){

            // Temporary surface-net mode: use average of border intersections.
            const Vector3 final_vertex = _surface_net_vertex(mass_point, num_vertices);

            // QEF solve kept for later re-enable.
            // A[0][0] +=alpha;
            // A[1][1] +=alpha;
            // A[2][2] +=alpha;

            // mass_point/=num_vertices;

            // b+= alpha*mass_point;

            // Vector3 final_vertex = A.inverse().xform(b);

            // Vector3 min_bound = Vector3(x, p_y_cell, p_z_cell);
            // Vector3 max_bound = min_bound + Vector3(1.0f, 1.0f, 1.0f);
            // final_vertex = final_vertex.clamp(min_bound, max_bound);

            const Vector3 final_vertex_world = ChunkMath::vertices_to_world(chunk_id, final_vertex);
            const float final_vertex_sdf = sdf->evaluate(final_vertex_world);
            if (std::abs(final_vertex_sdf) > 1.0f) {
                __debugbreak();
            }

            vertices_data.points[vertices_counter]= final_vertex;

            vertices_counter--;

        }

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
    vertices_data.cache();
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

    const Vector3 zero_point = Vector3(0.0f, 0.0f, 0.0f);

    uint32_t empty_count = 0;
    for (const Vector3 &point : vertices_data.points) {
        if (point == zero_point) {
            empty_count++;
        }
    }
    if(empty_count>0){
    print_line(vformat("empty points: %d / %d", empty_count, vertices_data.points.size()));
    }
}