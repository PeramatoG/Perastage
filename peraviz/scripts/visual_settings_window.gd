@tool
extends Window

class_name VisualSettingsWindow

signal settings_changed(settings: Dictionary)

const DEFAULT_SETTINGS := {
	"ambient_multiplier": 0.05,
	"spot_multiplier": 1.0,
	"beam_multiplier": 1.0,
	"bloom_multiplier": 1.0,
	"beam_render_mode": 0,
	"beam_quality": 2,
	"use_fog_volume_gobo_beam": true,
	"fog_volume_density_scale": 1.1,
	"fog_volume_emission_strength": 0.6,
	"fog_volume_edge_softness": 0.65,
	"fog_volume_invert_gobo": false,
	"beam_haze_density": 0.17,
	"beam_anisotropy": 0.62,
	"beam_noise_amount": 0.06,
	"beam_noise_scale": 1.4,
	"ambient_fog_density": 0.0,
	"volumetric_fog_density": 0.006,
	"volumetric_fog_fade": 0.02,
	"light_volumetric_fog_energy": 12.0,
	"use_native_fog_projector_gobos": true,
	"background_color": Color(0.129412, 0.137255, 0.156863, 1.0),
}

var _settings: Dictionary = DEFAULT_SETTINGS.duplicate(true)
var _ambient_slider: HSlider
var _ambient_value_label: Label
var _spot_slider: HSlider
var _spot_value_label: Label
var _beam_slider: HSlider
var _beam_value_label: Label
var _bloom_slider: HSlider
var _bloom_value_label: Label
var _fog_density_slider: HSlider
var _fog_density_value_label: Label
var _fog_fade_slider: HSlider
var _fog_fade_value_label: Label
var _light_fog_energy_slider: HSlider
var _light_fog_energy_value_label: Label
var _background_picker: ColorPickerButton
var _beam_render_mode_option: OptionButton
var _beam_quality_option: OptionButton

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

	var container: VBoxContainer = VBoxContainer.new()
	container.add_theme_constant_override("separation", 10)
	root.add_child(container)

	_ambient_slider = _add_slider_row(container, "Ambient light", "ambient_multiplier", 0.0, 3.0, 0.01)
	_spot_slider = _add_slider_row(container, "Spot intensity", "spot_multiplier", 0.0, 3.0, 0.01)
	_beam_slider = _add_slider_row(container, "Beam intensity", "beam_multiplier", 0.0, 8.0, 0.01)
	_bloom_slider = _add_slider_row(container, "Bloom", "bloom_multiplier", 0.0, 3.0, 0.01)
	_add_slider_row(container, "Ambient fog density", "ambient_fog_density", 0.0, 0.05, 0.001)
	_fog_density_slider = _add_slider_row(container, "Volumetric fog density", "volumetric_fog_density", 0.0, 0.03, 0.001)
	_fog_fade_slider = _add_slider_row(container, "Volumetric fog fade", "volumetric_fog_fade", 0.0, 0.2, 0.005)
	_light_fog_energy_slider = _add_slider_row(container, "Light fog energy", "light_volumetric_fog_energy", 0.0, 100.0, 0.5)
	_beam_render_mode_option = _add_option_row(container, "Beam rendering", ["Volumetric (default)", "Lightweight (legacy)"], _on_beam_render_mode_selected)
	_beam_quality_option = _add_option_row(container, "Beam quality", ["Low", "Medium", "High"], _on_beam_quality_selected)
	_add_toggle_row(container, "Use FogVolume gobo beam", "use_fog_volume_gobo_beam")
	_add_toggle_row(container, "Invert FogVolume gobo", "fog_volume_invert_gobo")
	_add_slider_row(container, "FogVolume density scale", "fog_volume_density_scale", 0.0, 10.0, 0.1)
	_add_slider_row(container, "FogVolume emission strength", "fog_volume_emission_strength", 0.0, 10.0, 0.1)
	_add_slider_row(container, "FogVolume edge softness", "fog_volume_edge_softness", 0.1, 1.0, 0.01)

	var background_row: HBoxContainer = HBoxContainer.new()
	background_row.add_theme_constant_override("separation", 8)
	container.add_child(background_row)

	var background_label: Label = Label.new()
	background_label.text = "Background color"
	background_label.custom_minimum_size = Vector2(150, 0)
	background_row.add_child(background_label)

	_background_picker = ColorPickerButton.new()
	_background_picker.custom_minimum_size = Vector2(180, 30)
	_background_picker.color_changed.connect(_on_background_color_changed)
	background_row.add_child(_background_picker)

	var actions_row: HBoxContainer = HBoxContainer.new()
	actions_row.alignment = BoxContainer.ALIGNMENT_END
	container.add_child(actions_row)

	var reset_button: Button = Button.new()
	reset_button.text = "Reset"
	reset_button.pressed.connect(_on_reset_pressed)
	actions_row.add_child(reset_button)

