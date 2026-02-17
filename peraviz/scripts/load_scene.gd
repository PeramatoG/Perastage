extends Node3D

@onready var proxies_root: Node3D = $Proxies
@onready var status_label: Label = $HUD/StatusLabel
@onready var picker: FileDialog = $HUD/FileDialog
@onready var camera: Camera3D = $Camera3D
@onready var manual_fixture_toggle: CheckButton = $HUD/ManualFixtureToggle
@onready var fixture_debug_panel: PanelContainer = $HUD/FixtureDebugPanel
@onready var fixture_list: ItemList = $HUD/FixtureDebugPanel/Margin/VBox/FixtureList
@onready var fixture_selected_label: Label = $HUD/FixtureDebugPanel/Margin/VBox/SelectedFixtureLabel
@onready var fixture_axis_label: Label = $HUD/FixtureDebugPanel/Margin/VBox/AxisAnchorsLabel
@onready var fixture_emitter_label: Label = $HUD/FixtureDebugPanel/Margin/VBox/EmitterAnchorsLabel
@onready var pan_min_input: SpinBox = $HUD/FixtureDebugPanel/Margin/VBox/LimitsGrid/PanMin
@onready var pan_max_input: SpinBox = $HUD/FixtureDebugPanel/Margin/VBox/LimitsGrid/PanMax
@onready var pan_value_input: SpinBox = $HUD/FixtureDebugPanel/Margin/VBox/LimitsGrid/PanValue
@onready var tilt_min_input: SpinBox = $HUD/FixtureDebugPanel/Margin/VBox/LimitsGrid/TiltMin
@onready var tilt_max_input: SpinBox = $HUD/FixtureDebugPanel/Margin/VBox/LimitsGrid/TiltMax
@onready var tilt_value_input: SpinBox = $HUD/FixtureDebugPanel/Margin/VBox/LimitsGrid/TiltValue
@onready var dimmer_min_input: SpinBox = $HUD/FixtureDebugPanel/Margin/VBox/LimitsGrid/DimmerMin
@onready var dimmer_max_input: SpinBox = $HUD/FixtureDebugPanel/Margin/VBox/LimitsGrid/DimmerMax
@onready var dimmer_value_input: SpinBox = $HUD/FixtureDebugPanel/Margin/VBox/LimitsGrid/DimmerValue
@onready var pan_slider: HSlider = $HUD/FixtureDebugPanel/Margin/VBox/PanSlider
@onready var tilt_slider: HSlider = $HUD/FixtureDebugPanel/Margin/VBox/TiltSlider
@onready var dimmer_slider: HSlider = $HUD/FixtureDebugPanel/Margin/VBox/DimmerSlider
@onready var quick_reset_button: Button = $HUD/FixtureDebugPanel/Margin/VBox/QuickResetButton

var _loader := PeravizLoader.new()
var _scene_registry := SceneRegistry.new()
var _loaded_bounds: AABB
var _has_loaded_bounds: bool = false
var _node_index: Dictionary = {}
var _asset_cache := PeravizRuntimeAssetCache.new()
var _debug_coords_enabled: bool = false
var _debug_asset_cache_enabled: bool = false
var _debug_gizmos_root: Node3D
var _manual_fixture_test_enabled: bool = false
var _selected_fixture_uuid: String = ""
var _fixture_manual_values: Dictionary = {}
var _fixture_emissive_cache: Dictionary = {}
var _fixture_emitter_light_cache: Dictionary = {}
var _fixture_emitter_photometrics: Dictionary = {}
var _fixture_lens_tuned: Dictionary = {}
var _updating_fixture_controls: bool = false

const DEBUG_TOGGLE_KEY: Key = KEY_C
const MANUAL_TEST_FLAG: String = "--peraviz_manual_fixture_test"
const MANUAL_DEFAULTS := {
	"pan": 0.0,
	"tilt": 0.0,
	"dimmer": 100.0,
}

const DEFAULT_EMITTER_PHOTOMETRICS := {
	"luminous_flux": 10000.0,
	"color_temperature": 6000.0,
	"beam_angle": 25.0,
	"field_angle": 25.0,
	"beam_radius": 0.05,
	"dominant_wavelength": 0.0,
}

# Keep emitter light direction aligned with fixture zero-position beam expectation
# (aligned with fixture lens output in runtime scenes) while still inheriting GDTF emitter transforms.
const EMITTER_LIGHT_DIRECTION_FIX: Vector3 = Vector3(-90.0, 0.0, 0.0)
const LENS_TINT_COLOR: Color = Color(0.72, 0.86, 1.0, 0.38)

