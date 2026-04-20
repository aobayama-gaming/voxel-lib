#include "mesh_generator_class.hpp"
#include "sdf_dummy.h"
#include "chunk_math.hpp"

#include <cmath>

inline constexpr int BINARY_SEARCH_STEP =7;

namespace {

float skirt_offset(float x,float y, float z, float value,Vector3i chunk_id){

    auto hash_scale_from_chunk_id = [](const Vector3i &seed_chunk_id) {
        uint32_t seed = 2166136261u;
        seed = (seed ^ static_cast<uint32_t>(seed_chunk_id.x)) * 16777619u;
        seed = (seed ^ static_cast<uint32_t>(seed_chunk_id.y)) * 16777619u;
        seed = (seed ^ static_cast<uint32_t>(seed_chunk_id.z)) * 16777619u;

        seed ^= seed >> 16;
        seed *= 0x7feb352du;
        seed ^= seed >> 15;
        seed *= 0x846ca68bu;
        seed ^= seed >> 16;

        const float unit = static_cast<float>(seed & 0x00ffffffu) / 16777215.0f;
        return ((unit * 2.0f) - 1.0f);
    };

    auto calculate_offset = [](float coord,float size,float hash_offset) {
        const float dist_from_min = coord;
        const float dist_from_max = static_cast<float>(VoxelEngineConstants::CHUNK_SIZE) - coord;
        const float nearest_edge_dist = MIN(dist_from_min, dist_from_max);

        const float skirt_size = static_cast<float>(VoxelEngineConstants::SKIRT_SIZE)*.5f;
        const float extended_slope_limit = static_cast<float>(VoxelEngineConstants::SKIRT_SIZE) * 1.f;
        const float micro_slope_amplitude = 0.01f;

        float offset = 0.0f;

        if (nearest_edge_dist < skirt_size) {
            offset = 1.0f - (nearest_edge_dist / skirt_size) * hash_offset;
            //offset*=offset;
        }

        if (nearest_edge_dist < extended_slope_limit) {
            const float t = 1.0f - (nearest_edge_dist / extended_slope_limit);
            offset += micro_slope_amplitude * t * hash_offset;
        }

        return offset*size;
    };

    const float hash_offset = hash_scale_from_chunk_id(chunk_id);

    float size= (1<<ChunkMath::get_lod(chunk_id))*VoxelEngineConstants::VOXEL_SIZE;

    float offset_x = calculate_offset(x,size,0.0f);
    float offset_y = calculate_offset(y,size,0.0f);
    float offset_z = calculate_offset(z,size,0.0f);

    //float max_offset = fmax(offset_x,fmax( offset_y,offset_z));
    float max_offset = offset_x+ offset_y + offset_z;
    //max_offset *= hash_scale_from_chunk_id(chunk_id);

    return value - max_offset + hash_offset*size*0.01f ;
}

float skirt_offset(Vector3 pos,float value,Vector3i chunk_id){
    return skirt_offset(pos.x,pos.y,pos.z,value,chunk_id);
}

// float skirt_offset(Vector3i pos,float value){
//     return skirt_offset(pos,value);
// }

bool solve_linear_system_3x3(const Basis &a, const Vector3 &b, Vector3 &x) {
    // Augmented matrix [A|b] solved with partial pivoting.
    double m[3][4] = {
        { static_cast<double>(a[0][0]), static_cast<double>(a[0][1]), static_cast<double>(a[0][2]), static_cast<double>(b.x) },
        { static_cast<double>(a[1][0]), static_cast<double>(a[1][1]), static_cast<double>(a[1][2]), static_cast<double>(b.y) },
        { static_cast<double>(a[2][0]), static_cast<double>(a[2][1]), static_cast<double>(a[2][2]), static_cast<double>(b.z) }
    };

    constexpr double eps = 1e-10;

    for (int pivot_col = 0; pivot_col < 3; ++pivot_col) {
        int best_row = pivot_col;
        double best_abs = std::abs(m[pivot_col][pivot_col]);

        for (int row = pivot_col + 1; row < 3; ++row) {
            const double current_abs = std::abs(m[row][pivot_col]);
            if (current_abs > best_abs) {
                best_abs = current_abs;
                best_row = row;
            }
        }

        if (best_abs < eps) {
            return false;
        }

        if (best_row != pivot_col) {
            for (int col = pivot_col; col < 4; ++col) {
                const double tmp = m[pivot_col][col];
                m[pivot_col][col] = m[best_row][col];
                m[best_row][col] = tmp;
            }
        }

        for (int row = pivot_col + 1; row < 3; ++row) {
            const double factor = m[row][pivot_col] / m[pivot_col][pivot_col];
            if (std::abs(factor) < eps) {
                continue;
            }
            for (int col = pivot_col; col < 4; ++col) {
                m[row][col] -= factor * m[pivot_col][col];
            }
        }
    }

    double result[3] = { 0.0, 0.0, 0.0 };
    for (int row = 2; row >= 0; --row) {
        double rhs = m[row][3];
        for (int col = row + 1; col < 3; ++col) {
            rhs -= m[row][col] * result[col];
        }

        const double diag = m[row][row];
        if (std::abs(diag) < eps) {
            return false;
        }
        result[row] = rhs / diag;
    }

    x = Vector3(static_cast<float>(result[0]), static_cast<float>(result[1]), static_cast<float>(result[2]));
    return true;
}

}

bool MeshBufferClass::_vertices_inside(int x,int y,int z){
    return x==0 || x==vertices_data.depth-1 || y==0 ||y==vertices_data.height-1
}

