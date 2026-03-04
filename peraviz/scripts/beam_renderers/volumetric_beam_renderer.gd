extends BeamRendererBase

class_name VolumetricBeamRenderer

const BEAM_META_KEY: String = "peraviz_volumetric_beam"
const EMITTER_CONE_MAX_BASE_RADIUS_M: float = 10.0
const VOLUMETRIC_INTENSITY_SCALE: float = 0.75
const GOBO_TEXTURE_META_KEY: String = "peraviz_gobo_texture"

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
	if light.has_meta(BEAM_META_KEY):
		return
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

func update_beam(light: SpotLight3D, params: Dictionary) -> void:
	ensure_beam(light)
	var cone: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
	if cone == null:
		return

	var intensity: float = clamp(float(params.get("scaled_intensity", 0.0)), 0.0, 8.0)
	var threshold: float = float(params.get("intensity_visibility_threshold", 0.015))
	var beam_range: float = max(float(params.get("beam_range", 0.1)), 0.01)
	var beam_angle: float = max(float(params.get("beam_angle", 1.0)), 0.1)

	if not bool(params.get("is_visible", true)) or intensity <= threshold:
		cone.visible = false
		return


	var beam_color: Color = params.get("beam_color", Color.WHITE)
	var lens_radius: float = max(float(params.get("lens_radius", 0.03)), 0.005)
	var distance_limit: float = float(params.get("distance_cull_m", 180.0))
	if _camera != null:
		var cam_distance: float = _camera.global_position.distance_to(light.global_position)
		if cam_distance > distance_limit or _camera.is_position_behind(light.global_position):
			cone.visible = false
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
	cone.set_instance_shader_parameter("beam_noise_amount", float(_settings.get("beam_noise_amount", 0.06)))
	cone.set_instance_shader_parameter("beam_noise_scale", float(_settings.get("beam_noise_scale", 1.4)))
	cone.set_instance_shader_parameter("beam_haze_density", float(_settings.get("beam_haze_density", 0.17)))
	cone.set_instance_shader_parameter("beam_anisotropy", float(_settings.get("beam_anisotropy", 0.62)))
	cone.set_instance_shader_parameter("beam_quality", int(_settings.get("beam_quality", 1)))
	var has_gobo: bool = light.has_meta(GOBO_TEXTURE_META_KEY)
	cone.set_instance_shader_parameter("gobo_enabled", has_gobo)
	if has_gobo:
		var gobo_texture: Texture2D = light.get_meta(GOBO_TEXTURE_META_KEY) as Texture2D
		cone.set_instance_shader_parameter("gobo_texture", gobo_texture)
	else:
		cone.set_instance_shader_parameter("gobo_texture", null)

func cleanup_beam(light: SpotLight3D) -> void:
	if not light.has_meta(BEAM_META_KEY):
		return
	var cone: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
	if cone != null and is_instance_valid(cone):
		cone.queue_free()
	light.remove_meta(BEAM_META_KEY)
