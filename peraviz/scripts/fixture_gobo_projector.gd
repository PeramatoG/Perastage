extends RefCounted
class_name FixtureGoboProjector

const FAKE_GOBO_TEXTURE_SIZE: int = 128

var _texture_cache: Dictionary = {}

func clear_cache() -> void:
	_texture_cache.clear()

func apply_gobo_projection(light: SpotLight3D, controls: Dictionary) -> void:
	if light == null or not is_instance_valid(light):
		return
	if not bool(controls.get("has_gobo", false)):
		light.light_projector = null
		light.shadow_enabled = false
		return

	# Godot requires shadow_enabled for light_projector to be applied.
	light.shadow_enabled = true
	_warn_if_projector_unsupported(light)

	var gobo_raw_8bit: int = _resolve_gobo_raw_8bit(controls)
	var active_slot_index: int = _resolve_active_gobo_slot_index(controls, gobo_raw_8bit)
	if active_slot_index <= 0:
		light.light_projector = _resolve_fake_gobo_texture(gobo_raw_8bit)
		return

	var gobo_texture: Texture2D = _resolve_gobo_texture_for_slot(controls, active_slot_index)
	if gobo_texture == null:
		light.light_projector = _resolve_fake_gobo_texture(gobo_raw_8bit)
		return
	light.light_projector = gobo_texture

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

func _resolve_fake_gobo_texture(gobo_raw_8bit: int) -> Texture2D:
	var fake_bucket: int = gobo_raw_8bit / 8
	var cache_key: String = "__fake_gobo_%d" % fake_bucket
	if _texture_cache.has(cache_key):
		return _texture_cache[cache_key] as Texture2D

	var image := Image.create(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE, false, Image.FORMAT_RGBA8)
	# Keep base fully open (white) so spot footprint is still visible while debugging.
	image.fill(Color(1.0, 1.0, 1.0, 1.0))

	# Bucket 0 emulates open gobo.
	if fake_bucket > 0:
		var stripe_step: int = max(6, int(6 + (fake_bucket % 12) * 2))
		var radius: float = float(FAKE_GOBO_TEXTURE_SIZE) * (0.20 + 0.012 * float(fake_bucket % 10))
		var center: Vector2 = Vector2(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE) * 0.5
		for y in range(FAKE_GOBO_TEXTURE_SIZE):
			for x in range(FAKE_GOBO_TEXTURE_SIZE):
				var uv: Vector2 = Vector2(float(x), float(y)) - center
				var dist: float = uv.length()
				var stripe: bool = ((x + y + fake_bucket) % stripe_step) < (stripe_step / 2)
				if dist < radius and stripe:
					image.set_pixel(x, y, Color(0.0, 0.0, 0.0, 1.0))

	var texture: ImageTexture = ImageTexture.create_from_image(image)
	_texture_cache[cache_key] = texture
	return texture

func _warn_if_projector_unsupported(light: SpotLight3D) -> void:
	if light.has_meta("peraviz_projector_support_checked"):
		return
	light.set_meta("peraviz_projector_support_checked", true)
	if RenderingServer.has_method("get_current_rendering_method"):
		var rendering_method: String = str(RenderingServer.get_current_rendering_method())
		if rendering_method == "gl_compatibility":
			push_warning("Peraviz gobo projector is not supported in Compatibility renderer. Use Forward+ or Mobile.")
