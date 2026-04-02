#include "mesh_generator_class.hpp"
#include "sdf_dummy.h"
#include "chunk_math.hpp"

void MeshBufferClass::initialize(const Vector3i &p_chunk_id, SDFBase *p_sdf) {

    sdf = p_sdf;
	chunk_id = p_chunk_id;

	const int32_t size = VoxelEngineConstants::CHUNK_SIZE + 1; //Vertices
	vertices_data.configure(size, size, size);
}

void MeshBufferClass::first_pass(const int32_t p_y_edge,const int32_t p_z_edge){

    bool last_sign = false;

    bool first_change = true;

    for(int32_t x=0;x<vertices_data.width;x++){

        const Vector3i vertices_coordinates =  Vector3i(x,p_y_edge,p_z_edge);

        const bool actual_sign = sdf->evaluate(ChunkMath::vertices_to_world(chunk_id , vertices_coordinates)) > 0.0f;
        
        vertices_data.sign_grid(vertices_coordinates) = actual_sign;

        if(x==0){ //initialisation
            last_sign=actual_sign;
        }

        if(last_sign != actual_sign){

            if(first_change){
                first_change=false;
                vertices_data.metadata(p_y_edge,p_z_edge).start_trim = x;
            }
            vertices_data.metadata(p_y_edge,p_z_edge).counts.x_edge++;
            last_sign=actual_sign;

            vertices_data.metadata(p_y_edge,p_z_edge).end_trim = x+1;
        }
    }

}

void MeshBufferClass::second_pass(const int32_t p_y_edge,const int32_t p_z_edge){

    if( p_y_edge>=vertices_data.height || p_z_edge>=vertices_data.depth ){
        //in case we run an extra row colum
        return ;
    }

    for( int32_t x=vertices_data.metadata(p_y_edge,p_z_edge).start_trim ; x<vertices_data.metadata(p_y_edge,p_z_edge).end_trim ; x++){

        const bool changed_y = vertices_data.sign_grid(x,p_y_edge,p_z_edge) != vertices_data.sign_grid(x,p_y_edge+1,p_z_edge);
        const bool changed_z = vertices_data.sign_grid(x,p_y_edge,p_z_edge) != vertices_data.sign_grid(x,p_y_edge,p_z_edge+1);

        if(x>0 && x ==vertices_data.metadata(p_y_edge,p_z_edge).start_trim && (changed_y || changed_z) )
        {
            vertices_data.metadata(p_y_edge,p_z_edge).start_trim = 0;
            x=0;
        }

        if(x == vertices_data.metadata(p_y_edge,p_z_edge).end_trim -1  && (changed_y || changed_z)){
            vertices_data.metadata(p_y_edge,p_z_edge).end_trim = vertices_data.width;
        }

        if(changed_y)
        {
            
        }
    }
}

