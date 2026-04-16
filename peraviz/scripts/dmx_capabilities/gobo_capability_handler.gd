extends RefCounted
class_name GoboCapabilityHandler

const GOBO_BEHAVIOR_FIXED: int = 0
const GOBO_BEHAVIOR_INDEX: int = 1
const GOBO_BEHAVIOR_ROTATION: int = 2
const GOBO_BEHAVIOR_SHAKE: int = 3
const GOBO_SHAKE_CONTROL_TYPE_SAME_CHANNEL_SELECT: int = 0
const GOBO_SHAKE_CONTROL_TYPE_DEDICATED_CHANNEL: int = 1
const GOBO_DEFAULT_SHAKE_FALLBACK_AMPLITUDE_MIN_DEG: float = 0.1
const GOBO_DEFAULT_SHAKE_FALLBACK_AMPLITUDE_MAX_DEG: float = 0.8
const GOBO_MIN_ACTIVE_SHAKE_FREQUENCY_HZ: float = 0.05
const GoboShakeLimitsScript = preload("res://scripts/gobo_shake_limits.gd")
const GOBO_MAX_SHAKE_FREQUENCY_HZ: float = GoboShakeLimitsScript.MAX_SHAKE_FREQUENCY_HZ
const GOBO_MAX_SHAKE_AMPLITUDE_DEG: float = GoboShakeLimitsScript.MAX_SHAKE_AMPLITUDE_DEG
const GOBO_ROTATION_DEBUG_SETTING_KEY: String = "peraviz_debug_gobo_rotation"

const DmxGoboRangeResolverScript = preload("res://scripts/dmx_gobo_range_resolver.gd")

static func collect(binding: Dictionary, frame: PackedByteArray, control_reader, normalizer, force_coarse_only: bool) -> Array:
	var block := {
		"type": "gobo",
		"has_gobo": false,
		"gobo_norm": 0.0,
		"has_gobo_index": false,
		"gobo_index_norm": 0.0,
		"has_gobo_rotation": false,
		"gobo_rotation_norm": 0.0,
	}
	_apply_channel(block, binding, frame, control_reader, force_coarse_only,
		"gobo1_channel_index_0", "gobo1_fine_channel_index_0", "gobo1_ultra_fine_channel_index_0", "gobo")
	if not block["has_gobo"]:
		_apply_channel(block, binding, frame, control_reader, force_coarse_only,
			"gobo_channel_index_0", "gobo_fine_channel_index_0", "gobo_ultra_fine_channel_index_0", "gobo")
	_apply_channel(block, binding, frame, control_reader, force_coarse_only,
		"gobo_index_channel_index_0", "gobo_index_fine_channel_index_0", "gobo_index_ultra_fine_channel_index_0", "gobo_index")
	_apply_channel(block, binding, frame, control_reader, force_coarse_only,
		"gobo_rotation_channel_index_0", "gobo_rotation_fine_channel_index_0", "gobo_rotation_ultra_fine_channel_index_0", "gobo_rotation")

	if block["has_gobo"]:
		block["gobo_slots"] = binding.get("gobo1_slots", binding.get("gobo_slots", []))
		block["gobo_ranges"] = binding.get("gobo1_ranges", binding.get("gobo_ranges", []))
		block["gobo_wheel_name"] = str(binding.get("gobo1_wheel_name", binding.get("gobo_wheel_name", "")))
		block["gobo_wheel_number"] = int(binding.get("gobo_wheel_number", 0))

	var runtime_bindings: Array = _build_runtime_gobo_bindings(binding, frame, control_reader, normalizer, force_coarse_only)
	if not runtime_bindings.is_empty():
		block["has_gobo"] = true
		block["gobo_runtime_bindings"] = runtime_bindings

	if not block["has_gobo"] and not block["has_gobo_index"] and not block["has_gobo_rotation"]:
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

