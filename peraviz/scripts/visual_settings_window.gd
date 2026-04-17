@tool
extends Window

class_name VisualSettingsWindow

signal settings_changed(settings: Dictionary)

const GoboShakeLimitsScript = preload("res://scripts/gobo_shake_limits.gd")
const UiVisibilityPolicyScript = preload("res://scripts/ui/ui_visibility_policy.gd")
const RendererSettingsPanelScript = preload("res://scripts/ui/visual/renderer_settings_panel.gd")
const EnvironmentSettingsPanelScript = preload("res://scripts/ui/visual/environment_settings_panel.gd")
const DebugGoboSettingsPanelScript = preload("res://scripts/ui/visual/debug_gobo_settings_panel.gd")

const DEFAULT_SETTINGS := {
	"ambient_multiplier": 0.08,
	"spot_multiplier": 1.0,
	"beam_multiplier": 20.0,
	"bloom_multiplier": 0.0,
	"beam_render_mode": 0,
	"beam_quality": 2,
	"beam_haze_density": 0.17,
	"beam_anisotropy": 0.62,
	"beam_noise_amount": 0.06,
	"beam_noise_scale": 1.4,
	"beam_softness": 0.32,
	"beam_radial_falloff": 1.25,
	"beam_longitudinal_falloff": 1.1,
	"haze_density_multiplier": 0.22,
	"gobo_scale": 1.0,
	"gobo_rotation_deg": 0.0,
	"beam_gobo_alignment_rotation_deg": 0.0,
	"beam_gobo_mirror_x": true,
	"beam_gobo_mirror_z": false,
	"lens_offset_m": 0.015,
	"near_offset": 0.015,
	"lens_shift_x": 0.0,
	"lens_shift_y": 0.0,
	"beam_debug_optics": false,
	"ambient_fog_density": 0.0,
	"volumetric_fog_density": 0.0,
	"volumetric_fog_fade": 0.02,
	"light_volumetric_fog_energy": 12.0,
	"use_native_fog_projector_gobos": true,
	"background_color": Color(0.129412, 0.137255, 0.156863, 1.0),
	"environment_enabled": true,
	"environment_use_continuous_cycle": false,
	"environment_current_preset": 1,
	"environment_time_of_day": 0.4,
	"environment_auto_advance": false,
	"environment_cycle_speed": 0.02,
	"environment_allow_blackout_night": true,
	"environment_day_light_intensity": 0.7,
	"environment_dusk_light_intensity": 0.18,
	"environment_night_light_intensity": 0.02,
	"environment_moon_light_intensity": 0.012,
	"environment_ambient_energy_day": 0.08,
	"environment_ambient_energy_night": 0.008,
	"environment_horizon_warmth": 0.6,
	"environment_horizon_intensity": 1.0,
	"gobo_debug_override_enabled": false,
	"gobo_debug_comparison_mode": 0,
	"gobo_debug_shake_enabled": false,
	"gobo_debug_shake_amplitude_deg": GoboShakeLimitsScript.DEFAULT_DEBUG_SHAKE_AMPLITUDE_DEG,
	"gobo_debug_shake_frequency_hz": 1.0,
	"gobo_debug_shake_waveform": 0,
}

const QUICK_PRESETS := {
	"Preview": {
		"beam_quality": 0,
		"beam_multiplier": 12.0,
		"spot_multiplier": 0.8,
		"bloom_multiplier": 0.0,
		"volumetric_fog_density": 0.0015,
		"light_volumetric_fog_energy": 8.0,
		"environment_current_preset": 1,
	},
	"Balanced": {
		"beam_quality": 1,
		"beam_multiplier": 20.0,
		"spot_multiplier": 1.0,
		"bloom_multiplier": 0.25,
		"volumetric_fog_density": 0.003,
		"light_volumetric_fog_energy": 12.0,
		"environment_current_preset": 2,
	},
	"Quality": {
		"beam_quality": 2,
		"beam_multiplier": 26.0,
		"spot_multiplier": 1.1,
		"bloom_multiplier": 0.4,
		"volumetric_fog_density": 0.004,
		"light_volumetric_fog_energy": 16.0,
		"environment_current_preset": 1,
	},
}

var _settings: Dictionary = DEFAULT_SETTINGS.duplicate(true)
var _ui_visibility_policy = UiVisibilityPolicyScript
var _tabs: TabContainer
var _advanced_mode_toggle: CheckBox
var _renderer_panel: RendererSettingsPanel
var _environment_panel: EnvironmentSettingsPanel
var _debug_panel: DebugGoboSettingsPanel

func _init() -> void:
	title = "Visual Settings"
	size = Vector2i(620, 760)
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

