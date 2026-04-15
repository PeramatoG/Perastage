extends RefCounted
class_name NodeFactory

func build_node_tree(nodes: Array, proxies_root: Node3D, node_index: Dictionary, rebuild_loaded_bounds: Callable, loader: PeravizLoader, asset_cache: PeravizRuntimeAssetCache) -> void:
	node_index.clear()
	for item in nodes:
		if item is Dictionary:
			var node_id: String = str(item.get("node_id", ""))
			if node_id.is_empty():
				continue
			node_index[node_id] = create_scene_node(item, loader, asset_cache)

	for item in nodes:
		if item is Dictionary:
			var node_id: String = str(item.get("node_id", ""))
			var parent_id: String = str(item.get("parent_id", ""))
			var node: Node3D = node_index.get(node_id)
			if node == null:
				continue
			var parent_node: Node3D = proxies_root
			if not parent_id.is_empty() and node_index.has(parent_id):
				parent_node = node_index[parent_id]
			parent_node.add_child(node)

	_apply_mesh_orientation_corrections(proxies_root, false, loader, asset_cache)
	_remove_redundant_dummy_meshes(proxies_root)
	rebuild_loaded_bounds.call()

func create_scene_node(data: Dictionary, loader: PeravizLoader, asset_cache: PeravizRuntimeAssetCache) -> Node3D:
	var item_type: String = str(data.get("type", "scene_object"))
	var item_class: String = _extract_node_class(data, item_type)
	var is_fixture: bool = bool(data.get("is_fixture", false))
	var is_axis: bool = bool(data.get("is_axis", false))
	var is_emitter: bool = bool(data.get("is_emitter", false))
	var is_lens: bool = bool(data.get("is_lens", false))
	var node_name: String = str(data.get("name", item_type))
	var flip_orientation: bool = false

	var root := Node3D.new()
	root.name = "%s_%s" % [item_class, node_name]
	root.set_meta("peraviz_type", item_type)
	root.set_meta("peraviz_is_axis", is_axis)
	root.set_meta("peraviz_is_emitter", is_emitter)
	root.set_meta("peraviz_is_lens", is_lens)
	var node_position: Vector3 = _safe_position(data.get("pos", Vector3.ZERO), "create_scene_node:" + root.name)
	if bool(data.get("has_basis", false)):
		var node_basis: Basis = _safe_basis_from_data(data)
		if not _is_basis_valid(node_basis):
			print("[PeravizCoordDebug] event=basis_invalid_fallback node=", root.name)
			node_basis = Basis.IDENTITY
		flip_orientation = node_basis.determinant() < 0.0
		root.transform = Transform3D(node_basis, node_position)
	else:
		root.position = node_position
		root.rotation_degrees = data.get("rot", Vector3.ZERO)
		root.scale = _safe_scale_from_data(data, "create_scene_node_euler:" + root.name)
		flip_orientation = root.transform.basis.determinant() < 0.0

	if is_axis:
		var pivot := Node3D.new()
		pivot.name = "AxisPivot"
		root.add_child(pivot)

	if is_emitter:
		var emitter := Node3D.new()
		emitter.name = "EmitterMarker"
		root.add_child(emitter)

	var visual_scale_hint: float = _extract_visual_scale_hint(data)
	var model_node: Node3D = _build_visual_node(data, item_type, item_class, is_fixture, visual_scale_hint, loader, asset_cache, flip_orientation)
	if model_node != null:
		root.add_child(model_node)
		if item_type == "fixture_geometry":
			_reparent_fixture_visual_children(root, model_node)

	return root

func _is_basis_valid(candidate_basis: Basis) -> bool:
	var determinant: float = candidate_basis.determinant()
	if is_zero_approx(determinant):
		return false
	return is_finite(determinant)

func _safe_basis_from_data(data: Dictionary) -> Basis:
	var basis_x: Vector3 = data.get("basis_x", Vector3.RIGHT)
	var basis_y: Vector3 = data.get("basis_y", Vector3.UP)
	var basis_z: Vector3 = data.get("basis_z", Vector3.BACK)
	var candidate_basis := Basis(basis_x, basis_y, basis_z)
	if _is_basis_valid(candidate_basis):
		return candidate_basis

	print("[PeravizCoordDebug] event=invalid_basis_fallback basis_x=", basis_x, " basis_y=", basis_y, " basis_z=", basis_z)
	return Basis.IDENTITY

