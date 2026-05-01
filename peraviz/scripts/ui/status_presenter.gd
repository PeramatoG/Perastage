extends RefCounted
class_name StatusPresenter

enum PresenterState {
	IDLE,
	LOADING,
	LOADED,
	WARNING,
}

const TOAST_DURATION_SEC: float = 2.6

var _status_label: Label
var _topbar_row: HBoxContainer
var _badge_row: HBoxContainer
var _badge_labels: Dictionary = {}
var _badge_values: Dictionary = {}
var _toast_label: Label
var _toast_timer: Timer
var _last_dmx_running: bool = false
var _last_dmx_receiving_signal: bool = false
var _has_dmx_state: bool = false

func configure(owner: Node,
		status_label: Label,
		topbar_row: HBoxContainer,
		load_button: Button,
		show_advanced_controls_toggle: CheckButton) -> void:
	_status_label = status_label
	_topbar_row = topbar_row
	_ensure_badges(load_button, show_advanced_controls_toggle)
	_ensure_toast(owner)
	set_scene_state_idle()
	set_dmx_badge(false, false)
	set_selected_fixture_badge("")
	set_render_mode_badge("Volumetric")

func set_scene_state_idle() -> void:
	_set_status(PresenterState.IDLE, "Idle · Load MVR")
	_set_badge("MVR", "Idle")

func set_scene_state_loading(path: String) -> void:
	_set_status(PresenterState.LOADING, "Loading · %s" % path.get_file())
	_set_badge("MVR", "Loading")

func set_scene_state_loaded(node_count: int) -> void:
	_set_status(PresenterState.LOADED, "Loaded · %d nodes" % node_count)
	_set_badge("MVR", "Loaded")

func set_scene_state_warning_unbound(unbound_count: int) -> void:
	_set_status(PresenterState.WARNING, "Warning · %d fixtures unbound" % unbound_count)
	_set_badge("MVR", "Warning")

func set_scene_state_load_error(message: String) -> void:
	_set_status(PresenterState.WARNING, "Load error")
	show_toast("Load failed: %s" % message)
	_set_badge("MVR", "Error")

func set_dmx_badge(is_running: bool, receiving_signal: bool) -> void:
	if _has_dmx_state and is_running == _last_dmx_running and receiving_signal == _last_dmx_receiving_signal:
		return
	_last_dmx_running = is_running
	_last_dmx_receiving_signal = receiving_signal
	_has_dmx_state = true
	if not is_running:
		_set_badge("DMX", "OFF")
		return
	_set_badge("DMX", "Connected" if receiving_signal else "No signal")

func set_selected_fixture_badge(fixture_uuid: String) -> void:
	_set_badge("Selected Fixture", "None" if fixture_uuid.is_empty() else fixture_uuid)

func set_render_mode_badge(render_mode_label: String) -> void:
	_set_badge("Render Mode", render_mode_label)

func show_toast(message: String) -> void:
	if _toast_label == null:
		return
	_toast_label.text = message
	_toast_label.visible = true
	if _toast_timer != null:
		_toast_timer.start(TOAST_DURATION_SEC)

func _set_status(state: int, text: String) -> void:
	if _status_label == null:
		return
	_status_label.text = text
	match state:
		PresenterState.IDLE:
			_status_label.modulate = Color(0.8, 0.82, 0.86)
		PresenterState.LOADING:
			_status_label.modulate = Color(0.95, 0.85, 0.45)
		PresenterState.LOADED:
			_status_label.modulate = Color(0.7, 0.95, 0.7)
		PresenterState.WARNING:
			_status_label.modulate = Color(1.0, 0.75, 0.45)

func _ensure_badges(load_button: Button, show_advanced_controls_toggle: CheckButton) -> void:
	if _topbar_row == null:
		return
	_badge_row = HBoxContainer.new()
	_badge_row.name = "StatusBadges"
	_badge_row.alignment = BoxContainer.ALIGNMENT_END
	_badge_row.add_theme_constant_override("separation", 6)

	var top_spacer: Control = _topbar_row.get_node_or_null("TopSpacer") as Control
	if top_spacer != null:
		_topbar_row.move_child(top_spacer, _topbar_row.get_child_count() - 1)
	if _status_label != null:
		_topbar_row.move_child(_status_label, _topbar_row.get_child_count() - 1)

	if _status_label != null:
		_topbar_row.add_child(_badge_row)
		_topbar_row.move_child(_badge_row, _status_label.get_index())
	else:
		_topbar_row.add_child(_badge_row)

	_create_badge("MVR")
	_create_badge("DMX")
	_create_badge("Selected Fixture")
	_create_badge("Render Mode")

	if load_button != null:
		load_button.tooltip_text = "Load an MVR file"
	if show_advanced_controls_toggle != null:
		show_advanced_controls_toggle.tooltip_text = "Toggle advanced controls"

func _create_badge(name: String) -> void:
	if _badge_row == null:
		return
	var badge := Label.new()
	badge.name = "%sBadge" % name.replace(" ", "")
	badge.text = "%s: --" % name
	badge.clip_text = true
	badge.custom_minimum_size = Vector2(112, 0)
	badge.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	badge.add_theme_font_size_override("font_size", 11)
	badge.modulate = Color(0.83, 0.86, 0.9)
	_badge_row.add_child(badge)
	_badge_labels[name] = badge

func _set_badge(name: String, value: String) -> void:
	if not _badge_labels.has(name):
		return
	if str(_badge_values.get(name, "")) == value:
		return
	_badge_values[name] = value
	var badge: Label = _badge_labels[name]
	badge.text = "%s: %s" % [name, value]

func _ensure_toast(owner: Node) -> void:
	if owner == null:
		return
	var root_control: Control = owner.get_node_or_null("UIRoot") as Control
	if root_control == null:
		return
	_toast_label = Label.new()
	_toast_label.name = "ToastLabel"
	_toast_label.visible = false
	_toast_label.z_index = 99
	_toast_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_toast_label.anchor_left = 1.0
	_toast_label.anchor_top = 0.0
	_toast_label.anchor_right = 1.0
	_toast_label.anchor_bottom = 0.0
	_toast_label.offset_left = -360.0
	_toast_label.offset_right = -16.0
	_toast_label.offset_top = 44.0
	_toast_label.offset_bottom = 74.0
	_toast_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_toast_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_toast_label.modulate = Color(1.0, 0.85, 0.62)
	_toast_label.add_theme_font_size_override("font_size", 12)
	root_control.add_child(_toast_label)

	_toast_timer = Timer.new()
	_toast_timer.one_shot = true
	_toast_timer.timeout.connect(_on_toast_timeout)
	owner.add_child(_toast_timer)

func _on_toast_timeout() -> void:
	if _toast_label != null:
		_toast_label.visible = false