func set_ui_visibility_policy(policy_script: Variant) -> void:
	if policy_script != null:
		_ui_visibility_policy = policy_script
	if is_node_ready() and _debug_panel != null:
		_debug_panel.visible = _is_debug_visible()

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

	var layout: VBoxContainer = VBoxContainer.new()
	layout.set_anchors_preset(Control.PRESET_FULL_RECT)
	layout.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	layout.size_flags_vertical = Control.SIZE_EXPAND_FILL
	layout.add_theme_constant_override("separation", 10)
	root.add_child(layout)

	var mode_row: HBoxContainer = HBoxContainer.new()
	mode_row.add_theme_constant_override("separation", 8)
	layout.add_child(mode_row)

	_advanced_mode_toggle = CheckBox.new()
	_advanced_mode_toggle.text = "Advanced mode"
	_advanced_mode_toggle.button_pressed = false
	_advanced_mode_toggle.toggled.connect(_on_advanced_mode_toggled)
	mode_row.add_child(_advanced_mode_toggle)

	var presets_label: Label = Label.new()
	presets_label.text = "Quick presets"
	mode_row.add_child(presets_label)

	for preset_name in ["Preview", "Balanced", "Quality"]:
		var preset_button: Button = Button.new()
		preset_button.text = preset_name
		preset_button.pressed.connect(func() -> void:
			_apply_quick_preset(preset_name)
		)
		mode_row.add_child(preset_button)

	_tabs = TabContainer.new()
	_tabs.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
	layout.add_child(_tabs)

	_renderer_panel = RendererSettingsPanelScript.new()
	_renderer_panel.name = "Renderer"
	_renderer_panel.setting_changed.connect(_on_panel_setting_changed)
	_tabs.add_child(_renderer_panel)

	_environment_panel = EnvironmentSettingsPanelScript.new()
	_environment_panel.name = "Environment"
	_environment_panel.setting_changed.connect(_on_panel_setting_changed)
	_tabs.add_child(_environment_panel)

	_debug_panel = DebugGoboSettingsPanelScript.new()
	_debug_panel.name = "Diagnostics"
	_debug_panel.setting_changed.connect(_on_panel_setting_changed)
	_debug_panel.visible = _is_debug_visible()
	_tabs.add_child(_debug_panel)

	var actions_row: HBoxContainer = HBoxContainer.new()
	actions_row.alignment = BoxContainer.ALIGNMENT_END
	layout.add_child(actions_row)

	var reset_button: Button = Button.new()
	reset_button.text = "Reset"
	reset_button.pressed.connect(_on_reset_pressed)
	actions_row.add_child(reset_button)

func _apply_settings_to_controls() -> void:
	if _renderer_panel != null:
		_renderer_panel.apply_settings(_settings)
	if _environment_panel != null:
		_environment_panel.apply_settings(_settings)
	if _debug_panel != null:
		_debug_panel.apply_settings(_settings)
	_apply_mode_to_panels()

func _apply_mode_to_panels() -> void:
	var advanced_enabled: bool = _advanced_mode_toggle != null and _advanced_mode_toggle.button_pressed
	if _renderer_panel != null:
		_renderer_panel.set_advanced_mode(advanced_enabled)
	if _environment_panel != null:
		_environment_panel.set_advanced_mode(advanced_enabled)
	if _debug_panel != null:
		_debug_panel.set_advanced_mode(advanced_enabled)


func set_advanced_mode_enabled(enabled: bool) -> void:
	if _advanced_mode_toggle == null:
		return
	_advanced_mode_toggle.button_pressed = enabled
	_apply_mode_to_panels()

func is_advanced_mode_enabled() -> bool:
	return _advanced_mode_toggle != null and _advanced_mode_toggle.button_pressed

func set_active_tab_index(tab_index: int) -> void:
	if _tabs == null:
		return
	if _tabs.get_tab_count() <= 0:
		return
	_tabs.current_tab = clampi(tab_index, 0, _tabs.get_tab_count() - 1)

func get_active_tab_index() -> int:
	if _tabs == null:
		return 0
	return _tabs.current_tab

func _apply_quick_preset(preset_name: String) -> void:
	if not QUICK_PRESETS.has(preset_name):
		return
	var preset_values: Dictionary = QUICK_PRESETS[preset_name]
	for key in preset_values.keys():
		_settings[key] = preset_values[key]
	_apply_settings_to_controls()
	_emit_settings_changed()

func _on_panel_setting_changed(key: String, value: Variant) -> void:
	_settings[key] = value
	_emit_settings_changed()

func _on_advanced_mode_toggled(_enabled: bool) -> void:
	_apply_mode_to_panels()

func _on_reset_pressed() -> void:
	_settings = DEFAULT_SETTINGS.duplicate(true)
	_apply_settings_to_controls()
	_emit_settings_changed()

func _emit_settings_changed() -> void:
	settings_changed.emit(_settings.duplicate(true))

func _is_debug_visible() -> bool:
	return _ui_visibility_policy != null and _ui_visibility_policy.is_module_visible(_ui_visibility_policy.MODULE_DEBUG)

func _on_close_requested() -> void:
	hide()
