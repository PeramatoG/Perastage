extends RefCounted
class_name DmxController

const DmxMonitorWindowScript = preload("res://scripts/dmx_monitor_window.gd")
const DmxFixtureRuntimeScript = preload("res://scripts/dmx_fixture_runtime.gd")
const DmxQuickPanelScript = preload("res://scripts/ui/dmx_quick_panel.gd")

var _owner: Node
var _get_controls_host_callback: Callable
var _apply_dmx_controls_callback: Callable

var _dmx_receiver = null
var _dmx_toggle_button: Button
var _dmx_monitor_button: Button
var _dmx_monitor_window: Window
var _dmx_quick_panel: DmxQuickPanel
var _dmx_timer: Timer
var _dmx_universe_offset_input: SpinBox
var _dmx_unbound_preview_label: Label
var _dmx_controls_panel: PanelContainer
var _dmx_fixture_runtime = null
var _last_dmx_tick_msec: int = 0
var _dmx_status_changed_callback: Callable
var _dmx_start_failed_callback: Callable
var _fixture_binding_summary: Dictionary = {}
var _last_updated_fixtures: int = 0
var _last_skipped_fixtures: int = 0
var _debug_force_full_apply: bool = false

func configure(owner: Node, get_controls_host_callback: Callable, apply_dmx_controls_callback: Callable) -> void:
	_owner = owner
	_get_controls_host_callback = get_controls_host_callback
	_apply_dmx_controls_callback = apply_dmx_controls_callback

func set_status_callbacks(dmx_status_changed_callback: Callable, dmx_start_failed_callback: Callable) -> void:
	_dmx_status_changed_callback = dmx_status_changed_callback
	_dmx_start_failed_callback = dmx_start_failed_callback

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
	_dmx_monitor_button.text = "Open Technical Monitor"
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

	_dmx_quick_panel = DmxQuickPanelScript.new()
	controls_vbox.add_child(_dmx_quick_panel)
	_dmx_quick_panel.open_technical_monitor_requested.connect(_on_dmx_monitor_pressed)
	_dmx_quick_panel.set_monitor_available(false)

	_dmx_timer = Timer.new()
	_dmx_timer.wait_time = 0.03
	_dmx_timer.autostart = true
	_owner.add_child(_dmx_timer)
	_dmx_timer.timeout.connect(_on_dmx_timer_timeout)

	if ClassDB.class_exists("PeravizDmxReceiver"):
		_dmx_receiver = ClassDB.instantiate("PeravizDmxReceiver")
		_dmx_monitor_button.disabled = false
		_dmx_quick_panel.set_monitor_available(true)
	else:
		_dmx_toggle_button.disabled = true
		_dmx_toggle_button.tooltip_text = "DMX unavailable (build without PERAVIZ_ENABLE_DMX)"
		_dmx_quick_panel.set_monitor_available(false)

	_refresh_dmx_quick_panel(false, false, PackedInt32Array(), -1)

func setup_fixture_runtime(loader: Variant, scene_registry: SceneRegistry) -> void:
	_dmx_fixture_runtime = DmxFixtureRuntimeScript.new()
	_dmx_fixture_runtime.configure(loader, scene_registry)
	_dmx_fixture_runtime.set_debug_force_full_apply(_debug_force_full_apply)

func set_debug_force_full_apply(enabled: bool) -> void:
	_debug_force_full_apply = enabled
	if _dmx_fixture_runtime != null:
		_dmx_fixture_runtime.set_debug_force_full_apply(enabled)

