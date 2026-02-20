extends Window

class_name VisualSettingsWindow

signal settings_changed(settings: Dictionary)

const DEFAULT_SETTINGS := {
	"ambient_multiplier": 1.0,
	"spot_multiplier": 1.0,
	"beam_multiplier": 1.0,
	"bloom_multiplier": 1.0,
	"beam_render_mode": 0,
	"beam_quality": 1,
	"beam_haze_density": 0.17,
	"beam_anisotropy": 0.62,
	"beam_noise_amount": 0.06,
	"beam_noise_scale": 1.4,
	"beam_gobo_enabled": true,
	"beam_gobo_strength": 1.0,
	"beam_gobo_softness": 0.8,
	"beam_gobo_gamma": 1.0,
	"beam_gobo_rotation": 0.0,
	"legacy_cone_count": 2,
	"legacy_overlay_scale": 0.96,
	"legacy_core_scale": 0.92,
	"legacy_beam_gobo_max_distance": 180.0,
	"background_color": Color(0.129412, 0.137255, 0.156863, 1.0),
}

var _settings: Dictionary = DEFAULT_SETTINGS.duplicate(true)
var _slider_controls: Dictionary = {}
var _slider_value_labels: Dictionary = {}
var _background_picker: ColorPickerButton
var _beam_render_mode_option: OptionButton
var _beam_quality_option: OptionButton
var _beam_gobo_enabled_toggle: CheckButton
var _legacy_cone_count_option: OptionButton

func _init() -> void:
	title = "Visual Settings"
	size = Vector2i(540, 560)
	unresizable = false

func _ready() -> void:
	close_requested.connect(_on_close_requested)
	_build_ui()
	_apply_settings_to_controls()

func configure(initial_settings: Dictionary) -> void:
	if initial_settings.is_empty():
		return
	for key in DEFAULT_SETTINGS.keys():
		if initial_settings.has(key):
			_settings[key] = initial_settings[key]
	if is_node_ready():
		_apply_settings_to_controls()

func popup_settings() -> void:
	popup_centered()
	grab_focus()

func _build_ui() -> void:
	var root: MarginContainer = MarginContainer.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.add_theme_constant_override("margin_left", 12)
	root.add_theme_constant_override("margin_top", 12)
	root.add_theme_constant_override("margin_right", 12)
	root.add_theme_constant_override("margin_bottom", 12)
	add_child(root)

	var scroll: ScrollContainer = ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	root.add_child(scroll)

	var container: VBoxContainer = VBoxContainer.new()
	container.add_theme_constant_override("separation", 10)
	scroll.add_child(container)

	_add_section_label(container, "Lighting")
	_add_slider_row(container, "Ambient light", "ambient_multiplier", 0.0, 3.0, 0.01)
	_add_slider_row(container, "Spot intensity", "spot_multiplier", 0.0, 3.0, 0.01)
	_add_slider_row(container, "Beam intensity", "beam_multiplier", 0.0, 3.0, 0.01)
	_add_slider_row(container, "Bloom", "bloom_multiplier", 0.0, 3.0, 0.01)
	_beam_render_mode_option = _add_option_row(container, "Beam rendering", ["Volumetric (default)", "Lightweight (legacy)"], _on_beam_render_mode_selected)
	_beam_quality_option = _add_option_row(container, "Beam quality", ["Low", "Medium", "High"], _on_beam_quality_selected)

	var background_row: HBoxContainer = HBoxContainer.new()
	background_row.add_theme_constant_override("separation", 8)
	container.add_child(background_row)
	var background_label: Label = Label.new()
	background_label.text = "Background color"
	background_label.custom_minimum_size = Vector2(180, 0)
	background_row.add_child(background_label)
	_background_picker = ColorPickerButton.new()
	_background_picker.custom_minimum_size = Vector2(180, 30)
	_background_picker.color_changed.connect(_on_background_color_changed)
	background_row.add_child(_background_picker)

	_add_section_label(container, "Beam Gobos")
	_beam_gobo_enabled_toggle = _add_toggle_row(container, "Enable gobo in beam", "beam_gobo_enabled")
	_add_slider_row(container, "Gobo strength", "beam_gobo_strength", 0.0, 1.0, 0.01)
	_add_slider_row(container, "Gobo softness", "beam_gobo_softness", 0.0, 6.0, 0.05)
	_add_slider_row(container, "Gobo gamma", "beam_gobo_gamma", 0.25, 4.0, 0.01)
	_add_slider_row(container, "Gobo rotation (rad)", "beam_gobo_rotation", -6.283, 6.283, 0.01)

	_add_section_label(container, "Legacy Beam Tuning")
	_legacy_cone_count_option = _add_option_row(container, "Legacy cone count", ["2 cones", "3 cones"], _on_legacy_cone_count_selected)
	_add_slider_row(container, "Legacy overlay scale", "legacy_overlay_scale", 0.85, 1.0, 0.005)
	_add_slider_row(container, "Legacy core scale", "legacy_core_scale", 0.80, 1.0, 0.005)
	_add_slider_row(container, "Legacy gobo max distance", "legacy_beam_gobo_max_distance", 0.0, 400.0, 1.0)

	var actions_row: HBoxContainer = HBoxContainer.new()
	actions_row.alignment = BoxContainer.ALIGNMENT_END
	container.add_child(actions_row)
	var reset_button: Button = Button.new()
	reset_button.text = "Reset"
	reset_button.pressed.connect(_on_reset_pressed)
	actions_row.add_child(reset_button)

