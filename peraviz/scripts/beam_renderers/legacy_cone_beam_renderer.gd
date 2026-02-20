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

const DEFAULT_CONE_COUNT: int = 2
const DEFAULT_OVERLAY_SCALE: float = 0.96
const DEFAULT_CORE_SCALE: float = 0.92

const GoboBeamMaterialCacheScript = preload("res://scripts/rendering/gobo_beam_material_cache.gd")

var _shared_material: ShaderMaterial
var _camera: Camera3D
var _settings: Dictionary = {}
var _gobo_material_cache: GoboBeamMaterialCache

func _init() -> void:
	_shared_material = ShaderMaterial.new()
	_shared_material.shader = load("res://scripts/shaders/legacy_beam_cone.gdshader")
	_gobo_material_cache = GoboBeamMaterialCacheScript.new()

func configure(view_camera: Camera3D, settings: Dictionary) -> void:
	_camera = view_camera
	_settings = settings.duplicate(true)
	if _gobo_material_cache != null:
		_gobo_material_cache.apply_settings_to_all_cached_materials(_settings)
		_gobo_material_cache.get_material(_shared_material, null, _settings, false)

func clear_cached_materials() -> void:
	if _gobo_material_cache != null:
		_gobo_material_cache.clear()

func ensure_beam(light: SpotLight3D) -> void:
	if not light.has_meta(MAIN_KEY):
		light.set_meta(MAIN_KEY, _create_cone("PeravizBeamCone"))
	if not light.has_meta(MID_KEY):
		light.set_meta(MID_KEY, _create_cone("PeravizBeamOverlayCone"))
	if not light.has_meta(CORE_KEY):
		light.set_meta(CORE_KEY, _create_cone("PeravizBeamCoreCone"))

func update_beam(light: SpotLight3D, params: Dictionary) -> void:
	ensure_beam(light)
	var cone: MeshInstance3D = light.get_meta(MAIN_KEY) as MeshInstance3D
	var overlay_cone: MeshInstance3D = light.get_meta(MID_KEY) as MeshInstance3D
	var core_cone: MeshInstance3D = light.get_meta(CORE_KEY) as MeshInstance3D
	if cone == null and overlay_cone == null and core_cone == null:
		return
	_attach_if_needed(light, cone)
	_attach_if_needed(light, overlay_cone)
	_attach_if_needed(light, core_cone)

	var intensity: float = clamp(float(params.get("normalized_dimmer", 0.0)), 0.0, 1.0)
	var scaled_intensity: float = clamp(float(params.get("scaled_intensity", 0.0)), 0.0, 3.0)
	var beam_range: float = max(float(params.get("beam_range", 0.1)), 0.01)
	var beam_angle: float = max(float(params.get("beam_angle", 1.0)), 0.1)
	var beam_color: Color = params.get("beam_color", Color.WHITE)
	var lens_radius: float = max(float(params.get("lens_radius", 0.03)), 0.005)
	var is_visible: bool = bool(params.get("is_visible", true)) and intensity > 0.015
	var cone_count: int = int(clamp(int(_settings.get("legacy_cone_count", DEFAULT_CONE_COUNT)), 2, 3))

	if cone != null:
		cone.visible = is_visible
	if overlay_cone != null:
		overlay_cone.visible = is_visible
	if core_cone != null:
		core_cone.visible = is_visible and cone_count >= 3
	if not is_visible:
		return

	var beam_half_angle_deg: float = beam_angle * 0.5
	var radius: float = tan(deg_to_rad(beam_half_angle_deg)) * beam_range
	var bottom_radius: float = clamp(radius, 0.03, EMITTER_CONE_MAX_BASE_RADIUS_M)
	var overlay_scale: float = clamp(float(_settings.get("legacy_overlay_scale", DEFAULT_OVERLAY_SCALE)), 0.85, 1.0)
	var core_scale: float = clamp(float(_settings.get("legacy_core_scale", DEFAULT_CORE_SCALE)), 0.8, overlay_scale)

	_update_cone_geometry(cone, lens_radius, bottom_radius, beam_range, 1.0)
	_update_cone_geometry(overlay_cone, lens_radius, bottom_radius, beam_range, overlay_scale)
	if cone_count >= 3:
		_update_cone_geometry(core_cone, lens_radius, bottom_radius, beam_range, core_scale)

	var color_alpha := Color(beam_color.r, beam_color.g, beam_color.b, 1.0)
	_update_cone_material(cone, color_alpha, scaled_intensity, beam_range, 0.35, 0.16, 0.06, 1.0, 1.0)
	_update_cone_material(overlay_cone, color_alpha, scaled_intensity, beam_range, 0.24, 0.28, 0.04, 1.1, 1.25)
	if cone_count >= 3:
		_update_cone_material(core_cone, color_alpha, scaled_intensity, beam_range, 0.2, 0.35, 0.02, 0.95, 1.15)

	if cone != null:
		cone.material_override = _shared_material
	if cone_count >= 3 and core_cone != null:
		core_cone.material_override = _shared_material
	if overlay_cone != null:
		overlay_cone.material_override = _resolve_overlay_material(light)

func cleanup_beam(light: SpotLight3D) -> void:
	for meta_key in [MAIN_KEY, MID_KEY, CORE_KEY]:
		if not light.has_meta(meta_key):
			continue
		var cone: MeshInstance3D = light.get_meta(meta_key) as MeshInstance3D
		if cone != null and is_instance_valid(cone):
			cone.queue_free()
		light.remove_meta(meta_key)

func _resolve_overlay_material(light: SpotLight3D) -> ShaderMaterial:
	var gobo_enabled: bool = bool(_settings.get("beam_gobo_enabled", true))
	if not gobo_enabled:
		return _gobo_material_cache.get_material(_shared_material, null, _settings, false)

	var max_distance: float = float(_settings.get("legacy_beam_gobo_max_distance", 180.0))
	if max_distance > 0.0 and _camera != null:
		var cam_distance: float = _camera.global_position.distance_to(light.global_position)
		if cam_distance > max_distance:
			return _gobo_material_cache.get_material(_shared_material, null, _settings, false)

	var gobo_tex: Texture2D = light.light_projector as Texture2D
	return _gobo_material_cache.get_material(_shared_material, gobo_tex, _settings, gobo_tex != null)

func _attach_if_needed(light: SpotLight3D, cone: MeshInstance3D) -> void:
	if cone != null and cone.get_parent() == null:
		light.add_child(cone)

func _create_cone(cone_name: String) -> MeshInstance3D:
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
	cone.material_override = _shared_material
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

func _update_cone_material(cone: MeshInstance3D, beam_color: Color, scaled_intensity: float, beam_range: float, lateral_softness: float, lateral_emission_boost: float, noise_strength: float, alpha_scale: float, emission_scale: float) -> void:
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
