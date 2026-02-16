extends Node3D

@onready var proxies_root: Node3D = $Proxies
@onready var status_label: Label = $HUD/StatusLabel
@onready var picker: FileDialog = $HUD/FileDialog
@onready var camera: Camera3D = $Camera3D

var _loader := PeravizLoader.new()
var _scene_registry := SceneRegistry.new()
var _loaded_bounds: AABB
var _has_loaded_bounds: bool = false
var _node_index: Dictionary = {}
var _asset_cache := PeravizRuntimeAssetCache.new()
var _debug_coords_enabled: bool = false
var _debug_asset_cache_enabled: bool = false
var _debug_gizmos_root: Node3D

const DEBUG_TOGGLE_KEY: Key = KEY_C

func _ready() -> void:
	_scene_registry.configure(proxies_root)
	$HUD/LoadButton.pressed.connect(_on_load_pressed)
	picker.file_selected.connect(_on_file_selected)
	picker.access = FileDialog.ACCESS_FILESYSTEM
	status_label.text = "Select a .mvr file"
	_debug_coords_enabled = bool(ProjectSettings.get_setting("peraviz_debug_coords", false))
	_debug_asset_cache_enabled = bool(ProjectSettings.get_setting("peraviz_debug_asset_cache", false))
	_asset_cache.configure_debug_logging(_debug_asset_cache_enabled, 100)
	_ensure_debug_gizmo_root()
	_update_debug_legend()

func _on_load_pressed() -> void:
	picker.popup_centered_ratio(0.7)

func _on_file_selected(path: String) -> void:
	_clear_scene()
	var native_path: String = ProjectSettings.globalize_path(path)
	var peraviz_debug_baseline: bool = bool(ProjectSettings.get_setting("peraviz_debug_baseline", false))
	var nodes: Array = _loader.load_mvr(native_path, peraviz_debug_baseline, _debug_coords_enabled)
	print("[Peraviz] Loaded render nodes: ", nodes.size(), " baseline_debug=", peraviz_debug_baseline, " coords_debug=", _debug_coords_enabled)
	_has_loaded_bounds = false
	_clear_debug_gizmos()

	_build_node_tree(nodes)
	_register_fixture_registry(nodes)
	_rebuild_debug_gizmos()
	_focus_loaded_scene()
	if _debug_asset_cache_enabled:
		var cache_summary: Dictionary = _asset_cache.debug_summary()
		var hit_by_kind: Dictionary = cache_summary.get("hits_by_kind", {})
		var miss_by_kind: Dictionary = cache_summary.get("misses_by_kind", {})
		print("[PeravizAssetCache] summary hits=", cache_summary.get("hits", 0),
			" misses=", cache_summary.get("misses", 0),
			" unique=", cache_summary.get("unique_resources", 0),
			" mesh(hit/miss)=", hit_by_kind.get("mesh", 0), "/", miss_by_kind.get("mesh", 0),
			" scene(hit/miss)=", hit_by_kind.get("scene", 0), "/", miss_by_kind.get("scene", 0),
			" material(hit/miss)=", hit_by_kind.get("material", 0), "/", miss_by_kind.get("material", 0))
	status_label.text = "Nodes: %d (press F to focus, C debug coords)" % nodes.size()
	_update_debug_legend()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_F:
		_focus_loaded_scene()
		get_viewport().set_input_as_handled()
		return

	if event is InputEventKey and event.pressed and not event.echo and event.keycode == DEBUG_TOGGLE_KEY:
		_debug_coords_enabled = not _debug_coords_enabled
		ProjectSettings.set_setting("peraviz_debug_coords", _debug_coords_enabled)
		print("[PeravizCoordDebug] event=toggle coords_debug=", _debug_coords_enabled)
		_rebuild_debug_gizmos()
		_update_debug_legend()
		get_viewport().set_input_as_handled()


