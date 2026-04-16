extends RefCounted
class_name DmxGoboRangeResolver

const GOBO_BEHAVIOR_FIXED: int = 0
const GOBO_BEHAVIOR_INDEX: int = 1
const GOBO_BEHAVIOR_ROTATION: int = 2
const GOBO_BEHAVIOR_SHAKE: int = 3

static func _behavior_priority(behavior: int) -> int:
	match behavior:
		GOBO_BEHAVIOR_SHAKE:
			return 3
		GOBO_BEHAVIOR_ROTATION:
			return 2
		GOBO_BEHAVIOR_INDEX:
			return 1
		_:
			return 0

static func resolve_active_range(raw_8bit: int, ranges: Array) -> Dictionary:
	var active_match: Dictionary = {}
	var has_match: bool = false
	var active_priority: int = -1

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

		var behavior: int = int(range_item.get("behavior", GOBO_BEHAVIOR_FIXED))
		var behavior_priority: int = _behavior_priority(behavior)
		if has_match and behavior_priority < active_priority:
			continue

		# Prefer richer behavior rows on overlap (shake > rotation > index > fixed).
		# For equal-priority overlaps preserve fixture-authored ChannelSet precedence
		# by keeping the latest matching row in declaration order.
		active_match = {
			"slot_index": int(range_item.get("slot_index", 0)),
			"behavior": behavior,
			"dmx_from": dmx_from,
			"dmx_to": dmx_to,
		}
		has_match = true
		active_priority = behavior_priority

	if not has_match:
		return {
			"slot_index": 0,
			"behavior": GOBO_BEHAVIOR_FIXED,
		}
	return active_match