static func _build_runtime_gobo_bindings(binding: Dictionary,
		frame: PackedByteArray,
		control_reader,
		normalizer,
		force_coarse_only: bool) -> Array:
	var runtime_bindings: Array = []
	var raw_wheels: Array = binding.get("gobo_wheels", [])
	for item in raw_wheels:
		if item is not Dictionary:
			continue
		var value: Dictionary = control_reader.read_indices(
			frame,
			int(item.get("channel_index_0", -1)),
			int(item.get("fine_channel_index_0", -1)),
			int(item.get("ultra_fine_channel_index_0", -1)),
			force_coarse_only
		)
		if not bool(value.get("has_value", false)):
			continue
		var raw_8bit: int = normalizer.raw_to_8bit(int(value.get("raw", 0)), int(value.get("resolution_bits", 8)))
		var ranges: Array = item.get("ranges", [])
		var active_range: Dictionary = _resolve_gobo_range(raw_8bit, ranges)
		var range_behavior: int = int(active_range.get("behavior", GOBO_BEHAVIOR_FIXED))
		var has_index_channel: bool = control_reader.has_control_channel(item, "index_channel_index_0", "index_fine_channel_index_0", "index_ultra_fine_channel_index_0")
		var has_rotation_channel: bool = control_reader.has_control_channel(item, "rotation_channel_index_0", "rotation_fine_channel_index_0", "rotation_ultra_fine_channel_index_0")
		var is_rotation_behavior: bool = range_behavior == GOBO_BEHAVIOR_ROTATION or range_behavior == GOBO_BEHAVIOR_SHAKE
		var uses_range_rotation: bool = is_rotation_behavior and not has_rotation_channel
		var supports_index: bool = (range_behavior == GOBO_BEHAVIOR_INDEX) or (has_index_channel and not is_rotation_behavior)
		var supports_rotation: bool = is_rotation_behavior or (has_rotation_channel and range_behavior != GOBO_BEHAVIOR_INDEX)
		var shake_ranges: Array = item.get("shake_ranges", [])
		var supports_shake: bool = range_behavior == GOBO_BEHAVIOR_SHAKE or not shake_ranges.is_empty()
		var index_norm: float = -1.0
		var index_raw_8bit: int = -1
		var index_raw_coarse: int = -1
		var index_raw_fine: int = -1
		var rotation_raw: int = raw_8bit
		var rotation_raw_coarse: int = raw_8bit
		var rotation_raw_fine: int = -1
		var rotation_raw_8bit: int = raw_8bit
		var rotation_source_channel: String = "select"
		var rotation_has_value: bool = false
		var rotation_ranges: Array = item.get("rotation_ranges", [])
		var resolved_rotation: Dictionary = {}
		var resolved_shake: Dictionary = {}
		var mode_master_value_8bit: int = raw_8bit
		var rotation_control_norm: float = -1.0
		var shake_source_channel: String = "none"
		var shake_raw_8bit: int = -1
		var shake_raw_coarse: int = -1
		var shake_raw_fine: int = -1

		if has_index_channel and supports_index:
			var index_value: Dictionary = control_reader.read_optional_control_value(
				frame,
				int(item.get("index_channel_index_0", -1)),
				int(item.get("index_fine_channel_index_0", -1)),
				int(item.get("index_ultra_fine_channel_index_0", -1)),
				force_coarse_only
			)
			if not index_value.is_empty():
				index_norm = normalizer.clamp_norm(float(index_value.get("norm", 0.0)))
				index_raw_8bit = normalizer.raw_to_8bit(int(index_value.get("raw", 0)), int(index_value.get("resolution_bits", 8)))
				var index_coarse_index: int = int(item.get("index_channel_index_0", -1))
				var index_fine_index: int = int(item.get("index_fine_channel_index_0", -1))
				index_raw_coarse = int(frame[index_coarse_index]) if control_reader.is_valid_channel_index(frame, index_coarse_index) else index_raw_8bit
				index_raw_fine = int(frame[index_fine_index]) if control_reader.is_valid_channel_index(frame, index_fine_index) else -1

		var rotation_value_available: bool = false
		var rotation_value_norm: float = -1.0
		if has_rotation_channel and (supports_rotation or (supports_index and not has_index_channel)):
			var rotation_coarse_index: int = int(item.get("rotation_channel_index_0", -1))
			var rotation_fine_index: int = int(item.get("rotation_fine_channel_index_0", -1))
			var rotation_value: Dictionary = control_reader.read_optional_control_value(
				frame,
				rotation_coarse_index,
				rotation_fine_index,
				int(item.get("rotation_ultra_fine_channel_index_0", -1)),
				force_coarse_only
			)
			if not rotation_value.is_empty():
				rotation_value_available = true
				rotation_value_norm = normalizer.clamp_norm(float(rotation_value.get("norm", 0.0)))
				rotation_raw_8bit = normalizer.raw_to_8bit(int(rotation_value.get("raw", 0)), int(rotation_value.get("resolution_bits", 8)))
				rotation_raw = rotation_raw_8bit
				rotation_source_channel = "rotation"
				rotation_raw_coarse = int(frame[rotation_coarse_index]) if control_reader.is_valid_channel_index(frame, rotation_coarse_index) else rotation_raw_8bit
				rotation_raw_fine = int(frame[rotation_fine_index]) if control_reader.is_valid_channel_index(frame, rotation_fine_index) else -1
				rotation_control_norm = rotation_value_norm

		if supports_index and index_norm < 0.0 and range_behavior == GOBO_BEHAVIOR_INDEX:
			if not has_index_channel and rotation_value_available:
				index_norm = rotation_value_norm
				index_raw_8bit = rotation_raw_8bit
				index_raw_coarse = rotation_raw_coarse
				index_raw_fine = rotation_raw_fine
			else:
				index_norm = normalizer.norm_from_active_range(raw_8bit, active_range)

		if supports_rotation:
			if has_rotation_channel and rotation_value_available:
				rotation_has_value = true
			elif not has_rotation_channel and has_index_channel and is_rotation_behavior and index_norm >= 0.0:
				rotation_raw_8bit = index_raw_8bit if index_raw_8bit >= 0 else raw_8bit
				rotation_raw = rotation_raw_8bit
				rotation_source_channel = "index"
				rotation_raw_coarse = index_raw_coarse if index_raw_coarse >= 0 else rotation_raw_8bit
				rotation_raw_fine = index_raw_fine
				rotation_has_value = true
				rotation_control_norm = normalizer.clamp_norm(index_norm)
			elif uses_range_rotation:
				rotation_raw = raw_8bit
				rotation_raw_8bit = raw_8bit
				rotation_source_channel = "select"
				rotation_raw_coarse = raw_8bit
				rotation_raw_fine = -1
				rotation_has_value = true
				rotation_control_norm = float(rotation_raw_8bit) / 255.0

		var dedicated_rotation_ranges: Array = []
		var non_dedicated_rotation_ranges: Array = []
		for rotation_range in rotation_ranges:
			if rotation_range is not Dictionary:
				continue
			if bool(rotation_range.get("is_rotation_channel_range", false)):
				dedicated_rotation_ranges.append(rotation_range)
			else:
				non_dedicated_rotation_ranges.append(rotation_range)

		var resolver_ranges: Array = rotation_ranges
		var prefer_rotation_channel_ranges: bool = rotation_source_channel == "rotation"
		if prefer_rotation_channel_ranges and not dedicated_rotation_ranges.is_empty():
			resolver_ranges = dedicated_rotation_ranges
			prefer_rotation_channel_ranges = false
		elif prefer_rotation_channel_ranges and non_dedicated_rotation_ranges.is_empty():
			prefer_rotation_channel_ranges = false

		if rotation_has_value and not resolver_ranges.is_empty():
			resolved_rotation = _resolve_rotation_runtime(
				rotation_raw_8bit,
				rotation_control_norm,
				resolver_ranges,
				mode_master_value_8bit,
				prefer_rotation_channel_ranges
			)
		if supports_shake and not shake_ranges.is_empty():
			var slot_index_for_shake: int = int(active_range.get("slot_index", -1))
			var select_shake_raw_8bit: int = raw_8bit
			var select_shake_norm: float = normalizer.norm_from_active_range(raw_8bit, active_range)
			var resolved_same_channel_shake: Dictionary = _resolve_same_channel_shake_runtime(
				select_shake_raw_8bit,
				slot_index_for_shake,
				shake_ranges
			)
			if not bool(resolved_same_channel_shake.get("has_range", false)):
				resolved_same_channel_shake = _resolve_shake_runtime(
					select_shake_raw_8bit,
					select_shake_norm,
					shake_ranges,
					mode_master_value_8bit,
					false
				)
			var resolved_dedicated_shake: Dictionary = {}
			if rotation_source_channel == "rotation":
				resolved_dedicated_shake = _resolve_dedicated_channel_shake_runtime(
					rotation_raw_8bit,
					rotation_control_norm,
					shake_ranges,
					mode_master_value_8bit
				)
			if bool(resolved_dedicated_shake.get("has_range", false)):
				resolved_shake = resolved_dedicated_shake
				shake_source_channel = "rotation"
				shake_raw_8bit = rotation_raw_8bit
				shake_raw_coarse = rotation_raw_coarse
				shake_raw_fine = rotation_raw_fine
			elif bool(resolved_same_channel_shake.get("has_range", false)):
				resolved_shake = resolved_same_channel_shake
				shake_source_channel = "select"
				shake_raw_8bit = select_shake_raw_8bit
				shake_raw_coarse = raw_8bit
				shake_raw_fine = -1
		var matched_range: Dictionary = resolved_rotation.get("range", {})
		var matched_shake_range: Dictionary = resolved_shake.get("range", {})
		var shake_active: bool = bool(resolved_shake.get("has_range", false)) or range_behavior == GOBO_BEHAVIOR_SHAKE
		var shake_freq_hz: float = float(resolved_shake.get("shake_frequency_hz", -1.0))
		var shake_amp_deg: float = float(resolved_shake.get("shake_amplitude_deg", -1.0))
		if shake_active and shake_freq_hz <= 0.0001:
			shake_freq_hz = GOBO_MIN_ACTIVE_SHAKE_FREQUENCY_HZ
		if shake_active and shake_amp_deg <= 0.0001:
			shake_amp_deg = _resolve_shake_fallback_min_amplitude()
		_debug_log_runtime_state(
			item,
			range_behavior,
			supports_rotation,
			bool(resolved_rotation.get("has_range", false)),
			float(resolved_rotation.get("rotation_speed_deg_per_sec", 0.0)),
			supports_shake,
			shake_active,
			shake_freq_hz,
			shake_amp_deg,
			rotation_source_channel,
			shake_source_channel,
			rotation_raw_8bit,
			shake_raw_8bit
		)
		runtime_bindings.append({
			"wheel_number": int(item.get("wheel_number", 0)),
			"wheel_name": str(item.get("wheel_name", "")),
			"raw_8bit": raw_8bit,
			"slot_index": int(active_range.get("slot_index", 0)),
			"mode_from_8bit": int(active_range.get("mode_from_8bit", 0)),
			"mode_to_8bit": int(active_range.get("mode_to_8bit", 255)),
			"behavior": range_behavior,
			"supports_index": supports_index,
			"supports_rotation": supports_rotation,
			"supports_shake": supports_shake,
			"has_index_physical_limits": bool(item.get("has_index_physical_limits", false)),
			"index_physical_min": float(item.get("index_physical_min", 0.0)),
			"index_physical_max": float(item.get("index_physical_max", 0.0)),
			"has_rotation_physical_ranges": not rotation_ranges.is_empty(),
			"has_rotation_physical_value": bool(resolved_rotation.get("has_range", false)),
			"rotation_physical": float(resolved_rotation.get("rotation_speed_deg_per_sec", 0.0)),
			"rotation_speed_deg_per_sec": float(resolved_rotation.get("rotation_speed_deg_per_sec", 0.0)),
			"rotation_direction_sign": int(resolved_rotation.get("direction_sign", 0)),
			"is_stop": bool(resolved_rotation.get("is_stop", true)),
			"has_resolved_rotation_range": bool(resolved_rotation.get("has_range", false)),
			"rotation_resolved_range": resolved_rotation.get("range", {}),
			"rotation_source_channel": rotation_source_channel,
			"rotation_raw_coarse": rotation_raw_coarse,
			"rotation_raw_fine": rotation_raw_fine,
			"rotation_raw_8bit": rotation_raw_8bit,
			"rotation_mode_window_from": mode_master_value_8bit,
			"rotation_mode_window_to": mode_master_value_8bit,
			"rotation_matched_range_start": int(matched_range.get("dmx_from", -1)),
			"rotation_matched_range_end": int(matched_range.get("dmx_to", -1)),
			"rotation_matched_physical_from": float(matched_range.get("physical_from", 0.0)),
			"rotation_matched_physical_to": float(matched_range.get("physical_to", 0.0)),
			"rotation_is_stop_range": bool(matched_range.get("is_stop_range", false)),
			"rotation_raw": rotation_raw,
			"rotation_raw_value": rotation_raw,
			"rotation_active_range": active_range,
			"rotation_ranges": rotation_ranges,
			"shake_ranges": shake_ranges,
			"has_shake_runtime_range": bool(resolved_shake.get("has_range", false)),
			"shake_active": shake_active,
			"shake_source_channel": shake_source_channel,
			"shake_raw_coarse": shake_raw_coarse,
			"shake_raw_fine": shake_raw_fine,
			"shake_raw_8bit": shake_raw_8bit,
			"shake_freq_hz": shake_freq_hz,
			"shake_amp_deg": shake_amp_deg,
			"shake_frequency_hz": shake_freq_hz,
			"shake_amplitude_deg": shake_amp_deg,
			"shake_matched_range_start": int(matched_shake_range.get("dmx_from", -1)),
			"shake_matched_range_end": int(matched_shake_range.get("dmx_to", -1)),
			"index_norm": index_norm,
			"slots": item.get("slots", []),
			"ranges": ranges,
		})
	return runtime_bindings