func _ensure_debug_gizmo_root() -> void:
	if _debug_gizmos_root != null and is_instance_valid(_debug_gizmos_root):
		return
	_debug_gizmos_root = Node3D.new()
	_debug_gizmos_root.name = "DebugGizmos"
	_debug_gizmos_root.top_level = true
	add_child(_debug_gizmos_root)

func _clear_debug_gizmos() -> void:
	_ensure_debug_gizmo_root()
	for child in _debug_gizmos_root.get_children():
		child.queue_free()

func _rebuild_debug_gizmos() -> void:
	_clear_debug_gizmos()
	if not _debug_coords_enabled:
		return

	_add_debug_gizmo_for_target(proxies_root, "scene_root", Color(1.0, 0.25, 1.0), 0.75)
	for node in _node_index.values():
		if node is not Node3D:
			continue
		var node3d: Node3D = node
		var metadata_type: String = str(node3d.get_meta("peraviz_type", ""))
		var is_axis: bool = bool(node3d.get_meta("peraviz_is_axis", false))
		var is_emitter: bool = bool(node3d.get_meta("peraviz_is_emitter", false))

		if metadata_type in ["fixture", "truss", "support", "scene_object"]:
			_add_debug_gizmo_for_target(node3d, "mvr_instance_root", Color(1.0, 0.9, 0.2), 0.55)
		if is_axis:
			_add_debug_gizmo_for_target(node3d, "gdtf_axis", Color(0.0, 1.0, 1.0), 0.35)
		if is_emitter:
			_add_debug_gizmo_for_target(node3d, "emitter", Color(1.0, 0.55, 0.15), 0.30)


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

func _safe_target_global_position(target: Node3D) -> Vector3:
	if target == null:
		return Vector3.ZERO
	var origin: Vector3 = target.global_position
	if not is_finite(origin.x) or not is_finite(origin.y) or not is_finite(origin.z):
		print("[PeravizCoordDebug] event=invalid_target_origin_fallback node=", target.name, " origin=", origin)
		return Vector3.ZERO
	return origin

func _add_debug_gizmo_for_target(target: Node3D, kind: String, origin_color: Color, length: float) -> void:
	if target == null:
		return
	var holder := Node3D.new()
	holder.name = "Gizmo_%s" % kind
	_debug_gizmos_root.add_child(holder)
	holder.global_position = _safe_target_global_position(target)
	holder.add_child(_create_axes_gizmo_node(origin_color, length))

func _create_axes_gizmo_node(origin_color: Color, length: float) -> Node3D:
	var immediate := ImmediateMesh.new()
	var line_material := ORMMaterial3D.new()
	line_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	line_material.vertex_color_use_as_albedo = true
	immediate.surface_begin(Mesh.PRIMITIVE_LINES, line_material)
	immediate.surface_set_color(Color(1, 0, 0))
	immediate.surface_add_vertex(Vector3.ZERO)
	immediate.surface_add_vertex(Vector3.RIGHT * length)
	immediate.surface_set_color(Color(0, 1, 0))
	immediate.surface_add_vertex(Vector3.ZERO)
	immediate.surface_add_vertex(Vector3.UP * length)
	immediate.surface_set_color(Color(0, 0.4, 1))
	immediate.surface_add_vertex(Vector3.ZERO)
	immediate.surface_add_vertex(Vector3.BACK * length)
	immediate.surface_end()

	var axes_instance := MeshInstance3D.new()
	axes_instance.mesh = immediate
	axes_instance.material_override = line_material

	var marker := SphereMesh.new()
	marker.radius = max(length * 0.08, 0.01)
	marker.height = marker.radius * 2.0
	var marker_material := StandardMaterial3D.new()
	marker_material.albedo_color = origin_color
	marker_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	var marker_instance := MeshInstance3D.new()
	marker_instance.mesh = marker
	marker_instance.material_override = marker_material

	var container := Node3D.new()
	container.add_child(axes_instance)
	container.add_child(marker_instance)
	return container

