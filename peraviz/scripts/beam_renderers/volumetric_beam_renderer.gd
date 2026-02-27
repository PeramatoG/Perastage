extends BeamRendererBase

class_name VolumetricBeamRenderer

const BEAM_META_KEY: String = "peraviz_volumetric_beam"
const GOBO_OCCLUDER_META_KEY: String = "peraviz_gobo_occluder"
const EMITTER_CONE_MAX_BASE_RADIUS_M: float = 10.0
const VOLUMETRIC_INTENSITY_SCALE: float = 0.62
const GOBO_OCCLUDER_DISTANCE_M: float = 0.043
const GOBO_PLANE_BASE_SIZE_M: float = 0.017
const GOBO_SIZE_ZOOM_MIN_DEG: float = 4.0
const GOBO_SIZE_ZOOM_MAX_DEG: float = 50.0
const GOBO_SIZE_SCALE_MIN: float = 0.555
const GOBO_SIZE_SCALE_MAX: float = 6.4
const GOBO_FOOTPRINT_CONE_FILL_RATIO: float = 1.0
const GOBO_VOLUMETRIC_CONE_FILL_RATIO: float = 0.86

var _beam_material_template: ShaderMaterial
var _gobo_occluder_material_template: ShaderMaterial
var _camera: Camera3D
var _settings: Dictionary = {}

func _init() -> void:
	_beam_material_template = ShaderMaterial.new()
	_beam_material_template.shader = load("res://scripts/shaders/volumetric_beam.gdshader")
	_gobo_occluder_material_template = ShaderMaterial.new()
	_gobo_occluder_material_template.shader = load("res://scripts/shaders/gobo_occluder.gdshader")

func configure(view_camera: Camera3D, settings: Dictionary) -> void:
	_camera = view_camera
	_settings = settings.duplicate(true)

func ensure_beam(light: SpotLight3D) -> void:
	if not light.has_meta(BEAM_META_KEY):
		var cone_mesh := CylinderMesh.new()
		cone_mesh.radial_segments = 48
		cone_mesh.rings = 12
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
		gobo_occluder.material_override = _gobo_occluder_material_template.duplicate(true)
		gobo_occluder.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
		gobo_occluder.visible = false
		light.add_child(gobo_occluder)
		light.set_meta(GOBO_OCCLUDER_META_KEY, gobo_occluder)

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

func _update_gobo_occluder(light: SpotLight3D, cone: MeshInstance3D, gobo_occluder: MeshInstance3D, beam_angle: float, beam_range: float) -> void:
	if gobo_occluder == null:
		return

	var gobo_material: ShaderMaterial = gobo_occluder.material_override as ShaderMaterial
	if gobo_material == null:
		return

	var gobo_texture: Texture2D = light.get_meta("peraviz_gobo_texture", null) as Texture2D
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

	light.shadow_enabled = true
	var gobo_zoom_value: float = beam_angle
	var gobo_size_mult: float = remap(clamp(gobo_zoom_value, GOBO_SIZE_ZOOM_MIN_DEG, GOBO_SIZE_ZOOM_MAX_DEG), GOBO_SIZE_ZOOM_MIN_DEG, GOBO_SIZE_ZOOM_MAX_DEG, GOBO_SIZE_SCALE_MIN, GOBO_SIZE_SCALE_MAX)
	var cone_diameter_at_occluder: float = _compute_cone_diameter_at_occluder(beam_angle)
	var footprint_plane_size: float = cone_diameter_at_occluder * GOBO_FOOTPRINT_CONE_FILL_RATIO
	var footprint_scale: float = max(footprint_plane_size / GOBO_PLANE_BASE_SIZE_M, 0.001)
	gobo_occluder.scale = Vector3(footprint_scale, footprint_scale, 1.0)
	gobo_occluder.position = Vector3(0.0, 0.0, -GOBO_OCCLUDER_DISTANCE_M)
	gobo_occluder.visible = true
	gobo_material.set_shader_parameter("gobo_texture", gobo_texture)
	var gobo_plane_size_raw: float = GOBO_PLANE_BASE_SIZE_M * gobo_size_mult
	var volumetric_plane_cap: float = cone_diameter_at_occluder * GOBO_VOLUMETRIC_CONE_FILL_RATIO
	var gobo_plane_size: float = min(gobo_plane_size_raw, volumetric_plane_cap)
	cone.set_instance_shader_parameter("gobo_enabled", true)
	cone.set_instance_shader_parameter("gobo_cutoff", 0.5)
	cone.set_instance_shader_parameter("gobo_start_ratio", GOBO_OCCLUDER_DISTANCE_M / max(beam_range, 0.001))
	cone.set_instance_shader_parameter("gobo_rotation", 0.0)
	cone.set_instance_shader_parameter("gobo_size", Vector2(gobo_plane_size, gobo_plane_size))
	cone.set_instance_shader_parameter("gobo_axis_sign", 1.0)
	var cone_material: ShaderMaterial = cone.material_override as ShaderMaterial
	if cone_material != null:
		cone_material.set_shader_parameter("gobo_texture", gobo_texture)
	else:
		cone.set_instance_shader_parameter("gobo_enabled", false)
