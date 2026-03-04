extends RefCounted
class_name GoboBeamController

const FAKE_GOBO_TEXTURE_SIZE: int = 1024
const GOBO_TEXTURE_META_KEY: String = "peraviz_gobo_texture"
const GOBO_OCCLUDER_META_KEY: String = "peraviz_gobo_occluder"
const GOBO_OCCLUDER_SHADER_MATERIAL_META_KEY: String = "peraviz_gobo_occluder_shader_material"
const GOBO_DEBUG_LOG_THROTTLE_MS: int = 400

const DEFAULT_DISTANCE_TO_OCCLUDER_M: float = 0.043
const DEFAULT_BASE_QUAD_SIZE_M: float = 0.017
const DEFAULT_OVERSCAN_RATIO: float = 1.12
const DEFAULT_GOBO_CUTOFF: float = 0.5
const DEFAULT_ALPHA_SCISSOR_THRESHOLD: float = 0.5

var _texture_cache: Dictionary = {}
var _gobo_occluder_material_template: ShaderMaterial
var _last_gobo_debug_log_ticks_by_light: Dictionary = {}

func _init() -> void:
	_gobo_occluder_material_template = ShaderMaterial.new()
	_gobo_occluder_material_template.shader = load("res://scripts/shaders/gobo_occluder.gdshader")

func clear_cache() -> void:
	_texture_cache.clear()

func resolve_composed_gobo_texture(controls: Dictionary) -> Texture2D:
	if not bool(controls.get("has_gobo", false)):
		return null
	var runtime_bindings: Array = controls.get("gobo_runtime_bindings", [])
	if runtime_bindings.is_empty():
		var fallback_raw_8bit: int = _resolve_gobo_raw_8bit(controls)
		runtime_bindings = [{
			"raw_8bit": fallback_raw_8bit,
			"slot_index": _resolve_active_gobo_slot_index(controls, fallback_raw_8bit),
			"slots": controls.get("gobo_slots", []),
		}]

	var active_textures: Array[Texture2D] = []
	for wheel in runtime_bindings:
		if wheel is not Dictionary:
			continue
		var slot_index: int = int(wheel.get("slot_index", 0))
		if slot_index <= 0:
			continue
		var wheel_controls := {
			"gobo_slots": wheel.get("slots", []),
		}
		var gobo_texture: Texture2D = _resolve_gobo_texture_for_slot(wheel_controls, slot_index)
		if gobo_texture == null and bool(controls.get("allow_fake_gobo_fallback", false)):
			gobo_texture = _resolve_fake_gobo_texture(int(wheel.get("raw_8bit", 0)))
		if gobo_texture != null:
			active_textures.append(gobo_texture)

	if active_textures.is_empty():
		return null
	return _compose_gobo_textures(active_textures)