func _update_debug_legend() -> void:
	if not _debug_coords_enabled:
		print("[PeravizCoordDebugLegend] disabled (press C to enable)")
		return

	print("[PeravizCoordDebugLegend] X=Red Y=Green Z=Blue scene_root_origin=Magenta mvr_instance_root_origin=Yellow gdtf_axis_origin=Cyan emitter_origin=Orange beam_expected_local=-Z")

func _build_node_tree(nodes: Array) -> void:
	_node_index.clear()
	for item in nodes:
		if item is Dictionary:
			var node_id: String = str(item.get("node_id", ""))
			if node_id.is_empty():
				continue
			_node_index[node_id] = _create_scene_node(item)

	for item in nodes:
		if item is Dictionary:
			var node_id: String = str(item.get("node_id", ""))
			var parent_id: String = str(item.get("parent_id", ""))
			var node: Node3D = _node_index.get(node_id)
			if node == null:
				continue
			var parent_node: Node3D = proxies_root
			if not parent_id.is_empty() and _node_index.has(parent_id):
				parent_node = _node_index[parent_id]
			parent_node.add_child(node)
			_expand_loaded_bounds_from_node(node)

func _create_scene_node(data: Dictionary) -> Node3D:
	var item_type: String = str(data.get("type", "scene_object"))
	var item_class: String = _extract_node_class(data, item_type)
	var is_fixture: bool = bool(data.get("is_fixture", false))
	var is_axis: bool = bool(data.get("is_axis", false))
	var is_emitter: bool = bool(data.get("is_emitter", false))
	var node_name: String = str(data.get("name", item_type))

	var root := Node3D.new()
	root.name = "%s_%s" % [item_class, node_name]
	root.set_meta("peraviz_type", item_type)
	root.set_meta("peraviz_is_axis", is_axis)
	root.set_meta("peraviz_is_emitter", is_emitter)
	var node_position: Vector3 = _safe_position(data.get("pos", Vector3.ZERO), "create_scene_node:" + root.name)
	if bool(data.get("has_basis", false)):
		var node_basis: Basis = _safe_basis_from_data(data)
		var safe_scale: Vector3 = _safe_scale_from_data(data, "create_scene_node_basis:" + root.name)
		node_basis = node_basis.scaled(safe_scale)
		if not _is_basis_valid(node_basis):
			print("[PeravizCoordDebug] event=scaled_basis_invalid_fallback node=", root.name, " scale=", safe_scale)
			node_basis = Basis.IDENTITY.scaled(safe_scale)
		root.transform = Transform3D(node_basis, node_position)
	else:
		root.position = node_position
		root.rotation_degrees = data.get("rot", Vector3.ZERO)
		root.scale = _safe_scale_from_data(data, "create_scene_node_euler:" + root.name)

	if is_axis:
		var pivot := Node3D.new()
		pivot.name = "AxisPivot"
		root.add_child(pivot)

	if is_emitter:
		var emitter := Node3D.new()
		emitter.name = "EmitterMarker"
		root.add_child(emitter)

	var visual_scale_hint: float = _extract_visual_scale_hint(data)
	var model_node: Node3D = _build_visual_node(data, item_type, item_class, is_fixture, visual_scale_hint)
	if model_node != null:
		root.add_child(model_node)
		if item_type == "fixture_geometry":
			_reparent_fixture_visual_children(root, model_node)

	return root

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
		geometry_node.add_child(child_node)
		child_node.transform = child_local_after
		moved_any_child = true

	if moved_any_child:
		model_root.queue_free()

func _build_visual_node(data: Dictionary, item_type: String, item_class: String, is_fixture: bool, visual_scale_hint: float) -> Node3D:
	var asset_path: String = str(data.get("asset_path", ""))
	var asset_kind: String = _extract_asset_kind(data, asset_path)
	if not asset_path.is_empty():
		var loaded: Variant = _load_3d_asset(asset_path, asset_kind)
		if loaded is Node3D:
			return loaded
		print("[Peraviz] Asset fallback for missing/invalid model: ", asset_path, " type=", item_type, " class=", item_class, " asset_kind=", asset_kind)

	if item_type == "fixture" or item_type == "fixture_geometry":
		return null

	return _create_dummy_mesh(is_fixture, visual_scale_hint)

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

