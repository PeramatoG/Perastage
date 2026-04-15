extends RefCounted
class_name ColorWheelCapabilityHandler

static func collect(binding: Dictionary, frame: PackedByteArray, control_reader, force_coarse_only: bool) -> Array:
	var block := {
		"type": "color_wheel",
		"has_cyan": false,
		"cyan_norm": 0.0,
		"has_magenta": false,
		"magenta_norm": 0.0,
		"has_yellow": false,
		"yellow_norm": 0.0,
	}
	_apply_channel(block, binding, frame, control_reader, force_coarse_only,
		"cyan_channel_index_0", "cyan_fine_channel_index_0", "cyan_ultra_fine_channel_index_0", "cyan")
	_apply_channel(block, binding, frame, control_reader, force_coarse_only,
		"magenta_channel_index_0", "magenta_fine_channel_index_0", "magenta_ultra_fine_channel_index_0", "magenta")
	_apply_channel(block, binding, frame, control_reader, force_coarse_only,
		"yellow_channel_index_0", "yellow_fine_channel_index_0", "yellow_ultra_fine_channel_index_0", "yellow")
	if not block["has_cyan"] and not block["has_magenta"] and not block["has_yellow"]:
		return []
	return [block]

static func _apply_channel(block: Dictionary,
		binding: Dictionary,
		frame: PackedByteArray,
		control_reader,
		force_coarse_only: bool,
		coarse_key: String,
		fine_key: String,
		ultra_fine_key: String,
		prefix: String) -> void:
	var value: Dictionary = control_reader.read_channel(binding, frame, coarse_key, fine_key, ultra_fine_key, force_coarse_only)
	if not bool(value.get("has_value", false)):
		return
	block["has_%s" % prefix] = true
	block["%s_norm" % prefix] = float(value.get("norm", 0.0))
	block["%s_raw_value" % prefix] = int(value.get("raw", 0))
	block["%s_resolution_bits" % prefix] = int(value.get("resolution_bits", 8))
	block["%s_bytes" % prefix] = value.get("bytes", PackedInt32Array())
