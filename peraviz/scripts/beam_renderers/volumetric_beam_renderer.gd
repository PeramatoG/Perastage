extends BeamRendererBase

class_name VolumetricBeamRenderer

const BEAM_META_KEY: String = "peraviz_volumetric_beam"
const EMITTER_CONE_MAX_BASE_RADIUS_M: float = 10.0
const VOLUMETRIC_INTENSITY_SCALE: float = 1.9
const GOBO_TEXTURE_META_KEY: String = "peraviz_gobo_texture"
const DEBUG_AXIS_KEY: String = "peraviz_beam_debug_axis"

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
	_ensure_debug_axis(light)

func update_beam(light: SpotLight3D, params: Dictionary) -> void:
	ensure_beam(light)
	var cone: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
	if cone == null:
		return

	var intensity: float = clamp(float(params.get("scaled_intensity", 0.0)), 0.0, 20.0)
	var threshold: float = float(params.get("intensity_visibility_threshold", 0.015))
	var beam_range: float = max(float(params.get("beam_range", 0.1)), 0.01)
	var beam_angle: float = max(float(params.get("beam_angle", 1.0)), 0.1)

	if not bool(params.get("is_visible", true)) or intensity <= threshold:
		cone.visible = false
		var hidden_axis: MeshInstance3D = _ensure_debug_axis(light)
		if hidden_axis != null:
			hidden_axis.visible = false
		return
	var debug_axis: MeshInstance3D = _ensure_debug_axis(light)
	if debug_axis != null:
		debug_axis.visible = bool(params.get("beam_debug_optics", false))

	# Keep debug/preview beam meshes visible even when native fog-projector gobos are enabled.
	# Native gobo projection controls footprint/fog in renderer, while this cone keeps operator-visible beam feedback.


	var beam_color: Color = params.get("beam_color", Color.WHITE)
	var lens_radius: float = max(float(params.get("lens_radius", 0.03)), 0.005)

	var half_angle_deg: float = beam_angle * 0.5
	var tan_half_angle: float = tan(deg_to_rad(half_angle_deg))
	var radius: float = tan_half_angle * beam_range
	var bottom_radius: float = clamp(radius, 0.03, EMITTER_CONE_MAX_BASE_RADIUS_M)
	if bool(params.get("beam_debug_optics", false)):
		print("[PeravizBeamOptics] angle_deg=", beam_angle, " range_m=", beam_range, " radius_end_m=", bottom_radius)
	var cone_mesh: CylinderMesh = cone.mesh as CylinderMesh
	var top_radius: float = max(lens_radius, 0.003)
	if cone_mesh != null:
		cone_mesh.top_radius = top_radius
		cone_mesh.bottom_radius = bottom_radius
		cone_mesh.height = beam_range
	var lens_offset_m: float = max(float(params.get("lens_offset_m", params.get("near_offset", 0.0))), 0.0)
	var lens_shift_x: float = float(params.get("lens_shift_x", 0.0))
	var lens_shift_y: float = float(params.get("lens_shift_y", 0.0))
	cone.position = Vector3(lens_shift_x, lens_shift_y, -(beam_range * 0.5 + lens_offset_m))
	cone.visible = true

	var intensity_alpha: float = clamp(intensity * VOLUMETRIC_INTENSITY_SCALE, 0.0, 2.5)
	cone.set_instance_shader_parameter("base_color", Color(beam_color.r, beam_color.g, beam_color.b, intensity_alpha))
	cone.set_instance_shader_parameter("max_brightness", lerp(2.0, 24.0, intensity / 20.0))
	cone.set_instance_shader_parameter("beam_noise_amount", float(_settings.get("beam_noise_amount", 0.06)))
	cone.set_instance_shader_parameter("beam_noise_scale", float(_settings.get("beam_noise_scale", 1.4)))
	var haze_density: float = max(float(params.get("haze_density", params.get("haze_density_multiplier", 0.22))), 0.01)
	cone.set_instance_shader_parameter("beam_haze_density", float(_settings.get("beam_haze_density", 0.17)) * haze_density)
	cone.set_instance_shader_parameter("haze_density", haze_density)
	cone.set_instance_shader_parameter("beam_anisotropy", float(_settings.get("beam_anisotropy", 0.62)))
	cone.set_instance_shader_parameter("beam_quality", int(_settings.get("beam_quality", 1)))
	cone.set_instance_shader_parameter("radial_falloff", max(float(params.get("beam_radial_falloff", 1.1)), 0.05))
	cone.set_instance_shader_parameter("longitudinal_falloff", max(float(params.get("beam_longitudinal_falloff", 1.0)), 0.05))
	cone.set_instance_shader_parameter("beam_softness", clamp(float(params.get("beam_softness", 0.35)), 0.02, 1.0))
	cone.set_instance_shader_parameter("gobo_scale", max(float(params.get("gobo_scale", 1.0)), 0.05))
	cone.set_instance_shader_parameter("gobo_rotation_deg", float(params.get("gobo_rotation_deg", 0.0)))
	cone.set_instance_shader_parameter("cone_height", max(beam_range, 0.001))
	cone.set_instance_shader_parameter("gobo_projection_radius", max(bottom_radius, 0.001))
	var gobo_texture: Texture2D = null
	if light.has_meta(GOBO_TEXTURE_META_KEY):
		gobo_texture = light.get_meta(GOBO_TEXTURE_META_KEY) as Texture2D
	# Keep cone beam visible in all modes; native projector may still affect footprint/fog.
	var cone_material: ShaderMaterial = cone.material_override as ShaderMaterial
	if cone_material != null:
		cone_material.set_shader_parameter("use_gobo", gobo_texture != null)
		cone_material.set_shader_parameter("gobo_invert", false)
		if gobo_texture != null:
			cone_material.set_shader_parameter("gobo_texture", gobo_texture)

func cleanup_beam(light: SpotLight3D) -> void:
	if not light.has_meta(BEAM_META_KEY):
		return
	var cone: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
	if cone != null and is_instance_valid(cone):
		cone.queue_free()
	light.remove_meta(BEAM_META_KEY)


func _ensure_debug_axis(light: SpotLight3D) -> MeshInstance3D:
	if light.has_meta(DEBUG_AXIS_KEY):
		var existing: MeshInstance3D = light.get_meta(DEBUG_AXIS_KEY) as MeshInstance3D
		if existing != null and is_instance_valid(existing):
			return existing
	var axis := MeshInstance3D.new()
	axis.name = "PeravizBeamDebugAxis"
	axis.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var mesh := BoxMesh.new()
	mesh.size = Vector3(0.03, 0.03, 2.0)
	axis.mesh = mesh
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(1.0, 0.1, 0.1, 0.95)
	material.emission_enabled = true
	material.emission = Color(1.0, 0.0, 0.0, 1.0)
	axis.material_override = material
	axis.position = Vector3(0.0, 0.0, -1.0)
	light.add_child(axis)
	light.set_meta(DEBUG_AXIS_KEY, axis)
	return axis
