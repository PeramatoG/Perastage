extends BeamRendererBase

class_name VolumetricBeamRenderer

const BEAM_META_KEY: String = "peraviz_volumetric_beam"
const VOLUMETRIC_INTENSITY_SCALE: float = 4.0
const VOLUMETRIC_INTENSITY_RESPONSE_EXPONENT: float = 2.2
const INTENSITY_REFERENCE_MAX: float = 20.0
const VOLUMETRIC_OVERDRIVE_BRIGHTNESS_MAX: float = 30.0
const DEBUG_AXIS_KEY: String = "peraviz_beam_debug_axis"
const LAST_UNIFORMS_META_KEY: String = "peraviz_last_beam_uniforms"
const SHAPE_MODE_GOBO_PRISM: String = "gobo_prism"
const SHAPE_MODE_CONE: String = "cone"

var _beam_material_template: ShaderMaterial
var _camera: Camera3D
var _settings: Dictionary = {}
var _shape_providers: Dictionary = {}
var _active_shape_provider: VolumetricBeamShapeProvider

func _init() -> void:
	_beam_material_template = ShaderMaterial.new()
	_beam_material_template.shader = load("res://scripts/shaders/volumetric_beam.gdshader")
	_shape_providers[SHAPE_MODE_GOBO_PRISM] = VolumetricGoboPrismShapeProvider.new()
	_shape_providers[SHAPE_MODE_CONE] = VolumetricConeShapeProvider.new()
	_active_shape_provider = _shape_providers[SHAPE_MODE_GOBO_PRISM] as VolumetricBeamShapeProvider

func configure(view_camera: Camera3D, settings: Dictionary) -> void:
	_camera = view_camera
	_settings = settings.duplicate(true)
	_active_shape_provider = _select_shape_provider()

func ensure_beam(light: SpotLight3D) -> void:
	if light.has_meta(BEAM_META_KEY):
		return
	var beam := MeshInstance3D.new()
	beam.name = "PeravizVolumetricBeam"
	beam.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	beam.material_override = _beam_material_template.duplicate(true)
	beam.rotation_degrees.x = 90.0
	beam.visible = false
	light.add_child(beam)
	light.set_meta(BEAM_META_KEY, beam)
	_ensure_debug_axis(light)

