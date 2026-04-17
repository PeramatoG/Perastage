extends Window

class_name DmxMonitorWindow

const CHANNEL_COUNT: int = 512

var _receiver = null
var _selected_universe: int = -1
var _universe_buttons: Dictionary = {}
var _channel_panels: Array[PanelContainer] = []
var _channel_labels: Array[Label] = []
var _channel_values: PackedInt32Array = PackedInt32Array()
var _show_only_active_channels: bool = false

var _state_label: Label
var _universes_flow: FlowContainer

func _init() -> void:
	title = "Technical Monitor"
	size = Vector2i(1040, 680)
	unresizable = false

func _ready() -> void:
	close_requested.connect(_on_close_requested)
	var root: VBoxContainer = VBoxContainer.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.offset_left = 12
	root.offset_top = 12
	root.offset_right = -12
	root.offset_bottom = -12
	add_child(root)

	_state_label = Label.new()
	_state_label.text = "DMX monitor inactive"
	root.add_child(_state_label)

	var filters_row: HBoxContainer = HBoxContainer.new()
	filters_row.add_theme_constant_override("separation", 8)
	root.add_child(filters_row)

	var active_channels_toggle: CheckBox = CheckBox.new()
	active_channels_toggle.text = "Show only active channels"
	active_channels_toggle.toggled.connect(_on_active_channels_toggled)
	filters_row.add_child(active_channels_toggle)

	var universe_scroll: ScrollContainer = ScrollContainer.new()
	universe_scroll.custom_minimum_size = Vector2(0, 100)
	universe_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	universe_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	root.add_child(universe_scroll)

	_universes_flow = FlowContainer.new()
	_universes_flow.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_universes_flow.add_theme_constant_override("h_separation", 6)
	_universes_flow.add_theme_constant_override("v_separation", 6)
	universe_scroll.add_child(_universes_flow)

	var channels_scroll: ScrollContainer = ScrollContainer.new()
	channels_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	channels_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	root.add_child(channels_scroll)

	var channels_grid: GridContainer = GridContainer.new()
	channels_grid.columns = 16
	channels_grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	channels_grid.size_flags_vertical = Control.SIZE_EXPAND_FILL
	channels_grid.add_theme_constant_override("h_separation", 4)
	channels_grid.add_theme_constant_override("v_separation", 4)
	channels_scroll.add_child(channels_grid)

	for channel_idx in range(CHANNEL_COUNT):
		var panel: PanelContainer = PanelContainer.new()
		panel.custom_minimum_size = Vector2(58, 24)
		channels_grid.add_child(panel)

		var label: Label = Label.new()
		label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
		label.text = "%03d" % (channel_idx + 1)
		panel.add_child(label)

		_channel_panels.append(panel)
		_channel_labels.append(label)
		_channel_values.append(0)

		_update_channel_cell(channel_idx, 0)

func configure(receiver) -> void:
	_receiver = receiver

func refresh(running: bool) -> void:
	if _receiver == null:
		_state_label.text = "DMX unavailable"
		return

	if not running:
		_state_label.text = "DMX OFF"
		_update_universe_buttons([])
		for channel_idx in range(CHANNEL_COUNT):
			_update_channel_cell(channel_idx, 0)
		return

	var active_universes: PackedInt32Array = _receiver.get_active_universes(2000)
	_update_universe_buttons(active_universes)

	if _selected_universe < 0 and active_universes.size() > 0:
		_selected_universe = int(active_universes[0])

	var selected_text: String = "none"
	if _selected_universe >= 0:
		selected_text = str(_selected_universe)
	_state_label.text = "Active universes: %d | Selected universe: %s" % [active_universes.size(), selected_text]

	var data: PackedByteArray = PackedByteArray()
	if _selected_universe >= 0:
		data = _receiver.get_universe_data(_selected_universe)

	for channel_idx in range(CHANNEL_COUNT):
		var channel_value: int = 0
		if channel_idx < data.size():
			channel_value = int(data[channel_idx])
		_update_channel_cell(channel_idx, channel_value)

func _update_universe_buttons(active_universes: PackedInt32Array) -> void:
	var active_map: Dictionary = {}
	for universe in active_universes:
		active_map[int(universe)] = true

	for universe_id in _universe_buttons.keys():
		var button: Button = _universe_buttons[universe_id]
		var is_active: bool = active_map.has(universe_id)
		button.modulate = Color(0.35, 1.0, 0.35) if is_active else Color(0.35, 0.35, 0.35)
		button.button_pressed = (int(universe_id) == _selected_universe)

	for universe in active_universes:
		var uni_id: int = int(universe)
		if _universe_buttons.has(uni_id):
			continue
		var button: Button = Button.new()
		button.toggle_mode = true
		button.text = "U%d" % uni_id
		button.custom_minimum_size = Vector2(74, 34)
		button.modulate = Color(0.35, 1.0, 0.35)
		button.pressed.connect(func() -> void:
			_selected_universe = uni_id
			for btn_uni in _universe_buttons.keys():
				var btn: Button = _universe_buttons[btn_uni]
				btn.button_pressed = (int(btn_uni) == _selected_universe)
		)
		_universe_buttons[uni_id] = button
		_universes_flow.add_child(button)

func _update_channel_cell(channel_idx: int, channel_value: int) -> void:
	var normalized: float = clamp(float(channel_value) / 255.0, 0.0, 1.0)
	var color: Color = Color(0.08, 0.16 + (0.84 * normalized), 0.08)

	var style: StyleBoxFlat = StyleBoxFlat.new()
	style.bg_color = color
	style.border_color = Color(0.05, 0.05, 0.05)
	style.border_width_left = 1
	style.border_width_right = 1
	style.border_width_top = 1
	style.border_width_bottom = 1

	_channel_panels[channel_idx].add_theme_stylebox_override("panel", style)
	_channel_labels[channel_idx].text = "%03d:%03d" % [channel_idx + 1, channel_value]
	_channel_values[channel_idx] = channel_value
	_channel_panels[channel_idx].visible = (channel_value > 0) if _show_only_active_channels else true


func _on_close_requested() -> void:
	hide()

func _on_active_channels_toggled(enabled: bool) -> void:
	_show_only_active_channels = enabled
	for channel_idx in range(CHANNEL_COUNT):
		var channel_value: int = int(_channel_values[channel_idx])
		_channel_panels[channel_idx].visible = (channel_value > 0) if _show_only_active_channels else true