void MeshBufferClass::initialize(const Vector3i &p_chunk_id, SDFBase *p_sdf) {

    sdf = p_sdf;
	chunk_id = p_chunk_id;

	const int32_t size = VoxelEngineConstants::CHUNK_SIZE + 1; //Vertices
	vertices_data.configure(size-1, size, size); // since it stores the vertices ID it is one less big in the x direction.
}

void MeshBufferClass::first_pass(const int32_t p_y_edge,const int32_t p_z_edge){

    float_t last_value = 0.0f;

    bool first_change = true;

    const float size = (1<<ChunkMath::get_lod(chunk_id))*VoxelEngineConstants::VOXEL_SIZE;

    for(int32_t x=0;x<=vertices_data.width;x++){

        const Vector3i vertices_coordinates =  Vector3i(x,p_y_edge,p_z_edge);

        const Vector3 evaluation_vector = ChunkMath::vertices_to_world(chunk_id , vertices_coordinates);

        float_t actual_value = skirt_offset(vertices_coordinates, sdf->evaluate(evaluation_vector),chunk_id);

        if(x>0){

            const uint32_t edge_case = (last_value < 0.0f ? 1 : 0) | (actual_value < 0.0f ? 2 : 0);

            vertices_data.x_edge_cases(Vector3i(x-1,p_y_edge,p_z_edge)) = edge_case;

            if(MeshEdgeUtils::edge_change(edge_case)){ // sign change

                if(first_change){
                    first_change=false;
                    vertices_data.metadata(p_y_edge,p_z_edge).x_start_trim = x-1;
                }
                vertices_data.metadata(p_y_edge,p_z_edge).counts.x_edge++;

                vertices_data.metadata(p_y_edge,p_z_edge).x_end_trim = x;
            }

        }

        last_value=actual_value;

    }

}