func _safe_position(value: Vector3, context: String) -> Vector3:
	if is_finite(value.x) and is_finite(value.y) and is_finite(value.z):
		return value
	print("[PeravizCoordDebug] event=invalid_position_fallback context=", context, " pos=", value)
	return Vector3.ZERO

func _safe_scale_from_data(data: Dictionary, context: String) -> Vector3:
	var raw_scale: Vector3 = data.get("scale", Vector3.ONE)
	if not is_finite(raw_scale.x) or not is_finite(raw_scale.y) or not is_finite(raw_scale.z):
		print("[PeravizCoordDebug] event=invalid_scale_fallback context=", context, " scale=", raw_scale)
		return Vector3.ONE

	var min_axis: float = 0.0001
	var sx: float = raw_scale.x
	var sy: float = raw_scale.y
	var sz: float = raw_scale.z
	if is_zero_approx(sx):
		sx = min_axis
	if is_zero_approx(sy):
		sy = min_axis
	if is_zero_approx(sz):
		sz = min_axis

	if not is_equal_approx(sx, raw_scale.x) or not is_equal_approx(sy, raw_scale.y) or not is_equal_approx(sz, raw_scale.z):
		print("[PeravizCoordDebug] event=zero_scale_sanitized context=", context, " raw_scale=", raw_scale, " sanitized_scale=", Vector3(sx, sy, sz))
	return Vector3(sx, sy, sz)

func _apply_mesh_orientation_corrections(node: Node3D, inherited_flip: bool, loader: PeravizLoader, asset_cache: PeravizRuntimeAssetCache) -> void:
	if node == null:
		return

	var node_flip: bool = node.transform.basis.determinant() < 0.0
	var global_flip: bool = inherited_flip != node_flip
	if node is MeshInstance3D:
		_rebuild_mesh_instance_for_orientation(node as MeshInstance3D, global_flip, loader, asset_cache)

	for child in node.get_children():
		if child is Node3D:
			_apply_mesh_orientation_corrections(child as Node3D, global_flip, loader, asset_cache)

func _rebuild_mesh_instance_for_orientation(mesh_instance: MeshInstance3D, flip_orientation: bool, loader: PeravizLoader, asset_cache: PeravizRuntimeAssetCache) -> void:
	if mesh_instance == null:
		return
	if not mesh_instance.has_meta("peraviz_asset_path"):
		return
	var asset_path: String = str(mesh_instance.get_meta("peraviz_asset_path", ""))
	if asset_path.is_empty():
		return

	var mesh_cache_key: String = asset_path
	if flip_orientation:
		mesh_cache_key = "%s#flipped" % asset_path

	var cached_mesh: Mesh = asset_cache.get_mesh(mesh_cache_key)
	if cached_mesh == null:
		var mesh_data: Dictionary = loader.load_3ds_mesh_data(asset_path)
		if not bool(mesh_data.get("ok", false)):
			return
		var rebuilt_mesh: ArrayMesh = _build_3ds_mesh(mesh_data, flip_orientation)
		if rebuilt_mesh == null:
			return
		asset_cache.store_mesh(mesh_cache_key, rebuilt_mesh)
		cached_mesh = rebuilt_mesh

	mesh_instance.mesh = cached_mesh
	mesh_instance.set_meta("peraviz_flip_orientation_applied", flip_orientation)

func _remove_redundant_dummy_meshes(root: Node) -> void:
	if root == null:
		return
	for child in root.get_children():
		_remove_redundant_dummy_meshes(child)
	if root is not Node3D:
		return
	var root_node: Node3D = root
	var placeholders_to_remove: Array[Node] = []
	for child in root_node.get_children():
		if child is MeshInstance3D and bool(child.get_meta("peraviz_dummy_placeholder", false)):
			placeholders_to_remove.push_back(child)
	if placeholders_to_remove.is_empty():
		return
	if not _node_has_real_visual_descendants(root_node, placeholders_to_remove):
		return
	for placeholder in placeholders_to_remove:
		placeholder.queue_free()

