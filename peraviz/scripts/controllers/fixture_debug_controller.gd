extends RefCounted
class_name FixtureDebugController

var _owner: Node3D
var _scene_registry: SceneRegistry
var _camera: Camera3D

var _manual_fixture_toggle: CheckButton
var _fixture_debug_panel: PanelContainer
var _fixture_list: ItemList
var _fixture_selected_label: Label
var _fixture_axis_label: Label
var _fixture_emitter_label: Label
var _pan_min_input: SpinBox
var _pan_max_input: SpinBox
var _pan_value_input: SpinBox
var _tilt_min_input: SpinBox
var _tilt_max_input: SpinBox
var _tilt_value_input: SpinBox
var _dimmer_min_input: SpinBox
var _dimmer_max_input: SpinBox
var _dimmer_value_input: SpinBox
var _pan_slider: HSlider
var _tilt_slider: HSlider
var _dimmer_slider: HSlider
var _quick_reset_button: Button

var _resolve_fixture_uuid_from_node_callback: Callable
var _apply_fixture_controls_callback: Callable

var _manual_fixture_test_enabled: bool = false
var _selected_fixture_uuid: String = ""
var _fixture_manual_values: Dictionary = {}
var _updating_fixture_controls: bool = false

const DIMMER_PERCENT_MIN: float = 0.0
const DIMMER_PERCENT_MAX: float = 100.0
const MANUAL_DEFAULTS := {
	"pan": 0.0,
	"tilt": 0.0,
	"dimmer": 100.0,
}

func configure(owner: Node3D,
		scene_registry: SceneRegistry,
		camera: Camera3D,
		ui_refs: Dictionary,
		resolve_fixture_uuid_from_node_callback: Callable,
		apply_fixture_controls_callback: Callable) -> void:
	_owner = owner
	_scene_registry = scene_registry
	_camera = camera
	_resolve_fixture_uuid_from_node_callback = resolve_fixture_uuid_from_node_callback
	_apply_fixture_controls_callback = apply_fixture_controls_callback

	_manual_fixture_toggle = ui_refs.get("manual_fixture_toggle")
	_fixture_debug_panel = ui_refs.get("fixture_debug_panel")
	_fixture_list = ui_refs.get("fixture_list")
	_fixture_selected_label = ui_refs.get("fixture_selected_label")
	_fixture_axis_label = ui_refs.get("fixture_axis_label")
	_fixture_emitter_label = ui_refs.get("fixture_emitter_label")
	_pan_min_input = ui_refs.get("pan_min_input")
	_pan_max_input = ui_refs.get("pan_max_input")
	_pan_value_input = ui_refs.get("pan_value_input")
	_tilt_min_input = ui_refs.get("tilt_min_input")
	_tilt_max_input = ui_refs.get("tilt_max_input")
	_tilt_value_input = ui_refs.get("tilt_value_input")
	_dimmer_min_input = ui_refs.get("dimmer_min_input")
	_dimmer_max_input = ui_refs.get("dimmer_max_input")
	_dimmer_value_input = ui_refs.get("dimmer_value_input")
	_pan_slider = ui_refs.get("pan_slider")
	_tilt_slider = ui_refs.get("tilt_slider")
	_dimmer_slider = ui_refs.get("dimmer_slider")
	_quick_reset_button = ui_refs.get("quick_reset_button")

	if _fixture_list != null and not _fixture_list.item_selected.is_connected(_on_fixture_list_item_selected):
		_fixture_list.item_selected.connect(_on_fixture_list_item_selected)
	if _pan_slider != null and not _pan_slider.value_changed.is_connected(_on_pan_slider_changed):
		_pan_slider.value_changed.connect(_on_pan_slider_changed)
	if _tilt_slider != null and not _tilt_slider.value_changed.is_connected(_on_tilt_slider_changed):
		_tilt_slider.value_changed.connect(_on_tilt_slider_changed)
	if _dimmer_slider != null and not _dimmer_slider.value_changed.is_connected(_on_dimmer_slider_changed):
		_dimmer_slider.value_changed.connect(_on_dimmer_slider_changed)
	if _pan_value_input != null and not _pan_value_input.value_changed.is_connected(_on_pan_input_changed):
		_pan_value_input.value_changed.connect(_on_pan_input_changed)
	if _tilt_value_input != null and not _tilt_value_input.value_changed.is_connected(_on_tilt_input_changed):
		_tilt_value_input.value_changed.connect(_on_tilt_input_changed)
	if _dimmer_value_input != null and not _dimmer_value_input.value_changed.is_connected(_on_dimmer_input_changed):
		_dimmer_value_input.value_changed.connect(_on_dimmer_input_changed)
	if _pan_min_input != null and not _pan_min_input.value_changed.is_connected(_on_limit_changed):
		_pan_min_input.value_changed.connect(_on_limit_changed)
	if _pan_max_input != null and not _pan_max_input.value_changed.is_connected(_on_limit_changed):
		_pan_max_input.value_changed.connect(_on_limit_changed)
	if _tilt_min_input != null and not _tilt_min_input.value_changed.is_connected(_on_limit_changed):
		_tilt_min_input.value_changed.connect(_on_limit_changed)
	if _tilt_max_input != null and not _tilt_max_input.value_changed.is_connected(_on_limit_changed):
		_tilt_max_input.value_changed.connect(_on_limit_changed)
	if _dimmer_min_input != null and not _dimmer_min_input.value_changed.is_connected(_on_limit_changed):
		_dimmer_min_input.value_changed.connect(_on_limit_changed)
	if _dimmer_max_input != null and not _dimmer_max_input.value_changed.is_connected(_on_limit_changed):
		_dimmer_max_input.value_changed.connect(_on_limit_changed)
	if _quick_reset_button != null and not _quick_reset_button.pressed.is_connected(_on_quick_reset_pressed):
		_quick_reset_button.pressed.connect(_on_quick_reset_pressed)