func _add_section_label(parent: VBoxContainer, label_text: String) -> void:
	var section_label: Label = Label.new()
	section_label.text = label_text
	section_label.add_theme_font_size_override("font_size", 15)
	parent.add_child(section_label)

func _add_slider_row(parent: VBoxContainer, label_text: String, key: String, min_value: float, max_value: float, step: float) -> HSlider:
	var row: HBoxContainer = HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	parent.add_child(row)

	var setting_label: Label = Label.new()
	setting_label.text = label_text
	setting_label.custom_minimum_size = Vector2(180, 0)
	row.add_child(setting_label)

	var slider: HSlider = HSlider.new()
	slider.min_value = min_value
	slider.max_value = max_value
	slider.step = step
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	slider.value_changed.connect(func(value: float) -> void:
		_on_slider_changed(key, value)
	)
	row.add_child(slider)

	var value_label: Label = Label.new()
	value_label.custom_minimum_size = Vector2(52, 0)
	value_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	row.add_child(value_label)

	_slider_controls[key] = slider
	_slider_value_labels[key] = value_label
	return slider

func _add_option_row(parent: VBoxContainer, label_text: String, options: Array[String], on_selected: Callable) -> OptionButton:
	var row: HBoxContainer = HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	parent.add_child(row)

	var setting_label: Label = Label.new()
	setting_label.text = label_text
	setting_label.custom_minimum_size = Vector2(180, 0)
	row.add_child(setting_label)

	var option_button: OptionButton = OptionButton.new()
	option_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for option_text in options:
		option_button.add_item(option_text)
	option_button.item_selected.connect(on_selected)
	row.add_child(option_button)
	return option_button

func _add_toggle_row(parent: VBoxContainer, label_text: String, key: String) -> CheckButton:
	var row: HBoxContainer = HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	parent.add_child(row)

	var label: Label = Label.new()
	label.text = label_text
	label.custom_minimum_size = Vector2(180, 0)
	row.add_child(label)

	var toggle: CheckButton = CheckButton.new()
	toggle.toggled.connect(func(enabled: bool) -> void:
		_settings[key] = enabled
		_emit_settings_changed()
	)
	row.add_child(toggle)
	return toggle

func _apply_settings_to_controls() -> void:
	for key in _slider_controls.keys():
		var slider: HSlider = _slider_controls[key] as HSlider
		if slider != null:
			slider.value = float(_settings.get(key, DEFAULT_SETTINGS.get(key, slider.value)))
	_beam_render_mode_option.select(clamp(int(_settings.get("beam_render_mode", 0)), 0, 1))
	_beam_quality_option.select(clamp(int(_settings.get("beam_quality", 1)), 0, 2))
	_legacy_cone_count_option.select(1 if int(_settings.get("legacy_cone_count", 2)) >= 3 else 0)
	_beam_gobo_enabled_toggle.button_pressed = bool(_settings.get("beam_gobo_enabled", true))
	_background_picker.color = _settings.get("background_color", DEFAULT_SETTINGS["background_color"])
	_update_value_labels()

func _on_slider_changed(key: String, value: float) -> void:
	_settings[key] = value
	_update_value_labels()
	_emit_settings_changed()

func _on_background_color_changed(color: Color) -> void:
	_settings["background_color"] = color
	_emit_settings_changed()

func _on_reset_pressed() -> void:
	_settings = DEFAULT_SETTINGS.duplicate(true)
	_apply_settings_to_controls()
	_emit_settings_changed()

func _on_beam_render_mode_selected(index: int) -> void:
	_settings["beam_render_mode"] = clamp(index, 0, 1)
	_emit_settings_changed()

func _on_beam_quality_selected(index: int) -> void:
	_settings["beam_quality"] = clamp(index, 0, 2)
	_emit_settings_changed()

func _on_legacy_cone_count_selected(index: int) -> void:
	_settings["legacy_cone_count"] = 3 if index > 0 else 2
	_emit_settings_changed()

func _update_value_labels() -> void:
	for key in _slider_value_labels.keys():
		var value_label: Label = _slider_value_labels[key] as Label
		if value_label == null:
			continue
		value_label.text = "%.2f" % float(_settings.get(key, 0.0))

func _emit_settings_changed() -> void:
	settings_changed.emit(_settings.duplicate(true))

func _on_close_requested() -> void:
	hide()