func _node_has_real_visual_descendants(root: Node, ignored_nodes: Array[Node]) -> bool:
	if root is MeshInstance3D:
		if not bool(root.get_meta("peraviz_dummy_placeholder", false)):
			return true
	elif root is MultiMeshInstance3D:
		return true
	for child in root.get_children():
		if ignored_nodes.has(child):
			continue
		if _node_has_real_visual_descendants(child, ignored_nodes):
			return true
	return false

func _reparent_fixture_visual_children(geometry_node: Node3D, model_root: Node3D) -> void:
	if geometry_node == null or model_root == null:
		return
	if model_root is MeshInstance3D:
		return
	var model_root_local: Transform3D = model_root.transform
	var moved_any_child: bool = false
	for child in model_root.get_children():
		if child is not Node3D:
			continue
		var child_node: Node3D = child
		var child_local_before: Transform3D = child_node.transform
		var child_local_after: Transform3D = model_root_local * child_local_before
		model_root.remove_child(child_node)
		child_node.owner = null
		geometry_node.add_child(child_node)
		child_node.transform = child_local_after
		moved_any_child = true
	if moved_any_child:
		model_root.queue_free()

func _build_visual_node(data: Dictionary, item_type: String, item_class: String, is_fixture: bool, visual_scale_hint: float, loader: PeravizLoader, asset_cache: PeravizRuntimeAssetCache, flip_orientation: bool = false) -> Node3D:
	var asset_path: String = str(data.get("asset_path", ""))
	var asset_kind: String = _extract_asset_kind(data, asset_path)
	if not asset_path.is_empty():
		var loaded: Variant = _load_3d_asset(asset_path, loader, asset_cache, asset_kind, flip_orientation)
		if loaded is Node3D:
			var loaded_node: Node3D = loaded
			_force_double_sided_materials(loaded_node)
			return loaded_node
		print("[Peraviz] Asset fallback for missing/invalid model: ", asset_path, " type=", item_type, " class=", item_class, " asset_kind=", asset_kind)

	if item_type == "fixture" or item_type == "fixture_geometry":
		if asset_kind == "primitive":
			var primitive_mesh: Node3D = _create_gdtf_primitive_mesh(data)
			_force_double_sided_materials(primitive_mesh)
			return primitive_mesh
		return null

	var dummy_mesh: Node3D = _create_dummy_mesh(is_fixture, visual_scale_hint, asset_cache)
	_force_double_sided_materials(dummy_mesh)
	return dummy_mesh

func _force_double_sided_materials(root: Node) -> void:
	if root == null:
		return
	if root is MeshInstance3D:
		_apply_double_sided_to_mesh_instance(root as MeshInstance3D)
	elif root is MultiMeshInstance3D:
		_apply_double_sided_to_multimesh_instance(root as MultiMeshInstance3D)
	for child in root.get_children():
		_force_double_sided_materials(child)

func _apply_double_sided_to_mesh_instance(mesh_instance: MeshInstance3D) -> void:
	if mesh_instance == null:
		return
	if mesh_instance.material_override != null:
		mesh_instance.material_override = _duplicate_as_double_sided(mesh_instance.material_override)
	_apply_double_sided_to_mesh_surfaces(mesh_instance, mesh_instance.mesh)

func _apply_double_sided_to_multimesh_instance(multimesh_instance: MultiMeshInstance3D) -> void:
	if multimesh_instance == null:
		return
	if multimesh_instance.material_override != null:
		multimesh_instance.material_override = _duplicate_as_double_sided(multimesh_instance.material_override)
	var mesh: Mesh = multimesh_instance.multimesh.mesh if multimesh_instance.multimesh != null else null
	_apply_double_sided_to_mesh_surfaces(multimesh_instance, mesh)

