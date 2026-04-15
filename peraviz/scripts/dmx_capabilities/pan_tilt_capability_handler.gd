extends RefCounted
class_name PanTiltCapabilityHandler

static func collect(binding: Dictionary, frame: PackedByteArray, control_reader, force_coarse_only: bool) -> Array:
	var block := {
		"type": "pan_tilt",
		"has_pan": false,
		"pan_norm": 0.0,
		"has_tilt": false,
		"tilt_norm": 0.0,
	}
	_apply_channel(block, binding, frame, control_reader, force_coarse_only,
		"pan_channel_index_0", "pan_fine_channel_index_0", "pan_ultra_fine_channel_index_0", "pan")
	_apply_channel(block, binding, frame, control_reader, force_coarse_only,
		"tilt_channel_index_0", "tilt_fine_channel_index_0", "tilt_ultra_fine_channel_index_0", "tilt")
	if not block["has_pan"] and not block["has_tilt"]:
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