func _ready() -> void:
	_scene_registry.configure(proxies_root)
	$HUD/LoadButton.pressed.connect(_on_load_pressed)
	picker.file_selected.connect(_on_file_selected)
	manual_fixture_toggle.toggled.connect(_on_manual_fixture_toggle)
	fixture_list.item_selected.connect(_on_fixture_list_item_selected)
	pan_slider.value_changed.connect(_on_pan_slider_changed)
	tilt_slider.value_changed.connect(_on_tilt_slider_changed)
	dimmer_slider.value_changed.connect(_on_dimmer_slider_changed)
	pan_value_input.value_changed.connect(_on_pan_input_changed)
	tilt_value_input.value_changed.connect(_on_tilt_input_changed)
	dimmer_value_input.value_changed.connect(_on_dimmer_input_changed)
	pan_min_input.value_changed.connect(_on_limit_changed)
	pan_max_input.value_changed.connect(_on_limit_changed)
	tilt_min_input.value_changed.connect(_on_limit_changed)
	tilt_max_input.value_changed.connect(_on_limit_changed)
	dimmer_min_input.value_changed.connect(_on_limit_changed)
	dimmer_max_input.value_changed.connect(_on_limit_changed)
	quick_reset_button.pressed.connect(_on_quick_reset_pressed)
	picker.access = FileDialog.ACCESS_FILESYSTEM
	status_label.text = "Select a .mvr file"
	_debug_coords_enabled = bool(ProjectSettings.get_setting("peraviz_debug_coords", false))
	_debug_asset_cache_enabled = bool(ProjectSettings.get_setting("peraviz_debug_asset_cache", false))
	_manual_fixture_test_enabled = _read_manual_fixture_test_setting()
	manual_fixture_toggle.button_pressed = _manual_fixture_test_enabled
	_asset_cache.configure_debug_logging(_debug_asset_cache_enabled, 100)
	_ensure_debug_gizmo_root()
	_update_debug_legend()
	_refresh_fixture_debug_panel()

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
	_populate_fixture_list()
	_sync_selection_state("scene_reload")
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

func _on_manual_fixture_toggle(enabled: bool) -> void:
	_manual_fixture_test_enabled = enabled
	ProjectSettings.set_setting("peraviz_manual_fixture_test", _manual_fixture_test_enabled)
	_refresh_fixture_debug_panel()

func _on_fixture_list_item_selected(index: int) -> void:
	if index < 0 or index >= fixture_list.get_item_count():
		return
	var fixture_uuid: String = fixture_list.get_item_metadata(index)
	_select_fixture_by_uuid(fixture_uuid, "list")

func _unhandled_input(event: InputEvent) -> void:
	_sync_selection_state("input")

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
		return

	if _manual_fixture_test_enabled and event is InputEventMouseButton:
		var mouse_event := event as InputEventMouseButton
		if mouse_event.pressed and mouse_event.button_index == MOUSE_BUTTON_LEFT:
			_try_select_fixture_from_mouse(mouse_event.position)
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
	holder.global_transform = Transform3D(target.global_basis, _safe_target_global_position(target))
	holder.add_child(_create_axes_gizmo_node(origin_color, length))
	if kind == "emitter":
		holder.add_child(_create_emitter_direction_gizmo(length))

func _create_emitter_direction_gizmo(length: float) -> MeshInstance3D:
	var immediate := ImmediateMesh.new()
	var beam_material := ORMMaterial3D.new()
	beam_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	beam_material.albedo_color = Color(1.0, 0.55, 0.15)
	beam_material.emission_enabled = true
	beam_material.emission = Color(1.0, 0.55, 0.15)
	beam_material.emission_energy_multiplier = 2.0
	immediate.surface_begin(Mesh.PRIMITIVE_LINES, beam_material)
	immediate.surface_add_vertex(Vector3.ZERO)
	immediate.surface_add_vertex(Vector3.BACK * (length * 1.8))
	immediate.surface_end()

	var beam := MeshInstance3D.new()
	beam.mesh = immediate
	beam.material_override = beam_material
	return beam

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
	_clear_selected_fixture("scene_clear")
	for child in proxies_root.get_children():
		child.queue_free()
	_node_index.clear()
	_asset_cache.clear()
	_fixture_emissive_cache.clear()
	_fixture_emitter_light_cache.clear()
	_fixture_emitter_photometrics.clear()
	_fixture_lens_tuned.clear()
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
			if bool(item.get("is_emitter", false)):
				var photometric_entries: Array = anchors.get("emitter_photometrics", [])
				photometric_entries.append(_extract_emitter_photometrics(item))
				anchors["emitter_photometrics"] = photometric_entries

	for fixture_uuid in fixture_anchors.keys():
		var fixture_node: Node3D = _node_index.get(fixture_uuid)
		if fixture_node == null:
			print("[PeravizSceneRegistry] register_fixture skipped: fixture node missing uuid=", fixture_uuid)
			continue
		var anchors: Dictionary = fixture_anchors[fixture_uuid]
		_scene_registry.register_fixture(fixture_uuid, fixture_node, anchors)
		_attach_fixture_pick_colliders(fixture_uuid, fixture_node)

func _read_manual_fixture_test_setting() -> bool:
	var setting_enabled: bool = bool(ProjectSettings.get_setting("peraviz_manual_fixture_test", false))
	if setting_enabled:
		return true
	return OS.get_cmdline_args().has(MANUAL_TEST_FLAG)

func _populate_fixture_list() -> void:
	fixture_list.clear()
	for fixture_uuid in _scene_registry.list_fixture_uuids():
		var index: int = fixture_list.get_item_count()
		fixture_list.add_item(fixture_uuid)
		fixture_list.set_item_metadata(index, fixture_uuid)
	if not _selected_fixture_uuid.is_empty():
		var selected_index: int = _find_fixture_list_index(_selected_fixture_uuid)
		if selected_index >= 0:
			fixture_list.select(selected_index)

func _find_fixture_list_index(fixture_uuid: String) -> int:
	for index in range(fixture_list.get_item_count()):
		var list_uuid: String = fixture_list.get_item_metadata(index)
		if list_uuid == fixture_uuid:
			return index
	return -1

