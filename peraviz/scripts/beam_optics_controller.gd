extends RefCounted
class_name BeamOpticsController

static func BuildBeamParams(light: SpotLight3D, beam_angle_deg: float, beam_color: Color,
		normalized_dimmer: float, scaled_intensity: float, lens_radius: float,
		visual_settings: Dictionary, defaults: Dictionary) -> Dictionary:
	var params := {
		"beam_angle": beam_angle_deg,
		"spot_angle_deg": beam_angle_deg,
		"beam_range": light.spot_range,
		"beam_color": beam_color,
		"normalized_dimmer": clamp(normalized_dimmer, 0.0, 1.0),
		"scaled_intensity": scaled_intensity,
		"beam_intensity": scaled_intensity,
		"lens_radius": lens_radius,
		"is_visible": light.visible,
		"beam_softness": float(visual_settings.get("beam_softness", defaults.get("beam_softness", 0.35))),
		"beam_radial_falloff": float(visual_settings.get("beam_radial_falloff", defaults.get("beam_radial_falloff", 1.1))),
		"beam_longitudinal_falloff": float(visual_settings.get("beam_longitudinal_falloff", defaults.get("beam_longitudinal_falloff", 1.0))),
		"haze_density_multiplier": float(visual_settings.get("haze_density_multiplier", defaults.get("haze_density_multiplier", 0.35))),
		"haze_density": float(visual_settings.get("haze_density_multiplier", defaults.get("haze_density_multiplier", 0.35))),
		"gobo_scale": float(visual_settings.get("gobo_scale", defaults.get("gobo_scale", 1.0))),
		"gobo_rotation_deg": float(visual_settings.get("gobo_rotation_deg", defaults.get("gobo_rotation_deg", 0.0))),
		"lens_offset_m": float(visual_settings.get("lens_offset_m", defaults.get("lens_offset_m", 0.0))),
		"near_offset": float(visual_settings.get("near_offset", defaults.get("near_offset", 0.0))),
		"lens_shift_x": float(visual_settings.get("lens_shift_x", defaults.get("lens_shift_x", 0.0))),
		"lens_shift_y": float(visual_settings.get("lens_shift_y", defaults.get("lens_shift_y", 0.0))),
		"beam_debug_optics": bool(visual_settings.get("beam_debug_optics", false)),
		"spot_range": light.spot_range,
		"spot_angle_half_deg": light.spot_angle,
		"beam_angle_source": "gdtf_full_angle_deg",
	}
	return params

static func BuildGoboControls(controls: Dictionary, visual_settings: Dictionary, defaults: Dictionary) -> Dictionary:
	var gobo_controls: Dictionary = controls.duplicate(true)
	gobo_controls["prefer_native_fog_projector"] = bool(visual_settings.get("use_native_fog_projector_gobos", true))
	gobo_controls["gobo_scale"] = float(visual_settings.get("gobo_scale", defaults.get("gobo_scale", 1.0)))
	gobo_controls["gobo_rotation_deg"] = float(visual_settings.get("gobo_rotation_deg", defaults.get("gobo_rotation_deg", 0.0)))
	return gobo_controls
