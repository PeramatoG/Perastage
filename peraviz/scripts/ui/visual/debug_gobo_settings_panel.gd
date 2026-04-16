@tool
extends VBoxContainer

class_name DebugGoboSettingsPanel

signal setting_changed(key: String, value: Variant)

const GoboShakeLimitsScript = preload("res://scripts/gobo_shake_limits.gd")

var _controls: Dictionary = {}
var _value_labels: Dictionary = {}
var _advanced_notice: Label

func _ready() -> void:
	if get_child_count() == 0:
		_build_ui()

func _build_ui() -> void:
	add_theme_constant_override("separation", 10)
	_advanced_notice = Label.new()
	_advanced_notice.text = "Diagnostics controls are available in Advanced mode."
	add_child(_advanced_notice)

	_add_toggle_row("Enable gobo debug overrides", "gobo_debug_override_enabled")
	_add_option_row("Comparison", "gobo_debug_comparison_mode", ["Rotation + Shake", "Rotation only", "Shake only"])
	_add_toggle_row("Enable shake", "gobo_debug_shake_enabled")
	_add_slider_row("Shake amplitude (deg)", "gobo_debug_shake_amplitude_deg", 0.0, GoboShakeLimitsScript.MAX_SHAKE_AMPLITUDE_DEG, 0.01)
	_add_slider_row("Shake frequency (Hz)", "gobo_debug_shake_frequency_hz", 0.0, GoboShakeLimitsScript.MAX_SHAKE_FREQUENCY_HZ, 0.05)
	_add_option_row("Shake waveform", "gobo_debug_shake_waveform", ["Triangle", "Sine", "Square"])

func set_advanced_mode(enabled: bool) -> void:
	_advanced_notice.visible = not enabled
	for key in _controls.keys():
		var control: Control = _controls[key]
		if control == null:
			continue
		if control is CheckBox:
			control.visible = enabled
		else:
			control.get_parent().visible = enabled

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

func _add_toggle_row(label_text: String, key: String) -> void:
	var toggle: CheckBox = CheckBox.new()
	toggle.text = label_text
	toggle.toggled.connect(func(value: bool) -> void:
		setting_changed.emit(key, value)
	)
	add_child(toggle)
	_controls[key] = toggle

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

func _update_value_labels() -> void:
	if _value_labels.has("gobo_debug_shake_amplitude_deg"):
		(_value_labels["gobo_debug_shake_amplitude_deg"] as Label).text = "%.2f" % float((_controls["gobo_debug_shake_amplitude_deg"] as HSlider).value)
	if _value_labels.has("gobo_debug_shake_frequency_hz"):
		(_value_labels["gobo_debug_shake_frequency_hz"] as Label).text = "%.2f" % float((_controls["gobo_debug_shake_frequency_hz"] as HSlider).value)