static func _debug_log_runtime_state(wheel_item: Dictionary,
		behavior: int,
		supports_rotation: bool,
		rotation_active: bool,
		rotation_speed_deg_per_sec: float,
		supports_shake: bool,
		shake_active: bool,
		shake_freq_hz: float,
		shake_amp_deg: float,
		rotation_source_channel: String,
		shake_source_channel: String,
		rotation_raw_8bit: int,
		shake_raw_8bit: int) -> void:
	if not bool(ProjectSettings.get_setting(GOBO_ROTATION_DEBUG_SETTING_KEY, false)):
		return
	var timestamp_ms: int = Time.get_ticks_msec()
	print("[PeravizGoboRuntime] ts_ms=%d wheel_number=%d wheel_name=%s behavior=%d supports_rotation=%s rotation_active=%s rotation_speed_dps=%.4f rotation_source=%s rotation_raw_8bit=%d supports_shake=%s shake_active=%s shake_freq_hz=%.4f shake_amp_deg=%.4f shake_source=%s shake_raw_8bit=%d overlap_active=%s" % [
		timestamp_ms,
		int(wheel_item.get("wheel_number", 0)),
		str(wheel_item.get("wheel_name", "")),
		behavior,
		str(supports_rotation),
		str(rotation_active),
		rotation_speed_deg_per_sec,
		rotation_source_channel,
		rotation_raw_8bit,
		str(supports_shake),
		str(shake_active),
		shake_freq_hz,
		shake_amp_deg,
		shake_source_channel,
		shake_raw_8bit,
		str(rotation_active and shake_active),
	])

