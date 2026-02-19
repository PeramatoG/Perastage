extends BeamRendererBase

class_name VolumetricBeamRenderer

const BEAM_META_KEY: String = "peraviz_volumetric_beam"
const EMITTER_CONE_MAX_BASE_RADIUS_M: float = 10.0
const VOLUMETRIC_INTENSITY_SCALE: float = 0.72

const QUALITY_PRESETS := {
	0: {"steps": 14.0, "noise_multiplier": 0.0},
	1: {"steps": 28.0, "noise_multiplier": 0.65},
	2: {"steps": 56.0, "noise_multiplier": 1.0},
}

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
	cone.position = Vector3(0.0, 0.0, -beam_range * 0.5)
	cone.visible = true

	var quality: int = int(_settings.get("beam_quality", 1))
	var quality_preset: Dictionary = QUALITY_PRESETS.get(quality, QUALITY_PRESETS[1])
	var haze_density: float = float(_settings.get("beam_haze_density", 0.22))
	var anisotropy: float = float(_settings.get("beam_anisotropy", 0.62))
	var noise_amount: float = float(_settings.get("beam_noise_amount", 0.06)) * float(quality_preset.get("noise_multiplier", 1.0))

	cone.set_instance_shader_parameter("beam_color", Color(beam_color.r, beam_color.g, beam_color.b, 1.0))
	cone.set_instance_shader_parameter("beam_intensity", intensity * VOLUMETRIC_INTENSITY_SCALE)
	cone.set_instance_shader_parameter("cone_height", beam_range)
	cone.set_instance_shader_parameter("top_radius", max(lens_radius, 0.003))
	cone.set_instance_shader_parameter("bottom_radius", bottom_radius)
	cone.set_instance_shader_parameter("haze_density", haze_density)
	cone.set_instance_shader_parameter("anisotropy", anisotropy)
	cone.set_instance_shader_parameter("noise_amount", noise_amount)
	cone.set_instance_shader_parameter("noise_scale", float(_settings.get("beam_noise_scale", 1.4)))
	cone.set_instance_shader_parameter("end_fade_ratio", float(params.get("fade_end_ratio", 0.82)))
	cone.set_instance_shader_parameter("step_count", float(quality_preset.get("steps", 28.0)))
	cone.set_instance_shader_parameter("beam_range", beam_range)
	cone.set_instance_shader_parameter("camera_inside_dimmer", 0.6)

func cleanup_beam(light: SpotLight3D) -> void:
	if not light.has_meta(BEAM_META_KEY):
		return
	var cone: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
	if cone != null and is_instance_valid(cone):
		cone.queue_free()
	light.remove_meta(BEAM_META_KEY)
