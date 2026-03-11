extends RefCounted
class_name FixtureGoboProjector

const FAKE_GOBO_TEXTURE_SIZE: int = 1024
const GOBO_TEXTURE_META_KEY: String = "peraviz_gobo_texture"
const FALLBACK_GOBO_META_KEY: String = "peraviz_is_vector_fallback_gobo"
const GOBO_PLANE_META_KEY: String = "peraviz_gobo_plane"
const GOBO_SHADER_PATH: String = "res://scripts/shaders/gobo_alpha_projector.gdshader"
const GOBO_PLANE_LOCAL_Z: float = -0.043
const GOBO_PLANE_MESH_SIZE: Vector2 = Vector2(0.017, 0.017)
const GOBO_MIN_ANGLE_DEG: float = 4.0
const GOBO_MAX_ANGLE_DEG: float = 50.0
const GOBO_MIN_SCALE: float = 0.555
const GOBO_MAX_SCALE: float = 6.4
const GOBO_APERTURE_SCALE: float = 1.05
const GOBO_DEFAULT_SCALE: float = 1.0
const GOBO_DEFAULT_ROTATION_DEG: float = 0.0
const VECTOR_FALLBACK_GOBO_CACHE_KEY: String = "__vector_fallback_gobo"
const GOBO_VECTOR_POLYGONS_META_KEY: String = "peraviz_gobo_vector_polygons"
const GOBO_VECTOR_WIDTH_META_KEY: String = "peraviz_gobo_vector_width"
const GOBO_VECTOR_HEIGHT_META_KEY: String = "peraviz_gobo_vector_height"
const GOBO_WHEEL_SPIN_META_KEY: String = "peraviz_gobo_wheel_spin"
const GOBO_LAST_UPDATE_MSEC_META_KEY: String = "peraviz_gobo_last_update_msec"
const GOBO_SPIN_MAX_DEG_PER_SEC: float = 720.0

var _texture_cache: Dictionary = {}

func clear_cache() -> void:
	_texture_cache.clear()

func apply_gobo_projection(light: SpotLight3D, controls: Dictionary) -> bool:
	if light == null or not is_instance_valid(light):
		return false
	var previous_meta_texture: Texture2D = null
	if light.has_meta(GOBO_TEXTURE_META_KEY):
		previous_meta_texture = light.get_meta(GOBO_TEXTURE_META_KEY) as Texture2D
	light.set_meta(FALLBACK_GOBO_META_KEY, false)
	if not bool(controls.get("has_gobo", false)):
		return _apply_vector_fallback_gobo(light, previous_meta_texture, controls)

	var runtime_bindings: Array = controls.get("gobo_runtime_bindings", [])
	if runtime_bindings.is_empty():
		var fallback_raw_8bit: int = _resolve_gobo_raw_8bit(controls)
		runtime_bindings = [{
			"raw_8bit": fallback_raw_8bit,
			"slot_index": _resolve_active_gobo_slot_index(controls, fallback_raw_8bit),
			"slots": controls.get("gobo_slots", []),
		}]

	var delta_sec: float = _resolve_elapsed_seconds(light, controls)
	var transformed_textures: Array[Texture2D] = []
	var gobo_scale: float = max(float(controls.get("gobo_scale", GOBO_DEFAULT_SCALE)), 0.05)
	var global_rotation_deg: float = float(controls.get("gobo_rotation_deg", GOBO_DEFAULT_ROTATION_DEG))

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
			continue
		var wheel_rotation_deg: float = _resolve_wheel_rotation_deg(light, controls, wheel, global_rotation_deg, delta_sec)
		transformed_textures.append(_transform_gobo_texture(gobo_texture, wheel_rotation_deg, gobo_scale))

	if transformed_textures.is_empty():
		return _apply_vector_fallback_gobo(light, previous_meta_texture, controls)

	var projected_gobo: Texture2D = _compose_gobo_textures(transformed_textures)
	_apply_gobo_visuals(light, projected_gobo, controls)
	light.set_meta(GOBO_TEXTURE_META_KEY, projected_gobo)
	light.set_meta(FALLBACK_GOBO_META_KEY, false)
	return projected_gobo != previous_meta_texture

func _resolve_elapsed_seconds(light: SpotLight3D, controls: Dictionary) -> float:
	if controls.has("frame_delta_sec"):
		return clamp(float(controls.get("frame_delta_sec", 0.0)), 0.0, 0.2)
	var now_msec: int = Time.get_ticks_msec()
	var delta_sec: float = 0.0
	if light.has_meta(GOBO_LAST_UPDATE_MSEC_META_KEY):
		var previous_msec: int = int(light.get_meta(GOBO_LAST_UPDATE_MSEC_META_KEY))
		if previous_msec > 0:
			delta_sec = max(float(now_msec - previous_msec) * 0.001, 0.0)
	light.set_meta(GOBO_LAST_UPDATE_MSEC_META_KEY, now_msec)
	return clamp(delta_sec, 0.0, 0.2)