static func _resolve_rotation_runtime(raw_8bit: int, control_norm: float, ranges: Array, mode_master_value_8bit: int, prefer_rotation_channel_ranges: bool) -> Dictionary:
	for pass_index in range(2):
		for item in ranges:
			if item is not Dictionary:
				continue
			var range_data: Dictionary = item
			var is_rotation_channel_range: bool = bool(range_data.get("is_rotation_channel_range", false))
			if pass_index == 0 and is_rotation_channel_range != prefer_rotation_channel_ranges:
				continue
			if pass_index == 1 and is_rotation_channel_range == prefer_rotation_channel_ranges:
				continue

			var range_mode_from: int = int(range_data.get("mode_from_8bit", 0))
			var range_mode_to: int = int(range_data.get("mode_to_8bit", 255))
			if range_mode_to < range_mode_from:
				var mode_swap: int = range_mode_from
				range_mode_from = range_mode_to
				range_mode_to = mode_swap
			if mode_master_value_8bit < range_mode_from or mode_master_value_8bit > range_mode_to:
				continue

			var dmx_from: int = int(range_data.get("dmx_from", 0))
			var dmx_to: int = int(range_data.get("dmx_to", dmx_from))
			if dmx_to < dmx_from:
				var swap_value: int = dmx_from
				dmx_from = dmx_to
				dmx_to = swap_value
			if raw_8bit < dmx_from or raw_8bit > dmx_to:
				continue

			var is_stop: bool = bool(range_data.get("is_stop_range", false))
			var speed_deg_per_sec: float = 0.0
			if not is_stop:
				if dmx_to <= dmx_from:
					speed_deg_per_sec = float(range_data.get("physical_to", range_data.get("physical_from", 0.0)))
				else:
					var ratio: float = float(raw_8bit - dmx_from) / float(dmx_to - dmx_from)
					if control_norm >= 0.0:
						var range_norm_from: float = float(dmx_from) / 255.0
						var range_norm_to: float = float(dmx_to) / 255.0
						var range_norm_span: float = range_norm_to - range_norm_from
						if range_norm_span > 0.0:
							ratio = clamp((control_norm - range_norm_from) / range_norm_span, 0.0, 1.0)
					speed_deg_per_sec = lerp(float(range_data.get("physical_from", 0.0)), float(range_data.get("physical_to", 0.0)), ratio)
			if absf(speed_deg_per_sec) <= 0.0001:
				is_stop = true
				speed_deg_per_sec = 0.0

			var direction_sign: int = 0
			if speed_deg_per_sec > 0.0:
				direction_sign = 1
			elif speed_deg_per_sec < 0.0:
				direction_sign = -1

			return {
				"has_range": true,
				"range": range_data,
				"rotation_speed_deg_per_sec": speed_deg_per_sec,
				"direction_sign": direction_sign,
				"is_stop": is_stop,
			}

	return {
		"has_range": false,
		"range": {},
		"rotation_speed_deg_per_sec": 0.0,
		"direction_sign": 0,
		"is_stop": true,
	}