func set_manual_fixture_test_enabled(enabled: bool) -> void:
	_manual_fixture_test_enabled = enabled
	refresh_fixture_debug_panel()

func get_selected_fixture_uuid() -> String:
	return _selected_fixture_uuid

func clear_selected_fixture(reason: String) -> void:
	if _selected_fixture_uuid.is_empty():
		refresh_fixture_debug_panel()
		return
	print("[PeravizFixtureTest] clear selection uuid=", _selected_fixture_uuid, " reason=", reason)
	_selected_fixture_uuid = ""
	_fixture_list.deselect_all()
	refresh_fixture_debug_panel()

func populate_fixture_list() -> void:
	_fixture_list.clear()
	for fixture_uuid in _scene_registry.list_fixture_uuids():
		var index: int = _fixture_list.get_item_count()
		_fixture_list.add_item(fixture_uuid)
		_fixture_list.set_item_metadata(index, fixture_uuid)
	if not _selected_fixture_uuid.is_empty():
		var selected_index: int = _find_fixture_list_index(_selected_fixture_uuid)
		if selected_index >= 0:
			_fixture_list.select(selected_index)

func sync_selection_state(reason: String) -> void:
	if _selected_fixture_uuid.is_empty():
		refresh_fixture_debug_panel()
		return
	if _scene_registry.has_fixture(_selected_fixture_uuid):
		refresh_fixture_debug_panel()
		return
	clear_selected_fixture(reason)

func try_select_fixture_from_mouse(mouse_position: Vector2) -> void:
	var world_3d: World3D = _owner.get_world_3d()
	if world_3d == null:
		return

	var query := PhysicsRayQueryParameters3D.create(
		_camera.project_ray_origin(mouse_position),
		_camera.project_ray_origin(mouse_position) + _camera.project_ray_normal(mouse_position) * 1000.0
	)
	query.collide_with_areas = false
	query.collide_with_bodies = true
	var result: Dictionary = world_3d.direct_space_state.intersect_ray(query)
	if result.is_empty():
		return

	var collider: Object = result.get("collider")
	if collider == null or not (collider is Node):
		return

	var fixture_uuid: String = ""
	if _resolve_fixture_uuid_from_node_callback.is_valid():
		fixture_uuid = str(_resolve_fixture_uuid_from_node_callback.call(collider as Node))
	if fixture_uuid.is_empty():
		return
	_select_fixture_by_uuid(fixture_uuid, "raycast")

func refresh_fixture_debug_panel() -> void:
	_manual_fixture_toggle.button_pressed = _manual_fixture_test_enabled
	_fixture_debug_panel.visible = _manual_fixture_test_enabled
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

	_fixture_selected_label.text = selected_text
	_fixture_axis_label.text = axis_text
	_fixture_emitter_label.text = emitter_text
	_sync_controls_from_selected_fixture()

func _on_fixture_list_item_selected(index: int) -> void:
	if index < 0 or index >= _fixture_list.get_item_count():
		return
	var fixture_uuid: String = _fixture_list.get_item_metadata(index)
	_select_fixture_by_uuid(fixture_uuid, "list")

func _select_fixture_by_uuid(fixture_uuid: String, source: String) -> void:
	if fixture_uuid.is_empty() or not _scene_registry.has_fixture(fixture_uuid):
		return
	_selected_fixture_uuid = fixture_uuid
	_ensure_fixture_manual_values(fixture_uuid)
	var selected_index: int = _find_fixture_list_index(fixture_uuid)
	if selected_index >= 0:
		_fixture_list.select(selected_index)
	print("[PeravizFixtureTest] selected uuid=", fixture_uuid, " source=", source)
	_apply_selected_fixture_controls("select")
	refresh_fixture_debug_panel()