func _add_slider_row(parent: VBoxContainer,
		label_text: String,
		key: String,
		min_value: float,
		max_value: float,
		step: float) -> HSlider:
	var row: HBoxContainer = HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	parent.add_child(row)

	var setting_label: Label = Label.new()
	setting_label.text = label_text
	setting_label.custom_minimum_size = Vector2(150, 0)
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
	value_label.custom_minimum_size = Vector2(56, 0)
	value_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	row.add_child(value_label)

	match key:
		"ambient_multiplier":
			_ambient_value_label = value_label
		"spot_multiplier":
			_spot_value_label = value_label
		"beam_multiplier":
			_beam_value_label = value_label
		"bloom_multiplier":
			_bloom_value_label = value_label
		"volumetric_fog_density":
			_fog_density_value_label = value_label
		"volumetric_fog_fade":
			_fog_fade_value_label = value_label
		"light_volumetric_fog_energy":
			_light_fog_energy_value_label = value_label

	return slider

func _add_option_row(parent: VBoxContainer, label_text: String, options: Array[String], on_selected: Callable) -> OptionButton:
	var row: HBoxContainer = HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	parent.add_child(row)

	var setting_label: Label = Label.new()
	setting_label.text = label_text
	setting_label.custom_minimum_size = Vector2(150, 0)
	row.add_child(setting_label)

	var option_button: OptionButton = OptionButton.new()
	option_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for option_text in options:
		option_button.add_item(option_text)
	option_button.item_selected.connect(on_selected)
	row.add_child(option_button)

	return option_button

func _add_toggle_row(parent: VBoxContainer, label_text: String, key: String) -> void:
	var toggle: CheckBox = CheckBox.new()
	toggle.text = label_text
	toggle.button_pressed = bool(_settings.get(key, false))
	toggle.toggled.connect(func(value: bool) -> void:
		_settings[key] = value
		_emit_settings_changed()
	)
	parent.add_child(toggle)

func _apply_settings_to_controls() -> void:
	_ambient_slider.value = float(_settings.get("ambient_multiplier", 0.05))
	_spot_slider.value = float(_settings.get("spot_multiplier", 1.0))
	_beam_slider.value = float(_settings.get("beam_multiplier", 1.0))
	_bloom_slider.value = float(_settings.get("bloom_multiplier", 1.0))
	_fog_density_slider.value = float(_settings.get("volumetric_fog_density", 0.006))
	_fog_fade_slider.value = float(_settings.get("volumetric_fog_fade", 0.02))
	_light_fog_energy_slider.value = float(_settings.get("light_volumetric_fog_energy", 12.0))
	_beam_render_mode_option.select(clamp(int(_settings.get("beam_render_mode", 0)), 0, 1))
	_beam_quality_option.select(clamp(int(_settings.get("beam_quality", 1)), 0, 2))
	_background_picker.color = _settings.get("background_color", DEFAULT_SETTINGS["background_color"])
	_update_value_labels()

func _on_slider_changed(key: String, value: float) -> void:
	if key == "volumetric_fog_fade":
		value = max(value, 0.005)
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

func _update_value_labels() -> void:
	_ambient_value_label.text = "%.2f" % float(_settings.get("ambient_multiplier", 0.05))
	_spot_value_label.text = "%.2f" % float(_settings.get("spot_multiplier", 1.0))
	_beam_value_label.text = "%.2f" % float(_settings.get("beam_multiplier", 1.0))
	_bloom_value_label.text = "%.2f" % float(_settings.get("bloom_multiplier", 1.0))
	_fog_density_value_label.text = "%.3f" % float(_settings.get("volumetric_fog_density", 0.006))
	_fog_fade_value_label.text = "%.3f" % float(_settings.get("volumetric_fog_fade", 0.02))
	_light_fog_energy_value_label.text = "%.2f" % float(_settings.get("light_volumetric_fog_energy", 12.0))

func _emit_settings_changed() -> void:
	settings_changed.emit(_settings.duplicate(true))

func _on_close_requested() -> void:
	hide()
