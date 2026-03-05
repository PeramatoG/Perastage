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
const GOBO_TEXTURE_META_KEY: String = "peraviz_gobo_texture"

var _material_template: ShaderMaterial

func _init() -> void:
	_material_template = ShaderMaterial.new()
	_material_template.shader = load("res://scripts/shaders/legacy_beam_cone.gdshader")

func ensure_beam(light: SpotLight3D) -> void:
	if not light.has_meta(MAIN_KEY):
		light.set_meta(MAIN_KEY, _create_cone("PeravizBeamCone"))
	if not light.has_meta(MID_KEY):
		light.set_meta(MID_KEY, _create_cone("PeravizBeamMidCone"))
	if not light.has_meta(CORE_KEY):
		light.set_meta(CORE_KEY, _create_cone("PeravizBeamCoreCone"))

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
	# Keep legacy cone beams visible even with native fog-projector gobos enabled.
	# This preserves beam readability while the spotlight projector handles gobo footprint/fog shading.


	var beam_half_angle_deg: float = beam_angle * 0.5
	var radius: float = tan(deg_to_rad(beam_half_angle_deg)) * beam_range
	var bottom_radius: float = clamp(radius, 0.03, EMITTER_CONE_MAX_BASE_RADIUS_M)

	_update_cone_geometry(cone, lens_radius, bottom_radius, beam_range, 1.0)
	_update_cone_geometry(mid_cone, lens_radius, bottom_radius, beam_range, 0.7)
	_update_cone_geometry(core_cone, lens_radius, bottom_radius, beam_range, 0.45)

	var gobo_texture: Texture2D = null
	if light.has_meta(GOBO_TEXTURE_META_KEY):
		gobo_texture = light.get_meta(GOBO_TEXTURE_META_KEY) as Texture2D
	var native_fog_projector_enabled: bool = bool(params.get("use_native_fog_projector_gobos", true))
	var fog_density: float = float(params.get("volumetric_fog_density", 0.0))
	var fog_projection_active: bool = native_fog_projector_enabled and gobo_texture != null and fog_density > 0.0001
	if fog_projection_active:
		# Keep engine fog projection authoritative only while volumetric fog is active.
		if cone != null:
			cone.visible = false
		if mid_cone != null:
			mid_cone.visible = false
		if core_cone != null:
			core_cone.visible = false
		return
	var color_alpha := Color(beam_color.r, beam_color.g, beam_color.b, 1.0)
	_update_cone_material(cone, color_alpha, scaled_intensity, beam_range, bottom_radius, 0.35, 0.16, 0.06, 1.0, 1.0, gobo_texture)
	_update_cone_material(mid_cone, color_alpha, scaled_intensity, beam_range, bottom_radius * 0.7, 0.18, 0.26, 0.04, 1.35, 1.25, gobo_texture)
	_update_cone_material(core_cone, color_alpha, scaled_intensity, beam_range, bottom_radius * 0.45, 0.09, 0.35, 0.02, 1.7, 1.5, gobo_texture)

func cleanup_beam(light: SpotLight3D) -> void:
	for meta_key in [MAIN_KEY, MID_KEY, CORE_KEY]:
		if not light.has_meta(meta_key):
			continue
		var cone: MeshInstance3D = light.get_meta(meta_key) as MeshInstance3D
		if cone != null and is_instance_valid(cone):
			cone.queue_free()
		light.remove_meta(meta_key)

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
	cone.material_override = _material_template.duplicate(true)
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

func _update_cone_material(cone: MeshInstance3D, beam_color: Color, scaled_intensity: float, beam_range: float, gobo_projection_radius: float, lateral_softness: float, lateral_emission_boost: float, noise_strength: float, alpha_scale: float, emission_scale: float, gobo_texture: Texture2D) -> void:
	if cone == null:
		return
	cone.set_instance_shader_parameter("beam_color", beam_color)
	cone.set_instance_shader_parameter("near_alpha", lerp(0.0, EMITTER_CONE_NEAR_ALPHA * alpha_scale, scaled_intensity))
	cone.set_instance_shader_parameter("far_alpha", lerp(0.0, EMITTER_CONE_FAR_ALPHA * alpha_scale, scaled_intensity))
	cone.set_instance_shader_parameter("near_emission", lerp(0.0, EMITTER_CONE_NEAR_EMISSION * emission_scale, scaled_intensity))
	cone.set_instance_shader_parameter("far_emission", lerp(0.0, EMITTER_CONE_FAR_EMISSION * emission_scale, scaled_intensity))
	cone.set_instance_shader_parameter("cone_height", max(beam_range, 0.001))
	cone.set_instance_shader_parameter("gobo_projection_radius", max(gobo_projection_radius, 0.001))
	cone.set_instance_shader_parameter("fade_end_ratio", EMITTER_CONE_FADE_END_RATIO)
	cone.set_instance_shader_parameter("lateral_softness", lateral_softness)
	cone.set_instance_shader_parameter("lateral_emission_boost", lateral_emission_boost)
	cone.set_instance_shader_parameter("volumetric_noise_strength", noise_strength)
	var cone_material: ShaderMaterial = cone.material_override as ShaderMaterial
	if cone_material != null:
		cone_material.set_shader_parameter("use_gobo", gobo_texture != null)
		cone_material.set_shader_parameter("gobo_invert", true)
		if gobo_texture != null:
			cone_material.set_shader_parameter("gobo_texture", gobo_texture)