static func _resolve_gobo_range(raw_8bit: int, ranges: Array) -> Dictionary:
	return DmxGoboRangeResolverScript.resolve_active_range(raw_8bit, ranges)

static func _resolve_same_channel_shake_runtime(raw_8bit: int, slot_index: int, ranges: Array) -> Dictionary:
	var filtered_ranges: Array = []
	var fallback_same_channel_ranges: Array = []
	for item in ranges:
		if item is not Dictionary:
			continue
		var range_data: Dictionary = item
		var control_type: int = int(range_data.get("control_type", GOBO_SHAKE_CONTROL_TYPE_SAME_CHANNEL_SELECT))
		if control_type != GOBO_SHAKE_CONTROL_TYPE_SAME_CHANNEL_SELECT:
			continue
		var normalized_range: Dictionary = range_data.duplicate(true)
		# For same-channel select ranges we are already in the active slot window.
		# Ignore external mode-window gating to avoid dropping valid slow->fast shake rows.
		normalized_range["mode_from_8bit"] = 0
		normalized_range["mode_to_8bit"] = 255
		fallback_same_channel_ranges.append(normalized_range)
		var range_slot_index: int = int(range_data.get("slot_index", slot_index))
		if range_slot_index != slot_index:
			continue
		filtered_ranges.append(normalized_range)
	if filtered_ranges.is_empty():
		filtered_ranges = fallback_same_channel_ranges
	if filtered_ranges.is_empty():
		return {"has_range": false, "range": {}, "shake_frequency_hz": -1.0, "shake_amplitude_deg": -1.0}
	return _resolve_shake_runtime(raw_8bit, -1.0, filtered_ranges, raw_8bit, false)

