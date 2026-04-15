extends RefCounted
class_name StrobeCapabilityHandler

static func collect(binding: Dictionary, frame: PackedByteArray, control_reader, force_coarse_only: bool) -> Array:
	var block := {
		"type": "strobe",
		"has_strobe": false,
		"strobe_norm": 0.0,
	}
	var value: Dictionary = control_reader.read_channel(
		binding,
		frame,
		"strobe_channel_index_0",
		"strobe_fine_channel_index_0",
		"strobe_ultra_fine_channel_index_0",
		force_coarse_only
	)
	if not bool(value.get("has_value", false)):
		return []
	block["has_strobe"] = true
	block["strobe_norm"] = float(value.get("norm", 0.0))
	block["strobe_raw_value"] = int(value.get("raw", 0))
	block["strobe_resolution_bits"] = int(value.get("resolution_bits", 8))
	block["strobe_bytes"] = value.get("bytes", PackedInt32Array())
	return [block]
