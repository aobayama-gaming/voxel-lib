#include "mesh_generator_class.hpp"
#include "sdf_dummy.h"
#include "chunk_math.hpp"

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

            if(edge_case == 1 || edge_case == 2){ // sign change

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

void MeshBufferClass::second_pass(const int32_t p_y_edge,const int32_t p_z_edge){


    if( p_y_edge>=vertices_data.height || p_z_edge>=vertices_data.depth ){
        //in case we run an extra row colum, this pass is iterate on CELL.
        return ;
    }

    for( int32_t x=vertices_data.metadata(p_y_edge,p_z_edge).start_trim ; x<vertices_data.metadata(p_y_edge,p_z_edge).end_trim ; x++){

        const uint32_t top_left_case = vertices_data.x_edge_cases(x,p_y_edge,p_z_edge);
        const uint32_t top_right_case = vertices_data.x_edge_cases(x,p_y_edge+1,p_z_edge);
        const uint32_t bottom_left_case = vertices_data.x_edge_cases(x,p_y_edge,p_z_edge+1);
        const uint32_t bottom_right_case = vertices_data.x_edge_cases(x,p_y_edge+1,p_z_edge+1);

        const bool front_top_changed = ((top_left_case ^ top_right_case) & 0b01); //y
        const bool front_bottom_changed = ((bottom_left_case ^ bottom_right_case) & 0b01); //y

        const bool rear_top_changed = ((top_left_case ^ top_right_case) & 0b10); //y
        const bool rear_bottom_changed = ((bottom_left_case ^ bottom_right_case) & 0b10); //y


        const bool front_left_changed = ((top_left_case ^ bottom_left_case) & 0b01); //z
        const bool front_right_changed = ((top_right_case ^ bottom_right_case) & 0b01); //z

        const bool rear_left_changed = ((top_left_case ^ bottom_left_case) & 0b10); //z
        const bool rear_right_changed = ((top_right_case ^ bottom_right_case) & 0b10); //z
        
        bool changed = front_top_changed || front_left_changed;

        const bool y_max = p_y_edge==vertices_data.height-1;
        const bool z_max = p_z_edge==vertices_data.depth-1;

        auto& meta = vertices_data.metadata(p_y_edge, p_z_edge);

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
            vertices_data.metadata(p_y_edge+1,p_z_edge).counts.z_edge += front_right_changed;
        }
        if(z_max){ // Edge case for max depth
            vertices_data.metadata(p_y_edge,p_z_edge+1).counts.y_edge += front_bottom_changed;
        }
        meta.counts.y_edge += front_top_changed;
        meta.counts.z_edge += front_left_changed;


        if(x == meta.end_trim -1){


            if(x==vertices_data.width-1){ // In case sdf cross on the futhermost 
                meta.counts.y_edge += rear_top_changed;
                meta.counts.z_edge += rear_left_changed;

                if(y_max){ // Edge case for max height
                    vertices_data.metadata(p_y_edge+1,p_z_edge).counts.z_edge += rear_right_changed;
                }
                if(z_max){ // Edge case for max depth
                    vertices_data.metadata(p_y_edge,p_z_edge+1).counts.y_edge += rear_bottom_changed;
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