func _try_select_fixture_from_mouse(mouse_position: Vector2) -> void:
	var world_3d: World3D = get_world_3d()
	if world_3d == null:
		return

	var query := PhysicsRayQueryParameters3D.create(
		camera.project_ray_origin(mouse_position),
		camera.project_ray_origin(mouse_position) + camera.project_ray_normal(mouse_position) * 1000.0
	)
	query.collide_with_areas = false
	query.collide_with_bodies = true
	var result: Dictionary = world_3d.direct_space_state.intersect_ray(query)
	if result.is_empty():
		return

	var collider: Object = result.get("collider")
	if collider == null or not (collider is Node):
		return

	var fixture_uuid: String = _resolve_fixture_uuid_from_node(collider as Node)
	if fixture_uuid.is_empty():
		return
	_select_fixture_by_uuid(fixture_uuid, "raycast")

func _resolve_fixture_uuid_from_node(node: Node) -> String:
	var current: Node = node
	while current != null:
		if current.has_meta("peraviz_fixture_uuid"):
			return str(current.get_meta("peraviz_fixture_uuid", ""))
		current = current.get_parent()
	return ""

func _attach_fixture_pick_colliders(fixture_uuid: String, fixture_node: Node3D) -> void:
	if fixture_node == null:
		return
	fixture_node.set_meta("peraviz_fixture_uuid", fixture_uuid)
	for child in fixture_node.get_children():
		_attach_fixture_pick_colliders_recursive(fixture_uuid, child)

func _attach_fixture_pick_colliders_recursive(fixture_uuid: String, node: Node) -> void:
	if node is MeshInstance3D:
		_attach_pick_collider_to_mesh(node as MeshInstance3D, fixture_uuid)
	if node is Node:
		node.set_meta("peraviz_fixture_uuid", fixture_uuid)
	for child in node.get_children():
		_attach_fixture_pick_colliders_recursive(fixture_uuid, child)

func _attach_pick_collider_to_mesh(mesh_instance: MeshInstance3D, fixture_uuid: String) -> void:
	if mesh_instance == null or mesh_instance.mesh == null:
		return
	var mesh_bounds: AABB = mesh_instance.get_aabb()
	if mesh_bounds.size == Vector3.ZERO:
		return

	var static_body := StaticBody3D.new()
	static_body.name = "FixturePickBody"
	static_body.set_meta("peraviz_fixture_uuid", fixture_uuid)
	mesh_instance.add_child(static_body)

	var shape := BoxShape3D.new()
	shape.size = mesh_bounds.size
	var collision := CollisionShape3D.new()
	collision.shape = shape
	collision.position = mesh_bounds.get_center()
	static_body.add_child(collision)

func _select_fixture_by_uuid(fixture_uuid: String, source: String) -> void:
	if fixture_uuid.is_empty() or not _scene_registry.has_fixture(fixture_uuid):
		return
	_selected_fixture_uuid = fixture_uuid
	_ensure_fixture_manual_values(fixture_uuid)
	var selected_index: int = _find_fixture_list_index(fixture_uuid)
	if selected_index >= 0:
		fixture_list.select(selected_index)
	print("[PeravizFixtureTest] selected uuid=", fixture_uuid, " source=", source)
	_apply_selected_fixture_controls("select")
	_refresh_fixture_debug_panel()

func _clear_selected_fixture(reason: String) -> void:
	if _selected_fixture_uuid.is_empty():
		_refresh_fixture_debug_panel()
		return
	print("[PeravizFixtureTest] clear selection uuid=", _selected_fixture_uuid, " reason=", reason)
	_selected_fixture_uuid = ""
	fixture_list.deselect_all()
	_refresh_fixture_debug_panel()

func _sync_selection_state(reason: String) -> void:
	if _selected_fixture_uuid.is_empty():
		_refresh_fixture_debug_panel()
		return
	if _scene_registry.has_fixture(_selected_fixture_uuid):
		_refresh_fixture_debug_panel()
		return
	_clear_selected_fixture(reason)

func _refresh_fixture_debug_panel() -> void:
	manual_fixture_toggle.button_pressed = _manual_fixture_test_enabled
	fixture_debug_panel.visible = _manual_fixture_test_enabled
	if not _manual_fixture_test_enabled:
		return

	var selected_text: String = "Selected fixture: <none>"
	var axis_text: String = "Axis anchors: 0"
	var emitter_text: String = "Emitter anchors: 0"
	if not _selected_fixture_uuid.is_empty() and _scene_registry.has_fixture(_selected_fixture_uuid):
		selected_text = "Selected fixture: %s" % _selected_fixture_uuid
		var axis_anchors: Variant = _scene_registry.get_anchor(_selected_fixture_uuid, "axis")
		var emitter_anchors: Variant = _scene_registry.get_anchor(_selected_fixture_uuid, "emitters")
		axis_text = "Axis anchors: %d" % _count_valid_nodes(axis_anchors)
		emitter_text = "Emitter anchors: %d" % _count_valid_nodes(emitter_anchors)

	fixture_selected_label.text = selected_text
	fixture_axis_label.text = axis_text
	fixture_emitter_label.text = emitter_text
	_sync_controls_from_selected_fixture()

