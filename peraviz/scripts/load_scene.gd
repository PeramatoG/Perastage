extends Node3D

@onready var proxies_root: Node3D = $Proxies
@onready var status_label: Label = $HUD/StatusLabel
@onready var picker: FileDialog = $HUD/FileDialog

var _loader := PeravizLoader.new()

func _ready() -> void:
	$HUD/LoadButton.pressed.connect(_on_load_pressed)
	picker.file_selected.connect(_on_file_selected)
	status_label.text = "Selecciona un archivo .mvr"

func _on_load_pressed() -> void:
	picker.popup_centered_ratio(0.7)

func _on_file_selected(path: String) -> void:
	_clear_scene()
	var instances: Array = _loader.load_mvr(path)
	print("[Peraviz] Instancias cargadas: ", instances.size())

	for item in instances:
		if item is Dictionary:
			_create_proxy(item)

	status_label.text = "Instancias: %d" % instances.size()

func _create_proxy(data: Dictionary) -> void:
	var is_fixture: bool = bool(data.get("is_fixture", false))
	var item_type: String = str(data.get("type", "scene_object"))

	var mesh_instance := MeshInstance3D.new()
	if is_fixture:
		var cone := ConeMesh.new()
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

func _clear_scene() -> void:
	for child in proxies_root.get_children():
		child.queue_free()