void MeshBufferClass::second_pass(const int32_t p_y_cell,const int32_t p_z_cell){
    // There is some parallel mess since we modify the start trim and end_trim following neighbour one, this only locally degrade the performance but we can imagine a x_start and a x_end that keep only the x.

    if( p_y_cell>=vertices_data.height-1 || p_z_cell>=vertices_data.depth-1 ){
        //in case we run an extra row colum, this pass is iterate on CELL.
        return ;
    }

    auto &meta = vertices_data.metadata(p_y_cell, p_z_cell);
    // If x-trim is collapsed, we still need to scan x to catch y/z-only crossings.

    const bool y_max = p_y_cell==vertices_data.height-2;
    const bool z_max = p_z_cell==vertices_data.depth-2;

    // in some weird surface this case can happens
    meta.start_trim = MIN(meta.x_start_trim,MIN(vertices_data.metadata(p_y_cell+1, p_z_cell).x_start_trim,vertices_data.metadata(p_y_cell, p_z_cell+1).x_start_trim));
    meta.end_trim = MAX(meta.x_end_trim,MAX(vertices_data.metadata(p_y_cell+1, p_z_cell).x_end_trim,vertices_data.metadata(p_y_cell, p_z_cell+1).x_end_trim));
    
    //meta.start_trim=0;
    //meta.end_trim = vertices_data.width;

    const bool collapsed_trim = ( meta.end_trim == 0);
    meta.end_trim+=collapsed_trim;

    uint32_t end_condition = meta.end_trim;
    uint32_t start_condition = meta.start_trim;

    bool collapsed_trim_z = false;

    if (z_max) {
        vertices_data.metadata(p_y_cell, p_z_cell+1).start_trim = MIN(vertices_data.metadata(p_y_cell+1, p_z_cell+1).x_start_trim,vertices_data.metadata(p_y_cell, p_z_cell+1).x_start_trim);
        vertices_data.metadata(p_y_cell, p_z_cell+1).end_trim = MAX(vertices_data.metadata(p_y_cell+1, p_z_cell+1).x_end_trim,vertices_data.metadata(p_y_cell, p_z_cell+1).x_end_trim);
        
        collapsed_trim_z = vertices_data.metadata(p_y_cell, p_z_cell+1).end_trim==0;
        vertices_data.metadata(p_y_cell, p_z_cell+1).end_trim+=collapsed_trim_z;

        //vertices_data.metadata(p_y_cell, p_z_cell+1).start_trim = 0 ;
        //vertices_data.metadata(p_y_cell, p_z_cell+1).end_trim = vertices_data.width;

        start_condition = MIN(start_condition,vertices_data.metadata(p_y_cell, p_z_cell+1).start_trim);
        end_condition = MAX(end_condition,vertices_data.metadata(p_y_cell, p_z_cell+1).end_trim);

    }

    bool collapsed_trim_y = false;

    if (y_max) {
        vertices_data.metadata(p_y_cell+1, p_z_cell).start_trim = MIN(vertices_data.metadata(p_y_cell+1, p_z_cell+1).x_start_trim,vertices_data.metadata(p_y_cell+1, p_z_cell).x_start_trim);
        vertices_data.metadata(p_y_cell+1, p_z_cell).end_trim = MAX(vertices_data.metadata(p_y_cell+1, p_z_cell+1).x_end_trim,vertices_data.metadata(p_y_cell+1, p_z_cell).x_end_trim);


        collapsed_trim_y = vertices_data.metadata(p_y_cell+1, p_z_cell).end_trim==0;
        vertices_data.metadata(p_y_cell+1, p_z_cell).end_trim+=collapsed_trim_y;
        
        //vertices_data.metadata(p_y_cell+1, p_z_cell).start_trim = 0 ;
        //vertices_data.metadata(p_y_cell+1, p_z_cell).end_trim = vertices_data.width;

        start_condition = MIN(start_condition,vertices_data.metadata(p_y_cell+1, p_z_cell).start_trim);
        end_condition = MAX(end_condition,vertices_data.metadata(p_y_cell+1, p_z_cell).end_trim);

    }


    // //edge case for x_start transfer for the border line
    if(y_max && z_max){
        vertices_data.metadata(p_y_cell+1, p_z_cell+1).start_trim =vertices_data.metadata(p_y_cell+1, p_z_cell+1).x_start_trim;
        vertices_data.metadata(p_y_cell+1, p_z_cell+1).end_trim =vertices_data.metadata(p_y_cell+1, p_z_cell+1).x_end_trim;

        //vertices_data.metadata(p_y_cell+1, p_z_cell+1).start_trim = 0 ;
        //vertices_data.metadata(p_y_cell+1, p_z_cell+1).end_trim = vertices_data.width;
    }

    for( int32_t x=start_condition ; x<end_condition ; x++){

        const uint32_t top_left_case = vertices_data.x_edge_cases(x,p_y_cell,p_z_cell);
        const uint32_t top_right_case = vertices_data.x_edge_cases(x,p_y_cell+1,p_z_cell);
        const uint32_t bottom_left_case = vertices_data.x_edge_cases(x,p_y_cell,p_z_cell+1);
        const uint32_t bottom_right_case = vertices_data.x_edge_cases(x,p_y_cell+1,p_z_cell+1);

        const bool top_left_changed = MeshEdgeUtils::edge_change(top_left_case);//x
        const bool top_right_changed = MeshEdgeUtils::edge_change(top_right_case);//x
        const bool bottom_left_changed = MeshEdgeUtils::edge_change(bottom_left_case);//x
        const bool bottom_right_changed = MeshEdgeUtils::edge_change(bottom_right_case);//x

        const bool front_top_changed = MeshEdgeUtils::transversal_change(top_left_case, top_right_case, true); //y
        const bool front_bottom_changed = MeshEdgeUtils::transversal_change(bottom_left_case, bottom_right_case, true); //y

        const bool rear_top_changed = MeshEdgeUtils::transversal_change(top_left_case, top_right_case, false); //y
        const bool rear_bottom_changed = MeshEdgeUtils::transversal_change(bottom_left_case, bottom_right_case, false); //y


        const bool front_left_changed = MeshEdgeUtils::transversal_change(top_left_case, bottom_left_case, true); //z
        const bool front_right_changed = MeshEdgeUtils::transversal_change(top_right_case, bottom_right_case, true); //z

        const bool rear_left_changed = MeshEdgeUtils::transversal_change(top_left_case, bottom_left_case, false); //z
        const bool rear_right_changed = MeshEdgeUtils::transversal_change(top_right_case, bottom_right_case, false); //z
        

        bool rerun = false;

        if( x == meta.start_trim && (front_top_changed || front_left_changed)){
            meta.start_trim = 0;

            rerun= start_condition != 0;
            
        }
        if( y_max && x == vertices_data.metadata(p_y_cell+1,p_z_cell).start_trim && front_right_changed){
            vertices_data.metadata(p_y_cell+1,p_z_cell).start_trim = 0;

            rerun= start_condition != 0;
            
            
        }
        if( z_max && x == vertices_data.metadata(p_y_cell,p_z_cell+1).start_trim && front_bottom_changed){
            vertices_data.metadata(p_y_cell,p_z_cell+1).start_trim = 0;

            rerun= start_condition != 0;
            
        }

        if(rerun){
            start_condition = 0;
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



        if(x==vertices_data.width-1){ // In case sdf cross on the futhermost 
            meta.counts.y_edge += rear_top_changed;
            meta.counts.z_edge += rear_left_changed;

            if(y_max){ // Edge case for max height
                vertices_data.metadata(p_y_cell+1,p_z_cell).counts.z_edge += rear_right_changed;
                //if(rear_right_changed) vertices_data.metadata(p_y_cell+1,p_z_cell).end_trim = vertices_data.width;

            }
            if(z_max){ // Edge case for max depth
                vertices_data.metadata(p_y_cell,p_z_cell+1).counts.y_edge += rear_bottom_changed;
                //if(rear_bottom_changed) vertices_data.metadata(p_y_cell,p_z_cell+1).end_trim = vertices_data.width;
            }

        }


        if(x == vertices_data.metadata(p_y_cell+1,p_z_cell).end_trim -1 && y_max && rear_right_changed){ // Edge case for max height
            end_condition = vertices_data.width;
            vertices_data.metadata(p_y_cell+1,p_z_cell).end_trim=vertices_data.width;
        }

        if(x == vertices_data.metadata(p_y_cell,p_z_cell+1).end_trim -1 && z_max && rear_bottom_changed){ // Edge case for max depth
            end_condition = vertices_data.width;
            vertices_data.metadata(p_y_cell,p_z_cell+1).end_trim=vertices_data.width;
        }

        if(x == meta.end_trim -1 && (rear_top_changed || rear_left_changed)){
            meta.end_trim = vertices_data.width;
            end_condition = vertices_data.width;
        }


    }

    if( collapsed_trim && meta.end_trim ==1 ){
        meta.end_trim=0;
    }

    if( collapsed_trim_z && vertices_data.metadata(p_y_cell, p_z_cell+1).end_trim ==1 ){
        vertices_data.metadata(p_y_cell, p_z_cell+1).end_trim=0;
    }

    if( collapsed_trim_y && vertices_data.metadata(p_y_cell+1, p_z_cell).end_trim ==1 ){
        vertices_data.metadata(p_y_cell+1, p_z_cell).end_trim=0;
    }
}

void MeshBufferClass::second_half_pass(const int32_t p_y_cell,const int32_t p_z_cell){

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

        vertices_data.metadata(p_y_cell, p_z_cell).counts.point+= _cell_has_point(x, p_y_cell, p_z_cell) ;

    }
}