func _resolve_wheel_rotation_deg(light: SpotLight3D, controls: Dictionary, wheel: Dictionary, global_rotation_deg: float, delta_sec: float) -> float:
	var wheel_key: String = _resolve_wheel_cache_key(wheel)
	var wheel_spin_state: Dictionary = light.get_meta(GOBO_WHEEL_SPIN_META_KEY, {})
	if wheel_spin_state is not Dictionary:
		wheel_spin_state = {}

	var index_norm: float = float(wheel.get("index_norm", -1.0))
	if index_norm < 0.0 and bool(controls.get("has_gobo_index", false)):
		index_norm = clamp(float(controls.get("gobo_index_norm", 0.0)), 0.0, 1.0)
	var base_rotation_deg: float = global_rotation_deg
	if index_norm >= 0.0:
		base_rotation_deg = lerp(0.0, 360.0, clamp(index_norm, 0.0, 1.0))

	var rotation_norm: float = float(wheel.get("rotation_norm", -1.0))
	if rotation_norm < 0.0 and bool(controls.get("has_gobo_rotation", false)):
		rotation_norm = clamp(float(controls.get("gobo_rotation_norm", 0.5)), 0.0, 1.0)

	var spin_angle_deg: float = float(wheel_spin_state.get(wheel_key, 0.0))
	if rotation_norm >= 0.0 and delta_sec > 0.0:
		var speed_deg_per_sec: float = (clamp(rotation_norm, 0.0, 1.0) - 0.5) * GOBO_SPIN_MAX_DEG_PER_SEC
		spin_angle_deg = wrapf(spin_angle_deg + (speed_deg_per_sec * delta_sec), -360000.0, 360000.0)
		wheel_spin_state[wheel_key] = spin_angle_deg
		light.set_meta(GOBO_WHEEL_SPIN_META_KEY, wheel_spin_state)

	return base_rotation_deg + spin_angle_deg

func _resolve_wheel_cache_key(wheel: Dictionary) -> String:
	var wheel_number: int = int(wheel.get("wheel_number", 0))
	if wheel_number > 0:
		return "wheel_%d" % wheel_number
	var wheel_name: String = str(wheel.get("wheel_name", ""))
	if not wheel_name.is_empty():
		return "name_%s" % wheel_name.to_lower()
	return "default"

func _apply_gobo_visuals(light: SpotLight3D, gobo_texture: Texture2D, controls: Dictionary = {}) -> void:
	if light == null or not is_instance_valid(light):
		return
	_set_light_projector_texture(light, gobo_texture)
	if gobo_texture == null:
		_remove_gobo_plane(light)
		return
	var prefer_native_fog_projector: bool = bool(controls.get("prefer_native_fog_projector", true))
	if prefer_native_fog_projector:
		_remove_gobo_plane(light)
		return
	var gobo_plane: MeshInstance3D = _ensure_gobo_plane(light)
	if gobo_plane == null:
		return
	if gobo_plane.material_override is ShaderMaterial:
		var gobo_material: ShaderMaterial = gobo_plane.material_override as ShaderMaterial
		gobo_material.set_shader_parameter("gobo_texture", gobo_texture)
	_update_gobo_plane_scale(light, gobo_plane)

func _set_light_projector_texture(light: SpotLight3D, texture: Texture2D) -> void:
	if light == null or not is_instance_valid(light):
		return
	# Godot custom branches may expose either `projector` or `light_projector`.
	if _has_property(light, "projector"):
		light.set("projector", texture)
	if _has_property(light, "light_projector"):
		light.set("light_projector", texture)

func _has_property(object: Object, property_name: String) -> bool:
	if object == null:
		return false
	for property_info in object.get_property_list():
		if str(property_info.get("name", "")) == property_name:
			return true
	return false

func _clear_gobo_visuals(light: SpotLight3D) -> void:
	if light == null or not is_instance_valid(light):
		return
	_set_light_projector_texture(light, null)
	_remove_gobo_plane(light)

