extends RefCounted
class_name PrismCapabilityHandler

static func collect(binding: Dictionary, frame: PackedByteArray, control_reader, force_coarse_only: bool) -> Array:
	var block := {
		"type": "prism",
		"has_prism": false,
		"prism_norm": 0.0,
	}
	var value: Dictionary = control_reader.read_channel(
		binding,
		frame,
		"prism_channel_index_0",
		"prism_fine_channel_index_0",
		"prism_ultra_fine_channel_index_0",
		force_coarse_only
	)
	if not bool(value.get("has_value", false)):
		return []
	block["has_prism"] = true
	block["prism_norm"] = float(value.get("norm", 0.0))
	block["prism_raw_value"] = int(value.get("raw", 0))
	block["prism_resolution_bits"] = int(value.get("resolution_bits", 8))
	block["prism_bytes"] = value.get("bytes", PackedInt32Array())
	return [block]
