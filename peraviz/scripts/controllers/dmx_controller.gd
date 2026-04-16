extends RefCounted
class_name DmxController

const DmxMonitorWindowScript = preload("res://scripts/dmx_monitor_window.gd")
const DmxFixtureRuntimeScript = preload("res://scripts/dmx_fixture_runtime.gd")

var _owner: Node
var _get_controls_host_callback: Callable
var _apply_dmx_controls_callback: Callable

var _dmx_receiver = null
var _dmx_toggle_button: Button
var _dmx_monitor_button: Button
var _dmx_monitor_window: Window
var _dmx_timer: Timer
var _dmx_universe_offset_input: SpinBox
var _dmx_unbound_preview_label: Label
var _dmx_controls_panel: PanelContainer
var _dmx_fixture_runtime = null
var _last_dmx_tick_msec: int = 0

func configure(owner: Node, get_controls_host_callback: Callable, apply_dmx_controls_callback: Callable) -> void:
	_owner = owner
	_get_controls_host_callback = get_controls_host_callback
	_apply_dmx_controls_callback = apply_dmx_controls_callback

func setup_controls() -> void:
	if _dmx_toggle_button != null and is_instance_valid(_dmx_toggle_button):
		return
	var controls_host: Control = resolve_controls_host()
	if controls_host == null:
		return
	_dmx_controls_panel = PanelContainer.new()
	_dmx_controls_panel.name = "DMXControlsPanel"
	controls_host.add_child(_dmx_controls_panel)
	var controls_margin := MarginContainer.new()
	controls_margin.add_theme_constant_override("margin_left", 8)
	controls_margin.add_theme_constant_override("margin_top", 8)
	controls_margin.add_theme_constant_override("margin_right", 8)
	controls_margin.add_theme_constant_override("margin_bottom", 8)
	_dmx_controls_panel.add_child(controls_margin)
	var controls_vbox := VBoxContainer.new()
	controls_margin.add_child(controls_vbox)
	var controls_header := Label.new()
	controls_header.text = "DMX"
	controls_vbox.add_child(controls_header)
	var controls_row := HBoxContainer.new()
	controls_vbox.add_child(controls_row)
	_dmx_toggle_button = Button.new()
	_dmx_toggle_button.text = "DMX OFF"
	_dmx_toggle_button.toggle_mode = true
	controls_row.add_child(_dmx_toggle_button)
	_dmx_toggle_button.pressed.connect(_on_dmx_toggle_pressed)
	_update_dmx_toggle_color(false, false)

	_dmx_monitor_button = Button.new()
	_dmx_monitor_button.text = "DMX Monitor"
	_dmx_monitor_button.disabled = true
	controls_row.add_child(_dmx_monitor_button)
	_dmx_monitor_button.pressed.connect(_on_dmx_monitor_pressed)

	_dmx_universe_offset_input = SpinBox.new()
	_dmx_universe_offset_input.custom_minimum_size = Vector2(90, 24)
	_dmx_universe_offset_input.min_value = -32
	_dmx_universe_offset_input.max_value = 32
	_dmx_universe_offset_input.step = 1
	_dmx_universe_offset_input.value = -1
	controls_row.add_child(_dmx_universe_offset_input)
	_dmx_universe_offset_input.value_changed.connect(_on_dmx_universe_offset_changed)

	_dmx_unbound_preview_label = Label.new()
	_dmx_unbound_preview_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_dmx_unbound_preview_label.visible = false
	controls_vbox.add_child(_dmx_unbound_preview_label)

	_dmx_timer = Timer.new()
	_dmx_timer.wait_time = 0.03
	_dmx_timer.autostart = true
	_owner.add_child(_dmx_timer)
	_dmx_timer.timeout.connect(_on_dmx_timer_timeout)

	if ClassDB.class_exists("PeravizDmxReceiver"):
		_dmx_receiver = ClassDB.instantiate("PeravizDmxReceiver")
		_dmx_monitor_button.disabled = false
	else:
		_dmx_toggle_button.disabled = true
		_dmx_toggle_button.tooltip_text = "DMX unavailable (build without PERAVIZ_ENABLE_DMX)"

func setup_fixture_runtime(loader: Variant, scene_registry: SceneRegistry) -> void:
	_dmx_fixture_runtime = DmxFixtureRuntimeScript.new()
	_dmx_fixture_runtime.configure(loader, scene_registry)

