extends RefCounted
class_name PeravizMeshFaceFlipUtils

static func apply_to_node_tree(root: Node3D, cache, cache_suffix: String = "#force_invert_all") -> int:
	if root == null:
		return 0
	var updated: int = 0
	for mesh_instance in _collect_mesh_instances_recursive(root):
		if mesh_instance.mesh == null:
			continue
		var asset_path: String = str(mesh_instance.get_meta("peraviz_asset_path", mesh_instance.name))
		var cache_key: String = asset_path + cache_suffix
		var flipped_mesh: Mesh = cache.get_mesh(cache_key)
		if flipped_mesh == null:
			flipped_mesh = _build_flipped_mesh(mesh_instance.mesh)
			if flipped_mesh != null:
				cache.store_mesh(cache_key, flipped_mesh)
		if flipped_mesh != null:
			mesh_instance.mesh = flipped_mesh
			updated += 1
	return updated

static func _build_flipped_mesh(source_mesh: Mesh) -> ArrayMesh:
	if source_mesh == null:
		return null
	var out := ArrayMesh.new()
	for surface_index in range(source_mesh.get_surface_count()):
		var arrays: Array = source_mesh.surface_get_arrays(surface_index)
		if arrays.is_empty():
			continue
		var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
		for tri_start in range(0, indices.size() - 2, 3):
			var i1: int = indices[tri_start + 1]
			indices[tri_start + 1] = indices[tri_start + 2]
			indices[tri_start + 2] = i1
		arrays[Mesh.ARRAY_INDEX] = indices

		var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
		for n in range(normals.size()):
			normals[n] = -normals[n]
		arrays[Mesh.ARRAY_NORMAL] = normals

		var tangents: PackedFloat32Array = arrays[Mesh.ARRAY_TANGENT]
		for t in range(0, tangents.size() - 3, 4):
			tangents[t] = -tangents[t]
			tangents[t + 1] = -tangents[t + 1]
			tangents[t + 2] = -tangents[t + 2]
		arrays[Mesh.ARRAY_TANGENT] = tangents

		out.add_surface_from_arrays(source_mesh.surface_get_primitive_type(surface_index), arrays)
		var material: Material = source_mesh.surface_get_material(surface_index)
		if material != null:
			out.surface_set_material(surface_index, material)
	return out

static func _collect_mesh_instances_recursive(root: Node3D) -> Array[MeshInstance3D]:
	var output: Array[MeshInstance3D] = []
	if root == null:
		return output
	if root is MeshInstance3D:
		output.append(root)
	for child in root.get_children():
		if child is Node3D:
			output.append_array(_collect_mesh_instances_recursive(child))
	return output
