extends VoxelEngineClass

func _ready() -> void:
	
	print(debug_get_parent_chunk(Vector3i(3,-1,8)))