func _load_3d_asset(asset_path: String, asset_kind_hint: String = "") -> Variant:
	var resolved_asset_kind: String = asset_kind_hint.to_lower()
	if resolved_asset_kind.is_empty() or resolved_asset_kind == "none":
		resolved_asset_kind = _infer_asset_kind_from_extension(asset_path)

	var extension: String = asset_path.get_extension().to_lower()
	if resolved_asset_kind == "mesh" or extension == "3ds":
		var cached_mesh: Mesh = _asset_cache.get_mesh(asset_path)
		if cached_mesh != null:
			var cached_mesh_instance := MeshInstance3D.new()
			cached_mesh_instance.mesh = cached_mesh
			return cached_mesh_instance

		var mesh_data: Dictionary = _loader.load_3ds_mesh_data(asset_path)
		if not bool(mesh_data.get("ok", false)):
			_asset_cache.mark_failed(asset_path)
			return null
		var mesh: ArrayMesh = _build_3ds_mesh(mesh_data)
		if mesh == null:
			_asset_cache.mark_failed(asset_path)
			return null
		_asset_cache.store_mesh(asset_path, mesh)
		var mesh_instance := MeshInstance3D.new()
		mesh_instance.mesh = mesh
		return mesh_instance

	if resolved_asset_kind == "scene" or extension == "glb" or extension == "gltf":
		var cached_scene_instance: Node3D = _asset_cache.instantiate_scene(asset_path)
		if cached_scene_instance != null:
			return cached_scene_instance

		var gltf := GLTFDocument.new()
		var state := GLTFState.new()
		var err: int = gltf.append_from_file(asset_path, state)
		if err != OK:
			_asset_cache.mark_failed(asset_path)
			return null
		var generated: Node = gltf.generate_scene(state)
		if generated is Node3D:
			var packed_scene := PackedScene.new()
			if packed_scene.pack(generated) == OK:
				_asset_cache.store_scene(asset_path, packed_scene)
				generated.free()
				return _asset_cache.instantiate_scene(asset_path)
		if generated != null:
			generated.free()
		_asset_cache.mark_failed(asset_path)
		return null

	var scene_instance: Node3D = _asset_cache.instantiate_scene(asset_path)
	if scene_instance != null:
		return scene_instance

	var resource: Resource = load(asset_path)
	if resource is PackedScene:
		_asset_cache.store_scene(asset_path, resource)
		return _asset_cache.instantiate_scene(asset_path)

	_asset_cache.mark_failed(asset_path)
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
	if extension == "glb" or extension == "gltf":
		return "scene"
	return "none"


func _build_3ds_mesh(mesh_data: Dictionary) -> ArrayMesh:
	var vertices: PackedVector3Array = mesh_data.get("vertices", PackedVector3Array())
	var normals: PackedVector3Array = mesh_data.get("normals", PackedVector3Array())
	var indices: PackedInt32Array = mesh_data.get("indices", PackedInt32Array())
	if vertices.is_empty() or indices.is_empty():
		return null

	var arrays: Array = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_INDEX] = indices

	var array_mesh := ArrayMesh.new()
	array_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return array_mesh


func _create_dummy_mesh(is_fixture: bool, visual_scale_hint: float) -> Node3D:
	var mesh_instance := MeshInstance3D.new()
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
	var material: Material = _asset_cache.get_material(material_key, true)
	if material == null:
		var base_material := StandardMaterial3D.new()
		base_material.albedo_color = Color(1.0, 0.5, 0.1) if is_fixture else Color(0.2, 0.8, 1.0)
		_asset_cache.store_material(material_key, base_material)
		material = _asset_cache.get_material(material_key, true)
	mesh_instance.material_override = material
	return mesh_instance

func _clear_scene() -> void:
	_scene_registry.clear("scene_reload")
	for child in proxies_root.get_children():
		child.queue_free()
	_node_index.clear()
	_asset_cache.clear()
	_has_loaded_bounds = false
	_clear_debug_gizmos()