func _ensure_gobo_plane(light: SpotLight3D) -> MeshInstance3D:
	if light.has_meta(GOBO_PLANE_META_KEY):
		var existing_plane: MeshInstance3D = light.get_meta(GOBO_PLANE_META_KEY) as MeshInstance3D
		if existing_plane != null and is_instance_valid(existing_plane):
			return existing_plane

	var gobo_plane := MeshInstance3D.new()
	gobo_plane.name = "PeravizGoboPlane"
	gobo_plane.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_DOUBLE_SIDED
	var quad := QuadMesh.new()
	quad.size = GOBO_PLANE_MESH_SIZE
	gobo_plane.mesh = quad
	var gobo_material := ShaderMaterial.new()
	gobo_material.shader = load(GOBO_SHADER_PATH)
	gobo_plane.material_override = gobo_material
	gobo_plane.position = Vector3(0.0, 0.0, GOBO_PLANE_LOCAL_Z)
	light.add_child(gobo_plane)
	light.set_meta(GOBO_PLANE_META_KEY, gobo_plane)
	return gobo_plane

func _remove_gobo_plane(light: SpotLight3D) -> void:
	if not light.has_meta(GOBO_PLANE_META_KEY):
		return
	var gobo_plane: MeshInstance3D = light.get_meta(GOBO_PLANE_META_KEY) as MeshInstance3D
	if gobo_plane != null and is_instance_valid(gobo_plane):
		gobo_plane.queue_free()
	light.remove_meta(GOBO_PLANE_META_KEY)

func _update_gobo_plane_scale(light: SpotLight3D, gobo_plane: MeshInstance3D) -> void:
	if light == null or gobo_plane == null:
		return
	var half_spot_angle_deg: float = clamp(light.spot_angle, GOBO_MIN_ANGLE_DEG * 0.5, GOBO_MAX_ANGLE_DEG * 0.5)
	var local_distance: float = abs(GOBO_PLANE_LOCAL_Z)
	var target_plane_size: float = 2.0 * local_distance * tan(deg_to_rad(half_spot_angle_deg)) * GOBO_APERTURE_SCALE
	var base_size: float = max(GOBO_PLANE_MESH_SIZE.x, 0.0001)
	var physical_scale: float = target_plane_size / base_size
	var clamped_scale: float = clamp(physical_scale, GOBO_MIN_SCALE, GOBO_MAX_SCALE)
	gobo_plane.scale = Vector3.ONE * clamped_scale

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
		if image.get_format() != Image.FORMAT_RGBA8:
			image.convert(Image.FORMAT_RGBA8)
		var texture: ImageTexture = ImageTexture.create_from_image(image)
		_apply_vector_metadata_from_slot(texture, item)
		_texture_cache[image_path] = texture
		return texture
	return null



func _apply_vector_fallback_gobo(light: SpotLight3D, previous_meta_texture: Texture2D, controls: Dictionary) -> bool:
	var fallback_texture: Texture2D = _resolve_vector_fallback_gobo_texture()
	if fallback_texture == null:
		_clear_gobo_visuals(light)
		light.set_meta(GOBO_TEXTURE_META_KEY, null)
		return previous_meta_texture != null
	_apply_gobo_visuals(light, fallback_texture, controls)
	light.set_meta(GOBO_TEXTURE_META_KEY, fallback_texture)
	return fallback_texture != previous_meta_texture

func _resolve_vector_fallback_gobo_texture() -> Texture2D:
	if _texture_cache.has(VECTOR_FALLBACK_GOBO_CACHE_KEY):
		return _texture_cache[VECTOR_FALLBACK_GOBO_CACHE_KEY] as Texture2D
	var image := Image.create(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE, false, Image.FORMAT_RGBA8)
	image.fill(Color(0.0, 0.0, 0.0, 1.0))
	var center: Vector2 = Vector2(float(FAKE_GOBO_TEXTURE_SIZE - 1), float(FAKE_GOBO_TEXTURE_SIZE - 1)) * 0.5
	var outer_radius: float = float(FAKE_GOBO_TEXTURE_SIZE) * 0.48
	for y in range(FAKE_GOBO_TEXTURE_SIZE):
		for x in range(FAKE_GOBO_TEXTURE_SIZE):
			var distance_to_center: float = Vector2(float(x), float(y)).distance_to(center)
			if distance_to_center <= outer_radius:
				image.set_pixel(x, y, Color(1.0, 1.0, 1.0, 1.0))
	var texture: ImageTexture = ImageTexture.create_from_image(image)
	_texture_cache[VECTOR_FALLBACK_GOBO_CACHE_KEY] = texture
	return texture

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
				var src_mask: float = src_luma * src.a
				var out_luma: float = dst.r * src_mask
				composed.set_pixel(x, y, Color(out_luma, out_luma, out_luma, out_luma))

	var out_texture: ImageTexture = ImageTexture.create_from_image(composed)
	if textures.size() == 1:
		_copy_vector_metadata(out_texture, textures[0])
	_texture_cache[cache_key] = out_texture
	return out_texture


