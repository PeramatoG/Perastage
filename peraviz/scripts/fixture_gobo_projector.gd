extends RefCounted
class_name FixtureGoboProjector

const FAKE_GOBO_TEXTURE_SIZE: int = 1024

# Meta keys (single source of truth shared by footprint and beam)
const GOBO_TEXTURE_META_KEY: String = "peraviz_gobo_texture"
const GOBO_MODE_META_KEY: String = "peraviz_gobo_projection_mode"
const GOBO_PLANE_SIZE_META_KEY: String = "peraviz_gobo_plane_size_world"
const GOBO_PLANE_DISTANCE_META_KEY: String = "peraviz_gobo_plane_distance_m"
const GOBO_CUTOFF_META_KEY: String = "peraviz_gobo_cutoff"
const GOBO_SOFTNESS_META_KEY: String = "peraviz_gobo_softness"
const GOBO_ROTATION_META_KEY: String = "peraviz_gobo_rotation_radians"

const GOBO_OCCLUDER_META_KEY: String = "peraviz_gobo_occluder"
const GOBO_OCCLUDER_MATERIAL_META_KEY: String = "peraviz_gobo_occluder_material"

# Render layer 20 (bit 19) so only the gobo occluder casts cookie shadows.
const GOBO_SHADOW_LAYER: int = 1 << 19

const DEFAULT_GOBO_DISTANCE_M: float = 0.043
const DEFAULT_GOBO_CUTOFF: float = 0.5
const DEFAULT_GOBO_SOFTNESS: float = 0.04
const DEFAULT_GOBO_SCALE_RATIO: float = 1.0
const DEFAULT_GOBO_ROTATION_RAD: float = 0.0

enum ProjectionMode {
	SHADOW_COOKIE,
	PROJECTOR_COOKIE,
}

var _texture_cache: Dictionary = {}
var _occluder_shader: Shader = preload("res://scripts/shaders/gobo_occluder.gdshader")

func clear_cache() -> void:
	_texture_cache.clear()

func apply_gobo_projection(light: SpotLight3D, controls: Dictionary, visual_settings: Dictionary = {}) -> bool:
	if light == null or not is_instance_valid(light):
		return false

	var previous_meta_texture: Texture2D = null
	if light.has_meta(GOBO_TEXTURE_META_KEY):
		previous_meta_texture = light.get_meta(GOBO_TEXTURE_META_KEY) as Texture2D

	if not bool(controls.get("has_gobo", false)):
		_disable_gobo_runtime(light)
		return previous_meta_texture != null

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
		if gobo_texture == null:
			gobo_texture = _resolve_fake_gobo_texture(int(wheel.get("raw_8bit", 0)))
		if gobo_texture != null:
			active_textures.append(gobo_texture)

	if active_textures.is_empty():
		_disable_gobo_runtime(light)
		return previous_meta_texture != null

	var composed_gobo: Texture2D = _compose_gobo_textures(active_textures)
	light.set_meta(GOBO_TEXTURE_META_KEY, composed_gobo)

	var mode_name: String = str(visual_settings.get("gobo_projection_mode", "shadow_cookie")).to_lower()
	var mode: int = ProjectionMode.SHADOW_COOKIE
	if mode_name == "projector_cookie":
		mode = ProjectionMode.PROJECTOR_COOKIE
	light.set_meta(GOBO_MODE_META_KEY, mode)

	var gobo_scale_ratio: float = max(float(visual_settings.get("gobo_scale_ratio", DEFAULT_GOBO_SCALE_RATIO)), 0.001)
	var gobo_rotation: float = float(visual_settings.get("gobo_rotation_radians", DEFAULT_GOBO_ROTATION_RAD))
	var gobo_cutoff: float = clamp(float(visual_settings.get("gobo_cutoff", DEFAULT_GOBO_CUTOFF)), 0.0, 1.0)
	var gobo_softness: float = max(float(visual_settings.get("gobo_softness", DEFAULT_GOBO_SOFTNESS)), 0.0)

	light.set_meta(GOBO_CUTOFF_META_KEY, gobo_cutoff)
	light.set_meta(GOBO_SOFTNESS_META_KEY, gobo_softness)
	light.set_meta(GOBO_ROTATION_META_KEY, gobo_rotation)

	var spot_angle_rad: float = deg_to_rad(max(light.spot_angle, 0.1))
	var spot_range: float = max(light.spot_range, 0.01)
	var lens_radius: float = max(float(light.get_meta("peraviz_lens_radius", 0.03)), 0.003)

	var gobo_plane_distance_m: float = DEFAULT_GOBO_DISTANCE_M
	if mode == ProjectionMode.PROJECTOR_COOKIE:
		gobo_plane_distance_m = min(DEFAULT_GOBO_DISTANCE_M, spot_range * 0.25)
	else:
		gobo_plane_distance_m = clamp(float(visual_settings.get("gobo_occluder_distance_m", DEFAULT_GOBO_DISTANCE_M)), 0.002, 0.25)

	var cone_radius_at_plane: float = tan(spot_angle_rad) * gobo_plane_distance_m
	var half_size: float = max(cone_radius_at_plane, lens_radius) * gobo_scale_ratio
	var plane_size_world: float = max(half_size * 2.0, 0.001)

	light.set_meta(GOBO_PLANE_DISTANCE_META_KEY, gobo_plane_distance_m)
	light.set_meta(GOBO_PLANE_SIZE_META_KEY, plane_size_world)

	if mode == ProjectionMode.PROJECTOR_COOKIE:
		light.shadow_enabled = true
		light.shadow_caster_mask = 0
		light.light_projector = composed_gobo
		_disable_shadow_cookie_occluder(light)
	else:
		light.light_projector = null
		light.shadow_enabled = true
		light.shadow_caster_mask = GOBO_SHADOW_LAYER
		_ensure_shadow_cookie_occluder(light, composed_gobo, plane_size_world, gobo_plane_distance_m, bool(visual_settings.get("gobo_debug_show_occluder", false)))

	return composed_gobo != previous_meta_texture

