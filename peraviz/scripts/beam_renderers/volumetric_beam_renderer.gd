extends BeamRendererBase

class_name VolumetricBeamRenderer

const BEAM_META_KEY: String = "peraviz_volumetric_beam"
const GOBO_OCCLUDER_META_KEY: String = "peraviz_gobo_occluder"
const GOBO_OCCLUDER_SHADER_MATERIAL_META_KEY: String = "peraviz_gobo_occluder_shader_material"
const EMITTER_CONE_MAX_BASE_RADIUS_M: float = 10.0
const VOLUMETRIC_INTENSITY_SCALE: float = 0.62
const GOBO_OCCLUDER_DISTANCE_M: float = 0.043
const GOBO_PLANE_BASE_SIZE_M: float = 0.017
const GOBO_FOOTPRINT_CONE_FILL_RATIO: float = 1.0
const GOBO_COOKIE_OVERSCAN_RATIO: float = 1.12
const GOBO_DEBUG_MATERIAL_COLOR: Color = Color(0.1, 0.9, 0.2, 0.3)
const GOBO_DEBUG_LOG_THROTTLE_MS: int = 400

var _beam_material_template: ShaderMaterial
var _gobo_occluder_material_template: ShaderMaterial
var _gobo_occluder_debug_material: StandardMaterial3D
var _camera: Camera3D
var _settings: Dictionary = {}
var _last_gobo_debug_log_ticks_by_light: Dictionary = {}

func _init() -> void:
	_beam_material_template = ShaderMaterial.new()
	_beam_material_template.shader = load("res://scripts/shaders/volumetric_beam.gdshader")
	_gobo_occluder_material_template = ShaderMaterial.new()
	_gobo_occluder_material_template.shader = load("res://scripts/shaders/gobo_occluder.gdshader")
	_gobo_occluder_debug_material = StandardMaterial3D.new()
	_gobo_occluder_debug_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_gobo_occluder_debug_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_gobo_occluder_debug_material.albedo_color = GOBO_DEBUG_MATERIAL_COLOR
	_gobo_occluder_debug_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	_gobo_occluder_debug_material.no_depth_test = true

func configure(view_camera: Camera3D, settings: Dictionary) -> void:
	_camera = view_camera
	_settings = settings.duplicate(true)

func ensure_beam(light: SpotLight3D) -> void:
	if not light.has_meta(BEAM_META_KEY):
		var cone_mesh := CylinderMesh.new()
		cone_mesh.radial_segments = 96
		cone_mesh.rings = 24
		cone_mesh.top_radius = 0.02
		cone_mesh.bottom_radius = 0.8
		cone_mesh.height = 8.0
		cone_mesh.cap_top = false
		cone_mesh.cap_bottom = false
		var cone := MeshInstance3D.new()
		cone.name = "PeravizVolumetricBeam"
		cone.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		cone.mesh = cone_mesh
		cone.material_override = _beam_material_template.duplicate(true)
		cone.rotation_degrees.x = 90.0
		cone.visible = false
		light.add_child(cone)
		light.set_meta(BEAM_META_KEY, cone)

	if not light.has_meta(GOBO_OCCLUDER_META_KEY):
		var gobo_mesh := QuadMesh.new()
		gobo_mesh.size = Vector2(GOBO_PLANE_BASE_SIZE_M, GOBO_PLANE_BASE_SIZE_M)
		var gobo_occluder := MeshInstance3D.new()
		gobo_occluder.name = "PeravizGoboOccluder"
		gobo_occluder.mesh = gobo_mesh
		var occluder_shader_material: ShaderMaterial = _gobo_occluder_material_template.duplicate(true)
		gobo_occluder.material_override = occluder_shader_material
		gobo_occluder.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
		gobo_occluder.visible = false
		light.add_child(gobo_occluder)
		light.set_meta(GOBO_OCCLUDER_META_KEY, gobo_occluder)
		light.set_meta(GOBO_OCCLUDER_SHADER_MATERIAL_META_KEY, occluder_shader_material)