func _ensure_fixture_manual_values(fixture_uuid: String) -> void:
	if fixture_uuid.is_empty():
		return
	if _fixture_manual_values.has(fixture_uuid):
		return
	_fixture_manual_values[fixture_uuid] = {
		"pan": float(MANUAL_DEFAULTS["pan"]),
		"tilt": float(MANUAL_DEFAULTS["tilt"]),
		"dimmer": float(MANUAL_DEFAULTS["dimmer"]),
	}

func _sync_controls_from_selected_fixture() -> void:
	if not _manual_fixture_test_enabled:
		return

	_sync_slider_ranges_from_limits()
	var values: Dictionary = _get_selected_fixture_values()
	_updating_fixture_controls = true
	pan_slider.value = float(values.get("pan", MANUAL_DEFAULTS["pan"]))
	tilt_slider.value = float(values.get("tilt", MANUAL_DEFAULTS["tilt"]))
	dimmer_slider.value = float(values.get("dimmer", MANUAL_DEFAULTS["dimmer"]))
	pan_value_input.value = pan_slider.value
	tilt_value_input.value = tilt_slider.value
	dimmer_value_input.value = dimmer_slider.value
	var controls_enabled: bool = not _selected_fixture_uuid.is_empty() and _scene_registry.has_fixture(_selected_fixture_uuid)
	pan_slider.editable = controls_enabled
	tilt_slider.editable = controls_enabled
	dimmer_slider.editable = controls_enabled
	pan_value_input.editable = controls_enabled
	tilt_value_input.editable = controls_enabled
	dimmer_value_input.editable = controls_enabled
	quick_reset_button.disabled = not controls_enabled
	_updating_fixture_controls = false

func _sync_slider_ranges_from_limits() -> void:
	var pan_min: float = min(pan_min_input.value, pan_max_input.value)
	var pan_max: float = max(pan_min_input.value, pan_max_input.value)
	var tilt_min: float = min(tilt_min_input.value, tilt_max_input.value)
	var tilt_max: float = max(tilt_min_input.value, tilt_max_input.value)
	var dimmer_min: float = min(dimmer_min_input.value, dimmer_max_input.value)
	var dimmer_max: float = max(dimmer_min_input.value, dimmer_max_input.value)

	pan_slider.min_value = pan_min
	pan_slider.max_value = pan_max
	tilt_slider.min_value = tilt_min
	tilt_slider.max_value = tilt_max
	dimmer_slider.min_value = dimmer_min
	dimmer_slider.max_value = dimmer_max

	pan_value_input.min_value = pan_min
	pan_value_input.max_value = pan_max
	tilt_value_input.min_value = tilt_min
	tilt_value_input.max_value = tilt_max
	dimmer_value_input.min_value = dimmer_min
	dimmer_value_input.max_value = dimmer_max

func _get_selected_fixture_values() -> Dictionary:
	if _selected_fixture_uuid.is_empty():
		return MANUAL_DEFAULTS.duplicate(true)
	_ensure_fixture_manual_values(_selected_fixture_uuid)
	return _fixture_manual_values.get(_selected_fixture_uuid, MANUAL_DEFAULTS.duplicate(true))

func _store_selected_fixture_values(values: Dictionary) -> void:
	if _selected_fixture_uuid.is_empty():
		return
	_ensure_fixture_manual_values(_selected_fixture_uuid)
	_fixture_manual_values[_selected_fixture_uuid] = {
		"pan": float(values.get("pan", MANUAL_DEFAULTS["pan"])),
		"tilt": float(values.get("tilt", MANUAL_DEFAULTS["tilt"])),
		"dimmer": float(values.get("dimmer", MANUAL_DEFAULTS["dimmer"])),
	}

func _on_pan_slider_changed(value: float) -> void:
	if _updating_fixture_controls:
		return
	pan_value_input.value = value
	_update_selected_fixture_value("pan", value)

func _on_tilt_slider_changed(value: float) -> void:
	if _updating_fixture_controls:
		return
	tilt_value_input.value = value
	_update_selected_fixture_value("tilt", value)

func _on_dimmer_slider_changed(value: float) -> void:
	if _updating_fixture_controls:
		return
	dimmer_value_input.value = value
	_update_selected_fixture_value("dimmer", value)

func _on_pan_input_changed(value: float) -> void:
	if _updating_fixture_controls:
		return
	pan_slider.value = value

func _on_tilt_input_changed(value: float) -> void:
	if _updating_fixture_controls:
		return
	tilt_slider.value = value

func _on_dimmer_input_changed(value: float) -> void:
	if _updating_fixture_controls:
		return
	dimmer_slider.value = value

func _on_limit_changed(_value: float) -> void:
	_sync_controls_from_selected_fixture()

func _on_quick_reset_pressed() -> void:
	if _selected_fixture_uuid.is_empty() or not _scene_registry.has_fixture(_selected_fixture_uuid):
		return
	_store_selected_fixture_values(MANUAL_DEFAULTS)
	_sync_controls_from_selected_fixture()
	_apply_selected_fixture_controls("quick_reset")

func _update_selected_fixture_value(key: String, value: float) -> void:
	if _selected_fixture_uuid.is_empty() or not _scene_registry.has_fixture(_selected_fixture_uuid):
		return
	var values: Dictionary = _get_selected_fixture_values()
	values[key] = value
	_store_selected_fixture_values(values)
	_apply_selected_fixture_controls("control_%s" % key)

