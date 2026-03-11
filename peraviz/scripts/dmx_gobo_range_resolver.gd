extends RefCounted
class_name DmxGoboRangeResolver

const GOBO_BEHAVIOR_FIXED: int = 0

static func resolve_active_range(raw_8bit: int, ranges: Array) -> Dictionary:
	var active_match: Dictionary = {}
	var has_match: bool = false

	for index in range(ranges.size()):
		var item: Variant = ranges[index]
		if item is not Dictionary:
			continue
		var range_item: Dictionary = item as Dictionary
		var dmx_from: int = int(range_item.get("dmx_from", 0))
		var dmx_to: int = int(range_item.get("dmx_to", dmx_from))
		if dmx_to < dmx_from:
			var swap_value: int = dmx_from
			dmx_from = dmx_to
			dmx_to = swap_value
		if raw_8bit < dmx_from or raw_8bit > dmx_to:
			continue

		# Preserve fixture-authored ChannelSet precedence: when multiple rows match,
		# keep the latest matching row in declaration order.
		active_match = {
			"slot_index": int(range_item.get("slot_index", 0)),
			"behavior": int(range_item.get("behavior", GOBO_BEHAVIOR_FIXED)),
			"dmx_from": dmx_from,
			"dmx_to": dmx_to,
		}
		has_match = true

	if not has_match:
		return {
			"slot_index": 0,
			"behavior": GOBO_BEHAVIOR_FIXED,
		}
	return active_match
