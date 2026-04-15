extends RefCounted
class_name DimmerCapabilityHandler

static func collect(binding: Dictionary, frame: PackedByteArray, control_reader, force_coarse_only: bool) -> Array:
	var block := {
		"type": "dimmer",
		"has_dimmer": false,
		"dimmer_norm": 0.0,
		"has_zoom": false,
		"zoom_norm": 0.0,
	}
	_apply_channel(block, binding, frame, control_reader, force_coarse_only,
		"dimmer_channel_index_0", "dimmer_fine_channel_index_0", "dimmer_ultra_fine_channel_index_0", "dimmer")
	_apply_channel(block, binding, frame, control_reader, force_coarse_only,
		"zoom_channel_index_0", "zoom_fine_channel_index_0", "zoom_ultra_fine_channel_index_0", "zoom")
	if block["has_zoom"]:
		block["has_zoom_physical_limits"] = bool(binding.get("has_zoom_physical_limits", false))
		block["zoom_physical_min_degrees"] = float(binding.get("zoom_physical_min_degrees", -1.0))
		block["zoom_physical_max_degrees"] = float(binding.get("zoom_physical_max_degrees", -1.0))
	if not block["has_dimmer"] and not block["has_zoom"]:
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
