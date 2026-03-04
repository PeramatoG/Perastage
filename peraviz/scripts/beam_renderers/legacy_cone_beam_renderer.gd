extends BeamRendererBase

class_name LegacyConeBeamRenderer

const EMITTER_CONE_MAX_BASE_RADIUS_M: float = 10.0
const EMITTER_CONE_FADE_END_RATIO: float = 0.82
const EMITTER_CONE_NEAR_ALPHA: float = 0.16
const EMITTER_CONE_FAR_ALPHA: float = 0.004
const EMITTER_CONE_NEAR_EMISSION: float = 0.45
const EMITTER_CONE_FAR_EMISSION: float = 0.04

const MAIN_KEY: String = "peraviz_beam_cone"
const MID_KEY: String = "peraviz_beam_cone_mid"
const CORE_KEY: String = "peraviz_beam_cone_core"
const MATERIAL_KEY: String = "peraviz_legacy_beam_material"

var _shared_shader: Shader

func _init() -> void:
	_shared_shader = load("res://scripts/shaders/legacy_beam_cone.gdshader")

func ensure_beam(light: SpotLight3D) -> void:
	var beam_material: ShaderMaterial = _ensure_material(light)
	if not light.has_meta(MAIN_KEY):
		light.set_meta(MAIN_KEY, _create_cone("PeravizBeamCone", beam_material))
	if not light.has_meta(MID_KEY):
		light.set_meta(MID_KEY, _create_cone("PeravizBeamMidCone", beam_material))
	if not light.has_meta(CORE_KEY):
		light.set_meta(CORE_KEY, _create_cone("PeravizBeamCoreCone", beam_material))

func update_beam(light: SpotLight3D, params: Dictionary) -> void:
	ensure_beam(light)
	var cone: MeshInstance3D = light.get_meta(MAIN_KEY) as MeshInstance3D
	var mid_cone: MeshInstance3D = light.get_meta(MID_KEY) as MeshInstance3D
	var core_cone: MeshInstance3D = light.get_meta(CORE_KEY) as MeshInstance3D
	if cone == null and mid_cone == null and core_cone == null:
		return
	_attach_if_needed(light, cone)
	_attach_if_needed(light, mid_cone)
	_attach_if_needed(light, core_cone)

	var intensity: float = clamp(float(params.get("normalized_dimmer", 0.0)), 0.0, 1.0)
	var scaled_intensity: float = clamp(float(params.get("scaled_intensity", 0.0)), 0.0, 8.0)
	var beam_range: float = max(float(params.get("beam_range", 0.1)), 0.01)
	var beam_angle: float = max(float(params.get("beam_angle", 1.0)), 0.1)
	var beam_color: Color = params.get("beam_color", Color.WHITE)
	var lens_radius: float = max(float(params.get("lens_radius", 0.03)), 0.005)
	var is_visible: bool = bool(params.get("is_visible", true)) and intensity > 0.015

	if cone != null:
		cone.visible = is_visible
	if mid_cone != null:
		mid_cone.visible = is_visible
	if core_cone != null:
		core_cone.visible = is_visible
	if not is_visible:
		return

	var beam_half_angle_deg: float = beam_angle * 0.5
	var radius: float = tan(deg_to_rad(beam_half_angle_deg)) * beam_range
	var bottom_radius: float = clamp(radius, 0.03, EMITTER_CONE_MAX_BASE_RADIUS_M)

	_update_cone_geometry(cone, lens_radius, bottom_radius, beam_range, 1.0)
	_update_cone_geometry(mid_cone, lens_radius, bottom_radius, beam_range, 0.7)
	_update_cone_geometry(core_cone, lens_radius, bottom_radius, beam_range, 0.45)

	var gobo_texture: Texture2D = light.get_meta("peraviz_gobo_texture", null) as Texture2D
	var gobo_size_world: float = float(light.get_meta("peraviz_gobo_plane_size_world", 0.0))
	var gobo_plane_dist_m: float = float(light.get_meta("peraviz_gobo_plane_distance_m", 0.0))
	var gobo_cutoff: float = float(light.get_meta("peraviz_gobo_cutoff", 0.5))
	var gobo_softness: float = float(light.get_meta("peraviz_gobo_softness", 0.04))
	var gobo_rotation: float = float(light.get_meta("peraviz_gobo_rotation_radians", 0.0))
	var gobo_enabled: bool = (gobo_texture != null) and (gobo_size_world > 0.001) and (gobo_plane_dist_m > 0.001)

	var material: ShaderMaterial = _ensure_material(light)
	if material != null:
		material.set_shader_parameter("gobo_texture", gobo_texture if gobo_enabled else null)

	var color_alpha := Color(beam_color.r, beam_color.g, beam_color.b, 1.0)
	_update_cone_material(cone, color_alpha, scaled_intensity, beam_range, 0.35, 0.16, 0.06, 1.0, 1.0, gobo_enabled, gobo_cutoff, gobo_softness, gobo_rotation, gobo_size_world, gobo_plane_dist_m)
	_update_cone_material(mid_cone, color_alpha, scaled_intensity, beam_range, 0.18, 0.26, 0.04, 1.35, 1.25, gobo_enabled, gobo_cutoff, gobo_softness, gobo_rotation, gobo_size_world, gobo_plane_dist_m)
	_update_cone_material(core_cone, color_alpha, scaled_intensity, beam_range, 0.09, 0.35, 0.02, 1.7, 1.5, gobo_enabled, gobo_cutoff, gobo_softness, gobo_rotation, gobo_size_world, gobo_plane_dist_m)