func _apply_selected_fixture_controls(reason: String) -> void:
	if _selected_fixture_uuid.is_empty() or not _scene_registry.has_fixture(_selected_fixture_uuid):
		return
	var values: Dictionary = _get_selected_fixture_values()
	var pan: float = float(values.get("pan", MANUAL_DEFAULTS["pan"]))
	var tilt: float = float(values.get("tilt", MANUAL_DEFAULTS["tilt"]))
	var dimmer: float = float(values.get("dimmer", MANUAL_DEFAULTS["dimmer"]))

	_apply_pan_tilt_to_fixture(_selected_fixture_uuid, pan, tilt)
	_apply_dimmer_feedback_to_fixture(_selected_fixture_uuid, dimmer)
	_print_emitter_debug_vectors(_selected_fixture_uuid, reason, pan, tilt, dimmer)

func _apply_pan_tilt_to_fixture(fixture_uuid: String, pan_degrees: float, tilt_degrees: float) -> void:
	var axis_nodes: Array = _to_node3d_array(_scene_registry.get_anchor(fixture_uuid, "axis"))
	var pan_axis: Node3D = _find_axis_for_role(axis_nodes, "pan")
	var tilt_axis: Node3D = _find_axis_for_role(axis_nodes, "tilt")
	if pan_axis != null:
		pan_axis.rotation_degrees.y = pan_degrees
	if tilt_axis != null:
		tilt_axis.rotation_degrees.x = tilt_degrees

func _find_axis_for_role(axis_nodes: Array, role: String) -> Node3D:
	if axis_nodes.is_empty():
		return null

	var role_name_hints: PackedStringArray = PackedStringArray([role])
	if role == "pan":
		role_name_hints.append("yoke")
	if role == "tilt":
		role_name_hints.append("head")

	for node in axis_nodes:
		if node == null:
			continue
		var normalized: String = node.name.to_lower()
		for hint in role_name_hints:
			if normalized.contains(hint):
				return node

	if role == "pan":
		return axis_nodes[0]
	if role == "tilt" and axis_nodes.size() > 1:
		return axis_nodes[1]
	return axis_nodes[0]

func _apply_dimmer_feedback_to_fixture(fixture_uuid: String, dimmer: float) -> void:
	var geometry_nodes: Array = _to_node3d_array(_scene_registry.get_anchor(fixture_uuid, "geometry_nodes"))
	var emitter_nodes: Array = _to_node3d_array(_scene_registry.get_anchor(fixture_uuid, "emitters"))
	if geometry_nodes.is_empty() and emitter_nodes.is_empty():
		return
	_apply_fixture_lens_visual_tuning(fixture_uuid, emitter_nodes)

	var dimmer_percent: float = clamp(dimmer, 0.0, 100.0)
	var normalized_dimmer: float = dimmer_percent / 100.0
	var emissive_materials: Array = _collect_fixture_emissive_materials(fixture_uuid, geometry_nodes)
	var energy_multiplier: float = lerp(0.0, 4.0, normalized_dimmer)
	for material in emissive_materials:
		if material is BaseMaterial3D:
			material.emission_enabled = true
			material.emission_energy_multiplier = energy_multiplier

	var emitter_lights: Array = _collect_fixture_emitter_lights(fixture_uuid, emitter_nodes)
	var emitter_photometrics: Array = _get_fixture_emitter_photometrics(fixture_uuid)
	for index in range(emitter_lights.size()):
		var light: SpotLight3D = emitter_lights[index]
		if light == null or not is_instance_valid(light):
			continue
		var photometric: Dictionary = DEFAULT_EMITTER_PHOTOMETRICS.duplicate(true)
		if index < emitter_photometrics.size() and emitter_photometrics[index] is Dictionary:
			photometric.merge(emitter_photometrics[index], true)
		_apply_emitter_light_state(light, photometric, normalized_dimmer)

func _collect_fixture_emissive_materials(fixture_uuid: String, geometry_nodes: Array) -> Array:
	if _fixture_emissive_cache.has(fixture_uuid):
		return _fixture_emissive_cache.get(fixture_uuid, [])

	var materials: Array = []
	for geometry in geometry_nodes:
		_collect_emissive_materials_recursive(geometry, materials)
	_fixture_emissive_cache[fixture_uuid] = materials
	return materials

func _collect_fixture_emitter_lights(fixture_uuid: String, emitter_nodes: Array) -> Array:
	if _fixture_emitter_light_cache.has(fixture_uuid):
		return _fixture_emitter_light_cache.get(fixture_uuid, [])

	var lights: Array = []
	for emitter_node in emitter_nodes:
		if emitter_node == null:
			continue
		var light: SpotLight3D = _find_or_create_emitter_light(emitter_node)
		if light != null:
			lights.append(light)

	_fixture_emitter_light_cache[fixture_uuid] = lights
	return lights