func apply_gobo(light: SpotLight3D, gobo_texture: Texture2D, beam_angle_deg: float, beam_range_m: float, lens_radius_m: float, settings: Dictionary) -> void:
	if light == null or not is_instance_valid(light):
		return

	light.set_meta(GOBO_TEXTURE_META_KEY, gobo_texture)
	light.light_projector = null
	light.shadow_enabled = gobo_texture != null
	light.light_volumetric_fog_energy = float(settings.get("light_volumetric_fog_energy", light.light_volumetric_fog_energy))

	var cone: MeshInstance3D = settings.get("beam_mesh", null) as MeshInstance3D
	if cone == null:
		return

	if gobo_texture == null:
		_disable_gobo_for_light(light, cone)
		return

	var distance_to_occluder: float = max(float(settings.get("distance_to_occluder_m", DEFAULT_DISTANCE_TO_OCCLUDER_M)), 0.0001)
	var base_quad_size: float = max(float(settings.get("base_quad_size_m", DEFAULT_BASE_QUAD_SIZE_M)), 0.0001)
	var overscan_ratio: float = max(float(settings.get("overscan_ratio", DEFAULT_OVERSCAN_RATIO)), 1.0)
	var gobo_scale_ratio: float = max(float(settings.get("gobo_scale_ratio", 1.0)), 0.001)
	var gobo_rotation_deg: float = float(settings.get("gobo_rotation_deg", 0.0))
	var invert_gobo: bool = bool(settings.get("invert_gobo", false))
	var alpha_scissor_threshold: float = clamp(float(settings.get("alpha_scissor_threshold", DEFAULT_ALPHA_SCISSOR_THRESHOLD)), 0.01, 0.99)
	var lens_reference_radius: float = max(float(settings.get("lens_reference_radius_m", 0.03)), 0.0001)
	var lens_radius_ratio: float = max(lens_radius_m / lens_reference_radius, 0.01)
	var lens_radius_influence: float = clamp(float(settings.get("lens_radius_influence", 0.0)), 0.0, 1.0)
	var lens_scale: float = lerp(1.0, lens_radius_ratio, lens_radius_influence)

	var gobo_size_m: float = _compute_cone_diameter_at_distance(beam_angle_deg, distance_to_occluder)
	gobo_size_m *= overscan_ratio * gobo_scale_ratio * lens_scale

	var gobo_occluder: MeshInstance3D = _ensure_gobo_occluder(light, base_quad_size)
	if gobo_occluder == null:
		return
	var gobo_material: ShaderMaterial = light.get_meta(GOBO_OCCLUDER_SHADER_MATERIAL_META_KEY, null) as ShaderMaterial
	if gobo_material == null:
		return

	var footprint_scale: float = max(gobo_size_m / base_quad_size, 0.001)
	gobo_occluder.scale = Vector3(footprint_scale, footprint_scale, 1.0)
	gobo_occluder.position = Vector3(0.0, 0.0, -distance_to_occluder)
	var debug_show_occluder: bool = bool(settings.get("debug_show_occluder", false))
	gobo_occluder.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON if debug_show_occluder else GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
	gobo_occluder.visible = true
	gobo_material.set_shader_parameter("gobo_texture", gobo_texture)
	gobo_material.set_shader_parameter("invert_mask", invert_gobo)
	gobo_material.set_shader_parameter("rotation_rad", deg_to_rad(gobo_rotation_deg))
	gobo_material.set_shader_parameter("alpha_scissor_threshold", alpha_scissor_threshold)
	gobo_occluder.material_override = gobo_material

	cone.set_instance_shader_parameter("gobo_enabled", true)
	cone.set_instance_shader_parameter("gobo_start_ratio", distance_to_occluder / max(beam_range_m, 0.001))
	cone.set_instance_shader_parameter("gobo_size", Vector2(gobo_size_m, gobo_size_m))
	cone.set_instance_shader_parameter("gobo_rotation", deg_to_rad(gobo_rotation_deg))
	cone.set_instance_shader_parameter("gobo_cutoff", float(settings.get("gobo_cutoff", DEFAULT_GOBO_CUTOFF)))
	cone.set_instance_shader_parameter("gobo_invert", invert_gobo)
	cone.set_instance_shader_parameter("disable_fog", bool(settings.get("disable_fog", false)))
	var cone_material: ShaderMaterial = cone.material_override as ShaderMaterial
	if cone_material != null:
		cone_material.set_shader_parameter("gobo_texture", gobo_texture)

	_maybe_log_gobo_parameters(light, beam_angle_deg, beam_range_m, gobo_size_m, settings)

func cleanup_gobo(light: SpotLight3D) -> void:
	if light == null or not is_instance_valid(light):
		return
	light.light_projector = null
	light.remove_meta(GOBO_TEXTURE_META_KEY)
	if light.has_meta(GOBO_OCCLUDER_META_KEY):
		var gobo_occluder: MeshInstance3D = light.get_meta(GOBO_OCCLUDER_META_KEY) as MeshInstance3D
		if gobo_occluder != null and is_instance_valid(gobo_occluder):
			gobo_occluder.queue_free()
		light.remove_meta(GOBO_OCCLUDER_META_KEY)
	if light.has_meta(GOBO_OCCLUDER_SHADER_MATERIAL_META_KEY):
		light.remove_meta(GOBO_OCCLUDER_SHADER_MATERIAL_META_KEY)

func _disable_gobo_for_light(light: SpotLight3D, cone: MeshInstance3D) -> void:
	light.shadow_enabled = false
	if light.has_meta(GOBO_OCCLUDER_META_KEY):
		var gobo_occluder: MeshInstance3D = light.get_meta(GOBO_OCCLUDER_META_KEY) as MeshInstance3D
		if gobo_occluder != null:
			gobo_occluder.visible = false
	var cone_material: ShaderMaterial = cone.material_override as ShaderMaterial
	if cone_material != null:
		cone_material.set_shader_parameter("gobo_texture", null)
	cone.set_instance_shader_parameter("gobo_enabled", false)
	cone.set_instance_shader_parameter("gobo_invert", false)

