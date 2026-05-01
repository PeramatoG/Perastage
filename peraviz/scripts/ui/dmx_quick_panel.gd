extends PanelContainer

class_name DmxQuickPanel

signal open_technical_monitor_requested

var _status_value_label: Label
var _universes_value_label: Label
var _last_packet_value_label: Label
var _fixtures_value_label: Label
var _dmx_tick_value_label: Label
var _open_monitor_button: Button
var _monitor_available: bool = false
var _running: bool = false
var _receiving_signal: bool = false
var _active_universes: PackedInt32Array = PackedInt32Array()
var _last_packet_ms: int = -1
var _linked_fixtures: int = 0
var _unlinked_fixtures: int = 0
var _updated_fixtures: int = 0
var _skipped_fixtures: int = 0

func _ready() -> void:
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 8)
	margin.add_theme_constant_override("margin_top", 8)
	margin.add_theme_constant_override("margin_right", 8)
	margin.add_theme_constant_override("margin_bottom", 8)
	add_child(margin)

	var layout := VBoxContainer.new()
	layout.add_theme_constant_override("separation", 6)
	margin.add_child(layout)

	var title := Label.new()
	title.text = "DMX Quick Panel"
	layout.add_child(title)

	_status_value_label = _add_metric_row(layout, "State", "OFF")
	_universes_value_label = _add_metric_row(layout, "Active universes", "0")
	_last_packet_value_label = _add_metric_row(layout, "Last packet", "n/a")
	_fixtures_value_label = _add_metric_row(layout, "Fixtures", "0 linked / 0 unlinked")
	_dmx_tick_value_label = _add_metric_row(layout, "DMX tick", "0 updated / 0 skipped")

	_open_monitor_button = Button.new()
	_open_monitor_button.text = "Open Technical Monitor"
	_open_monitor_button.pressed.connect(_on_open_monitor_pressed)
	layout.add_child(_open_monitor_button)
	_apply_state()

func set_monitor_available(available: bool) -> void:
	_monitor_available = available
	_apply_state()

func refresh(running: bool, receiving_signal: bool, active_universes: PackedInt32Array, last_packet_ms: int, linked_fixtures: int, unlinked_fixtures: int, updated_fixtures: int, skipped_fixtures: int) -> void:
	_running = running
	_receiving_signal = receiving_signal
	_active_universes = active_universes
	_last_packet_ms = last_packet_ms
	_linked_fixtures = max(linked_fixtures, 0)
	_unlinked_fixtures = max(unlinked_fixtures, 0)
	_updated_fixtures = max(updated_fixtures, 0)
	_skipped_fixtures = max(skipped_fixtures, 0)
	_apply_state()

func _add_metric_row(parent: VBoxContainer, label_text: String, initial_value: String) -> Label:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 6)
	parent.add_child(row)
	var label := Label.new()
	label.text = "%s:" % label_text
	row.add_child(label)
	var value := Label.new()
	value.text = initial_value
	row.add_child(value)
	return value

func _format_universes(active_universes: PackedInt32Array) -> String:
	if active_universes.is_empty():
		return "0"
	var values := PackedStringArray()
	for universe in active_universes:
		values.append(str(universe))
	return "%d (%s)" % [active_universes.size(), ", ".join(values)]

func _on_open_monitor_pressed() -> void:
	emit_signal("open_technical_monitor_requested")

func _apply_state() -> void:
	if _status_value_label == null:
		return
	_open_monitor_button.disabled = not _monitor_available
	if not _monitor_available:
		_status_value_label.text = "Unavailable"
	elif not _running:
		_status_value_label.text = "OFF"
	else:
		_status_value_label.text = "ON" if _receiving_signal else "ON (no signal)"

	_universes_value_label.text = _format_universes(_active_universes)
	_last_packet_value_label.text = "%d ms" % _last_packet_ms if _last_packet_ms >= 0 else "n/a"
	_fixtures_value_label.text = "%d linked / %d unlinked" % [_linked_fixtures, _unlinked_fixtures]
	if _dmx_tick_value_label != null:
		_dmx_tick_value_label.text = "%d updated / %d skipped" % [_updated_fixtures, _skipped_fixtures]