func _find_or_create_emitter_light(emitter_node: Node3D) -> SpotLight3D:
	var lens_radius: float = _estimate_emitter_lens_radius(emitter_node)
	for child in emitter_node.get_children():
		if child is SpotLight3D and child.name == "PeravizEmitterLight":
			if child.rotation_degrees != EMITTER_LIGHT_DIRECTION_FIX:
				child.rotation_degrees = EMITTER_LIGHT_DIRECTION_FIX
			child.set_meta("peraviz_lens_radius", lens_radius)
			return child

	var light := SpotLight3D.new()
	light.name = "PeravizEmitterLight"
	light.position = Vector3.ZERO
	light.rotation_degrees = EMITTER_LIGHT_DIRECTION_FIX
	light.light_negative = false
	light.shadow_enabled = true
	light.spot_range = 20.0
	light.spot_angle = 25.0
	light.spot_attenuation = 1.0
	light.set_meta("peraviz_beam_cone", _create_emitter_beam_cone())
	light.set_meta("peraviz_lens_radius", lens_radius)
	emitter_node.add_child(light)
	return light

func _estimate_emitter_lens_radius(emitter_node: Node3D) -> float:
	var default_radius: float = 0.03
	if emitter_node == null:
		return default_radius

	var hinted_radius: float = _estimate_emitter_lens_radius_from_root(emitter_node, true)
	if hinted_radius > 0.0:
		return hinted_radius

	# Some fixtures do not name the lens geometry explicitly; fallback to nearest
	# mesh around the emitter origin so we still keep beam base and lens width aligned.
	var fallback_radius: float = _estimate_emitter_lens_radius_from_root(emitter_node, false)
	if fallback_radius > 0.0:
		return fallback_radius

	if emitter_node.get_parent() is Node3D:
		var parent_node: Node3D = emitter_node.get_parent() as Node3D
		var parent_hinted_radius: float = _estimate_emitter_lens_radius_from_root(parent_node, true)
		if parent_hinted_radius > 0.0:
			return parent_hinted_radius
		var parent_fallback_radius: float = _estimate_emitter_lens_radius_from_root(parent_node, false)
		if parent_fallback_radius > 0.0:
			return parent_fallback_radius
	return default_radius

func _estimate_emitter_lens_radius_from_root(root: Node3D, require_name_hints: bool) -> float:
	if root == null:
		return -1.0
	var candidates: Array = []
	var world_to_root: Transform3D = root.global_transform.affine_inverse()
	_collect_emitter_lens_candidates_recursive(root, world_to_root, require_name_hints, candidates)
	if candidates.is_empty():
		return -1.0

	var best_score: float = INF
	var best_radius: float = -1.0
	for candidate in candidates:
		if not (candidate is Dictionary):
			continue
		var center: Vector3 = candidate.get("center", Vector3.ZERO)
		var radius: float = float(candidate.get("radius", -1.0))
		if radius < 0.005:
			continue
		var radial_distance: float = Vector2(center.x, center.y).length()
		var axial_distance: float = abs(center.z)
		var score: float = axial_distance * 1.5 + radial_distance
		if score < best_score or (is_equal_approx(score, best_score) and radius > best_radius):
			best_score = score
			best_radius = radius
	return max(best_radius, -1.0)

func _collect_emitter_lens_candidates_recursive(node: Node3D, world_to_root: Transform3D, require_name_hints: bool, output_candidates: Array) -> void:
	if node is MeshInstance3D:
		var mesh_instance: MeshInstance3D = node
		if (not require_name_hints) or _is_emitter_lens_mesh(mesh_instance):
			var mesh_bounds: AABB = mesh_instance.get_aabb()
			if mesh_bounds.size != Vector3.ZERO:
				var local_bounds: AABB = world_to_root * (mesh_instance.global_transform * mesh_bounds)
				var candidate_radius: float = max(local_bounds.size.x, local_bounds.size.y) * 0.5
				if candidate_radius > 0.0001:
					output_candidates.append({
						"center": local_bounds.get_center(),
						"radius": max(candidate_radius, 0.001)
					})

	for child in node.get_children():
		if child is Node3D:
			_collect_emitter_lens_candidates_recursive(child, world_to_root, require_name_hints, output_candidates)

func _is_emitter_lens_mesh(mesh_instance: MeshInstance3D) -> bool:
	var name_hint: String = mesh_instance.name.to_lower()
	return name_hint.contains("lens") or name_hint.contains("emitter") or name_hint.contains("beam")

func _get_fixture_emitter_photometrics(fixture_uuid: String) -> Array:
	if _fixture_emitter_photometrics.has(fixture_uuid):
		return _fixture_emitter_photometrics.get(fixture_uuid, [])
	var entries: Variant = _scene_registry.get_anchor(fixture_uuid, "emitter_photometrics")
	if entries is Array:
		_fixture_emitter_photometrics[fixture_uuid] = entries
		return entries
	var empty: Array = []
	_fixture_emitter_photometrics[fixture_uuid] = empty
	return empty

func _extract_emitter_photometrics(item: Dictionary) -> Dictionary:
	var data: Dictionary = DEFAULT_EMITTER_PHOTOMETRICS.duplicate(true)
	if bool(item.get("has_luminous_flux", false)):
		data["luminous_flux"] = float(item.get("luminous_flux", data["luminous_flux"]))
	if bool(item.get("has_color_temperature", false)):
		data["color_temperature"] = float(item.get("color_temperature", data["color_temperature"]))
	if bool(item.get("has_beam_angle", false)):
		data["beam_angle"] = float(item.get("beam_angle", data["beam_angle"]))
	if bool(item.get("has_field_angle", false)):
		data["field_angle"] = float(item.get("field_angle", data["field_angle"]))
	if bool(item.get("has_beam_radius", false)):
		data["beam_radius"] = float(item.get("beam_radius", data["beam_radius"]))
	if bool(item.get("has_dominant_wavelength", false)):
		data["dominant_wavelength"] = float(item.get("dominant_wavelength", 0.0))
	return data