func update_beam(light: SpotLight3D, params: Dictionary) -> void:
	ensure_beam(light)
	var beam: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
	if beam == null:
		return

	var intensity_max: float = max(float(params.get("intensity_max", 100.0)), 0.01)
	var intensity: float = clamp(float(params.get("scaled_intensity", 0.0)), 0.0, intensity_max)
	var reference_max: float = max(INTENSITY_REFERENCE_MAX, 0.01)
	var beam_intensity_norm: float = clamp(intensity / reference_max, 0.0, 1.0)
	var perceptual_intensity: float = pow(beam_intensity_norm, VOLUMETRIC_INTENSITY_RESPONSE_EXPONENT)
	var overdrive_norm: float = 0.0
	if intensity_max > reference_max:
		overdrive_norm = clamp((intensity - reference_max) / (intensity_max - reference_max), 0.0, 1.0)
	var threshold: float = float(params.get("intensity_visibility_threshold", 0.015))
	var beam_range: float = max(float(params.get("beam_range", 0.1)), 0.01)
	var beam_angle: float = max(float(params.get("beam_angle", 1.0)), 0.1)

	if not bool(params.get("is_visible", true)) or intensity <= threshold:
		beam.visible = false
		_set_instance_shader_parameter_if_changed(light, beam, "beam_visibility", 0.0)
		var hidden_axis: MeshInstance3D = _ensure_debug_axis(light)
		if hidden_axis != null:
			hidden_axis.visible = false
		return
	var debug_axis: MeshInstance3D = _ensure_debug_axis(light)
	if debug_axis != null:
		debug_axis.visible = bool(params.get("beam_debug_optics", false))

	var beam_color: Color = params.get("beam_color", Color.WHITE)
	var shape_result: Dictionary = _active_shape_provider.apply_shape(beam, light, params)
	var gobo_projection_radius: float = max(float(shape_result.get("gobo_projection_radius", 0.1)), 0.001)
	var beam_rotation_deg: float = float(shape_result.get("beam_rotation_deg", 0.0))

	if bool(params.get("beam_debug_optics", false)):
		print("[PeravizBeamOptics] mode=", _active_shape_provider.shape_mode(), " angle_deg=", beam_angle, " range_m=", beam_range, " radius_end_m=", gobo_projection_radius)

	beam.visible = true

	var intensity_alpha: float = clamp((intensity / reference_max) * VOLUMETRIC_INTENSITY_SCALE, 0.0, 3.6)
	_set_instance_shader_parameter_if_changed(light, beam, "base_color", Color(beam_color.r, beam_color.g, beam_color.b, intensity_alpha))
	_set_instance_shader_parameter_if_changed(light, beam, "beam_visibility", 1.0)
	var overdrive_brightness_gain: float = lerp(1.0, VOLUMETRIC_OVERDRIVE_BRIGHTNESS_MAX, overdrive_norm)
	_set_instance_shader_parameter_if_changed(light, beam, "max_brightness", lerp(8.0, 120.0, beam_intensity_norm) * overdrive_brightness_gain)
	_set_instance_shader_parameter_if_changed(light, beam, "beam_noise_amount", float(_settings.get("beam_noise_amount", 0.06)))
	_set_instance_shader_parameter_if_changed(light, beam, "beam_noise_scale", float(_settings.get("beam_noise_scale", 1.4)))
	var haze_density: float = max(float(params.get("haze_density", params.get("haze_density_multiplier", 0.22))), 0.01)
	_set_instance_shader_parameter_if_changed(light, beam, "beam_haze_density", float(_settings.get("beam_haze_density", 0.17)) * haze_density)
	_set_instance_shader_parameter_if_changed(light, beam, "haze_density", max(haze_density, 0.2))
	_set_instance_shader_parameter_if_changed(light, beam, "beam_anisotropy", float(_settings.get("beam_anisotropy", 0.62)))
	_set_instance_shader_parameter_if_changed(light, beam, "beam_quality", int(_settings.get("beam_quality", 1)))
	_set_instance_shader_parameter_if_changed(light, beam, "radial_falloff", max(float(params.get("beam_radial_falloff", 1.1)), 0.05))
	_set_instance_shader_parameter_if_changed(light, beam, "longitudinal_falloff", max(float(params.get("beam_longitudinal_falloff", 1.0)), 0.05))
	_set_instance_shader_parameter_if_changed(light, beam, "beam_softness", clamp(float(params.get("beam_softness", 0.35)), 0.02, 1.0))
	_set_instance_shader_parameter_if_changed(light, beam, "gobo_scale", max(float(params.get("gobo_scale", 1.0)), 0.05))
	_set_instance_shader_parameter_if_changed(light, beam, "gobo_rotation_deg", beam_rotation_deg)
	_set_instance_shader_parameter_if_changed(light, beam, "cone_height", max(beam_range, 0.001))
	_set_instance_shader_parameter_if_changed(light, beam, "gobo_projection_radius", gobo_projection_radius)
	_set_instance_shader_parameter_if_changed(light, beam, "beam_intensity", perceptual_intensity)
	_set_instance_shader_parameter_if_changed(light, beam, "beam_overdrive", overdrive_norm)
	var far_fade_end: float = max(400.0, beam_range * 12.0)
	var beam_material: ShaderMaterial = beam.material_override as ShaderMaterial
	if beam_material != null:
		_set_material_shader_parameter_if_changed(light, beam_material, "near_fade_end", max(2.0, beam_range * 0.2))
		_set_material_shader_parameter_if_changed(light, beam_material, "far_fade_start", far_fade_end * 0.6)
		_set_material_shader_parameter_if_changed(light, beam_material, "far_fade_end", far_fade_end)
		_set_material_shader_parameter_if_changed(light, beam_material, "use_gobo", false)
		_set_material_shader_parameter_if_changed(light, beam_material, "gobo_invert", false)
		_set_material_shader_parameter_if_changed(light, beam_material, "gobo_mirror_x", bool(shape_result.get("mirror_x", true)))
		_set_material_shader_parameter_if_changed(light, beam_material, "gobo_mirror_z", bool(shape_result.get("mirror_z", false)))
		_set_material_shader_parameter_if_changed(light, beam_material, "depth_feather_enabled", false)


func _set_instance_shader_parameter_if_changed(light: SpotLight3D, instance: GeometryInstance3D, name: String, value: Variant) -> void:
	if instance == null:
		return
	var uniforms := _ensure_last_uniforms_meta(light)
	var previous: Variant = uniforms.get(name, null)
	if _uniform_values_equal(previous, value):
		return
	instance.set_instance_shader_parameter(name, value)
	uniforms[name] = value

