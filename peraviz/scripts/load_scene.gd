extends Node3D

@onready var proxies_root: Node3D = $Proxies
@onready var status_label: Label = $HUD/StatusLabel
@onready var picker: FileDialog = $HUD/FileDialog
@onready var camera: Camera3D = $Camera3D

var _loader := PeravizLoader.new()
var _loaded_bounds: AABB
var _has_loaded_bounds: bool = false
var _node_index: Dictionary = {}
var _asset_cache: Dictionary = {}

func _ready() -> void:
	$HUD/LoadButton.pressed.connect(_on_load_pressed)
	picker.file_selected.connect(_on_file_selected)
	picker.access = FileDialog.ACCESS_FILESYSTEM
	status_label.text = "Select a .mvr file"

func _on_load_pressed() -> void:
	picker.popup_centered_ratio(0.7)

func _on_file_selected(path: String) -> void:
	_clear_scene()
	var native_path: String = ProjectSettings.globalize_path(path)
	var nodes: Array = _loader.load_mvr(native_path)
	print("[Peraviz] Loaded render nodes: ", nodes.size())
	_has_loaded_bounds = false

	_build_node_tree(nodes)
	_focus_loaded_scene()
	status_label.text = "Nodes: %d (press F to focus)" % nodes.size()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_F:
		_focus_loaded_scene()
		get_viewport().set_input_as_handled()

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
	var is_fixture: bool = bool(data.get("is_fixture", false))
	var is_axis: bool = bool(data.get("is_axis", false))
	var is_emitter: bool = bool(data.get("is_emitter", false))
	var node_name: String = str(data.get("name", item_type))

	var root := Node3D.new()
	root.name = "%s_%s" % [item_type, node_name]
	root.position = data.get("pos", Vector3.ZERO)
	root.rotation_degrees = data.get("rot", Vector3.ZERO)
	root.scale = data.get("scale", Vector3.ONE)

	if is_axis:
		var pivot := Node3D.new()
		pivot.name = "AxisPivot"
		root.add_child(pivot)

	if is_emitter:
		var emitter := Node3D.new()
		emitter.name = "EmitterMarker"
		root.add_child(emitter)

	var model_node: Node3D = _build_visual_node(data, is_fixture)
	if model_node != null:
		root.add_child(model_node)

	return root

func _build_visual_node(data: Dictionary, is_fixture: bool) -> Node3D:
	var asset_path: String = str(data.get("asset_path", ""))
	if not asset_path.is_empty():
		var loaded: Variant = _load_3d_asset(asset_path)
		if loaded is Node3D:
			return loaded
		print("[Peraviz] Asset fallback for missing/invalid model: ", asset_path)

	return _create_dummy_mesh(is_fixture)

func _load_3d_asset(asset_path: String) -> Variant:
	if _asset_cache.has(asset_path):
		return _asset_cache[asset_path]

	var extension: String = asset_path.get_extension().to_lower()
	var loaded_node: Node3D = null

	if extension == "glb" or extension == "gltf":
		var gltf := GLTFDocument.new()
		var state := GLTFState.new()
		var err: int = gltf.append_from_file(asset_path, state)
		if err == OK:
			var generated: Node = gltf.generate_scene(state)
			if generated is Node3D:
				loaded_node = generated
	else:
		var resource: Resource = load(asset_path)
		if resource is PackedScene:
			var packed_instance: Node = resource.instantiate()
			if packed_instance is Node3D:
				loaded_node = packed_instance

	_asset_cache[asset_path] = loaded_node
	if loaded_node == null:
		return null
	return loaded_node.duplicate(DUPLICATE_USE_INSTANTIATION)

func _create_dummy_mesh(is_fixture: bool) -> Node3D:
	var mesh_instance := MeshInstance3D.new()
	if is_fixture:
		var cone := CylinderMesh.new()
		cone.top_radius = 0.0
		cone.bottom_radius = 0.15
		cone.height = 0.4
		mesh_instance.mesh = cone
	else:
		var box := BoxMesh.new()
		box.size = Vector3(0.3, 0.3, 0.3)
		mesh_instance.mesh = box

	var material := StandardMaterial3D.new()
	material.albedo_color = Color(1.0, 0.5, 0.1) if is_fixture else Color(0.2, 0.8, 1.0)
	mesh_instance.material_override = material
	return mesh_instance

func _clear_scene() -> void:
	for child in proxies_root.get_children():
		child.queue_free()
	_node_index.clear()
	_asset_cache.clear()
	_has_loaded_bounds = false

func _expand_loaded_bounds_from_node(node: Node3D) -> void:
	for child in node.get_children():
		if child is MeshInstance3D:
			_expand_loaded_bounds(child)

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
