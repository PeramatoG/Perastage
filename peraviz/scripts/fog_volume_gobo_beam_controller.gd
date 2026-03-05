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
	fog_material.set_shader_parameter("density_scale", float(visual_settings.get("fog_volume_density_scale", 2.5)))
	fog_material.set_shader_parameter("emission_strength", float(visual_settings.get("fog_volume_emission_strength", 2.0)))
	fog_material.set_shader_parameter("edge_softness", float(visual_settings.get("fog_volume_edge_softness", 0.85)))
	fog_material.set_shader_parameter("invert_gobo", bool(visual_settings.get("fog_volume_invert_gobo", false)))

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
	fog_volume.shape = FogVolume.SHAPE_CONE
	fog_volume.material = ShaderMaterial.new()
	var fog_material: ShaderMaterial = fog_volume.material as ShaderMaterial
	fog_material.shader = load(FOG_SHADER_PATH)
	light.add_child(fog_volume)
	return fog_volume
