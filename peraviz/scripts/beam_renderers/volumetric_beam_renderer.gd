extends RefCounted
class_name VolumetricBeamRenderer

const BEAM_META_KEY: String = "peraviz_volumetric_beam"
const LOCAL_FOG_VOLUME_META_KEY: String = "peraviz_local_fog_volume"
const EMITTER_CONE_MAX_BASE_RADIUS_M: float = 10.0
const VOLUMETRIC_INTENSITY_SCALE: float = 0.75

var _beam_material_template: ShaderMaterial
var _camera: Camera3D
var _settings: Dictionary = {}
var _gobo_controller: GoboBeamController

func _init(gobo_controller: GoboBeamController = null) -> void:
	_beam_material_template = ShaderMaterial.new()
	_beam_material_template.shader = load("res://scripts/shaders/volumetric_beam.gdshader")
	_gobo_controller = gobo_controller if gobo_controller != null else GoboBeamController.new()

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
	var gobo_texture: Texture2D = params.get("gobo_texture", null) as Texture2D

	if not bool(params.get("is_visible", true)) or intensity <= threshold:
		cone.visible = false
		if _gobo_controller != null:
			_gobo_controller.apply_gobo(light, null, beam_angle, beam_range, 0.03, _settings)
		return

	var distance_limit: float = float(params.get("distance_cull_m", 180.0))
	if _camera != null:
		var cam_distance: float = _camera.global_position.distance_to(light.global_position)
		if cam_distance > distance_limit or _camera.is_position_behind(light.global_position):
			cone.visible = false
			if _gobo_controller != null:
				_gobo_controller.apply_gobo(light, null, beam_angle, beam_range, 0.03, _settings)
			return

	var beam_color: Color = params.get("beam_color", Color.WHITE)
	var lens_radius: float = max(float(params.get("lens_radius", 0.03)), 0.005)
	var bottom_radius: float = clamp(tan(deg_to_rad(beam_angle * 0.5)) * beam_range, 0.03, EMITTER_CONE_MAX_BASE_RADIUS_M)
	var cone_mesh: CylinderMesh = cone.mesh as CylinderMesh
	if cone_mesh != null:
		cone_mesh.top_radius = max(lens_radius, 0.003)
		cone_mesh.bottom_radius = bottom_radius
		cone_mesh.height = beam_range
	cone.position = Vector3(0.0, 0.0, -beam_range * 0.5)
	var render_mesh_for_gobo: bool = bool(params.get("render_mesh_for_gobo", false))
	var mesh_visible: bool = (gobo_texture == null) or render_mesh_for_gobo
	cone.visible = mesh_visible

	var intensity_alpha: float = clamp(intensity * VOLUMETRIC_INTENSITY_SCALE, 0.0, 1.0)
	if gobo_texture != null and not render_mesh_for_gobo:
		intensity_alpha = 0.0
	cone.set_instance_shader_parameter("base_color", Color(beam_color.r, beam_color.g, beam_color.b, intensity_alpha))
	cone.set_instance_shader_parameter("max_brightness", lerp(1.0, 10.0, intensity))
	cone.set_instance_shader_parameter("beam_top_radius", max(lens_radius, 0.003))
	cone.set_instance_shader_parameter("beam_bottom_radius", bottom_radius)
	cone.set_instance_shader_parameter("beam_height", beam_range)

	var gobo_settings: Dictionary = _settings.duplicate(true)
	for key in params.keys():
		if str(key).begins_with("gobo_"):
			gobo_settings[key] = params[key]
	for key in ["distance_to_occluder_m", "base_quad_size_m", "overscan_ratio", "invert_gobo", "disable_fog", "light_volumetric_fog_energy", "debug_show_occluder"]:
		if params.has(key):
			gobo_settings[key] = params[key]
	gobo_settings["beam_mesh"] = cone
	gobo_settings["light_volumetric_fog_energy"] = params.get("light_volumetric_fog_energy", _settings.get("light_volumetric_fog_energy", 1.0))
	_update_local_fog_volume(light, gobo_texture != null, beam_range, bottom_radius, params)
	if _gobo_controller != null:
		_gobo_controller.apply_gobo(light, gobo_texture, beam_angle, beam_range, lens_radius, gobo_settings)

func cleanup_beam(light: SpotLight3D) -> void:
	if light.has_meta(BEAM_META_KEY):
		var cone: MeshInstance3D = light.get_meta(BEAM_META_KEY) as MeshInstance3D
		if cone != null and is_instance_valid(cone):
			cone.queue_free()
		light.remove_meta(BEAM_META_KEY)
	if light.has_meta(LOCAL_FOG_VOLUME_META_KEY):
		var local_fog: FogVolume = light.get_meta(LOCAL_FOG_VOLUME_META_KEY) as FogVolume
		if local_fog != null and is_instance_valid(local_fog):
			local_fog.queue_free()
		light.remove_meta(LOCAL_FOG_VOLUME_META_KEY)
	if _gobo_controller != null:
		_gobo_controller.cleanup_gobo(light)

func _update_local_fog_volume(light: SpotLight3D, gobo_active: bool, beam_range: float, bottom_radius: float, params: Dictionary) -> void:
	var use_local_fog_volume: bool = bool(params.get("use_local_fog_volume", true))
	if not use_local_fog_volume:
		if light.has_meta(LOCAL_FOG_VOLUME_META_KEY):
			var existing: FogVolume = light.get_meta(LOCAL_FOG_VOLUME_META_KEY) as FogVolume
			if existing != null and is_instance_valid(existing):
				existing.queue_free()
			light.remove_meta(LOCAL_FOG_VOLUME_META_KEY)
		return

	var fog_volume: FogVolume = null
	if light.has_meta(LOCAL_FOG_VOLUME_META_KEY):
		fog_volume = light.get_meta(LOCAL_FOG_VOLUME_META_KEY) as FogVolume
	if fog_volume == null or not is_instance_valid(fog_volume):
		fog_volume = FogVolume.new()
		fog_volume.name = "PeravizBeamLocalFogVolume"
		# Use numeric shape assignment for broad Godot 4.x compatibility.
		# Default 2 keeps a cone-like volume aligned with the spotlight beam.
		fog_volume.set("shape", int(params.get("local_fog_shape", 2)))
		var fog_material := FogMaterial.new()
		fog_volume.material = fog_material
		light.add_child(fog_volume)
		light.set_meta(LOCAL_FOG_VOLUME_META_KEY, fog_volume)

	var fog_material_ref: FogMaterial = fog_volume.material as FogMaterial
	if fog_material_ref != null:
		fog_material_ref.density = float(params.get("local_fog_density", 0.015))

	fog_volume.position = Vector3(0.0, 0.0, -beam_range * 0.5)
	var diameter: float = max(bottom_radius * 2.0, 0.08)
	fog_volume.size = Vector3(diameter, diameter, max(beam_range * 1.05, 0.25))