func _ensure_gobo_occluder(light: SpotLight3D, base_quad_size: float) -> MeshInstance3D:
	if light.has_meta(GOBO_OCCLUDER_META_KEY):
		var existing: MeshInstance3D = light.get_meta(GOBO_OCCLUDER_META_KEY) as MeshInstance3D
		if existing != null and is_instance_valid(existing):
			return existing

	var gobo_mesh := QuadMesh.new()
	gobo_mesh.size = Vector2(base_quad_size, base_quad_size)
	var gobo_occluder := MeshInstance3D.new()
	gobo_occluder.name = "PeravizGoboOccluder"
	gobo_occluder.mesh = gobo_mesh
	gobo_occluder.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
	gobo_occluder.visible = false
	gobo_occluder.gi_mode = GeometryInstance3D.GI_MODE_DISABLED
	var occluder_shader_material: ShaderMaterial = _gobo_occluder_material_template.duplicate(true)
	gobo_occluder.material_override = occluder_shader_material
	light.add_child(gobo_occluder)
	light.set_meta(GOBO_OCCLUDER_META_KEY, gobo_occluder)
	light.set_meta(GOBO_OCCLUDER_SHADER_MATERIAL_META_KEY, occluder_shader_material)
	return gobo_occluder

func _compute_cone_diameter_at_distance(beam_angle_deg: float, distance_m: float) -> float:
	var half_angle_rad: float = deg_to_rad(max(beam_angle_deg, 0.1) * 0.5)
	return max(2.0 * tan(half_angle_rad) * max(distance_m, 0.0001), 0.001)

func _resolve_gobo_raw_8bit(controls: Dictionary) -> int:
	var gobo_raw: int = int(round(clamp(float(controls.get("gobo_norm", 0.0)), 0.0, 1.0) * 255.0))
	if controls.has("gobo_raw_value") and controls.has("gobo_resolution_bits"):
		var raw_value: int = int(controls.get("gobo_raw_value", gobo_raw))
		var resolution_bits: int = int(controls.get("gobo_resolution_bits", 8))
		if resolution_bits > 8:
			var shift_bits: int = max(0, resolution_bits - 8)
			gobo_raw = raw_value >> shift_bits
		else:
			gobo_raw = raw_value
	return clampi(gobo_raw, 0, 255)

func _resolve_active_gobo_slot_index(controls: Dictionary, gobo_raw_8bit: int) -> int:
	var gobo_ranges: Array = controls.get("gobo_ranges", [])
	if gobo_ranges.is_empty():
		return -1
	for item in gobo_ranges:
		if item is not Dictionary:
			continue
		var dmx_from: int = int(item.get("dmx_from", 0))
		var dmx_to: int = int(item.get("dmx_to", dmx_from))
		if dmx_to < dmx_from:
			var swap_value: int = dmx_from
			dmx_from = dmx_to
			dmx_to = swap_value
		if gobo_raw_8bit >= dmx_from and gobo_raw_8bit <= dmx_to:
			return int(item.get("slot_index", -1))
	return -1

func _resolve_gobo_texture_for_slot(controls: Dictionary, slot_index: int) -> Texture2D:
	var gobo_slots: Array = controls.get("gobo_slots", [])
	for item in gobo_slots:
		if item is not Dictionary:
			continue
		if int(item.get("slot_index", -1)) != slot_index:
			continue
		var image_path: String = str(item.get("image_path", ""))
		if image_path.is_empty():
			return null
		if _texture_cache.has(image_path):
			return _texture_cache[image_path] as Texture2D
		var image := Image.new()
		var load_error: Error = image.load(image_path)
		if load_error != OK:
			return null
		var texture: ImageTexture = ImageTexture.create_from_image(image)
		_texture_cache[image_path] = texture
		return texture
	return null