func _disable_gobo_runtime(light: SpotLight3D) -> void:
	light.set_meta(GOBO_TEXTURE_META_KEY, null)
	light.set_meta(GOBO_PLANE_SIZE_META_KEY, 0.0)
	light.set_meta(GOBO_PLANE_DISTANCE_META_KEY, 0.0)
	light.set_meta(GOBO_CUTOFF_META_KEY, DEFAULT_GOBO_CUTOFF)
	light.set_meta(GOBO_SOFTNESS_META_KEY, DEFAULT_GOBO_SOFTNESS)
	light.set_meta(GOBO_ROTATION_META_KEY, DEFAULT_GOBO_ROTATION_RAD)
	light.set_meta(GOBO_MODE_META_KEY, ProjectionMode.SHADOW_COOKIE)
	light.light_projector = null
	light.shadow_enabled = false
	light.shadow_caster_mask = 0
	_disable_shadow_cookie_occluder(light)

func _ensure_shadow_cookie_occluder(light: SpotLight3D, gobo_texture: Texture2D, plane_size_world: float, plane_distance_m: float, debug_show: bool) -> void:
	var occluder: MeshInstance3D = null
	var material: ShaderMaterial = null

	if light.has_meta(GOBO_OCCLUDER_META_KEY):
		occluder = light.get_meta(GOBO_OCCLUDER_META_KEY) as MeshInstance3D
	if light.has_meta(GOBO_OCCLUDER_MATERIAL_META_KEY):
		material = light.get_meta(GOBO_OCCLUDER_MATERIAL_META_KEY) as ShaderMaterial

	if occluder == null or not is_instance_valid(occluder):
		var quad := QuadMesh.new()
		quad.size = Vector2(plane_size_world, plane_size_world)

		occluder = MeshInstance3D.new()
		occluder.name = "PeravizGoboOccluder"
		occluder.mesh = quad
		occluder.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
		occluder.visible = true
		occluder.layers = GOBO_SHADOW_LAYER

		material = ShaderMaterial.new()
		material.shader = _occluder_shader
		occluder.material_override = material

		light.add_child(occluder)
		light.set_meta(GOBO_OCCLUDER_META_KEY, occluder)
		light.set_meta(GOBO_OCCLUDER_MATERIAL_META_KEY, material)

	var quad_mesh: QuadMesh = occluder.mesh as QuadMesh
	if quad_mesh != null:
		quad_mesh.size = Vector2(plane_size_world, plane_size_world)

	occluder.position = Vector3(0.0, 0.0, -plane_distance_m)
	occluder.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON if debug_show else GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY

	if material != null:
		material.set_shader_parameter("gobo_texture", gobo_texture)

func _disable_shadow_cookie_occluder(light: SpotLight3D) -> void:
	if light.has_meta(GOBO_OCCLUDER_META_KEY):
		var occluder: MeshInstance3D = light.get_meta(GOBO_OCCLUDER_META_KEY) as MeshInstance3D
		if occluder != null and is_instance_valid(occluder):
			occluder.queue_free()
		light.remove_meta(GOBO_OCCLUDER_META_KEY)
	if light.has_meta(GOBO_OCCLUDER_MATERIAL_META_KEY):
		light.remove_meta(GOBO_OCCLUDER_MATERIAL_META_KEY)

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

		image.generate_mipmaps()

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
				var src_luma: float = (src.r + src.g + src.b) / 3.0
				var out_luma: float = dst.r * src_luma
				composed.set_pixel(x, y, Color(out_luma, out_luma, out_luma, 1.0))

	composed.generate_mipmaps()

	var out_texture: ImageTexture = ImageTexture.create_from_image(composed)
	_texture_cache[cache_key] = out_texture
	return out_texture

func _resolve_fake_gobo_texture(gobo_raw_8bit: int) -> Texture2D:
	var fake_bucket: int = gobo_raw_8bit >> 3
	var cache_key: String = "__fake_gobo_%d" % fake_bucket
	if _texture_cache.has(cache_key):
		return _texture_cache[cache_key] as Texture2D

	var image := Image.create(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE, false, Image.FORMAT_RGBA8)
	image.fill(Color(1.0, 1.0, 1.0, 1.0))
	if fake_bucket > 0:
		var step: int = max(4, int(4 + (fake_bucket % 6) * 2))
		var center: Vector2 = Vector2(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE) * 0.5
		var max_radius: float = float(FAKE_GOBO_TEXTURE_SIZE) * 0.48
		for y in range(FAKE_GOBO_TEXTURE_SIZE):
			for x in range(FAKE_GOBO_TEXTURE_SIZE):
				var uv: Vector2 = Vector2(float(x), float(y)) - center
				var dist: float = uv.length()
				if dist > max_radius:
					continue
				var x_cell: int = int(floor(float(x) / float(step)))
				var y_cell: int = int(floor(float(y) / float(step)))
				var checker: bool = (((x_cell + y_cell) + fake_bucket) % 2) == 0
				if checker:
					image.set_pixel(x, y, Color(0.0, 0.0, 0.0, 1.0))

	image.generate_mipmaps()

	var texture: ImageTexture = ImageTexture.create_from_image(image)
	_texture_cache[cache_key] = texture
	return texture