static func _resolve_dedicated_channel_shake_runtime(raw_8bit: int, control_norm: float, ranges: Array, mode_master_value_8bit: int) -> Dictionary:
	var dedicated_ranges: Array = []
	for item in ranges:
		if item is not Dictionary:
			continue
		var range_data: Dictionary = item
		var control_type: int = int(range_data.get("control_type", GOBO_SHAKE_CONTROL_TYPE_SAME_CHANNEL_SELECT))
		if control_type != GOBO_SHAKE_CONTROL_TYPE_DEDICATED_CHANNEL:
			continue
		dedicated_ranges.append(range_data)
	if dedicated_ranges.is_empty():
		return {
			"has_range": false,
			"range": {},
			"shake_frequency_hz": -1.0,
			"shake_amplitude_deg": -1.0,
		}
	return _resolve_shake_runtime(raw_8bit, control_norm, dedicated_ranges, mode_master_value_8bit, true)

static func _resolve_shake_runtime(raw_8bit: int, control_norm: float, ranges: Array, mode_master_value_8bit: int, prefer_dedicated_shake_ranges: bool) -> Dictionary:
	var fallback_amplitude_min: float = _resolve_shake_fallback_min_amplitude()
	var fallback_amplitude_max: float = _resolve_shake_fallback_max_amplitude(fallback_amplitude_min)

	for pass_index in range(2):
		for item in ranges:
			if item is not Dictionary:
				continue
			var range_data: Dictionary = item
			var control_type: int = int(range_data.get("control_type", GOBO_SHAKE_CONTROL_TYPE_SAME_CHANNEL_SELECT))
			var is_dedicated_shake_range: bool = control_type == GOBO_SHAKE_CONTROL_TYPE_DEDICATED_CHANNEL
			if pass_index == 0 and is_dedicated_shake_range != prefer_dedicated_shake_ranges:
				continue
			if pass_index == 1 and is_dedicated_shake_range == prefer_dedicated_shake_ranges:
				continue

			var range_mode_from: int = int(range_data.get("mode_from_8bit", 0))
			var range_mode_to: int = int(range_data.get("mode_to_8bit", 255))
			if range_mode_to < range_mode_from:
				var mode_swap: int = range_mode_from
				range_mode_from = range_mode_to
				range_mode_to = mode_swap
			if mode_master_value_8bit < range_mode_from or mode_master_value_8bit > range_mode_to:
				continue

			var dmx_from: int = int(range_data.get("dmx_from", 0))
			var dmx_to: int = int(range_data.get("dmx_to", dmx_from))
			if dmx_to < dmx_from:
				var swap_value: int = dmx_from
				dmx_from = dmx_to
				dmx_to = swap_value
			if raw_8bit < dmx_from or raw_8bit > dmx_to:
				continue

			var ratio: float = 0.0
			if dmx_to > dmx_from:
				ratio = float(raw_8bit - dmx_from) / float(dmx_to - dmx_from)
				if control_norm >= 0.0:
					var range_norm_from: float = float(dmx_from) / 255.0
					var range_norm_to: float = float(dmx_to) / 255.0
					var range_norm_span: float = range_norm_to - range_norm_from
					if range_norm_span > 0.0:
						ratio = clamp((control_norm - range_norm_from) / range_norm_span, 0.0, 1.0)

			var shake_frequency_hz: float = lerp(float(range_data.get("physical_from", 0.0)), float(range_data.get("physical_to", 0.0)), ratio)
			shake_frequency_hz = clamp(absf(shake_frequency_hz), GOBO_MIN_ACTIVE_SHAKE_FREQUENCY_HZ, GOBO_MAX_SHAKE_FREQUENCY_HZ)

			var shake_amplitude_deg: float = 0.0
			if bool(range_data.get("has_explicit_amplitude", false)):
				shake_amplitude_deg = lerp(float(range_data.get("amplitude_from_degrees", 0.0)), float(range_data.get("amplitude_to_degrees", 0.0)), ratio)
				shake_amplitude_deg = GoboShakeLimitsScript.clamp_amplitude_deg(shake_amplitude_deg)
			else:
				var frequency_edge_a: float = clamp(absf(float(range_data.get("physical_from", 0.0))), 0.0, GOBO_MAX_SHAKE_FREQUENCY_HZ)
				var frequency_edge_b: float = clamp(absf(float(range_data.get("physical_to", 0.0))), 0.0, GOBO_MAX_SHAKE_FREQUENCY_HZ)
				var min_frequency_edge: float = minf(frequency_edge_a, frequency_edge_b)
				var max_frequency_edge: float = maxf(frequency_edge_a, frequency_edge_b)
				var frequency_ratio: float = 0.0
				if max_frequency_edge > min_frequency_edge + 0.0001:
					frequency_ratio = clamp((shake_frequency_hz - min_frequency_edge) / (max_frequency_edge - min_frequency_edge), 0.0, 1.0)
				elif max_frequency_edge > 0.0001:
					frequency_ratio = 1.0
				shake_amplitude_deg = lerp(fallback_amplitude_min, fallback_amplitude_max, frequency_ratio)

			return {
				"has_range": true,
				"range": range_data,
				"shake_frequency_hz": shake_frequency_hz,
				"shake_amplitude_deg": shake_amplitude_deg,
			}

	return {
		"has_range": false,
		"range": {},
		"shake_frequency_hz": -1.0,
		"shake_amplitude_deg": -1.0,
	}

static func _resolve_shake_fallback_min_amplitude() -> float:
	return maxf(float(ProjectSettings.get_setting("peraviz/gobo_shake_fallback_amplitude_min_deg", GOBO_DEFAULT_SHAKE_FALLBACK_AMPLITUDE_MIN_DEG)), 0.0)

static func _resolve_shake_fallback_max_amplitude(fallback_amplitude_min: float) -> float:
	var configured_max: float = float(ProjectSettings.get_setting("peraviz/gobo_shake_fallback_amplitude_max_deg", GOBO_DEFAULT_SHAKE_FALLBACK_AMPLITUDE_MAX_DEG))
	return maxf(configured_max, fallback_amplitude_min)