func _apply_double_sided_to_mesh_surfaces(geometry_instance: GeometryInstance3D, mesh: Mesh) -> void:
	if geometry_instance == null or mesh == null:
		return
	var surface_count: int = mesh.get_surface_count()
	for surface_index in range(surface_count):
		var override_material: Material = geometry_instance.get_surface_override_material(surface_index)
		if override_material != null:
			geometry_instance.set_surface_override_material(surface_index, _duplicate_as_double_sided(override_material))
			continue
		var surface_material: Material = mesh.surface_get_material(surface_index)
		if surface_material != null:
			geometry_instance.set_surface_override_material(surface_index, _duplicate_as_double_sided(surface_material))
		else:
			geometry_instance.set_surface_override_material(surface_index, _create_default_double_sided_material())

func _duplicate_as_double_sided(material: Material) -> Material:
	if material is not BaseMaterial3D:
		return material
	var base_material: BaseMaterial3D = material as BaseMaterial3D
	if base_material.cull_mode == BaseMaterial3D.CULL_DISABLED:
		return material
	var duplicated_material: BaseMaterial3D = base_material.duplicate(true)
	duplicated_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	return duplicated_material

func _create_default_double_sided_material() -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	return material

func _extract_visual_scale_hint(data: Dictionary) -> float:
	if bool(data.get("has_basis", false)):
		var basis_x: Vector3 = data.get("basis_x", Vector3.RIGHT)
		var basis_y: Vector3 = data.get("basis_y", Vector3.UP)
		var basis_z: Vector3 = data.get("basis_z", Vector3.BACK)
		var average_basis_length: float = (basis_x.length() + basis_y.length() + basis_z.length()) / 3.0
		return max(average_basis_length, 0.0001)
	var node_scale: Vector3 = data.get("scale", Vector3.ONE)
	var average_scale: float = (abs(node_scale.x) + abs(node_scale.y) + abs(node_scale.z)) / 3.0
	if not is_finite(average_scale):
		return 1.0
	return max(average_scale, 0.0001)

func _load_3d_asset(asset_path: String, loader: PeravizLoader, asset_cache: PeravizRuntimeAssetCache, asset_kind_hint: String = "", flip_orientation: bool = false) -> Variant:
	var resolved_asset_kind: String = asset_kind_hint.to_lower()
	if resolved_asset_kind.is_empty() or resolved_asset_kind == "none":
		resolved_asset_kind = _infer_asset_kind_from_extension(asset_path)
	var extension: String = asset_path.get_extension().to_lower()
	if resolved_asset_kind == "mesh" or extension == "3ds":
		var mesh_cache_key: String = asset_path
		if flip_orientation:
			mesh_cache_key = "%s#flipped" % asset_path
		var cached_mesh: Mesh = asset_cache.get_mesh(mesh_cache_key)
		if cached_mesh != null:
			var cached_mesh_instance := MeshInstance3D.new()
			cached_mesh_instance.mesh = cached_mesh
			cached_mesh_instance.set_meta("peraviz_asset_path", asset_path)
			cached_mesh_instance.set_meta("peraviz_asset_kind", "mesh")
			cached_mesh_instance.set_meta("peraviz_flip_orientation_applied", flip_orientation)
			return cached_mesh_instance
		var mesh_data: Dictionary = loader.load_3ds_mesh_data(asset_path)
		if not bool(mesh_data.get("ok", false)):
			asset_cache.mark_failed(asset_path)
			return null
		var mesh: ArrayMesh = _build_3ds_mesh(mesh_data, flip_orientation)
		if mesh == null:
			asset_cache.mark_failed(asset_path)
			return null
		asset_cache.store_mesh(mesh_cache_key, mesh)
		var mesh_instance := MeshInstance3D.new()
		mesh_instance.mesh = mesh
		mesh_instance.set_meta("peraviz_asset_path", asset_path)
		mesh_instance.set_meta("peraviz_asset_kind", "mesh")
		mesh_instance.set_meta("peraviz_flip_orientation_applied", flip_orientation)
		return mesh_instance
	if resolved_asset_kind == "scene" or extension == "glb" or extension == "gltf":
		var cached_scene_instance: Node3D = asset_cache.instantiate_scene(asset_path)
		if cached_scene_instance != null:
			return cached_scene_instance
		var gltf := GLTFDocument.new()
		var state := GLTFState.new()
		var err: int = gltf.append_from_file(asset_path, state)
		if err != OK:
			asset_cache.mark_failed(asset_path)
			return null
		var generated: Node = gltf.generate_scene(state)
		if generated is Node3D:
			var packed_scene := PackedScene.new()
			if packed_scene.pack(generated) == OK:
				asset_cache.store_scene(asset_path, packed_scene)
				generated.free()
				return asset_cache.instantiate_scene(asset_path)
		if generated != null:
			generated.free()
		asset_cache.mark_failed(asset_path)
		return null
	var scene_instance: Node3D = asset_cache.instantiate_scene(asset_path)
	if scene_instance != null:
		return scene_instance
	var resource: Resource = load(asset_path)
	if resource is PackedScene:
		asset_cache.store_scene(asset_path, resource)
		return asset_cache.instantiate_scene(asset_path)
	asset_cache.mark_failed(asset_path)
	return null

