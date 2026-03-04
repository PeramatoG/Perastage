extends BeamRendererBase

class_name VolumetricBeamRenderer

const MAIN_KEY: String = "peraviz_volumetric_beam"
const MID_KEY: String = "peraviz_volumetric_beam_mid"
const CORE_KEY: String = "peraviz_volumetric_beam_core"
const EMITTER_CONE_MAX_BASE_RADIUS_M: float = 10.0
const VOLUMETRIC_INTENSITY_SCALE: float = 0.75

var _beam_material_template: ShaderMaterial
var _camera: Camera3D
var _settings: Dictionary = {}

func _init() -> void:
	_beam_material_template = ShaderMaterial.new()
	_beam_material_template.shader = load("res://scripts/shaders/volumetric_beam.gdshader")

func configure(view_camera: Camera3D, settings: Dictionary) -> void:
	_camera = view_camera
	_settings = settings.duplicate(true)

func ensure_beam(light: SpotLight3D) -> void:
	if not light.has_meta(MAIN_KEY):
		light.set_meta(MAIN_KEY, _create_cone("PeravizVolumetricBeam"))
	if not light.has_meta(MID_KEY):
		light.set_meta(MID_KEY, _create_cone("PeravizVolumetricBeamMid"))
	if not light.has_meta(CORE_KEY):
		light.set_meta(CORE_KEY, _create_cone("PeravizVolumetricBeamCore"))

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

	var intensity: float = clamp(float(params.get("scaled_intensity", 0.0)), 0.0, 8.0)
	var threshold: float = float(params.get("intensity_visibility_threshold", 0.015))
	var beam_range: float = max(float(params.get("beam_range", 0.1)), 0.01)
	var beam_angle: float = max(float(params.get("beam_angle", 1.0)), 0.1)

	var is_visible: bool = bool(params.get("is_visible", true)) and intensity > threshold
	if cone != null:
		cone.visible = is_visible
	if mid_cone != null:
		mid_cone.visible = is_visible
	if core_cone != null:
		core_cone.visible = is_visible
	if not is_visible:
		return

	var beam_color: Color = params.get("beam_color", Color.WHITE)
	var lens_radius: float = max(float(params.get("lens_radius", 0.03)), 0.005)
	var distance_limit: float = float(params.get("distance_cull_m", 180.0))
	if _camera != null:
		var cam_distance: float = _camera.global_position.distance_to(light.global_position)
		if cam_distance > distance_limit or _camera.is_position_behind(light.global_position):
			if cone != null:
				cone.visible = false
			if mid_cone != null:
				mid_cone.visible = false
			if core_cone != null:
				core_cone.visible = false
			return

	var half_angle_deg: float = beam_angle * 0.5
	var tan_half_angle: float = tan(deg_to_rad(half_angle_deg))
	var radius: float = tan_half_angle * beam_range
	var bottom_radius: float = clamp(radius, 0.03, EMITTER_CONE_MAX_BASE_RADIUS_M)

	_update_cone_geometry(cone, lens_radius, bottom_radius, beam_range, 1.0)
	_update_cone_geometry(mid_cone, lens_radius, bottom_radius, beam_range, 0.72)
	_update_cone_geometry(core_cone, lens_radius, bottom_radius, beam_range, 0.46)

	var gobo_texture: Texture2D = light.get_meta("peraviz_gobo_texture", null) as Texture2D
	var gobo_size_world: float = float(light.get_meta("peraviz_gobo_plane_size_world", 0.0))
	var gobo_plane_dist_m: float = float(light.get_meta("peraviz_gobo_plane_distance_m", 0.0))
	var gobo_cutoff: float = float(light.get_meta("peraviz_gobo_cutoff", 0.5))
	var gobo_softness: float = float(light.get_meta("peraviz_gobo_softness", 0.04))
	var gobo_rotation: float = float(light.get_meta("peraviz_gobo_rotation_radians", 0.0))
	var gobo_enabled: bool = (gobo_texture != null) and (gobo_size_world > 0.001) and (gobo_plane_dist_m > 0.001)

	var intensity_alpha: float = clamp(intensity * VOLUMETRIC_INTENSITY_SCALE, 0.0, 1.0)
	var base := Color(beam_color.r, beam_color.g, beam_color.b, intensity_alpha)

	_update_cone_material(cone, base, intensity, beam_range, lens_radius, bottom_radius, gobo_texture, gobo_enabled, gobo_size_world, gobo_plane_dist_m, gobo_cutoff, gobo_softness, gobo_rotation, 1.0)
	_update_cone_material(mid_cone, base, intensity, beam_range, lens_radius, bottom_radius, gobo_texture, gobo_enabled, gobo_size_world, gobo_plane_dist_m, gobo_cutoff, gobo_softness, gobo_rotation, 0.75)
	_update_cone_material(core_cone, base, intensity, beam_range, lens_radius, bottom_radius, gobo_texture, gobo_enabled, gobo_size_world, gobo_plane_dist_m, gobo_cutoff, gobo_softness, gobo_rotation, 0.52)

