extends BeamRendererBase

class_name VolumetricBeamRenderer

const BEAM_META_KEY: String = "peraviz_volumetric_beam"
const GOBO_OVERLAY_META_KEY: String = "peraviz_gobo_overlay"
const EMITTER_CONE_MAX_BASE_RADIUS_M: float = 10.0
const VOLUMETRIC_INTENSITY_SCALE: float = 0.62
const GOBO_OVERLAY_INTENSITY_SCALE: float = 1.2
const GOBO_OVERLAY_MIN_DISTANCE_M: float = 0.05

var _shared_material: ShaderMaterial
var _gobo_overlay_shader: Shader = preload("res://scripts/shaders/volumetric_gobo_overlay.gdshader")
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

	var overlay_mesh := QuadMesh.new()
	overlay_mesh.size = Vector2.ONE
	var overlay := MeshInstance3D.new()
	overlay.name = "PeravizGoboOverlay"
	overlay.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	overlay.mesh = overlay_mesh
	overlay.visible = false
	var overlay_material := ShaderMaterial.new()
	overlay_material.shader = _gobo_overlay_shader
	overlay.material_override = overlay_material
	light.add_child(overlay)
	light.set_meta(GOBO_OVERLAY_META_KEY, overlay)

func update_beam(light: SpotLight3D, params: Dictionary) -> void:
	ensure_beam(light)
	var cone: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
	if cone == null:
		return

	var intensity: float = clamp(float(params.get("scaled_intensity", 0.0)), 0.0, 3.0)
	var threshold: float = float(params.get("intensity_visibility_threshold", 0.015))
	if intensity <= threshold or not bool(params.get("is_visible", true)):
		cone.visible = false
		_hide_gobo_overlay(light)
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
			_hide_gobo_overlay(light)
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

	var gobo_overlay_params := {
		"beam_range": beam_range,
		"beam_angle": beam_angle,
		"lens_radius": lens_radius,
		"scaled_intensity": intensity,
	}
	_update_gobo_overlay(light, gobo_overlay_params)

func update_gobo_overlay(light: SpotLight3D) -> void:
	if not light.has_meta("peraviz_beam_last_params"):
		return
	var params: Dictionary = light.get_meta("peraviz_beam_last_params", {})
	if params.is_empty():
		return
	_update_gobo_overlay(light, params)

func _update_gobo_overlay(light: SpotLight3D, params: Dictionary) -> void:
	var overlay: MeshInstance3D = light.get_meta(GOBO_OVERLAY_META_KEY) as MeshInstance3D if light.has_meta(GOBO_OVERLAY_META_KEY) else null
	if overlay == null:
		return

	var gobo_texture: Texture2D = light.light_projector
	if gobo_texture == null:
		overlay.visible = false
		return

	var intensity: float = clamp(float(params.get("scaled_intensity", 0.0)), 0.0, 3.0)
	if intensity <= 0.001:
		overlay.visible = false
		return

	var beam_range: float = max(float(params.get("beam_range", 0.1)), GOBO_OVERLAY_MIN_DISTANCE_M)
	var beam_angle: float = max(float(params.get("beam_angle", 1.0)), 0.1)
	var lens_radius: float = max(float(params.get("lens_radius", 0.03)), 0.005)
	var half_angle_deg: float = beam_angle * 0.5
	var overlay_distance: float = max(beam_range * 0.05, lens_radius * 2.0, GOBO_OVERLAY_MIN_DISTANCE_M)
	var radius: float = tan(deg_to_rad(half_angle_deg)) * overlay_distance
	radius = max(radius, lens_radius)

	overlay.position = Vector3(0.0, 0.0, -overlay_distance)
	overlay.scale = Vector3(radius * 2.0, radius * 2.0, 1.0)

	var overlay_material: ShaderMaterial = overlay.material_override as ShaderMaterial
	if overlay_material == null:
		overlay.visible = false
		return
	overlay_material.set_shader_parameter("gobo_texture", gobo_texture)
	overlay_material.set_shader_parameter("intensity", clamp(intensity * GOBO_OVERLAY_INTENSITY_SCALE, 0.0, 3.0))
	overlay_material.set_shader_parameter("radial_softness", 0.08)
	overlay.visible = true

func _hide_gobo_overlay(light: SpotLight3D) -> void:
	if not light.has_meta(GOBO_OVERLAY_META_KEY):
		return
	var overlay: MeshInstance3D = light.get_meta(GOBO_OVERLAY_META_KEY) as MeshInstance3D
	if overlay != null:
		overlay.visible = false

func cleanup_beam(light: SpotLight3D) -> void:
	if not light.has_meta(BEAM_META_KEY):
		return
	var cone: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
	if cone != null and is_instance_valid(cone):
		cone.queue_free()
	light.remove_meta(BEAM_META_KEY)
	if light.has_meta(GOBO_OVERLAY_META_KEY):
		var overlay: MeshInstance3D = light.get_meta(GOBO_OVERLAY_META_KEY) as MeshInstance3D
		if overlay != null and is_instance_valid(overlay):
			overlay.queue_free()
		light.remove_meta(GOBO_OVERLAY_META_KEY)
