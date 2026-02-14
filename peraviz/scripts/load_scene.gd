extends Node3D

@onready var proxies_root: Node3D = $Proxies
@onready var status_label: Label = $HUD/StatusLabel
@onready var picker: FileDialog = $HUD/FileDialog
@onready var camera: Camera3D = $Camera3D

var _loader := PeravizLoader.new()
var _loaded_bounds: AABB
var _has_loaded_bounds: bool = false

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
	var instances: Array = _loader.load_mvr(native_path)
	print("[Peraviz] Loaded instances: ", instances.size())
	_has_loaded_bounds = false

	for item in instances:
		if item is Dictionary:
			_create_proxy(item)

	_focus_loaded_scene()
	status_label.text = "Instances: %d (press F to focus)" % instances.size()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_F:
		_focus_loaded_scene()
		get_viewport().set_input_as_handled()

func _create_proxy(data: Dictionary) -> void:
	var is_fixture: bool = bool(data.get("is_fixture", false))
	var item_type: String = str(data.get("type", "scene_object"))

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

	mesh_instance.position = data.get("pos", Vector3.ZERO)
	mesh_instance.rotation_degrees = data.get("rot", Vector3.ZERO)
	mesh_instance.scale = data.get("scale", Vector3.ONE)

	var material := StandardMaterial3D.new()
	material.albedo_color = Color(1.0, 0.5, 0.1) if is_fixture else Color(0.2, 0.8, 1.0)
	mesh_instance.material_override = material
	mesh_instance.name = "%s_%s" % [item_type, str(data.get("id", "item"))]

	proxies_root.add_child(mesh_instance)
	_expand_loaded_bounds(mesh_instance)

func _clear_scene() -> void:
	for child in proxies_root.get_children():
		child.queue_free()
	_has_loaded_bounds = false

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