func update_beam(light: SpotLight3D, params: Dictionary) -> void:
	ensure_beam(light)
	var cone: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
	var gobo_occluder: MeshInstance3D = light.get_meta(GOBO_OCCLUDER_META_KEY) as MeshInstance3D
	if cone == null:
		return

	var intensity: float = clamp(float(params.get("scaled_intensity", 0.0)), 0.0, 3.0)
	var threshold: float = float(params.get("intensity_visibility_threshold", 0.015))
	if intensity <= threshold or not bool(params.get("is_visible", true)):
		cone.visible = false
		if gobo_occluder != null:
			gobo_occluder.visible = false
		return

	var beam_range: float = max(float(params.get("beam_range", 0.1)), 0.01)
	var beam_angle: float = max(float(params.get("beam_angle", 1.0)), 0.1)
	var beam_color: Color = params.get("beam_color", Color.WHITE)
	var lens_radius: float = max(float(params.get("lens_radius", 0.03)), 0.005)
	var distance_limit: float = float(params.get("distance_cull_m", 180.0))

	if _camera != null:
		var cam_distance: float = _camera.global_position.distance_to(light.global_position)
		if cam_distance > distance_limit or _camera.is_position_behind(light.global_position):
			cone.visible = false
			if gobo_occluder != null:
				gobo_occluder.visible = false
			return

	var half_angle_deg: float = beam_angle * 0.5
	var tan_half_angle: float = tan(deg_to_rad(half_angle_deg))
	var radius: float = tan_half_angle * beam_range
	var bottom_radius: float = clamp(radius, 0.03, EMITTER_CONE_MAX_BASE_RADIUS_M)
	var cone_mesh: CylinderMesh = cone.mesh as CylinderMesh
	if cone_mesh != null:
		cone_mesh.top_radius = max(lens_radius, 0.003)
		cone_mesh.bottom_radius = bottom_radius
		cone_mesh.height = beam_range
	cone.position = Vector3(0.0, 0.0, -beam_range * 0.5)
	cone.visible = true

	var intensity_alpha: float = clamp(intensity * VOLUMETRIC_INTENSITY_SCALE, 0.0, 1.0)
	cone.set_instance_shader_parameter("base_color", Color(beam_color.r, beam_color.g, beam_color.b, intensity_alpha))
	cone.set_instance_shader_parameter("max_brightness", lerp(1.0, 10.0, intensity))
	cone.set_instance_shader_parameter("beam_top_radius", max(lens_radius, 0.003))
	cone.set_instance_shader_parameter("beam_bottom_radius", bottom_radius)
	cone.set_instance_shader_parameter("beam_height", beam_range)

	_update_gobo_occluder(light, cone, gobo_occluder, beam_angle, beam_range)

func _compute_cone_diameter_at_occluder(beam_angle: float) -> float:
	var half_angle_rad: float = deg_to_rad(max(beam_angle, 0.1) * 0.5)
	var cone_radius_at_occluder: float = tan(half_angle_rad) * GOBO_OCCLUDER_DISTANCE_M
	return max(cone_radius_at_occluder * 2.0, 0.001)

func cleanup_beam(light: SpotLight3D) -> void:
	if light.has_meta(BEAM_META_KEY):
		var cone: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
		if cone != null and is_instance_valid(cone):
			cone.queue_free()
		light.remove_meta(BEAM_META_KEY)

	if light.has_meta(GOBO_OCCLUDER_META_KEY):
		var gobo_occluder: MeshInstance3D = light.get_meta(GOBO_OCCLUDER_META_KEY) as MeshInstance3D
		if gobo_occluder != null and is_instance_valid(gobo_occluder):
			gobo_occluder.queue_free()
		light.remove_meta(GOBO_OCCLUDER_META_KEY)
	if light.has_meta(GOBO_OCCLUDER_SHADER_MATERIAL_META_KEY):
		light.remove_meta(GOBO_OCCLUDER_SHADER_MATERIAL_META_KEY)

