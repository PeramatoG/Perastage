extends RefCounted
class_name FogVolumeGoboBeamController

const FOG_VOLUME_NODE_NAME: String = "PeravizFogVolumeGoboBeam"
const FOG_SHADER_PATH: String = "res://scripts/shaders/fog_volume_gobo_beam.gdshader"
const LAST_UNIFORMS_META_KEY: String = "peraviz_last_beam_uniforms"

func update_for_light(light: SpotLight3D, beam_params: Dictionary, gobo_texture: Texture2D, visual_settings: Dictionary) -> void:
	if light == null or not is_instance_valid(light):
		return
	if gobo_texture == null:
		clear_for_light(light)
		return

	var fog_volume: FogVolume = _ensure_volume(light)
	if fog_volume == null:
		return

	var beam_range: float = max(float(beam_params.get("beam_range", light.spot_range)), 0.1)
	var beam_angle: float = max(float(beam_params.get("beam_angle", light.spot_angle * 2.0)), 0.1)
	var cone_radius: float = tan(deg_to_rad(beam_angle * 0.5)) * beam_range
	fog_volume.size = Vector3(max(cone_radius * 2.0, 0.1), max(cone_radius * 2.0, 0.1), beam_range)
	fog_volume.position = Vector3(0.0, 0.0, -beam_range * 0.5)
	fog_volume.visible = bool(beam_params.get("is_visible", true))

	var fog_material: ShaderMaterial = fog_volume.material as ShaderMaterial
	if fog_material == null:
		return
	_set_fog_shader_parameter_if_changed(light, fog_material, "gobo_texture", gobo_texture, true)
	_set_fog_shader_parameter_if_changed(light, fog_material, "light_color", Color(beam_params.get("beam_color", Color.WHITE)))
	var haze_density: float = max(float(beam_params.get("haze_density_multiplier", 0.22)), 0.01)
	_set_fog_shader_parameter_if_changed(light, fog_material, "density_scale", float(visual_settings.get("fog_volume_density_scale", 0.9)) * haze_density)
	_set_fog_shader_parameter_if_changed(light, fog_material, "emission_strength", float(visual_settings.get("fog_volume_emission_strength", 0.55)))
	_set_fog_shader_parameter_if_changed(light, fog_material, "edge_softness", float(visual_settings.get("fog_volume_edge_softness", 0.72)))
	_set_fog_shader_parameter_if_changed(light, fog_material, "invert_gobo", bool(visual_settings.get("fog_volume_invert_gobo", false)))
	_set_fog_shader_parameter_if_changed(light, fog_material, "gobo_scale", max(float(beam_params.get("gobo_scale", 1.0)), 0.05), true)
	_set_fog_shader_parameter_if_changed(light, fog_material, "gobo_rotation_deg", float(beam_params.get("gobo_rotation_deg", 0.0)), true)
	_set_fog_shader_parameter_if_changed(light, fog_material, "radial_falloff", max(float(beam_params.get("beam_radial_falloff", 1.25)), 0.05))
	_set_fog_shader_parameter_if_changed(light, fog_material, "longitudinal_falloff", max(float(beam_params.get("beam_longitudinal_falloff", 1.1)), 0.05))


func _set_fog_shader_parameter_if_changed(light: SpotLight3D, material: ShaderMaterial, name: String, value: Variant, force_update: bool = false) -> void:
	if material == null:
		return
	var uniforms := _ensure_last_uniforms_meta(light)
	var key: String = "fog::" + name
	var previous: Variant = uniforms.get(key, null)
	if not force_update and _uniform_values_equal(previous, value):
		return
	material.set_shader_parameter(name, value)
	uniforms[key] = value

func _ensure_last_uniforms_meta(light: SpotLight3D) -> Dictionary:
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
		return is_equal_approx(previous.r, current.r) and is_equal_approx(previous.g, current.g) and is_equal_approx(previous.b, current.b) and is_equal_approx(previous.a, current.a)
	if previous is Vector2 and current is Vector2:
		return is_equal_approx(previous.x, current.x) and is_equal_approx(previous.y, current.y)
	if previous is Vector3 and current is Vector3:
		return is_equal_approx(previous.x, current.x) and is_equal_approx(previous.y, current.y) and is_equal_approx(previous.z, current.z)
	if previous is Vector4 and current is Vector4:
		return is_equal_approx(previous.x, current.x) and is_equal_approx(previous.y, current.y) and is_equal_approx(previous.z, current.z) and is_equal_approx(previous.w, current.w)
	return previous == current

func clear_for_light(light: SpotLight3D) -> void:
	if light == null or not is_instance_valid(light):
		return
	var existing: Node = light.get_node_or_null(FOG_VOLUME_NODE_NAME)
	if existing != null:
		existing.queue_free()

func _ensure_volume(light: SpotLight3D) -> FogVolume:
	var existing: Node = light.get_node_or_null(FOG_VOLUME_NODE_NAME)
	if existing is FogVolume and is_instance_valid(existing):
		return existing as FogVolume

	var fog_volume := FogVolume.new()
	fog_volume.name = FOG_VOLUME_NODE_NAME
	_assign_fog_volume_shape(fog_volume)
	fog_volume.material = ShaderMaterial.new()
	var fog_material: ShaderMaterial = fog_volume.material as ShaderMaterial
	fog_material.shader = load(FOG_SHADER_PATH)
	light.add_child(fog_volume)
	return fog_volume

func _assign_fog_volume_shape(fog_volume: FogVolume) -> void:
	if fog_volume == null:
		return
	var cone_shape: Variant = ClassDB.instantiate("ConeFogVolumeShape3D")
	if cone_shape != null:
		fog_volume.shape = cone_shape
		return
	var cylinder_shape: Variant = ClassDB.instantiate("CylinderFogVolumeShape3D")
	if cylinder_shape != null:
		fog_volume.shape = cylinder_shape