func cleanup_beam(light: SpotLight3D) -> void:
	for meta_key in [MAIN_KEY, MID_KEY, CORE_KEY]:
		if not light.has_meta(meta_key):
			continue
		var cone: MeshInstance3D = light.get_meta(meta_key) as MeshInstance3D
		if cone != null and is_instance_valid(cone):
			cone.queue_free()
		light.remove_meta(meta_key)
	if light.has_meta(MATERIAL_KEY):
		light.remove_meta(MATERIAL_KEY)

func _ensure_material(light: SpotLight3D) -> ShaderMaterial:
	if light.has_meta(MATERIAL_KEY):
		var cached: ShaderMaterial = light.get_meta(MATERIAL_KEY) as ShaderMaterial
		if cached != null:
			return cached
	var material := ShaderMaterial.new()
	material.shader = _shared_shader
	light.set_meta(MATERIAL_KEY, material)
	return material

func _attach_if_needed(light: SpotLight3D, cone: MeshInstance3D) -> void:
	if cone != null and cone.get_parent() == null:
		light.add_child(cone)

func _create_cone(cone_name: String, material: ShaderMaterial) -> MeshInstance3D:
	var cone_mesh := CylinderMesh.new()
	cone_mesh.radial_segments = 40
	cone_mesh.rings = 6
	cone_mesh.top_radius = 0.02
	cone_mesh.bottom_radius = 0.6
	cone_mesh.height = 8.0
	var cone := MeshInstance3D.new()
	cone.name = cone_name
	cone.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	cone.mesh = cone_mesh
	cone.material_override = material
	cone.rotation_degrees.x = 90.0
	cone.visible = false
	return cone

func _update_cone_geometry(cone: MeshInstance3D, lens_radius: float, bottom_radius: float, beam_range: float, radius_scale: float) -> void:
	if cone == null:
		return
	var cone_mesh: CylinderMesh = cone.mesh as CylinderMesh
	if cone_mesh == null:
		return
	cone_mesh.top_radius = max(lens_radius * radius_scale, 0.003)
	cone_mesh.bottom_radius = clamp(bottom_radius * radius_scale, 0.02, EMITTER_CONE_MAX_BASE_RADIUS_M)
	cone_mesh.height = beam_range
	cone.position = Vector3(0.0, 0.0, -beam_range * 0.5)

func _update_cone_material(cone: MeshInstance3D, beam_color: Color, scaled_intensity: float, beam_range: float, lateral_softness: float, lateral_emission_boost: float, noise_strength: float, alpha_scale: float, emission_scale: float, gobo_enabled: bool, gobo_cutoff: float, gobo_softness: float, gobo_rotation: float, gobo_size_world: float, gobo_plane_dist_m: float) -> void:
	if cone == null:
		return
	cone.set_instance_shader_parameter("beam_color", beam_color)
	cone.set_instance_shader_parameter("near_alpha", lerp(0.0, EMITTER_CONE_NEAR_ALPHA * alpha_scale, scaled_intensity))
	cone.set_instance_shader_parameter("far_alpha", lerp(0.0, EMITTER_CONE_FAR_ALPHA * alpha_scale, scaled_intensity))
	cone.set_instance_shader_parameter("near_emission", lerp(0.0, EMITTER_CONE_NEAR_EMISSION * emission_scale, scaled_intensity))
	cone.set_instance_shader_parameter("far_emission", lerp(0.0, EMITTER_CONE_FAR_EMISSION * emission_scale, scaled_intensity))
	cone.set_instance_shader_parameter("cone_height", max(beam_range, 0.001))
	cone.set_instance_shader_parameter("fade_end_ratio", EMITTER_CONE_FADE_END_RATIO)
	cone.set_instance_shader_parameter("lateral_softness", lateral_softness)
	cone.set_instance_shader_parameter("lateral_emission_boost", lateral_emission_boost)
	cone.set_instance_shader_parameter("volumetric_noise_strength", noise_strength)
	cone.set_instance_shader_parameter("gobo_enabled", gobo_enabled)
	cone.set_instance_shader_parameter("gobo_cutoff", gobo_cutoff)
	cone.set_instance_shader_parameter("gobo_softness", gobo_softness)
	cone.set_instance_shader_parameter("gobo_rotation", gobo_rotation)
	cone.set_instance_shader_parameter("gobo_size", Vector2(gobo_size_world, gobo_size_world))
	cone.set_instance_shader_parameter("gobo_start_ratio", gobo_plane_dist_m / max(beam_range, 0.001))
