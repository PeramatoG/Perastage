extends RefCounted
class_name BeamOpticsController

const DEFAULT_MASTER_OPTICS := {
	"beam_softness": 0.32,
	"beam_radial_falloff": 1.25,
	"beam_longitudinal_falloff": 1.1,
	"haze_density_multiplier": 0.22,
	"gobo_scale": 1.0,
	"gobo_rotation_deg": 0.0,
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
	var base_rotation: float = float(visual_settings.get("gobo_rotation_deg", merged_defaults.get("gobo_rotation_deg", 0.0)))
	if bool(controls.get("has_gobo_index", false)):
		base_rotation = _resolve_index_rotation_deg_from_controls(controls, base_rotation)
	if bool(controls.get("has_gobo_rotation", false)):
		base_rotation += _resolve_continuous_rotation_deg_from_controls(controls)
	gobo_controls["gobo_rotation_deg"] = base_rotation
	return gobo_controls

static func _resolve_index_rotation_deg_from_controls(controls: Dictionary, default_rotation: float) -> float:
	var norm_value: float = -1.0
	if controls.has("gobo_index_norm"):
		norm_value = clamp(float(controls.get("gobo_index_norm", 0.0)), 0.0, 1.0)
	if controls.has("gobo_runtime_bindings"):
		var runtime_bindings: Array = controls.get("gobo_runtime_bindings", [])
		for item in runtime_bindings:
			if item is not Dictionary:
				continue
			var binding_norm: float = float(item.get("index_norm", -1.0))
			if binding_norm >= 0.0:
				norm_value = clamp(binding_norm, 0.0, 1.0)
				break
	if norm_value < 0.0:
		return default_rotation
	return lerp(0.0, 360.0, norm_value)

static func _resolve_continuous_rotation_deg_from_controls(controls: Dictionary) -> float:
	var norm_value: float = clamp(float(controls.get("gobo_rotation_norm", 0.5)), 0.0, 1.0)
	if controls.has("gobo_runtime_bindings"):
		var runtime_bindings: Array = controls.get("gobo_runtime_bindings", [])
		for item in runtime_bindings:
			if item is not Dictionary:
				continue
			var binding_norm: float = float(item.get("rotation_norm", -1.0))
			if binding_norm >= 0.0:
				norm_value = clamp(binding_norm, 0.0, 1.0)
				break
	# Center value (0.5) means no movement, below is one direction, above opposite direction.
	return (norm_value - 0.5) * 360.0