func _set_material_shader_parameter_if_changed(light: SpotLight3D, material: ShaderMaterial, name: String, value: Variant) -> void:
	if material == null:
		return
	var uniforms := _ensure_last_uniforms_meta(light)
	var key: String = "material::" + name
	var previous: Variant = uniforms.get(key, null)
	if _uniform_values_equal(previous, value):
		return
	material.set_shader_parameter(name, value)
	uniforms[key] = value

func _ensure_last_uniforms_meta(light: SpotLight3D) -> Dictionary:
	if light == null:
		return {}
	if light.has_meta(LAST_UNIFORMS_META_KEY):
		var existing: Variant = light.get_meta(LAST_UNIFORMS_META_KEY)
		if existing is Dictionary:
			return existing as Dictionary
	var created: Dictionary = {}
	light.set_meta(LAST_UNIFORMS_META_KEY, created)
	return created

func _uniform_values_equal(previous: Variant, current: Variant) -> bool:
	if previous == null and current == null:
		return true
	if previous == null or current == null:
		return false
	if previous is float and current is float:
		return is_equal_approx(previous, current)
	if previous is Color and current is Color:
		return _color_equal_approx(previous, current)
	if previous is Vector2 and current is Vector2:
		return _vector2_equal_approx(previous, current)
	if previous is Vector3 and current is Vector3:
		return _vector3_equal_approx(previous, current)
	if previous is Vector4 and current is Vector4:
		return _vector4_equal_approx(previous, current)
	return previous == current

func _color_equal_approx(a: Color, b: Color) -> bool:
	return is_equal_approx(a.r, b.r) and is_equal_approx(a.g, b.g) and is_equal_approx(a.b, b.b) and is_equal_approx(a.a, b.a)

func _vector2_equal_approx(a: Vector2, b: Vector2) -> bool:
	return is_equal_approx(a.x, b.x) and is_equal_approx(a.y, b.y)

func _vector3_equal_approx(a: Vector3, b: Vector3) -> bool:
	return is_equal_approx(a.x, b.x) and is_equal_approx(a.y, b.y) and is_equal_approx(a.z, b.z)

func _vector4_equal_approx(a: Vector4, b: Vector4) -> bool:
	return is_equal_approx(a.x, b.x) and is_equal_approx(a.y, b.y) and is_equal_approx(a.z, b.z) and is_equal_approx(a.w, b.w)

func cleanup_beam(light: SpotLight3D) -> void:
	if not light.has_meta(BEAM_META_KEY):
		return
	var beam: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
	if beam != null and is_instance_valid(beam):
		beam.queue_free()
	light.remove_meta(BEAM_META_KEY)
	for provider in _shape_providers.values():
		if provider is VolumetricBeamShapeProvider:
			(provider as VolumetricBeamShapeProvider).clear_cache()

func _select_shape_provider() -> VolumetricBeamShapeProvider:
	var requested_mode: String = str(_settings.get("volumetric_shape_mode", SHAPE_MODE_GOBO_PRISM)).to_lower()
	if _shape_providers.has(requested_mode):
		return _shape_providers[requested_mode] as VolumetricBeamShapeProvider
	return _shape_providers[SHAPE_MODE_GOBO_PRISM] as VolumetricBeamShapeProvider

func _ensure_debug_axis(light: SpotLight3D) -> MeshInstance3D:
	if light.has_meta(DEBUG_AXIS_KEY):
		var existing: MeshInstance3D = light.get_meta(DEBUG_AXIS_KEY) as MeshInstance3D
		if existing != null and is_instance_valid(existing):
			return existing
	var axis := MeshInstance3D.new()
	axis.name = "PeravizBeamDebugAxis"
	axis.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var mesh := BoxMesh.new()
	mesh.size = Vector3(0.03, 0.03, 2.0)
	axis.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(1.0, 0.1, 0.1, 0.95)
	material.emission_enabled = true
	material.emission = Color(1.0, 0.0, 0.0, 1.0)
	axis.material_override = material
	axis.position = Vector3(0.0, 0.0, -1.0)
	light.add_child(axis)
	light.set_meta(DEBUG_AXIS_KEY, axis)
	return axis