func _apply_emitter_light_state(light: SpotLight3D, photometric: Dictionary, normalized_dimmer: float) -> void:
	var luminous_flux: float = max(float(photometric.get("luminous_flux", 10000.0)), 0.0)
	var beam_angle: float = clamp(float(photometric.get("beam_angle", 25.0)), 0.1, 179.0)
	var field_angle: float = clamp(float(photometric.get("field_angle", beam_angle)), beam_angle, 179.0)
	var beam_radius_m: float = max(float(photometric.get("beam_radius", 0.05)), 0.001)

	light.visible = normalized_dimmer > 0.0001
	light.light_energy = luminous_flux * normalized_dimmer
	light.spot_angle = beam_angle
	light.spot_attenuation = clamp(beam_angle / field_angle, 0.2, 1.0)
	light.spot_range = clamp(beam_radius_m * 400.0, 0.5, 60.0)
	light.light_color = _derive_emitter_color(photometric)
	_update_emitter_beam_cone(light, beam_angle, light.spot_range, light.light_color, normalized_dimmer)

func _create_emitter_beam_cone() -> MeshInstance3D:
	var cone := MeshInstance3D.new()
	cone.name = "PeravizBeamCone"
	cone.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	cone.visible = false
	var cone_mesh := CylinderMesh.new()
	cone_mesh.top_radius = 0.01
	cone_mesh.bottom_radius = 0.2
	cone_mesh.height = 1.0
	cone.mesh = cone_mesh
	cone.rotation_degrees.x = 90.0

	var cone_material := StandardMaterial3D.new()
	cone_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	cone_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	cone_material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	cone_material.no_depth_test = true
	cone_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	cone_material.albedo_color = Color(1.0, 0.9, 0.7, 0.08)
	cone_material.emission_enabled = true
	cone_material.emission = Color(1.0, 0.85, 0.55)
	cone_material.emission_energy_multiplier = 0.7
	cone.material_override = cone_material
	return cone

func _update_emitter_beam_cone(light: SpotLight3D, beam_angle: float, beam_range: float, beam_color: Color, normalized_dimmer: float) -> void:
	if not light.has_meta("peraviz_beam_cone"):
		light.set_meta("peraviz_beam_cone", _create_emitter_beam_cone())
	var cone: MeshInstance3D = light.get_meta("peraviz_beam_cone") as MeshInstance3D
	if cone == null:
		return
	if cone.get_parent() == null:
		light.add_child(cone)

	var intensity: float = clamp(normalized_dimmer, 0.0, 1.0)
	cone.visible = intensity > 0.015
	var cone_mesh: CylinderMesh = cone.mesh as CylinderMesh
	if cone_mesh != null:
		var radius: float = tan(deg_to_rad(beam_angle * 0.5)) * beam_range
		var lens_radius: float = max(float(light.get_meta("peraviz_lens_radius", 0.03)), 0.005)
		cone_mesh.top_radius = lens_radius
		cone_mesh.bottom_radius = max(radius, 0.03)
		cone_mesh.height = beam_range
	cone.position = Vector3(0.0, 0.0, -beam_range * 0.5)

	var material: StandardMaterial3D = cone.material_override as StandardMaterial3D
	if material != null:
		material.albedo_color = Color(beam_color.r, beam_color.g, beam_color.b, lerp(0.01, 0.12, intensity))
		material.emission = beam_color
		material.emission_energy_multiplier = lerp(0.1, 1.1, intensity)

func _apply_fixture_lens_visual_tuning(fixture_uuid: String, emitter_nodes: Array) -> void:
	if _fixture_lens_tuned.get(fixture_uuid, false):
		return
	for emitter_node in emitter_nodes:
		_tint_emitter_lens_recursive(emitter_node)
	_fixture_lens_tuned[fixture_uuid] = true

func _tint_emitter_lens_recursive(node: Node3D) -> void:
	if node == null:
		return
	if node is MeshInstance3D:
		var mesh_instance: MeshInstance3D = node
		if mesh_instance.mesh != null:
			for surface_index in range(mesh_instance.mesh.get_surface_count()):
				var material: Material = mesh_instance.get_active_material(surface_index)
				if _should_apply_lens_tint(mesh_instance, material):
					mesh_instance.set_surface_override_material(surface_index, _build_lens_material_override(material))
	for child in node.get_children():
		if child is Node3D:
			_tint_emitter_lens_recursive(child)

func _should_apply_lens_tint(mesh_instance: MeshInstance3D, material: Material) -> bool:
	var name_hint: String = mesh_instance.name.to_lower()
	if name_hint.contains("lens") or name_hint.contains("emitter") or name_hint.contains("beam"):
		return true
	if material is BaseMaterial3D:
		var base_material: BaseMaterial3D = material
		if base_material.transparency != BaseMaterial3D.TRANSPARENCY_DISABLED:
			return true
		if base_material.emission_enabled:
			return true
	return false