func refresh_fixture_bindings() -> void:
	if _dmx_fixture_runtime == null or _dmx_universe_offset_input == null:
		return
	var summary: Dictionary = _dmx_fixture_runtime.rebuild(int(_dmx_universe_offset_input.value))
	var unbound_preview: PackedStringArray = summary.get("unbound_preview", PackedStringArray())
	_dmx_unbound_preview_label.visible = unbound_preview.size() > 0
	_dmx_unbound_preview_label.text = "Unbound fixtures:\n" + "\n".join(unbound_preview)

func resolve_controls_host() -> Control:
	if not _get_controls_host_callback.is_valid():
		return null
	return _get_controls_host_callback.call() as Control

func exit_tree() -> void:
	if _dmx_receiver != null:
		_dmx_receiver.stop()

func _on_dmx_universe_offset_changed(_value: float) -> void:
	refresh_fixture_bindings()

func _on_dmx_toggle_pressed() -> void:
	if _dmx_receiver == null:
		_dmx_toggle_button.button_pressed = false
		return
	if _dmx_toggle_button.button_pressed:
		_dmx_receiver.stop()
		var started: bool = _dmx_receiver.start("0.0.0.0", 6454)
		if not started:
			_dmx_receiver.stop()
			started = _dmx_receiver.start("0.0.0.0", 6454)
		if not started:
			_dmx_toggle_button.button_pressed = false
			_dmx_toggle_button.text = "DMX OFF"
			_update_dmx_toggle_color(false, false)
			var startup_error: String = ""
			if _dmx_receiver.has_method("get_last_error"):
				startup_error = str(_dmx_receiver.get_last_error())
			_dmx_toggle_button.tooltip_text = "DMX failed to start" if startup_error.is_empty() else "DMX failed to start: %s" % startup_error
			return
		_dmx_toggle_button.text = "DMX ON"
		_dmx_toggle_button.tooltip_text = ""
	else:
		_dmx_receiver.stop()
		_dmx_toggle_button.text = "DMX OFF"
		_update_dmx_toggle_color(false, false)
		_refresh_dmx_monitor_window(false)

func _on_dmx_monitor_pressed() -> void:
	if _dmx_receiver == null:
		return
	if _dmx_monitor_window == null or not is_instance_valid(_dmx_monitor_window):
		_dmx_monitor_window = DmxMonitorWindowScript.new()
		_owner.add_child(_dmx_monitor_window)
		_dmx_monitor_window.configure(_dmx_receiver)
	_dmx_monitor_window.popup_centered_ratio(0.75)
	_refresh_dmx_monitor_window(_dmx_receiver.is_running())

func _on_dmx_timer_timeout() -> void:
	if _dmx_receiver == null:
		return
	if not _dmx_receiver.is_running():
		if _dmx_toggle_button != null and not _dmx_toggle_button.button_pressed:
			_update_dmx_toggle_color(false, false)
		_refresh_dmx_monitor_window(false)
		return

	var now_msec: int = Time.get_ticks_msec()
	var delta_sec: float = 0.0
	if _last_dmx_tick_msec > 0:
		delta_sec = max(float(now_msec - _last_dmx_tick_msec) * 0.001, 0.0)
	_last_dmx_tick_msec = now_msec

	if _dmx_fixture_runtime != null and _apply_dmx_controls_callback.is_valid():
		_dmx_fixture_runtime.apply_dmx(_dmx_receiver, func(fixture_uuid: String, controls: Dictionary) -> void:
			controls["frame_delta_sec"] = delta_sec
			_apply_dmx_controls_callback.call(fixture_uuid, controls)
		)

	var stats: Dictionary = _dmx_receiver.get_stats()
	var active_universes: PackedInt32Array = _dmx_receiver.get_active_universes(2000)
	var last_ms: int = int(stats.get("last_packet_ms_ago", -1))
	var receiving: bool = active_universes.size() > 0 and last_ms >= 0 and last_ms <= 2000
	_update_dmx_toggle_color(true, receiving)
	_refresh_dmx_monitor_window(true)

func _update_dmx_toggle_color(enabled: bool, receiving_signal: bool) -> void:
	if _dmx_toggle_button == null:
		return
	if not enabled:
		_dmx_toggle_button.modulate = Color(0.75, 0.75, 0.75)
		return
	_dmx_toggle_button.modulate = Color(0.25, 0.95, 0.25) if receiving_signal else Color(0.95, 0.25, 0.25)

func _refresh_dmx_monitor_window(running: bool) -> void:
	if _dmx_monitor_window == null or not is_instance_valid(_dmx_monitor_window):
		return
	_dmx_monitor_window.refresh(running)