func _register_fixture_registry(nodes: Array) -> void:
	if nodes.is_empty():
		return

	var parent_lookup: Dictionary = {}
	var type_lookup: Dictionary = {}
	for item in nodes:
		if item is not Dictionary:
			continue
		var node_id: String = str(item.get("node_id", ""))
		if node_id.is_empty():
			continue
		parent_lookup[node_id] = str(item.get("parent_id", ""))
		type_lookup[node_id] = str(item.get("type", ""))

	var fixture_anchors: Dictionary = {}
	for node_id in type_lookup.keys():
		if str(type_lookup.get(node_id, "")) != "fixture":
			continue
		fixture_anchors[node_id] = {
			"axis": [],
			"emitters": [],
			"geometry_nodes": [],
		}

	for item in nodes:
		if item is not Dictionary:
			continue
		var node_id: String = str(item.get("node_id", ""))
		if node_id.is_empty():
			continue

		var fixture_uuid: String = _resolve_fixture_uuid(node_id, parent_lookup, type_lookup, {})
		if fixture_uuid.is_empty() or not fixture_anchors.has(fixture_uuid):
			continue

		if node_id == fixture_uuid:
			continue

		var node: Node3D = _node_index.get(node_id)
		if node == null:
			continue

		var anchors: Dictionary = fixture_anchors[fixture_uuid]
		if bool(item.get("is_axis", false)):
			var axis_nodes: Array = anchors.get("axis", [])
			axis_nodes.append(node)
			anchors["axis"] = axis_nodes
		if bool(item.get("is_emitter", false)):
			var emitter_nodes: Array = anchors.get("emitters", [])
			emitter_nodes.append(node)
			anchors["emitters"] = emitter_nodes
		if str(item.get("type", "")) == "fixture_geometry":
			var geometry_nodes: Array = anchors.get("geometry_nodes", [])
			geometry_nodes.append(node)
			anchors["geometry_nodes"] = geometry_nodes

	for fixture_uuid in fixture_anchors.keys():
		var fixture_node: Node3D = _node_index.get(fixture_uuid)
		if fixture_node == null:
			print("[PeravizSceneRegistry] register_fixture skipped: fixture node missing uuid=", fixture_uuid)
			continue
		var anchors: Dictionary = fixture_anchors[fixture_uuid]
		_scene_registry.register_fixture(fixture_uuid, fixture_node, anchors)

func _resolve_fixture_uuid(node_id: String, parent_lookup: Dictionary, type_lookup: Dictionary, cache: Dictionary) -> String:
	if cache.has(node_id):
		return str(cache.get(node_id, ""))

	var node_type: String = str(type_lookup.get(node_id, ""))
	if node_type == "fixture":
		cache[node_id] = node_id
		return node_id

	var parent_id: String = str(parent_lookup.get(node_id, ""))
	if parent_id.is_empty() or parent_id == node_id:
		cache[node_id] = ""
		return ""

	var fixture_uuid: String = _resolve_fixture_uuid(parent_id, parent_lookup, type_lookup, cache)
	cache[node_id] = fixture_uuid
	return fixture_uuid

func _expand_loaded_bounds_from_node(node: Node3D) -> void:
	if node is MeshInstance3D:
		_expand_loaded_bounds(node)

	for child in node.get_children():
		if child is Node3D:
			_expand_loaded_bounds_from_node(child)

func _expand_loaded_bounds(mesh_instance: MeshInstance3D) -> void:
	var mesh_bounds: AABB = mesh_instance.get_aabb()
	var world_bounds: AABB = mesh_instance.global_transform * mesh_bounds
	if not _has_loaded_bounds:
		_loaded_bounds = world_bounds
		_has_loaded_bounds = true
		return

	_loaded_bounds = _loaded_bounds.merge(world_bounds)

func _focus_loaded_scene() -> void:
	if not _has_loaded_bounds:
		return

	if camera.has_method("focus_on_aabb"):
		camera.call("focus_on_aabb", _loaded_bounds)