func _update_gobo_occluder(light: SpotLight3D, cone: MeshInstance3D, gobo_occluder: MeshInstance3D, beam_angle: float, beam_range: float) -> void:
	if gobo_occluder == null:
		return

	var gobo_material: ShaderMaterial = light.get_meta(GOBO_OCCLUDER_SHADER_MATERIAL_META_KEY, null) as ShaderMaterial
	if gobo_material == null:
		return

	var gobo_texture: Texture2D = light.get_meta("peraviz_gobo_texture", null) as Texture2D
	var gobo_projection_mode: int = int(light.get_meta("peraviz_gobo_projection_mode", 0))
	if gobo_projection_mode == 1:
		gobo_occluder.visible = false
		cone.set_instance_shader_parameter("gobo_enabled", false)
		return

	var gobo_active := gobo_texture != null
	if not gobo_active:
		gobo_occluder.visible = false
		gobo_material.set_shader_parameter("gobo_texture", null)
		cone.set_instance_shader_parameter("gobo_enabled", false)
		cone.set_instance_shader_parameter("gobo_axis_sign", 1.0)
		var cone_material_disabled: ShaderMaterial = cone.material_override as ShaderMaterial
		if cone_material_disabled != null:
			cone_material_disabled.set_shader_parameter("gobo_texture", null)
		return

	var gobo_scale_ratio: float = max(float(_settings.get("gobo_scale_ratio", 1.0)), 0.001)
	var cone_diameter_at_occluder: float = _compute_cone_diameter_at_occluder(beam_angle)
	var footprint_plane_size: float = cone_diameter_at_occluder * GOBO_FOOTPRINT_CONE_FILL_RATIO
	# Slight overscan avoids having the cone boundary and cookie boundary collide exactly,
	# which reduces checker/dither-like borders in volumetric fog froxels.
	var gobo_plane_size_world: float = max(footprint_plane_size * gobo_scale_ratio * GOBO_COOKIE_OVERSCAN_RATIO, 0.001)
	var footprint_scale: float = max(gobo_plane_size_world / GOBO_PLANE_BASE_SIZE_M, 0.001)
	gobo_occluder.scale = Vector3(footprint_scale, footprint_scale, 1.0)
	gobo_occluder.position = Vector3(0.0, 0.0, -GOBO_OCCLUDER_DISTANCE_M)
	gobo_occluder.visible = true
	gobo_material.set_shader_parameter("gobo_texture", gobo_texture)

	var gobo_debug_visible: bool = bool(_settings.get("gobo_debug_show_occluder", false))
	if gobo_debug_visible:
		gobo_occluder.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
		gobo_occluder.material_override = _gobo_occluder_debug_material
	else:
		gobo_occluder.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
		gobo_occluder.material_override = gobo_material

	cone.set_instance_shader_parameter("gobo_enabled", true)
	cone.set_instance_shader_parameter("gobo_cutoff", 0.5)
	cone.set_instance_shader_parameter("gobo_start_ratio", GOBO_OCCLUDER_DISTANCE_M / max(beam_range, 0.001))
	cone.set_instance_shader_parameter("gobo_rotation", 0.0)
	cone.set_instance_shader_parameter("gobo_size", Vector2(gobo_plane_size_world, gobo_plane_size_world))
	cone.set_instance_shader_parameter("gobo_axis_sign", 1.0)
	var cone_material: ShaderMaterial = cone.material_override as ShaderMaterial
	if cone_material != null:
		cone_material.set_shader_parameter("gobo_texture", gobo_texture)
	else:
		cone.set_instance_shader_parameter("gobo_enabled", false)

	_maybe_log_gobo_parameters(light, beam_angle, beam_range, gobo_plane_size_world)

func _maybe_log_gobo_parameters(light: SpotLight3D, beam_angle: float, beam_range_visual: float, gobo_plane_size_world: float) -> void:
	if not bool(_settings.get("gobo_debug_log_parameters", false)):
		return

	var light_id: int = light.get_instance_id()
	var now_ticks: int = Time.get_ticks_msec()
	var last_ticks: int = int(_last_gobo_debug_log_ticks_by_light.get(light_id, 0))
	if (now_ticks - last_ticks) < GOBO_DEBUG_LOG_THROTTLE_MS:
		return
	_last_gobo_debug_log_ticks_by_light[light_id] = now_ticks

	var volumetric_size: int = int(ProjectSettings.get_setting("rendering/environment/volumetric_fog/volume_size", int(round(float(_settings.get("volumetric_fog_volume_size", -1))))))
	var volumetric_depth: float = float(ProjectSettings.get_setting("rendering/environment/volumetric_fog/volume_depth", int(round(float(_settings.get("volumetric_fog_depth", -1.0))))))
	var volumetric_filter_active: bool = bool(ProjectSettings.get_setting("rendering/environment/volumetric_fog/use_filter", bool(_settings.get("volumetric_fog_use_filter", true))))
	var beam_angle_source: String = str(light.get_meta("peraviz_beam_angle_source", "unknown"))
	if bool(_settings.get("gobo_debug_log_volumetric_details", false)):
		print("[PeravizGoboDebug] light=", light.name,
			" spot_angle_half_deg=", light.spot_angle,
			" beam_angle_source=", beam_angle_source,
			" beam_angle_full_deg=", beam_angle,
			" spot_range=", light.spot_range,
			" beam_range_visual=", beam_range_visual,
			" spot_attenuation=", light.spot_attenuation,
			" shadow_bias=", light.shadow_bias,
			" shadow_normal_bias=", light.shadow_normal_bias,
			" shadow_blur=", light.shadow_blur,
			" light_volumetric_fog_energy=", light.light_volumetric_fog_energy,
			" env_volumetric_density=", float(_settings.get("volumetric_fog_density", 0.0)),
			" env_volume_size=", volumetric_size,
			" env_volume_depth=", volumetric_depth,
			" env_use_filter=", volumetric_filter_active,
			" gobo_size_world=", gobo_plane_size_world)
	else:
		print("[PeravizGoboDebug] light=", light.name,
			" spot_angle_half_deg=", light.spot_angle,
			" beam_angle_full_deg=", beam_angle,
			" spot_range=", light.spot_range,
			" beam_range_visual=", beam_range_visual,
			" shadow_blur=", light.shadow_blur,
			" env_volume_size=", volumetric_size,
			" env_use_filter=", volumetric_filter_active)