bool MeshEdgeUtils::edge_change(uint32_t edge_case)
{
    return edge_case == 1 || edge_case == 2;
}

bool MeshEdgeUtils::transversal_change(uint32_t origin_edge, uint32_t paired_edge, bool front)
{
    const uint32_t mask = front ? 0b01u : 0b10u;
    return ((origin_edge ^ paired_edge) & mask) != 0;
}

uint32_t MeshEdgeUtils::transversal_combination(uint32_t origin_edge, uint32_t paired_edge, bool front)
{
    const uint32_t origin_bit = front ? (origin_edge & 0b01u) : ((origin_edge & 0b10u) >> 1);
    const uint32_t paired_bit = front ? (paired_edge & 0b01u) : ((paired_edge & 0b10u) >> 1);
    return (paired_bit << 1) | origin_bit;
}

void MeshBufferClass::_find_edge_intersection(const Vector3i &start_point, const Vector3i &end_point, const uint32_t edge_case, VerticesData::EdgeCompute &edge_data, uint32_t &index)
{
    if(edge_case==1 || edge_case==2){
        

        const float chunk_size=(1<< ChunkMath::get_lod( chunk_id))*VoxelEngineConstants::VOXEL_SIZE;

        bool left = edge_case & 0b01u;
        bool right = (edge_case & 0b10u) >> 1;

        Vector3 left_vector = start_point;
        Vector3 right_vector = end_point;

        Vector3 mid_vector = (right_vector+left_vector)/2;

        for(int i=0;i<BINARY_SEARCH_STEP;i++){

            const Vector3 evaluation_vector = ChunkMath::vertices_to_world(chunk_id,mid_vector);

            const bool mid = skirt_offset(mid_vector, sdf->evaluate(evaluation_vector),chunk_id)< 0.0f;

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

    uint32_t x_edge_counter = vertices_data.metadata.cum(p_y_edge,p_z_edge).x_edge -1; // Init the counter at the end of array
    uint32_t y_edge_counter = vertices_data.metadata.cum(p_y_edge,p_z_edge).y_edge -1; // Init the counter at the end of array
    uint32_t z_edge_counter = vertices_data.metadata.cum(p_y_edge,p_z_edge).z_edge -1; // Init the counter at the end of array

    const bool y_max = p_y_edge==vertices_data.height-1;
    const bool z_max = p_z_edge==vertices_data.depth-1;


    int32_t x_start = vertices_data.metadata(p_y_edge, p_z_edge).start_trim;
    int32_t x_end = vertices_data.metadata(p_y_edge, p_z_edge).end_trim;

    x_start = 0;
    x_end = vertices_data.width;

    if(!y_max){
        x_start = MIN(x_start,vertices_data.metadata(p_y_edge + 1, p_z_edge).start_trim);
    }

    if(!z_max){
        x_end = MAX(x_end,vertices_data.metadata(p_y_edge, p_z_edge + 1).end_trim);
    }


    for( int32_t x=x_start ; x<x_end ; x++){

        const uint32_t x_edge = vertices_data.x_edge_cases(x,p_y_edge,p_z_edge);
        const Vector3i start_position = Vector3i(x,p_y_edge,p_z_edge);

        _find_edge_intersection(start_position,Vector3i(x+1,p_y_edge,p_z_edge),x_edge,vertices_data.x_edge,x_edge_counter);

        if(!y_max){

            const uint32_t neighbor_edge =  vertices_data.x_edge_cases(x,p_y_edge+1,p_z_edge);
            const uint32_t y_edge = MeshEdgeUtils::transversal_combination(x_edge, neighbor_edge, true);

            _find_edge_intersection(start_position,Vector3i(x,p_y_edge+1,p_z_edge),y_edge,vertices_data.y_edge,y_edge_counter);
        }

        if(!z_max){

            const uint32_t neighbor_edge =  vertices_data.x_edge_cases(x,p_y_edge,p_z_edge+1);
            const uint32_t z_edge = MeshEdgeUtils::transversal_combination(x_edge, neighbor_edge, true);

            _find_edge_intersection(start_position,Vector3i(x,p_y_edge,p_z_edge+1),z_edge,vertices_data.z_edge,z_edge_counter);
        }

    }

    //escaping edge case (not symetric with entry case because we check by default the entry y,z edge with the first x edge)
    if(x_end == vertices_data.width){

        const uint32_t x = vertices_data.width-1;
        const Vector3i start_position = Vector3i(x+1,p_y_edge,p_z_edge);

        const uint32_t x_edge = vertices_data.x_edge_cases(x,p_y_edge,p_z_edge);
        if(!y_max){

            const uint32_t neighbor_edge =  vertices_data.x_edge_cases(x,p_y_edge+1,p_z_edge);
            const uint32_t y_edge = MeshEdgeUtils::transversal_combination(x_edge, neighbor_edge, false);

            _find_edge_intersection(start_position,Vector3i(x+1,p_y_edge+1,p_z_edge),y_edge,vertices_data.y_edge,y_edge_counter);
        }

        if(!z_max){

            const uint32_t neighbor_edge =  vertices_data.x_edge_cases(x,p_y_edge,p_z_edge+1);
            const uint32_t z_edge = MeshEdgeUtils::transversal_combination(x_edge, neighbor_edge, false);

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

void MeshBufferClass::fourth_pass(const int32_t p_y_cell,const int32_t p_z_cell,const float_t alpha=0.01f){

    if( p_y_cell>=vertices_data.height-1 || p_z_cell>=vertices_data.depth-1 ){
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

    int32_t x_start = MIN(
        vertices_data.metadata(p_y_cell, p_z_cell).start_trim,
        MIN(vertices_data.metadata(p_y_cell + 1, p_z_cell).start_trim,
            MIN(vertices_data.metadata(p_y_cell, p_z_cell + 1).start_trim,
                vertices_data.metadata(p_y_cell + 1, p_z_cell + 1).start_trim))
    );

    int32_t x_end = MAX(
        vertices_data.metadata(p_y_cell, p_z_cell).end_trim,
        MAX(vertices_data.metadata(p_y_cell + 1, p_z_cell).end_trim,
            MAX(vertices_data.metadata(p_y_cell, p_z_cell + 1).end_trim,
                vertices_data.metadata(p_y_cell + 1, p_z_cell + 1).end_trim))
    );

    x_start = 0;
    x_end = vertices_data.width;


    // Normalize QEF coordinates by cell size so conditioning stays consistent across LOD.
    const int32_t lod = ChunkMath::get_lod(chunk_id);
    const float cell_size = VoxelEngineConstants::VOXEL_SIZE * static_cast<float>(1 << lod);
    const float inv_cell_size = (1.0f / cell_size);

    for( int32_t x=x_start ; x<x_end ; x++){
        Basis A = Basis(0,0,0,0,0,0,0,0,0);
        Vector3 b = Vector3(0,0,0);

        Vector3 mass_point=Vector3(0,0,0);

        int32_t num_vertices = 0;

        const uint32_t top_left_case = vertices_data.x_edge_cases(x,p_y_cell,p_z_cell);
        const uint32_t top_right_case = vertices_data.x_edge_cases(x,p_y_cell+1,p_z_cell);
        const uint32_t bottom_left_case = vertices_data.x_edge_cases(x,p_y_cell,p_z_cell+1);
        const uint32_t bottom_right_case = vertices_data.x_edge_cases(x,p_y_cell+1,p_z_cell+1);

        const bool top_left_changed = MeshEdgeUtils::edge_change(top_left_case);//x
        const bool top_right_changed = MeshEdgeUtils::edge_change(top_right_case);//x
        const bool bottom_left_changed = MeshEdgeUtils::edge_change(bottom_left_case);//x
        const bool bottom_right_changed = MeshEdgeUtils::edge_change(bottom_right_case);//x

        const bool front_top_changed = MeshEdgeUtils::transversal_change(top_left_case, top_right_case, true); //y
        const bool front_bottom_changed = MeshEdgeUtils::transversal_change(bottom_left_case, bottom_right_case, true); //y

        const bool front_left_changed = MeshEdgeUtils::transversal_change(top_left_case, bottom_left_case, true); //z
        const bool front_right_changed = MeshEdgeUtils::transversal_change(top_right_case, bottom_right_case, true); //z

        const bool rear_top_changed = MeshEdgeUtils::transversal_change(top_left_case, top_right_case, false); //y
        const bool rear_bottom_changed = MeshEdgeUtils::transversal_change(bottom_left_case, bottom_right_case, false); //y

        const bool rear_left_changed = MeshEdgeUtils::transversal_change(top_left_case, bottom_left_case, false); //z
        const bool rear_right_changed = MeshEdgeUtils::transversal_change(top_right_case, bottom_right_case, false); //z

        const uint32_t rear_top_edge_counter = front_top_edge_counter - front_top_changed;
        const uint32_t rear_bottom_edge_counter = front_bottom_edge_counter - front_bottom_changed;
        const uint32_t rear_left_edge_counter = front_left_edge_counter - front_left_changed;
        const uint32_t rear_right_edge_counter = front_right_edge_counter - front_right_changed;

        const Vector3 mid_cell = ChunkMath::vertices_to_world(chunk_id, Vector3(x+0.5f,p_y_cell+0.5f,p_z_cell+0.5f));

        auto accumulate_from_edge = [&](bool changed, const VerticesData::EdgeCompute &edge, uint32_t edge_index) {
            if (!changed) {
                return;
            }

            const Vector3 &local_position = edge.local_positions[edge_index];
            const Vector3 position = (ChunkMath::vertices_to_world(chunk_id, local_position) - mid_cell) * inv_cell_size;

            // if(position.length() > 0.9f){
            //     print_line("out of bound");
            // }

            _accumulate_qef(edge.normals[edge_index], position, A, b);
            mass_point += position;
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

            // // Tikhonov regularization in normalized space (dimensionless).
            const float a = alpha;
            

            // // QEF solve kept for later re-enable.
            A[0][0] +=a;
            A[1][1] +=a;
            A[2][2] +=a;

            mass_point/=num_vertices;

            b+= a*mass_point;

            // //const float det = A.determinant();
            // // if (std::abs(det) < 1.0f) {
            // //     print_line(vformat("bad determinant in fourth_pass at cell (%d, %d), x=%d: det=%f, num_vertices=%d",
            // //         p_y_cell,
            // //         p_z_cell,
            // //         x,
            // //         det,
            // //         num_vertices));
            // // }

            Vector3 qef_solution;
            const bool solved = solve_linear_system_3x3(A, b, qef_solution);
            const Vector3 final_vertex_world = ((solved ? qef_solution : mass_point) * cell_size) + mid_cell;
            Vector3 final_vertex = ChunkMath::world_to_vertices(chunk_id,final_vertex_world);

            if(!solved){
                print_line("error generating chunk");
            }
            

            // const float margin = 0.1f;

            // Vector3 min_bound = Vector3(x+margin, p_y_cell+margin, p_z_cell+margin);
            // Vector3 max_bound = min_bound + Vector3(1.0f-margin, 1.0f-margin, 1.0f-margin);
            // final_vertex = final_vertex.clamp(min_bound, max_bound);

            //const Vector3 final_vertex = ChunkMath::world_to_vertices(chunk_id,mass_point*cell_size+mid_cell);

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

bool MeshBufferClass::_cell_has_point(int32_t x, int32_t y, int32_t z) {
        const uint32_t top_left_case = vertices_data.x_edge_cases(x, y, z);
        const uint32_t top_right_case = vertices_data.x_edge_cases(x, y + 1, z);
        const uint32_t bottom_left_case = vertices_data.x_edge_cases(x, y, z + 1);
        const uint32_t bottom_right_case = vertices_data.x_edge_cases(x, y + 1, z + 1);

        const bool top_left_changed = MeshEdgeUtils::edge_change(top_left_case);
        const bool top_right_changed = MeshEdgeUtils::edge_change(top_right_case);
        const bool bottom_left_changed = MeshEdgeUtils::edge_change(bottom_left_case);
        const bool bottom_right_changed = MeshEdgeUtils::edge_change(bottom_right_case);

        const bool front_top_changed = MeshEdgeUtils::transversal_change(top_left_case, top_right_case, true);
        const bool front_bottom_changed = MeshEdgeUtils::transversal_change(bottom_left_case, bottom_right_case, true);

        const bool front_left_changed = MeshEdgeUtils::transversal_change(top_left_case, bottom_left_case, true);
        const bool front_right_changed = MeshEdgeUtils::transversal_change(top_right_case, bottom_right_case, true);

        const bool rear_top_changed = MeshEdgeUtils::transversal_change(top_left_case, top_right_case, false);
        const bool rear_bottom_changed = MeshEdgeUtils::transversal_change(bottom_left_case, bottom_right_case, false);

        const bool rear_left_changed = MeshEdgeUtils::transversal_change(top_left_case, bottom_left_case, false);
        const bool rear_right_changed = MeshEdgeUtils::transversal_change(top_right_case, bottom_right_case, false);

        return top_left_changed || top_right_changed || bottom_left_changed || bottom_right_changed ||
            front_top_changed || front_bottom_changed || front_left_changed || front_right_changed ||
            rear_top_changed || rear_bottom_changed || rear_left_changed || rear_right_changed;
};

void MeshBufferClass::fifth_pass(const int32_t p_y_qcell, const int32_t p_z_qcell) {

    // Need to check all code, been partialy generated by Gemini 3 pro
    
    const int32_t y = p_y_qcell;
    const int32_t z = p_z_qcell;

    // Local copy of point counters for the 4 rows in YZ
    uint32_t pt_00 = vertices_data.metadata.cum(y, z).point - 1;
    uint32_t pt_10 = vertices_data.metadata.cum(y + 1, z).point - 1;
    uint32_t pt_01 = vertices_data.metadata.cum(y, z + 1).point - 1;
    uint32_t pt_11 = vertices_data.metadata.cum(y + 1, z + 1).point - 1;

    // Edge counters starting exactly where the previous pass left them
    uint32_t x_edge_counter = vertices_data.metadata.cum(y + 1, z + 1).x_edge - 1;
    uint32_t y_edge_counter = vertices_data.metadata.cum(y + 1, z + 1).y_edge - 1;
    uint32_t z_edge_counter = vertices_data.metadata.cum(y + 1, z + 1).z_edge - 1;
    uint32_t y_far_edge_counter = vertices_data.metadata.cum(y, z + 1).y_edge - 1;
    uint32_t z_far_edge_counter = vertices_data.metadata.cum(y + 1, z).z_edge - 1;

    // Memory buffer indexing for the previous iteration
    uint32_t prev_x_idx_00 = 0, prev_x_idx_10 = 0, prev_x_idx_01 = 0, prev_x_idx_11 = 0;

    // Pre-calculate cumulative offsets for vertex array partitioning 
    const auto& max_meta = vertices_data.metadata.cum(vertices_data.height - 1, vertices_data.depth - 1);
    const uint32_t y_offset = max_meta.x_edge;
    const uint32_t z_offset = max_meta.x_edge + max_meta.y_edge;

    auto& vertices = vertices_data.output_vertices;

    auto write_quad = [&](uint32_t write_idx, bool positive, uint32_t idA, uint32_t idB, uint32_t idC, uint32_t idD) {
        if (positive) {
            vertices[write_idx + 0] = idA; vertices[write_idx + 1] = idB; vertices[write_idx + 2] = idC;
            vertices[write_idx + 3] = idA; vertices[write_idx + 4] = idC; vertices[write_idx + 5] = idD;
        } else {
            vertices[write_idx + 0] = idA; vertices[write_idx + 1] = idD; vertices[write_idx + 2] = idC;
            vertices[write_idx + 3] = idA; vertices[write_idx + 4] = idC; vertices[write_idx + 5] = idB;
        }
    };

    // Helper lambda to determine if a specific cell contains a point
    auto _cell_has_vertex = [&](int32_t cx, int32_t cy, int32_t cz) -> bool {
        const uint32_t tl = vertices_data.x_edge_cases(cx, cy, cz);
        const uint32_t tr = vertices_data.x_edge_cases(cx, cy + 1, cz);
        const uint32_t bl = vertices_data.x_edge_cases(cx, cy, cz + 1);
        const uint32_t br = vertices_data.x_edge_cases(cx, cy + 1, cz + 1);

        if (MeshEdgeUtils::edge_change(tl) || MeshEdgeUtils::edge_change(tr) || MeshEdgeUtils::edge_change(bl) || MeshEdgeUtils::edge_change(br)) return true;
        if (MeshEdgeUtils::transversal_change(tl, tr, true) || MeshEdgeUtils::transversal_change(bl, br, true)) return true;
        if (MeshEdgeUtils::transversal_change(tl, bl, true) || MeshEdgeUtils::transversal_change(tr, br, true)) return true;
        if (MeshEdgeUtils::transversal_change(tl, tr, false) || MeshEdgeUtils::transversal_change(bl, br, false)) return true;
        if (MeshEdgeUtils::transversal_change(tl, bl, false) || MeshEdgeUtils::transversal_change(tr, br, false)) return true;
        
        return false;
    };

    for (int32_t x = 0; x < vertices_data.width; x++) {

        bool v00 = _cell_has_vertex(x, y, z);
        bool v10 = _cell_has_vertex(x, y + 1, z);
        bool v01 = _cell_has_vertex(x, y, z + 1);
        bool v11 = _cell_has_vertex(x, y + 1, z + 1);

        // Capture the vertex ID (index) for the current cells
        uint32_t cur_idx_00 = pt_00;
        uint32_t cur_idx_10 = pt_10;
        uint32_t cur_idx_01 = pt_01;
        uint32_t cur_idx_11 = pt_11;

        // Decrement counters exactly like the QEF pass to maintain synchronization
        if (v00) pt_00--;
        if (v10) pt_10--;
        if (v01) pt_01--;
        if (v11) pt_11--;

        uint32_t case_x = vertices_data.x_edge_cases(x, y + 1, z + 1);
        bool x_changed = MeshEdgeUtils::edge_change(case_x);

        if (x_changed) {
            bool positive = (case_x == 1);
            
            // X-edge Quad indices (using the IDs of the 4 surrounding cells on the YZ plane)
            uint32_t idA = cur_idx_00;
            uint32_t idB = cur_idx_10;
            uint32_t idC = cur_idx_11;
            uint32_t idD = cur_idx_01;

            uint32_t write_idx = x_edge_counter * 6;

            write_quad(write_idx, positive, idA, idB, idC, idD);
            x_edge_counter--;
        }


        // Generate Y & Z Edge Quads mapping cells behind it
        if (x > 0) {


            uint32_t case_y = vertices_data.x_edge_cases(x, y + 2, z + 1);
            bool y_changed = MeshEdgeUtils::transversal_change(case_x, case_y, true);

            if (y_changed) {
                // Y-edge Quad indices (spans across the X axis, so we mix prev_x and cur_x)
                uint32_t idA = prev_x_idx_10;
                uint32_t idB = cur_idx_10;
                uint32_t idC = cur_idx_11;
                uint32_t idD = prev_x_idx_11;

                bool positive = (MeshEdgeUtils::transversal_combination(case_x, case_y, true) == 1);
                uint32_t write_idx = (y_offset + y_edge_counter) * 6;

                // Keep the previous Y-edge winding convention.
                write_quad(write_idx, !positive, idA, idB, idC, idD);
                y_edge_counter--;
            }

            if(y==0){
                uint32_t case_y_far = vertices_data.x_edge_cases(x, y , z + 1);
                bool y_far_changed = MeshEdgeUtils::transversal_change(case_y_far,case_x, true);
                if(y_far_changed){

                uint32_t idA = prev_x_idx_00;
                uint32_t idB = cur_idx_00;
                uint32_t idC = cur_idx_01;
                uint32_t idD = prev_x_idx_01;
                bool positive = (MeshEdgeUtils::transversal_combination(case_y_far,case_x, true) == 1);
                uint32_t write_idx = (y_offset + y_far_edge_counter) * 6;

                // Keep the previous Y-edge winding convention.
                write_quad(write_idx, !positive, idA, idB, idC, idD);
                y_far_edge_counter--;
                }
            }

            uint32_t case_z = vertices_data.x_edge_cases(x, y + 1, z + 2);
            bool z_changed = MeshEdgeUtils::transversal_change(case_x, case_z, true);

            if (z_changed) {
                // Z-edge Quad indices
                uint32_t idA = prev_x_idx_01;
                uint32_t idB = cur_idx_01;
                uint32_t idC = cur_idx_11;
                uint32_t idD = prev_x_idx_11;

                bool positive = (MeshEdgeUtils::transversal_combination(case_x, case_z, true) == 1);
                uint32_t write_idx = (z_offset + z_edge_counter) * 6;

                write_quad(write_idx, positive, idA, idB, idC, idD);
                z_edge_counter--;
            }

            if(z==0){
                uint32_t case_z_far = vertices_data.x_edge_cases(x, y +1 , z);
                bool z_far_changed = MeshEdgeUtils::transversal_change(case_z_far,case_x, true);
                if(z_far_changed){

                uint32_t idA = prev_x_idx_00;
                uint32_t idB = cur_idx_00;
                uint32_t idC = cur_idx_10;
                uint32_t idD = prev_x_idx_10;
                bool positive = (MeshEdgeUtils::transversal_combination(case_z_far,case_x, true) == 1);
                uint32_t write_idx = (z_offset + z_far_edge_counter) * 6;

                // Keep the previous Y-edge winding convention.
                write_quad(write_idx, positive, idA, idB, idC, idD);
                z_far_edge_counter--;
                }
            }
        }

        // Push current points indices to previous for next iteration step
        prev_x_idx_00 = cur_idx_00;
        prev_x_idx_10 = cur_idx_10;
        prev_x_idx_01 = cur_idx_01;
        prev_x_idx_11 = cur_idx_11;
    }
}

void MeshBufferClass::cache_edge() {
    vertices_data.edge_cache.clear();

    if (vertices_data.points.empty()) {
        return;
    }

    const int32_t x_max = vertices_data.width - 1;
    const int32_t y_max = vertices_data.height - 2;
    const int32_t z_max = vertices_data.depth - 2;


    for (int32_t z = 0; z < vertices_data.depth - 1; ++z) {
        for (int32_t y = 0; y < vertices_data.height - 1; ++y) {

            const int row_points = vertices_data.metadata(y,z).counts.point;

            if(row_points==0){continue;}

            const bool yz_boundary = (y == 0 || y == y_max || z == 0 || z == z_max);

            const int32_t x_start = MIN(
                vertices_data.metadata(y, z).start_trim,
                MIN(vertices_data.metadata(y + 1, z).start_trim,
                    MIN(vertices_data.metadata(y, z + 1).start_trim,
                        vertices_data.metadata(y + 1, z + 1).start_trim))
            );

            const int32_t x_end = MAX(
                vertices_data.metadata(y, z).end_trim,
                MAX(vertices_data.metadata(y + 1, z).end_trim,
                    MAX(vertices_data.metadata(y, z + 1).end_trim,
                        vertices_data.metadata(y + 1, z + 1).end_trim))
            );

            // For interior YZ rows, only X-face boundary cells may be cached.
            if (!yz_boundary && x_start > 0 && x_end <= x_max) {
                continue;
            }

            uint32_t point_index = vertices_data.metadata.cum(y, z).point - 1;
            // Fast path: interior YZ rows only need x=0 and x=x_max checks.
            if (!yz_boundary) {


                const bool has_x0_in_trim = (x_start <= 0 && 0 < x_end);
                if (has_x0_in_trim && _cell_has_point(0, y, z)) {
                    vertices_data.edge_cache.insert(Vector3i(0, y, z), point_index);
                }

                const bool has_xmax_in_trim = (x_start <= x_max && x_max < x_end);
                if (has_xmax_in_trim && _cell_has_point(x_max, y, z)) {
                    vertices_data.edge_cache.insert(Vector3i(x_max, y, z), point_index-(row_points-1));
                }

                continue;
            }

            
            for (int32_t x = x_start; x < x_end; ++x) {
                if (!_cell_has_point(x, y, z)) {
                    continue;
                }

                const bool is_boundary_cell = yz_boundary || x == 0 || x == x_max;
                if (!is_boundary_cell) {
                    point_index--;
                    continue;
                }

                vertices_data.edge_cache.insert(Vector3i(x, y, z), point_index);
                point_index--;
            }
        }
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

    for (int32_t z = 0; z < vertices_data.depth - 1; ++z) {
        for (int32_t y = 0; y < vertices_data.height - 1; ++y) {
            second_half_pass(y, z);
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

    // Fifth pass: generate vertices using QEF solver
    for (int32_t z = 0; z < vertices_data.depth - 2; ++z) {
        for (int32_t y = 0; y < vertices_data.height - 2; ++y) {
            fifth_pass(y, z);
        }
    }

    cache_edge(); // for inter-chunk patching


    // const Vector3 zero_point = Vector3(0.0f, 0.0f, 0.0f);

    // uint32_t empty_count = 0;
    // for (const Vector3 &point : vertices_data.points) {
    //     if (point == zero_point) {
    //         empty_count++;
    //     }
    // }
    // if(empty_count>0){
    // print_line(vformat("empty points: %d / %d", empty_count, vertices_data.points.size()));
    // }

    // auto count_empty_edge_positions = [&](const VerticesData::EdgeCompute &edge) {
    //     uint32_t count = 0;
    //     for (const Vector3 &pos : edge.local_positions) {
    //         if (pos == zero_point) {
    //             count++;
    //         }
    //     }
    //     return count;
    // };

    // const uint32_t empty_x = count_empty_edge_positions(vertices_data.x_edge);
    // const uint32_t empty_y = count_empty_edge_positions(vertices_data.y_edge);
    // const uint32_t empty_z = count_empty_edge_positions(vertices_data.z_edge);

    // if (empty_x > 0) {
    //     print_line(vformat("empty x_edge: %d / %d", empty_x, vertices_data.x_edge.local_positions.size()));
    // }
    // if (empty_y > 0) {
    //     print_line(vformat("empty y_edge: %d / %d", empty_y, vertices_data.y_edge.local_positions.size()));
    // }
    // if (empty_z > 0) {
    //     print_line(vformat("empty z_edge: %d / %d", empty_z, vertices_data.z_edge.local_positions.size()));
    // }

}



