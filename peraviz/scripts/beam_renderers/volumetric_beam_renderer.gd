extends BeamRendererBase

class_name VolumetricBeamRenderer

const BEAM_META_KEY: String = "peraviz_volumetric_beam"
const EMITTER_CONE_MAX_BASE_RADIUS_M: float = 10.0
const VOLUMETRIC_INTENSITY_SCALE: float = 0.62

var _shared_material: ShaderMaterial
var _camera: Camera3D
var _settings: Dictionary = {}

func _init() -> void:
	_shared_material = ShaderMaterial.new()
	_shared_material.shader = load("res://scripts/shaders/volumetric_beam.gdshader")

func configure(view_camera: Camera3D, settings: Dictionary) -> void:
	_camera = view_camera
	_settings = settings.duplicate(true)

func ensure_beam(light: SpotLight3D) -> void:
	if light.has_meta(BEAM_META_KEY):
		return
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
	cone.material_override = _shared_material
	cone.rotation_degrees.x = 90.0
	cone.visible = false
	light.add_child(cone)
	light.set_meta(BEAM_META_KEY, cone)

func update_beam(light: SpotLight3D, params: Dictionary) -> void:
	ensure_beam(light)
	var cone: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
	if cone == null:
		return

	var intensity: float = clamp(float(params.get("scaled_intensity", 0.0)), 0.0, 3.0)
	var threshold: float = float(params.get("intensity_visibility_threshold", 0.015))
	if intensity <= threshold or not bool(params.get("is_visible", true)):
		cone.visible = false
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
			return

	var half_angle_deg: float = beam_angle * 0.5
	var radius: float = tan(deg_to_rad(half_angle_deg)) * beam_range
	var bottom_radius: float = clamp(radius, 0.03, EMITTER_CONE_MAX_BASE_RADIUS_M)
	var cone_mesh: CylinderMesh = cone.mesh as CylinderMesh
	if cone_mesh != null:
		cone_mesh.top_radius = max(lens_radius, 0.003)
		cone_mesh.bottom_radius = bottom_radius
		cone_mesh.height = beam_range
		cone.set_instance_shader_parameter("beam_top_radius", cone_mesh.top_radius)
		cone.set_instance_shader_parameter("beam_bottom_radius", cone_mesh.bottom_radius)
	cone.position = Vector3(0.0, 0.0, -beam_range * 0.5)
	cone.visible = true

	var intensity_alpha: float = clamp(intensity * VOLUMETRIC_INTENSITY_SCALE, 0.0, 1.0)
	cone.set_instance_shader_parameter("base_color", Color(beam_color.r, beam_color.g, beam_color.b, intensity_alpha))
	cone.set_instance_shader_parameter("falloff_power", 8.0)
	cone.set_instance_shader_parameter("facing_boost", 1.5)
	cone.set_instance_shader_parameter("facing_power", 4.0)
	cone.set_instance_shader_parameter("boost_along_y", 1.0)
	cone.set_instance_shader_parameter("feather_sharpness", 4.0)
	cone.set_instance_shader_parameter("feather_intensity", 1.0)
	cone.set_instance_shader_parameter("near_fade_start", 0.01)
	cone.set_instance_shader_parameter("near_fade_end", min(max(1.0, beam_range * 0.04), 50.0))
	cone.set_instance_shader_parameter("far_fade_start", min(max(25.0, beam_range * 0.45), 100.0))
	cone.set_instance_shader_parameter("far_fade_end", min(max(80.0, beam_range * 0.9), 100.0))
	cone.set_instance_shader_parameter("depth_feather_enabled", true)
	cone.set_instance_shader_parameter("depth_fade_distance", 0.5)
	cone.set_instance_shader_parameter("max_brightness", lerp(1.0, 10.0, intensity))

	var gobo_tex: Texture2D = light.light_projector
	var gobo_active := gobo_tex != null
	cone.set_instance_shader_parameter("gobo_enabled", gobo_active)
	if gobo_active:
		light.shadow_enabled = true
		cone.set_instance_shader_parameter("gobo_texture", gobo_tex)
		cone.set_instance_shader_parameter("gobo_strength", 1.0)
		cone.set_instance_shader_parameter("gobo_rotation", 0.0)

func cleanup_beam(light: SpotLight3D) -> void:
	if not light.has_meta(BEAM_META_KEY):
		return
	var cone: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
	if cone != null and is_instance_valid(cone):
		cone.queue_free()
	light.remove_meta(BEAM_META_KEY)