func _transform_gobo_texture(base_texture: Texture2D, rotation_deg: float, scale_factor: float) -> Texture2D:
	if base_texture == null:
		return null
	var normalized_rotation: float = wrapf(rotation_deg, 0.0, 360.0)
	var clamped_scale: float = clamp(scale_factor, 0.05, 8.0)
	if abs(normalized_rotation) < 0.001 and is_equal_approx(clamped_scale, 1.0):
		return base_texture
	var cache_key: String = "__transformed_gobo_%d_%.3f_%.3f" % [base_texture.get_rid().get_id(), normalized_rotation, clamped_scale]
	if _texture_cache.has(cache_key):
		return _texture_cache[cache_key] as Texture2D
	var src_image: Image = base_texture.get_image()
	if src_image == null:
		return base_texture
	if src_image.get_format() != Image.FORMAT_RGBA8:
		src_image.convert(Image.FORMAT_RGBA8)
	if src_image.get_width() != FAKE_GOBO_TEXTURE_SIZE or src_image.get_height() != FAKE_GOBO_TEXTURE_SIZE:
		src_image.resize(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE, Image.INTERPOLATE_LANCZOS)
	var out_image: Image = Image.create(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE, false, Image.FORMAT_RGBA8)
	out_image.fill(Color(0.0, 0.0, 0.0, 0.0))
	var center: Vector2 = Vector2(float(FAKE_GOBO_TEXTURE_SIZE - 1), float(FAKE_GOBO_TEXTURE_SIZE - 1)) * 0.5
	var inv_scale: float = 1.0 / clamped_scale
	var rotation_rad: float = deg_to_rad(normalized_rotation)
	var cos_r: float = cos(rotation_rad)
	var sin_r: float = sin(rotation_rad)
	for y in range(FAKE_GOBO_TEXTURE_SIZE):
		for x in range(FAKE_GOBO_TEXTURE_SIZE):
			var dst: Vector2 = Vector2(float(x), float(y)) - center
			var scaled: Vector2 = dst * inv_scale
			var src_local := Vector2(
				(cos_r * scaled.x) + (sin_r * scaled.y),
				(-sin_r * scaled.x) + (cos_r * scaled.y)
			)
			var src_pos: Vector2 = src_local + center
			if src_pos.x < 0.0 or src_pos.y < 0.0 or src_pos.x >= float(FAKE_GOBO_TEXTURE_SIZE) or src_pos.y >= float(FAKE_GOBO_TEXTURE_SIZE):
				continue
			out_image.set_pixel(x, y, src_image.get_pixel(int(src_pos.x), int(src_pos.y)))
	var texture: ImageTexture = ImageTexture.create_from_image(out_image)
	_copy_vector_metadata(texture, base_texture)
	_texture_cache[cache_key] = texture
	return texture

func _apply_vector_metadata_from_slot(texture: Texture2D, slot: Dictionary) -> void:
	if texture == null:
		return
	if slot.has("vector_polygons"):
		texture.set_meta(GOBO_VECTOR_POLYGONS_META_KEY, slot.get("vector_polygons", []))
	if slot.has("vector_width"):
		texture.set_meta(GOBO_VECTOR_WIDTH_META_KEY, int(slot.get("vector_width", 0)))
	if slot.has("vector_height"):
		texture.set_meta(GOBO_VECTOR_HEIGHT_META_KEY, int(slot.get("vector_height", 0)))

func _copy_vector_metadata(target_texture: Texture2D, source_texture: Texture2D) -> void:
	if target_texture == null or source_texture == null:
		return
	if source_texture.has_meta(GOBO_VECTOR_POLYGONS_META_KEY):
		target_texture.set_meta(GOBO_VECTOR_POLYGONS_META_KEY, source_texture.get_meta(GOBO_VECTOR_POLYGONS_META_KEY))
	if source_texture.has_meta(GOBO_VECTOR_WIDTH_META_KEY):
		target_texture.set_meta(GOBO_VECTOR_WIDTH_META_KEY, source_texture.get_meta(GOBO_VECTOR_WIDTH_META_KEY))
	if source_texture.has_meta(GOBO_VECTOR_HEIGHT_META_KEY):
		target_texture.set_meta(GOBO_VECTOR_HEIGHT_META_KEY, source_texture.get_meta(GOBO_VECTOR_HEIGHT_META_KEY))

