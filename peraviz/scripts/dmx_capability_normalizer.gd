extends RefCounted
class_name DmxCapabilityNormalizer

static func clamp_norm(value: float) -> float:
	return clamp(value, 0.0, 1.0)

static func raw_to_8bit(raw_value: int, resolution_bits: int) -> int:
	if resolution_bits >= 32:
		return clampi(raw_value >> 24, 0, 255)
	if resolution_bits >= 24:
		return clampi(raw_value >> 16, 0, 255)
	if resolution_bits >= 16:
		return clampi(raw_value >> 8, 0, 255)
	return clampi(raw_value, 0, 255)

static func norm_from_active_range(raw_8bit: int, active_range: Dictionary) -> float:
	var dmx_from: int = int(active_range.get("dmx_from", 0))
	var dmx_to: int = int(active_range.get("dmx_to", dmx_from))
	if dmx_to < dmx_from:
		var swap_value: int = dmx_from
		dmx_from = dmx_to
		dmx_to = swap_value
	if dmx_to <= dmx_from:
		return 0.0
	var clamped_raw: int = clampi(raw_8bit, dmx_from, dmx_to)
	return float(clamped_raw - dmx_from) / float(dmx_to - dmx_from)
