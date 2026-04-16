@tool
extends VBoxContainer

class_name EnvironmentSettingsPanel

signal setting_changed(key: String, value: Variant)

const BASIC_KEYS := {
	"environment_enabled": true,
	"environment_current_preset": true,
	"environment_time_of_day": true,
	"environment_auto_advance": true,
	"environment_cycle_speed": true,
	"environment_day_light_intensity": true,
	"environment_night_light_intensity": true,
}

var _controls: Dictionary = {}
var _value_labels: Dictionary = {}
var _advanced_rows: Array[Control] = []

func _ready() -> void:
	if get_child_count() == 0:
		_build_ui()

func _build_ui() -> void:
	add_theme_constant_override("separation", 10)
	_add_toggle_row("Enable environment controller", "environment_enabled")
	_add_toggle_row("Use continuous cycle", "environment_use_continuous_cycle")
	_add_option_row("Environment preset", "environment_current_preset", ["Dawn", "Day", "Dusk", "Night", "BlackoutNight"])
	_add_slider_row("Time of day", "environment_time_of_day", 0.0, 1.0, 0.001)
	_add_toggle_row("Auto advance cycle", "environment_auto_advance")
	_add_slider_row("Cycle speed", "environment_cycle_speed", 0.0, 0.2, 0.0005)
	_add_toggle_row("Allow blackout night", "environment_allow_blackout_night")
	_add_slider_row("Day light intensity", "environment_day_light_intensity", 0.0, 2.0, 0.01)
	_add_slider_row("Dusk light intensity", "environment_dusk_light_intensity", 0.0, 1.0, 0.01)
	_add_slider_row("Night light intensity", "environment_night_light_intensity", 0.0, 0.3, 0.001)
	_add_slider_row("Moon light intensity", "environment_moon_light_intensity", 0.0, 0.2, 0.001)
	_add_slider_row("Ambient day energy", "environment_ambient_energy_day", 0.0, 0.3, 0.001)
	_add_slider_row("Ambient night energy", "environment_ambient_energy_night", 0.0, 0.1, 0.001)
	_add_slider_row("Horizon warmth", "environment_horizon_warmth", 0.0, 1.0, 0.01)
	_add_slider_row("Horizon intensity", "environment_horizon_intensity", 0.0, 2.0, 0.01)

func set_advanced_mode(enabled: bool) -> void:
	for row in _advanced_rows:
		row.visible = enabled

func apply_settings(settings: Dictionary) -> void:
	for key in _controls.keys():
		var control: Control = _controls[key]
		if control is HSlider:
			(control as HSlider).value = float(settings.get(key, (control as HSlider).value))
		elif control is CheckBox:
			(control as CheckBox).button_pressed = bool(settings.get(key, false))
		elif control is OptionButton:
			var option: OptionButton = control as OptionButton
			option.select(clamp(int(settings.get(key, 0)), 0, max(0, option.item_count - 1)))
	_update_value_labels()

func _is_advanced(key: String) -> bool:
	return not BASIC_KEYS.has(key)

func _add_toggle_row(label_text: String, key: String) -> void:
	var toggle: CheckBox = CheckBox.new()
	toggle.text = label_text
	toggle.toggled.connect(func(value: bool) -> void:
		setting_changed.emit(key, value)
	)
	add_child(toggle)
	_controls[key] = toggle
	if _is_advanced(key):
		_advanced_rows.append(toggle)

func _add_slider_row(label_text: String, key: String, min_value: float, max_value: float, step: float) -> void:
	var row: HBoxContainer = HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	add_child(row)

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
		setting_changed.emit(key, value)
		_update_value_labels()
	)
	row.add_child(slider)

	var value_label: Label = Label.new()
	value_label.custom_minimum_size = Vector2(64, 0)
	value_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	row.add_child(value_label)

	_controls[key] = slider
	_value_labels[key] = value_label
	if _is_advanced(key):
		_advanced_rows.append(row)

func _add_option_row(label_text: String, key: String, options: Array[String]) -> void:
	var row: HBoxContainer = HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	add_child(row)

	var setting_label: Label = Label.new()
	setting_label.text = label_text
	setting_label.custom_minimum_size = Vector2(180, 0)
	row.add_child(setting_label)

	var option_button: OptionButton = OptionButton.new()
	option_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for option_text in options:
		option_button.add_item(option_text)
	option_button.item_selected.connect(func(index: int) -> void:
		setting_changed.emit(key, index)
	)
	row.add_child(option_button)

	_controls[key] = option_button
	if _is_advanced(key):
		_advanced_rows.append(row)

func _update_value_labels() -> void:
	for key in _value_labels.keys():
		var value_label: Label = _value_labels[key]
		var value: float = float((_controls[key] as HSlider).value)
		match key:
			"environment_time_of_day":
				value_label.text = "%.3f" % value
			"environment_cycle_speed":
				value_label.text = "%.4f" % value
			_:
				value_label.text = "%.3f" % value