func _compose_gobo_textures(textures: Array[Texture2D]) -> Texture2D:
	if textures.is_empty():
		return null
	if textures.size() == 1:
		return textures[0]

	var key_parts: PackedStringArray = PackedStringArray()
	for texture in textures:
		if texture != null:
			key_parts.append(str(texture.get_rid().get_id()))
	var cache_key: String = "__composed_gobo_" + "_".join(key_parts)
	if _texture_cache.has(cache_key):
		return _texture_cache[cache_key] as Texture2D

	var composed: Image = Image.create(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE, false, Image.FORMAT_RGBA8)
	composed.fill(Color(1.0, 1.0, 1.0, 1.0))
	for texture in textures:
		if texture == null:
			continue
		var image: Image = texture.get_image()
		if image == null:
			continue
		if image.get_width() != FAKE_GOBO_TEXTURE_SIZE or image.get_height() != FAKE_GOBO_TEXTURE_SIZE:
			image.resize(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE, Image.INTERPOLATE_LANCZOS)
		for y in range(FAKE_GOBO_TEXTURE_SIZE):
			for x in range(FAKE_GOBO_TEXTURE_SIZE):
				var dst: Color = composed.get_pixel(x, y)
				var src: Color = image.get_pixel(x, y)
				composed.set_pixel(x, y, Color(dst.r * src.r, dst.g * src.g, dst.b * src.b, dst.a * src.a))

	var composed_texture: ImageTexture = ImageTexture.create_from_image(composed)
	_texture_cache[cache_key] = composed_texture
	return composed_texture

func _resolve_fake_gobo_texture(gobo_raw_8bit: int) -> Texture2D:
	var cache_key: String = "__fake_gobo_" + str(gobo_raw_8bit)
	if _texture_cache.has(cache_key):
		return _texture_cache[cache_key] as Texture2D

	var image: Image = Image.create(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE, false, Image.FORMAT_RGBA8)
	image.fill(Color(1.0, 1.0, 1.0, 1.0))
	var normalized: float = clamp(float(gobo_raw_8bit) / 255.0, 0.0, 1.0)
	if normalized > 0.02:
		var spoke_count: int = int(lerp(4.0, 18.0, normalized))
		var aperture: float = lerp(0.85, 0.22, normalized)
		var rotation_offset: float = normalized * TAU
		var center: Vector2 = Vector2(FAKE_GOBO_TEXTURE_SIZE * 0.5, FAKE_GOBO_TEXTURE_SIZE * 0.5)
		for y in range(FAKE_GOBO_TEXTURE_SIZE):
			for x in range(FAKE_GOBO_TEXTURE_SIZE):
				var uv: Vector2 = (Vector2(x, y) - center) / float(FAKE_GOBO_TEXTURE_SIZE)
				var radius: float = uv.length() * 2.0
				if radius > 1.0:
					image.set_pixel(x, y, Color.BLACK)
					continue
				var angle: float = atan2(uv.y, uv.x) + rotation_offset
				var spoke_wave: float = sin(angle * float(spoke_count)) * 0.5 + 0.5
				var radial_gate: float = smoothstep(aperture, aperture - 0.18, radius)
				var value: float = clamp(1.0 - max(spoke_wave * radial_gate, 0.0), 0.0, 1.0)
				image.set_pixel(x, y, Color(value, value, value, 1.0))

	var texture: ImageTexture = ImageTexture.create_from_image(image)
	_texture_cache[cache_key] = texture
	return texture

func _maybe_log_gobo_parameters(light: SpotLight3D, beam_angle: float, beam_range_visual: float, gobo_plane_size_world: float, settings: Dictionary) -> void:
	if not bool(settings.get("gobo_debug_log_parameters", false)):
		return

	var light_id: int = light.get_instance_id()
	var now_ticks: int = Time.get_ticks_msec()
	var last_ticks: int = int(_last_gobo_debug_log_ticks_by_light.get(light_id, 0))
	if (now_ticks - last_ticks) < GOBO_DEBUG_LOG_THROTTLE_MS:
		return
	_last_gobo_debug_log_ticks_by_light[light_id] = now_ticks

	var volumetric_size: int = int(ProjectSettings.get_setting("rendering/environment/volumetric_fog/volume_size", int(round(float(settings.get("volumetric_fog_volume_size", -1))))))
	var volumetric_depth: float = float(ProjectSettings.get_setting("rendering/environment/volumetric_fog/volume_depth", int(round(float(settings.get("volumetric_fog_depth", -1.0))))))
	var volumetric_filter_active: bool = bool(ProjectSettings.get_setting("rendering/environment/volumetric_fog/use_filter", bool(settings.get("volumetric_fog_use_filter", true))))
	print("[PeravizGoboDebug] light=", light.name,
		" beam_angle_full_deg=", beam_angle,
		" beam_range_visual=", beam_range_visual,
		" light_volumetric_fog_energy=", light.light_volumetric_fog_energy,
		" env_volume_size=", volumetric_size,
		" env_volume_depth=", volumetric_depth,
		" env_use_filter=", volumetric_filter_active,
		" gobo_size_world=", gobo_plane_size_world)
