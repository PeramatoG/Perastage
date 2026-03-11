extends RefCounted
class_name DmxGoboRangeResolver

const GOBO_BEHAVIOR_FIXED: int = 0

static func resolve_active_range(raw_8bit: int, ranges: Array) -> Dictionary:
	var best_match: Dictionary = {}
	var best_index: int = -1
	var best_from: int = -1
	var best_span: int = 1_000_000

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

		var span: int = dmx_to - dmx_from
		var is_better: bool = false
		if best_index < 0:
			is_better = true
		elif dmx_from > best_from:
			is_better = true
		elif dmx_from == best_from and span < best_span:
			is_better = true
		elif dmx_from == best_from and span == best_span and index > best_index:
			is_better = true

		if is_better:
			best_match = {
				"slot_index": int(range_item.get("slot_index", 0)),
				"behavior": int(range_item.get("behavior", GOBO_BEHAVIOR_FIXED)),
			}
			best_index = index
			best_from = dmx_from
			best_span = span

	if best_index < 0:
		return {
			"slot_index": 0,
			"behavior": GOBO_BEHAVIOR_FIXED,
		}
	return best_match