func _build_lens_material_override(source: Material) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	if source is BaseMaterial3D:
		var base_source: BaseMaterial3D = source
		material.albedo_color = base_source.albedo_color.lerp(LENS_TINT_COLOR, 0.65)
	else:
		material.albedo_color = LENS_TINT_COLOR
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.roughness = 0.08
	material.metallic = 0.0
	material.clearcoat_enabled = true
	material.clearcoat = 0.75
	material.specular = 1.0
	material.emission_enabled = true
	material.emission = material.albedo_color
	material.emission_energy_multiplier = 0.35
	return material

func _derive_emitter_color(photometric: Dictionary) -> Color:
	var wavelength: float = float(photometric.get("dominant_wavelength", 0.0))
	if wavelength > 0.0:
		return _wavelength_to_rgb(wavelength)
	var temperature_kelvin: float = clamp(float(photometric.get("color_temperature", 6000.0)), 1000.0, 40000.0)
	return _color_temperature_to_rgb(temperature_kelvin)

func _color_temperature_to_rgb(temperature_kelvin: float) -> Color:
	var t: float = temperature_kelvin / 100.0
	var red: float
	var green: float
	var blue: float
	if t <= 66.0:
		red = 255.0
		green = 99.4708025861 * log(t) - 161.1195681661
		if t <= 19.0:
			blue = 0.0
		else:
			blue = 138.5177312231 * log(t - 10.0) - 305.0447927307
	else:
		red = 329.698727446 * pow(t - 60.0, -0.1332047592)
		green = 288.1221695283 * pow(t - 60.0, -0.0755148492)
		blue = 255.0
	return Color(clamp(red / 255.0, 0.0, 1.0), clamp(green / 255.0, 0.0, 1.0), clamp(blue / 255.0, 0.0, 1.0))

func _wavelength_to_rgb(wavelength_nm: float) -> Color:
	var wavelength: float = clamp(wavelength_nm, 380.0, 780.0)
	var red: float = 0.0
	var green: float = 0.0
	var blue: float = 0.0
	if wavelength < 440.0:
		red = -(wavelength - 440.0) / (440.0 - 380.0)
		blue = 1.0
	elif wavelength < 490.0:
		green = (wavelength - 440.0) / (490.0 - 440.0)
		blue = 1.0
	elif wavelength < 510.0:
		green = 1.0
		blue = -(wavelength - 510.0) / (510.0 - 490.0)
	elif wavelength < 580.0:
		red = (wavelength - 510.0) / (580.0 - 510.0)
		green = 1.0
	elif wavelength < 645.0:
		red = 1.0
		green = -(wavelength - 645.0) / (645.0 - 580.0)
	else:
		red = 1.0

	var factor: float = 1.0
	if wavelength < 420.0:
		factor = 0.3 + 0.7 * ((wavelength - 380.0) / (420.0 - 380.0))
	elif wavelength > 700.0:
		factor = 0.3 + 0.7 * ((780.0 - wavelength) / (780.0 - 700.0))
	return Color(clamp(red * factor, 0.0, 1.0), clamp(green * factor, 0.0, 1.0), clamp(blue * factor, 0.0, 1.0))

func _collect_emissive_materials_recursive(node: Node3D, output_materials: Array) -> void:
	if node == null:
		return
	if node is MeshInstance3D:
		var mesh_instance: MeshInstance3D = node
		for surface_index in range(mesh_instance.get_surface_override_material_count()):
			var override_material: Material = mesh_instance.get_surface_override_material(surface_index)
			_maybe_add_emissive_material(override_material, output_materials)
		if mesh_instance.material_override != null:
			_maybe_add_emissive_material(mesh_instance.material_override, output_materials)
		if mesh_instance.mesh != null:
			for surface_index in range(mesh_instance.mesh.get_surface_count()):
				var mesh_material: Material = mesh_instance.mesh.surface_get_material(surface_index)
				_maybe_add_emissive_material(mesh_material, output_materials)

	for child in node.get_children():
		if child is Node3D:
			_collect_emissive_materials_recursive(child, output_materials)

func _maybe_add_emissive_material(material: Material, output_materials: Array) -> void:
	if material == null:
		return
	if not (material is BaseMaterial3D):
		return
	var base_material: BaseMaterial3D = material
	if output_materials.has(base_material):
		return
	if base_material.emission_enabled or base_material.emission != Color.BLACK:
		output_materials.append(base_material)

func _print_emitter_debug_vectors(fixture_uuid: String, reason: String, pan: float, tilt: float, dimmer: float) -> void:
	var emitter_nodes: Array = _to_node3d_array(_scene_registry.get_anchor(fixture_uuid, "emitters"))
	for emitter in emitter_nodes:
		var emitter_direction: Vector3 = (-emitter.global_basis.z).normalized()
		print("[PeravizFixtureTest] fixture=", fixture_uuid,
			" reason=", reason,
			" emitter=", emitter.name,
			" dir=", emitter_direction,
			" pan=", pan,
			" tilt=", tilt,
			" dimmer=", dimmer)

func _to_node3d_array(value: Variant) -> Array:
	var nodes: Array = []
	if value is Node3D:
		nodes.append(value)
		return nodes
	if value is Array:
		for item in value:
			if item is Node3D and is_instance_valid(item):
				nodes.append(item)
	return nodes

func _count_valid_nodes(value: Variant) -> int:
	if value is Node:
		return 1 if is_instance_valid(value) else 0
	if value is Array:
		var total: int = 0
		for item in value:
			total += _count_valid_nodes(item)
		return total
	return 0

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
