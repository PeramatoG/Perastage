extends RefCounted
class_name PeravizMeshRuntimeMirrorFix

static func hierarchy_determinant_sign(node: Node3D) -> float:
	if node == null:
		return 1.0
	var sign: float = 1.0
	var current: Node = node
	while current is Node3D:
		var node3d: Node3D = current
		var det: float = node3d.transform.basis.determinant()
		if det < 0.0:
			sign *= -1.0
		current = node3d.get_parent()
	return sign

static func build_flipped_winding_mesh(source_mesh: Mesh) -> ArrayMesh:
	if source_mesh == null:
		return null
	var out := ArrayMesh.new()
	for surface_index in range(source_mesh.get_surface_count()):
		var arrays: Array = source_mesh.surface_get_arrays(surface_index)
		if arrays.is_empty():
			continue
		var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
		if not indices.is_empty():
			for tri_start in range(0, indices.size() - 2, 3):
				var i1: int = indices[tri_start + 1]
				indices[tri_start + 1] = indices[tri_start + 2]
				indices[tri_start + 2] = i1
			arrays[Mesh.ARRAY_INDEX] = indices

		var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
		if not normals.is_empty():
			for n in range(normals.size()):
				normals[n] = -normals[n]
			arrays[Mesh.ARRAY_NORMAL] = normals

		var tangents: PackedFloat32Array = arrays[Mesh.ARRAY_TANGENT]
		if not tangents.is_empty():
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