func _extract_node_class(data: Dictionary, item_type: String) -> String:
	var node_class: String = str(data.get("class", ""))
	if node_class.is_empty():
		node_class = item_type
	return node_class

func _extract_asset_kind(data: Dictionary, asset_path: String) -> String:
	var kind: String = str(data.get("asset_kind", "")).to_lower()
	if kind.is_empty() or kind == "none":
		kind = _infer_asset_kind_from_extension(asset_path)
	return kind

func _infer_asset_kind_from_extension(asset_path: String) -> String:
	var extension: String = asset_path.get_extension().to_lower()
	if extension == "3ds":
		return "mesh"
	if extension == "glb" or extension == "gltf" or extension == "obj" or extension == "dae" or extension == "fbx" or extension == "stl":
		return "scene"
	return "none"

func _build_3ds_mesh(mesh_data: Dictionary, flip_orientation: bool = false) -> ArrayMesh:
	var vertices: PackedVector3Array = mesh_data.get("vertices", PackedVector3Array())
	var normals: PackedVector3Array = mesh_data.get("normals", PackedVector3Array())
	var texcoords: PackedVector2Array = mesh_data.get("texcoords", PackedVector2Array())
	var indices: PackedInt32Array = mesh_data.get("indices", PackedInt32Array())
	var texture_path: String = str(mesh_data.get("texture_path", ""))
	var has_material_base_color: bool = bool(mesh_data.get("has_material_base_color", false))
	var material_base_color_vec: Vector3 = mesh_data.get("material_base_color", Vector3.ONE)
	var material_base_color: Color = Color(material_base_color_vec.x, material_base_color_vec.y, material_base_color_vec.z, 1.0)
	if vertices.is_empty() or indices.is_empty():
		return null
	if flip_orientation:
		if (indices.size() % 3) != 0:
			push_warning("[Peraviz] 3DS mesh index count is not divisible by 3; skipping orientation flip.")
		else:
			for i in range(0, indices.size(), 3):
				var tmp: int = indices[i + 1]
				indices[i + 1] = indices[i + 2]
				indices[i + 2] = tmp
	var arrays: Array = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	if texcoords.size() == vertices.size():
		arrays[Mesh.ARRAY_TEX_UV] = texcoords
	arrays[Mesh.ARRAY_INDEX] = indices
	var array_mesh := ArrayMesh.new()
	array_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	if not texture_path.is_empty() and texcoords.size() == vertices.size():
		var image := Image.new()
		var image_error: Error = image.load(texture_path)
		if image_error == OK:
			if image.get_format() != Image.FORMAT_RGBA8:
				image.convert(Image.FORMAT_RGBA8)
			var texture: ImageTexture = ImageTexture.create_from_image(image)
			var material := StandardMaterial3D.new()
			material.albedo_color = Color(1.0, 1.0, 1.0, 1.0)
			material.albedo_texture = texture
			material.cull_mode = BaseMaterial3D.CULL_DISABLED
			array_mesh.surface_set_material(0, material)
		else:
			push_warning("[Peraviz] Failed to load 3DS texture image: %s (error=%d)" % [texture_path, image_error])
	elif has_material_base_color:
		var base_color_material := StandardMaterial3D.new()
		base_color_material.albedo_color = material_base_color
		base_color_material.cull_mode = BaseMaterial3D.CULL_DISABLED
		array_mesh.surface_set_material(0, base_color_material)
	else:
		var textured_fallback_material := StandardMaterial3D.new()
		textured_fallback_material.albedo_color = Color(0.2, 0.2, 0.2, 1.0)
		textured_fallback_material.cull_mode = BaseMaterial3D.CULL_DISABLED
		array_mesh.surface_set_material(0, textured_fallback_material)
	return array_mesh