func cleanup_beam(light: SpotLight3D) -> void:
	for meta_key in [MAIN_KEY, MID_KEY, CORE_KEY]:
		if not light.has_meta(meta_key):
			continue
		var cone: MeshInstance3D = light.get_meta(meta_key) as MeshInstance3D
		if cone != null and is_instance_valid(cone):
			cone.queue_free()
		light.remove_meta(meta_key)

func _create_cone(cone_name: String) -> MeshInstance3D:
	var cone_mesh := CylinderMesh.new()
	cone_mesh.radial_segments = 96
	cone_mesh.rings = 24
	cone_mesh.top_radius = 0.02
	cone_mesh.bottom_radius = 0.8
	cone_mesh.height = 8.0
	cone_mesh.cap_top = false
	cone_mesh.cap_bottom = false
	var cone := MeshInstance3D.new()
	cone.name = cone_name
	cone.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	cone.mesh = cone_mesh
	cone.material_override = _beam_material_template.duplicate(true)
	cone.rotation_degrees.x = 90.0
	cone.visible = false
	return cone

func _attach_if_needed(light: SpotLight3D, cone: MeshInstance3D) -> void:
	if cone != null and cone.get_parent() == null:
		light.add_child(cone)

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

func _update_cone_material(cone: MeshInstance3D, base_color: Color, intensity: float, beam_range: float, lens_radius: float, bottom_radius: float, gobo_texture: Texture2D, gobo_enabled: bool, gobo_size_world: float, gobo_plane_dist_m: float, gobo_cutoff: float, gobo_softness: float, gobo_rotation: float, layer_alpha_scale: float) -> void:
	if cone == null:
		return
	cone.set_instance_shader_parameter("base_color", Color(base_color.r, base_color.g, base_color.b, clamp(base_color.a * layer_alpha_scale, 0.0, 1.0)))
	cone.set_instance_shader_parameter("max_brightness", lerp(1.0, 10.0, intensity))
	cone.set_instance_shader_parameter("beam_haze_density", float(_settings.get("beam_haze_density", 0.17)))
	cone.set_instance_shader_parameter("beam_noise_amount", float(_settings.get("beam_noise_amount", 0.06)))
	cone.set_instance_shader_parameter("beam_noise_scale", float(_settings.get("beam_noise_scale", 1.4)))
	cone.set_instance_shader_parameter("gobo_enabled", gobo_enabled)
	cone.set_instance_shader_parameter("gobo_cutoff", gobo_cutoff)
	cone.set_instance_shader_parameter("gobo_softness", gobo_softness)
	cone.set_instance_shader_parameter("gobo_rotation", gobo_rotation)
	cone.set_instance_shader_parameter("gobo_size", Vector2(gobo_size_world, gobo_size_world))
	cone.set_instance_shader_parameter("gobo_start_ratio", gobo_plane_dist_m / max(beam_range, 0.001))
	cone.set_instance_shader_parameter("beam_height", beam_range)
	cone.set_instance_shader_parameter("beam_top_radius", max(lens_radius, 0.003))
	cone.set_instance_shader_parameter("beam_bottom_radius", bottom_radius)

	var cone_material: ShaderMaterial = cone.material_override as ShaderMaterial
	if cone_material != null:
		cone_material.set_shader_parameter("gobo_texture", gobo_texture if gobo_enabled else null)