func refresh_fixture_bindings() -> Dictionary:
	if _dmx_fixture_runtime == null or _dmx_universe_offset_input == null:
		return {}
	var summary: Dictionary = _dmx_fixture_runtime.rebuild(int(_dmx_universe_offset_input.value))
	_fixture_binding_summary = summary
	var unbound_preview: PackedStringArray = summary.get("unbound_preview", PackedStringArray())
	_dmx_unbound_preview_label.visible = unbound_preview.size() > 0
	_dmx_unbound_preview_label.text = "Unbound fixtures:\n" + "\n".join(unbound_preview)
	_refresh_dmx_quick_panel(false, false, PackedInt32Array(), -1)
	return summary

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
		_emit_dmx_status(false, false)
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
			_emit_dmx_start_failed(startup_error)
			_refresh_dmx_quick_panel(false, false, PackedInt32Array(), -1)
			_emit_dmx_status(false, false)
			return
		_dmx_toggle_button.text = "DMX ON"
		_dmx_toggle_button.tooltip_text = ""
		_emit_dmx_status(true, false)
	else:
		_dmx_receiver.stop()
		_dmx_toggle_button.text = "DMX OFF"
		_update_dmx_toggle_color(false, false)
		_refresh_dmx_monitor_window(false)
		_last_updated_fixtures = 0
		_last_skipped_fixtures = 0
		_refresh_dmx_quick_panel(false, false, PackedInt32Array(), -1)
		_emit_dmx_status(false, false)

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
		_last_updated_fixtures = 0
		_last_skipped_fixtures = 0
		_refresh_dmx_quick_panel(false, false, PackedInt32Array(), -1)
		return

	var now_msec: int = Time.get_ticks_msec()
	var delta_sec: float = 0.0
	if _last_dmx_tick_msec > 0:
		delta_sec = max(float(now_msec - _last_dmx_tick_msec) * 0.001, 0.0)
	_last_dmx_tick_msec = now_msec

	if _dmx_fixture_runtime != null and _apply_dmx_controls_callback.is_valid():
		var decode_phase_start: int = Time.get_ticks_usec()
		var apply_stats: Dictionary = _dmx_fixture_runtime.apply_dmx(_dmx_receiver, func(fixture_uuid: String, controls: Dictionary) -> void:
			controls["frame_delta_sec"] = delta_sec
			_apply_dmx_controls_callback.call(fixture_uuid, controls)
		)
		_last_updated_fixtures = int(apply_stats.get("updated", 0))
		_last_skipped_fixtures = int(apply_stats.get("skipped", 0))
		if _owner != null and _owner.has_method("bridge_record_dmx_decode_phase"):
			_owner.bridge_record_dmx_decode_phase(max(Time.get_ticks_usec() - decode_phase_start, 0))
		_apply_fixture_time_tick(delta_sec)

	var stats: Dictionary = _dmx_receiver.get_stats()
	var active_universes: PackedInt32Array = _dmx_receiver.get_active_universes(2000)
	var last_ms: int = int(stats.get("last_packet_ms_ago", -1))
	var receiving: bool = active_universes.size() > 0 and last_ms >= 0 and last_ms <= 2000
	_update_dmx_toggle_color(true, receiving)
	_refresh_dmx_monitor_window(true)
	_refresh_dmx_quick_panel(true, receiving, active_universes, last_ms)
	_emit_dmx_status(true, receiving)

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

func _refresh_dmx_quick_panel(running: bool, receiving_signal: bool, active_universes: PackedInt32Array, last_packet_ms: int) -> void:
	if _dmx_quick_panel == null or not is_instance_valid(_dmx_quick_panel):
		return
	var linked_fixtures: int = int(_fixture_binding_summary.get("bound", 0))
	var unlinked_fixtures: int = int(_fixture_binding_summary.get("unbound", 0))
	_dmx_quick_panel.refresh(running, receiving_signal, active_universes, last_packet_ms, linked_fixtures, unlinked_fixtures, _last_updated_fixtures, _last_skipped_fixtures)

func _apply_fixture_time_tick(delta_sec: float) -> void:
	if delta_sec <= 0.0:
		return
	if _dmx_fixture_runtime == null or not _apply_dmx_controls_callback.is_valid():
		return
	for fixture_uuid in _dmx_fixture_runtime.get_time_tick_fixture_ids():
		_apply_dmx_controls_callback.call(str(fixture_uuid), {
			"frame_delta_sec": delta_sec,
			"time_tick_only": true,
		})

func _emit_dmx_status(running: bool, receiving_signal: bool) -> void:
	if _dmx_status_changed_callback.is_valid():
		_dmx_status_changed_callback.call(running, receiving_signal)

func _emit_dmx_start_failed(error_message: String) -> void:
	if _dmx_start_failed_callback.is_valid():
		_dmx_start_failed_callback.call(error_message)