func _find_fixture_list_index(fixture_uuid: String) -> int:
	for index in range(_fixture_list.get_item_count()):
		var list_uuid: String = _fixture_list.get_item_metadata(index)
		if list_uuid == fixture_uuid:
			return index
	return -1

func _ensure_fixture_manual_values(fixture_uuid: String) -> void:
	if fixture_uuid.is_empty() or _fixture_manual_values.has(fixture_uuid):
		return
	_fixture_manual_values[fixture_uuid] = MANUAL_DEFAULTS.duplicate(true)

func _sync_controls_from_selected_fixture() -> void:
	if not _manual_fixture_test_enabled:
		return
	_sync_slider_ranges_from_limits()
	var values: Dictionary = _get_selected_fixture_values()
	_updating_fixture_controls = true
	_pan_slider.value = float(values.get("pan", MANUAL_DEFAULTS["pan"]))
	_tilt_slider.value = float(values.get("tilt", MANUAL_DEFAULTS["tilt"]))
	_dimmer_slider.value = float(values.get("dimmer", MANUAL_DEFAULTS["dimmer"]))
	_pan_value_input.value = _pan_slider.value
	_tilt_value_input.value = _tilt_slider.value
	_dimmer_value_input.value = _dimmer_slider.value
	var controls_enabled: bool = not _selected_fixture_uuid.is_empty() and _scene_registry.has_fixture(_selected_fixture_uuid)
	_pan_slider.editable = controls_enabled
	_tilt_slider.editable = controls_enabled
	_dimmer_slider.editable = controls_enabled
	_pan_value_input.editable = controls_enabled
	_tilt_value_input.editable = controls_enabled
	_dimmer_value_input.editable = controls_enabled
	_quick_reset_button.disabled = not controls_enabled
	_updating_fixture_controls = false

func _sync_slider_ranges_from_limits() -> void:
	var pan_min: float = min(_pan_min_input.value, _pan_max_input.value)
	var pan_max: float = max(_pan_min_input.value, _pan_max_input.value)
	var tilt_min: float = min(_tilt_min_input.value, _tilt_max_input.value)
	var tilt_max: float = max(_tilt_min_input.value, _tilt_max_input.value)
	_dimmer_min_input.value = DIMMER_PERCENT_MIN
	_dimmer_max_input.value = DIMMER_PERCENT_MAX

	_pan_slider.min_value = pan_min
	_pan_slider.max_value = pan_max
	_tilt_slider.min_value = tilt_min
	_tilt_slider.max_value = tilt_max
	_dimmer_slider.min_value = DIMMER_PERCENT_MIN
	_dimmer_slider.max_value = DIMMER_PERCENT_MAX

	_pan_value_input.min_value = pan_min
	_pan_value_input.max_value = pan_max
	_tilt_value_input.min_value = tilt_min
	_tilt_value_input.max_value = tilt_max
	_dimmer_value_input.min_value = DIMMER_PERCENT_MIN
	_dimmer_value_input.max_value = DIMMER_PERCENT_MAX

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
		"dimmer": clamp(float(values.get("dimmer", MANUAL_DEFAULTS["dimmer"])), DIMMER_PERCENT_MIN, DIMMER_PERCENT_MAX),
	}

func _on_pan_slider_changed(value: float) -> void:
	if _updating_fixture_controls:
		return
	_pan_value_input.value = value
	_update_selected_fixture_value("pan", value)

func _on_tilt_slider_changed(value: float) -> void:
	if _updating_fixture_controls:
		return
	_tilt_value_input.value = value
	_update_selected_fixture_value("tilt", value)

func _on_dimmer_slider_changed(value: float) -> void:
	if _updating_fixture_controls:
		return
	_dimmer_value_input.value = value
	_update_selected_fixture_value("dimmer", value)

func _on_pan_input_changed(value: float) -> void:
	if _updating_fixture_controls:
		return
	_pan_slider.value = value

func _on_tilt_input_changed(value: float) -> void:
	if _updating_fixture_controls:
		return
	_tilt_slider.value = value

func _on_dimmer_input_changed(value: float) -> void:
	if _updating_fixture_controls:
		return
	_dimmer_slider.value = value

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
	if _apply_fixture_controls_callback.is_valid():
		_apply_fixture_controls_callback.call(_selected_fixture_uuid, values, reason)

func _count_valid_nodes(nodes_variant: Variant) -> int:
	if nodes_variant is Array:
		var total: int = 0
		for node in nodes_variant:
			if node is Node and is_instance_valid(node):
				total += 1
		return total
	if nodes_variant is Node and is_instance_valid(nodes_variant):
		return 1
	return 0