func _create_dummy_mesh(is_fixture: bool, visual_scale_hint: float, asset_cache: PeravizRuntimeAssetCache) -> Node3D:
	var mesh_instance := MeshInstance3D.new()
	mesh_instance.set_meta("peraviz_dummy_placeholder", true)
	var world_target_size: float = 0.35
	var normalized_scale: float = max(visual_scale_hint, 0.0001)
	var local_size_multiplier: float = world_target_size / normalized_scale
	if is_fixture:
		var cone := CylinderMesh.new()
		cone.top_radius = 0.0
		cone.bottom_radius = 0.15 * local_size_multiplier
		cone.height = 0.4 * local_size_multiplier
		mesh_instance.mesh = cone
	else:
		var box := BoxMesh.new()
		box.size = Vector3.ONE * (0.3 * local_size_multiplier)
		mesh_instance.mesh = box
	var material_key: String = "builtin://material/dummy_fixture" if is_fixture else "builtin://material/dummy_object"
	var material: Material = asset_cache.get_material(material_key, true)
	if material == null:
		var base_material := StandardMaterial3D.new()
		base_material.albedo_color = Color(1.0, 0.5, 0.1) if is_fixture else Color(0.2, 0.8, 1.0)
		asset_cache.store_material(material_key, base_material)
		material = asset_cache.get_material(material_key, true)
	mesh_instance.material_override = material
	return mesh_instance

func _create_gdtf_primitive_mesh(data: Dictionary) -> Node3D:
	var primitive_type: String = str(data.get("primitive_type", "")).to_lower()
	var primitive_shape: String = _classify_gdtf_primitive_shape(primitive_type)
	var source_size_x: float = max(float(data.get("primitive_size_x", 0.1)), 0.001)
	var source_size_y: float = max(float(data.get("primitive_size_y", 0.1)), 0.001)
	var source_size_z: float = max(float(data.get("primitive_size_z", 0.1)), 0.001)
	var sx: float = source_size_x
	var sy: float = source_size_z
	var sz: float = source_size_y
	var primitive_scale: Vector3 = Vector3(sx, sy, sz)
	var mesh_instance := MeshInstance3D.new()
	if primitive_shape == "sphere":
		var sphere := SphereMesh.new()
		sphere.radius = 0.5
		sphere.height = 1.0
		mesh_instance.mesh = sphere
	elif primitive_shape == "cylinder":
		var cylinder := CylinderMesh.new()
		cylinder.top_radius = 0.5
		cylinder.bottom_radius = 0.5
		cylinder.height = 1.0
		mesh_instance.mesh = cylinder
	elif primitive_shape == "cone":
		var cone := CylinderMesh.new()
		cone.top_radius = 0.0
		cone.bottom_radius = 0.5
		cone.height = 1.0
		mesh_instance.mesh = cone
	else:
		var box := BoxMesh.new()
		box.size = Vector3.ONE
		mesh_instance.mesh = box
	mesh_instance.scale = primitive_scale
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.2, 0.2, 0.22, 1.0)
	material.roughness = 0.3
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	mesh_instance.material_override = material
	return mesh_instance

func _classify_gdtf_primitive_shape(primitive_type: String) -> String:
	var normalized: String = primitive_type.strip_edges().to_lower()
	if normalized.is_empty() or normalized == "undefined":
		return "box"
	if normalized.contains("cone"):
		return "cone"
	if normalized.contains("cylinder"):
		return "cylinder"
	if normalized.contains("sphere"):
		return "sphere"
	if normalized in ["yoke", "scanner", "scanner1_1", "pigtail"]:
		return "cylinder"
	if normalized in ["head"]:
		return "sphere"
	if normalized in ["base", "base1_1", "conventional", "conventional1_1", "cube"]:
		return "box"
	return "box"
