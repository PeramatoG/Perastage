extends RefCounted
class_name BeamOpticsController

const GoboShakeLimitsScript = preload("res://scripts/gobo_shake_limits.gd")

const DEFAULT_MASTER_OPTICS := {
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
}

static func BuildDefaultMasterOptics() -> Dictionary:
	return DEFAULT_MASTER_OPTICS.duplicate(true)

static func BuildBeamParams(light: SpotLight3D, beam_angle_deg: float, beam_color: Color,
		normalized_dimmer: float, scaled_intensity: float, lens_radius: float,
		visual_settings: Dictionary, defaults: Dictionary) -> Dictionary:
	var merged_defaults: Dictionary = BuildDefaultMasterOptics()
	for key in defaults.keys():
		merged_defaults[key] = defaults[key]
	var params := {
		"beam_angle": beam_angle_deg,
		"beam_angle_deg": beam_angle_deg,
		"spot_angle_deg": beam_angle_deg,
		"beam_range": light.spot_range,
		"beam_color": beam_color,
		"normalized_dimmer": clamp(normalized_dimmer, 0.0, 1.0),
		"scaled_intensity": scaled_intensity,
		"beam_intensity": scaled_intensity,
		"lens_radius": lens_radius,
		"is_visible": light.visible,
		"beam_softness": float(visual_settings.get("beam_softness", merged_defaults.get("beam_softness", 0.32))),
		"beam_radial_falloff": float(visual_settings.get("beam_radial_falloff", merged_defaults.get("beam_radial_falloff", 1.25))),
		"beam_longitudinal_falloff": float(visual_settings.get("beam_longitudinal_falloff", merged_defaults.get("beam_longitudinal_falloff", 1.1))),
		"haze_density_multiplier": float(visual_settings.get("haze_density_multiplier", merged_defaults.get("haze_density_multiplier", 0.22))),
		"haze_density": float(visual_settings.get("haze_density_multiplier", merged_defaults.get("haze_density_multiplier", 0.22))),
		"gobo_scale": float(visual_settings.get("gobo_scale", merged_defaults.get("gobo_scale", 1.0))),
		"gobo_rotation_deg": float(visual_settings.get("gobo_rotation_deg", merged_defaults.get("gobo_rotation_deg", 0.0))),
		"beam_gobo_alignment_rotation_deg": float(visual_settings.get("beam_gobo_alignment_rotation_deg", merged_defaults.get("beam_gobo_alignment_rotation_deg", 0.0))),
		"beam_gobo_mirror_x": bool(visual_settings.get("beam_gobo_mirror_x", merged_defaults.get("beam_gobo_mirror_x", true))),
		"beam_gobo_mirror_z": bool(visual_settings.get("beam_gobo_mirror_z", merged_defaults.get("beam_gobo_mirror_z", true))),
		"lens_offset_m": float(visual_settings.get("lens_offset_m", merged_defaults.get("lens_offset_m", 0.015))),
		"near_offset": float(visual_settings.get("near_offset", merged_defaults.get("near_offset", 0.015))),
		"lens_shift_x": float(visual_settings.get("lens_shift_x", merged_defaults.get("lens_shift_x", 0.0))),
		"lens_shift_y": float(visual_settings.get("lens_shift_y", merged_defaults.get("lens_shift_y", 0.0))),
		"beam_debug_optics": bool(visual_settings.get("beam_debug_optics", false)),
		"spot_range": light.spot_range,
		"spot_angle_half_deg": light.spot_angle,
		"beam_angle_source": "gdtf_full_angle_deg",
	}
	return params

static func BuildGoboControls(controls: Dictionary, visual_settings: Dictionary, defaults: Dictionary) -> Dictionary:
	var merged_defaults: Dictionary = BuildDefaultMasterOptics()
	for key in defaults.keys():
		merged_defaults[key] = defaults[key]
	var gobo_controls: Dictionary = controls.duplicate(true)
	gobo_controls["prefer_native_fog_projector"] = bool(visual_settings.get("use_native_fog_projector_gobos", true))
	gobo_controls["gobo_scale"] = float(visual_settings.get("gobo_scale", merged_defaults.get("gobo_scale", 1.0)))
	gobo_controls["gobo_rotation_deg"] = float(visual_settings.get("gobo_rotation_deg", merged_defaults.get("gobo_rotation_deg", 0.0)))
	if OS.is_debug_build():
		gobo_controls["gobo_debug_override_enabled"] = bool(visual_settings.get("gobo_debug_override_enabled", false))
		gobo_controls["gobo_debug_comparison_mode"] = int(visual_settings.get("gobo_debug_comparison_mode", 0))
		gobo_controls["gobo_debug_shake_enabled"] = bool(visual_settings.get("gobo_debug_shake_enabled", false))
		gobo_controls["gobo_debug_shake_amplitude_deg"] = GoboShakeLimitsScript.clamp_amplitude_deg(float(visual_settings.get("gobo_debug_shake_amplitude_deg", GoboShakeLimitsScript.DEFAULT_DEBUG_SHAKE_AMPLITUDE_DEG)))
		gobo_controls["gobo_debug_shake_frequency_hz"] = GoboShakeLimitsScript.clamp_frequency_hz(float(visual_settings.get("gobo_debug_shake_frequency_hz", GoboShakeLimitsScript.DEFAULT_DEBUG_SHAKE_FREQUENCY_HZ)))
		gobo_controls["gobo_debug_shake_waveform"] = int(visual_settings.get("gobo_debug_shake_waveform", 0))
	return gobo_controls
