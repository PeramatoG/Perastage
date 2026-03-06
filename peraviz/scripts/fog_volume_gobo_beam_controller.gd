extends RefCounted
class_name FogVolumeGoboBeamController

const FOG_VOLUME_NODE_NAME: String = "PeravizFogVolumeGoboBeam"
const FOG_SHADER_PATH: String = "res://scripts/shaders/fog_volume_gobo_beam.gdshader"

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
	fog_material.set_shader_parameter("gobo_texture", gobo_texture)
	fog_material.set_shader_parameter("light_color", Color(beam_params.get("beam_color", Color.WHITE)))
	var haze_density: float = max(float(beam_params.get("haze_density_multiplier", 0.22)), 0.01)
	fog_material.set_shader_parameter("density_scale", float(visual_settings.get("fog_volume_density_scale", 0.9)) * haze_density)
	fog_material.set_shader_parameter("emission_strength", float(visual_settings.get("fog_volume_emission_strength", 0.55)))
	fog_material.set_shader_parameter("edge_softness", float(visual_settings.get("fog_volume_edge_softness", 0.72)))
	fog_material.set_shader_parameter("invert_gobo", bool(visual_settings.get("fog_volume_invert_gobo", false)))
	fog_material.set_shader_parameter("gobo_scale", max(float(beam_params.get("gobo_scale", 1.0)), 0.05))
	fog_material.set_shader_parameter("gobo_rotation_deg", float(beam_params.get("gobo_rotation_deg", 0.0)))
	fog_material.set_shader_parameter("radial_falloff", max(float(beam_params.get("beam_radial_falloff", 1.25)), 0.05))
	fog_material.set_shader_parameter("longitudinal_falloff", max(float(beam_params.get("beam_longitudinal_falloff", 1.1)), 0.05))

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
