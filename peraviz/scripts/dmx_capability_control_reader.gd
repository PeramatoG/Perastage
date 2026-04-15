extends RefCounted
class_name DmxCapabilityControlReader

const DMX_8BIT_MAX_VALUE: float = 255.0
const DMX_8BIT_STEPS: int = 256
const DMX_16BIT_STEPS: int = 65536
const DMX_24BIT_STEPS: int = 16777216

static func read_channel(binding: Dictionary,
		frame: PackedByteArray,
		coarse_key: String,
		fine_key: String,
		ultra_fine_key: String,
		force_coarse_only: bool = false) -> Dictionary:
	var coarse_index: int = int(binding.get(coarse_key, -1))
	var fine_index: int = int(binding.get(fine_key, -1))
	var ultra_fine_index: int = int(binding.get(ultra_fine_key, -1))
	return read_indices(frame, coarse_index, fine_index, ultra_fine_index, force_coarse_only)

static func read_indices(frame: PackedByteArray,
		coarse_index: int,
		fine_index: int,
		ultra_fine_index: int,
		force_coarse_only: bool = false) -> Dictionary:
	var resolved_indices: Dictionary = resolve_control_indices(frame, coarse_index, fine_index, ultra_fine_index)
	if not bool(resolved_indices.get("has_value", false)):
		return {"has_value": false}
	var value: Dictionary = read_control_value(
		frame,
		int(resolved_indices.get("coarse", -1)),
		int(resolved_indices.get("fine", -1)),
		int(resolved_indices.get("ultra_fine", -1)),
		force_coarse_only
	)
	value["has_value"] = true
	return value

static func has_control_channel(binding: Dictionary, coarse_key: String, fine_key: String, ultra_fine_key: String) -> bool:
	return int(binding.get(coarse_key, -1)) >= 0 or int(binding.get(fine_key, -1)) >= 0 or int(binding.get(ultra_fine_key, -1)) >= 0

static func read_control_value(frame: PackedByteArray,
		coarse_index: int,
		fine_index: int,
		ultra_fine_index: int,
		force_coarse_only: bool = false) -> Dictionary:
	var coarse: int = int(frame[coarse_index])

	if force_coarse_only:
		return {
			"raw": coarse,
			"norm": float(coarse) / DMX_8BIT_MAX_VALUE,
			"resolution_bits": 8,
			"bytes": PackedInt32Array([coarse]),
		}

	if is_valid_channel_index(frame, fine_index) and is_valid_channel_index(frame, ultra_fine_index):
		var fine: int = int(frame[fine_index])
		var ultra_fine: int = int(frame[ultra_fine_index])
		var raw_value_24: int = resolve_24bit_raw_value(coarse, fine, ultra_fine)
		return {
			"raw": raw_value_24,
			"norm": float(raw_value_24) / float(DMX_24BIT_STEPS - 1),
			"resolution_bits": 24,
			"bytes": PackedInt32Array([coarse, fine, ultra_fine]),
		}

	if is_valid_channel_index(frame, fine_index):
		var fine: int = int(frame[fine_index])
		var raw_value_16: int = resolve_16bit_raw_value(coarse, fine)
		return {
			"raw": raw_value_16,
			"norm": float(raw_value_16) / float(DMX_16BIT_STEPS - 1),
			"resolution_bits": 16,
			"bytes": PackedInt32Array([coarse, fine]),
		}

	return {
		"raw": coarse,
		"norm": float(coarse) / DMX_8BIT_MAX_VALUE,
		"resolution_bits": 8,
		"bytes": PackedInt32Array([coarse]),
	}

static func read_optional_control_value(frame: PackedByteArray, coarse_index: int, fine_index: int, ultra_fine_index: int, force_coarse_only: bool = false) -> Dictionary:
	var value: Dictionary = read_indices(frame, coarse_index, fine_index, ultra_fine_index, force_coarse_only)
	if not bool(value.get("has_value", false)):
		return {}
	return value

static func resolve_control_indices(frame: PackedByteArray, coarse_index: int, fine_index: int, ultra_fine_index: int) -> Dictionary:
	if is_valid_channel_index(frame, coarse_index):
		return {
			"has_value": true,
			"coarse": coarse_index,
			"fine": fine_index,
			"ultra_fine": ultra_fine_index,
		}

	if is_valid_channel_index(frame, fine_index):
		return {
			"has_value": true,
			"coarse": fine_index,
			"fine": ultra_fine_index,
			"ultra_fine": -1,
		}

	if is_valid_channel_index(frame, ultra_fine_index):
		return {
			"has_value": true,
			"coarse": ultra_fine_index,
			"fine": -1,
			"ultra_fine": -1,
		}

	return {"has_value": false}

static func is_valid_channel_index(frame: PackedByteArray, channel_index_0: int) -> bool:
	return channel_index_0 >= 0 and channel_index_0 < frame.size()

static func resolve_16bit_raw_value(coarse: int, fine: int) -> int:
	var safe_coarse: int = clampi(coarse, 0, int(DMX_8BIT_MAX_VALUE))
	var safe_fine: int = clampi(fine, 0, int(DMX_8BIT_MAX_VALUE))
	return (safe_coarse * DMX_8BIT_STEPS) + safe_fine

static func resolve_24bit_raw_value(coarse: int, fine: int, ultra_fine: int) -> int:
	var safe_coarse: int = clampi(coarse, 0, int(DMX_8BIT_MAX_VALUE))
	var safe_fine: int = clampi(fine, 0, int(DMX_8BIT_MAX_VALUE))
	var safe_ultra_fine: int = clampi(ultra_fine, 0, int(DMX_8BIT_MAX_VALUE))
	return (safe_coarse * DMX_16BIT_STEPS) + (safe_fine * DMX_8BIT_STEPS) + safe_ultra_fine
