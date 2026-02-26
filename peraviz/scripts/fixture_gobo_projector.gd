extends RefCounted
class_name FixtureGoboProjector

const FAKE_GOBO_TEXTURE_SIZE: int = 128
const GOBO_QUAD_META_KEY: String = "peraviz_gobo_alpha_quad"
const GOBO_QUAD_MATERIAL_META_KEY: String = "peraviz_gobo_alpha_quad_material"
const GOBO_QUAD_DEBUG_DISTANCE_M: float = 1.0
const GOBO_QUAD_MIN_SIZE_M: float = 0.015
const GOBO_QUAD_MAX_SIZE_M: float = 4.0
const GOBO_UV_OFFSET_DEFAULT: Vector2 = Vector2.ZERO
const GOBO_UV_SCALE_DEFAULT: Vector2 = Vector2.ONE

const GOBO_ALPHA_QUAD_SHADER: Shader = preload("res://scripts/shaders/gobo_alpha_quad.gdshader")

var _texture_cache: Dictionary = {}

func clear_cache() -> void:
	_texture_cache.clear()

func apply_gobo_projection(light: SpotLight3D, controls: Dictionary) -> void:
	if light == null or not is_instance_valid(light):
		return
	if not bool(controls.get("has_gobo", false)):
		light.light_projector = null
		light.shadow_enabled = false
		_clear_gobo_alpha_quad(light)
		return

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
		light.light_projector = null
		light.shadow_enabled = false
		_clear_gobo_alpha_quad(light)
		return

	light.shadow_enabled = true
	light.light_projector = null
	_apply_gobo_alpha_quad(light, _compose_gobo_textures(active_textures))

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
			image.resize(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE, Image.INTERPOLATE_BILINEAR)
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
	var fake_bucket: int = gobo_raw_8bit / 8
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
				var checker: bool = (((x / step) + (y / step) + fake_bucket) % 2) == 0
				if checker:
					image.set_pixel(x, y, Color(0.0, 0.0, 0.0, 1.0))

	image.generate_mipmaps()
	var texture: ImageTexture = ImageTexture.create_from_image(image)
	_texture_cache[cache_key] = texture
	return texture

func _apply_gobo_alpha_quad(light: SpotLight3D, gobo_texture: Texture2D) -> void:
	if gobo_texture == null:
		_clear_gobo_alpha_quad(light)
		return
	var quad: MeshInstance3D = _ensure_gobo_alpha_quad(light)
	if quad == null:
		return

	var quad_material: ShaderMaterial = _ensure_gobo_alpha_quad_material(light)
	if quad_material == null:
		return

	var texture_fit: Dictionary = _resolve_texture_fit(gobo_texture)
	quad_material.set_shader_parameter("gobo_texture", gobo_texture)
	quad_material.set_shader_parameter("alpha_cutoff", 0.5)
	quad_material.set_shader_parameter("uv_offset", texture_fit.get("uv_offset", GOBO_UV_OFFSET_DEFAULT))
	quad_material.set_shader_parameter("uv_scale", texture_fit.get("uv_scale", GOBO_UV_SCALE_DEFAULT))
	quad.material_override = quad_material
	quad.visible = true

	var cone_half_angle_rad: float = deg_to_rad(max(light.spot_angle, 0.1))
	var quad_distance: float = GOBO_QUAD_DEBUG_DISTANCE_M
	var quad_radius: float = tan(cone_half_angle_rad) * quad_distance
	var quad_size: float = clamp(quad_radius * 2.0, GOBO_QUAD_MIN_SIZE_M, GOBO_QUAD_MAX_SIZE_M)
	var quad_mesh: QuadMesh = quad.mesh as QuadMesh
	if quad_mesh != null:
		quad_mesh.size = Vector2(quad_size, quad_size)
	quad.position = Vector3(0.0, 0.0, -quad_distance)

func _ensure_gobo_alpha_quad(light: SpotLight3D) -> MeshInstance3D:
	if light.has_meta(GOBO_QUAD_META_KEY):
		var existing_quad: MeshInstance3D = light.get_meta(GOBO_QUAD_META_KEY) as MeshInstance3D
		if existing_quad != null and is_instance_valid(existing_quad):
			return existing_quad

	var quad_mesh := QuadMesh.new()
	quad_mesh.size = Vector2(0.06, 0.06)
	var quad := MeshInstance3D.new()
	quad.name = "PeravizGoboAlphaQuad"
	quad.mesh = quad_mesh
	# Debug mode: keep the gobo quad visible while validating alignment and shadow response.
	quad.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	quad.visible = false
	light.add_child(quad)
	light.set_meta(GOBO_QUAD_META_KEY, quad)
	return quad

func _ensure_gobo_alpha_quad_material(light: SpotLight3D) -> ShaderMaterial:
	if light.has_meta(GOBO_QUAD_MATERIAL_META_KEY):
		var existing_material: ShaderMaterial = light.get_meta(GOBO_QUAD_MATERIAL_META_KEY) as ShaderMaterial
		if existing_material != null and is_instance_valid(existing_material):
			return existing_material

	var material := ShaderMaterial.new()
	material.shader = GOBO_ALPHA_QUAD_SHADER
	light.set_meta(GOBO_QUAD_MATERIAL_META_KEY, material)
	return material

func _clear_gobo_alpha_quad(light: SpotLight3D) -> void:
	if light.has_meta(GOBO_QUAD_META_KEY):
		var quad: MeshInstance3D = light.get_meta(GOBO_QUAD_META_KEY) as MeshInstance3D
		if quad != null and is_instance_valid(quad):
			quad.visible = false


func _resolve_texture_fit(gobo_texture: Texture2D) -> Dictionary:
	if gobo_texture == null:
		return {
			"uv_offset": GOBO_UV_OFFSET_DEFAULT,
			"uv_scale": GOBO_UV_SCALE_DEFAULT,
		}

	var texture_size: Vector2 = gobo_texture.get_size()
	if texture_size.x <= 0.0 or texture_size.y <= 0.0:
		return {
			"uv_offset": GOBO_UV_OFFSET_DEFAULT,
			"uv_scale": GOBO_UV_SCALE_DEFAULT,
		}

	var aspect_ratio: float = texture_size.x / texture_size.y
	if is_equal_approx(aspect_ratio, 1.0):
		return {
			"uv_offset": GOBO_UV_OFFSET_DEFAULT,
			"uv_scale": GOBO_UV_SCALE_DEFAULT,
		}

	if aspect_ratio > 1.0:
		var y_scale: float = clamp(1.0 / aspect_ratio, 0.001, 1.0)
		return {
			"uv_offset": Vector2(0.0, (1.0 - y_scale) * 0.5),
			"uv_scale": Vector2(1.0, y_scale),
		}

	var x_scale: float = clamp(aspect_ratio, 0.001, 1.0)
	return {
		"uv_offset": Vector2((1.0 - x_scale) * 0.5, 0.0),
		"uv_scale": Vector2(x_scale, 1.0),
	}
