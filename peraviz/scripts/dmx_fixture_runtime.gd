extends RefCounted
class_name DmxFixtureRuntime

const MAX_UNBOUND_PREVIEW: int = 8
const DMX_8BIT_MAX_VALUE: float = 255.0
const DMX_8BIT_STEPS: int = 256
const DMX_16BIT_STEPS: int = 65536
const DMX_24BIT_STEPS: int = 16777216
const FORCE_COARSE_ONLY_DMX_READ: bool = false

const GOBO_BEHAVIOR_FIXED: int = 0
const GOBO_BEHAVIOR_INDEX: int = 1
const GOBO_BEHAVIOR_ROTATION: int = 2
const GOBO_BEHAVIOR_SHAKE: int = 3

const GoboVectorizationCacheScript = preload("res://scripts/gobo_vectorization/gobo_vectorization_cache.gd")
const DmxGoboRangeResolverScript = preload("res://scripts/dmx_gobo_range_resolver.gd")

var _loader = null
var _scene_registry: SceneRegistry = null
var _bindings: Array = []
var _unbound: Array = []
var _fixture_nodes: Dictionary = {}
var _used_universes: Dictionary = {}
var _gobo_vectorization_cache: GoboVectorizationCache = null

func configure(loader, scene_registry: SceneRegistry) -> void:
	_loader = loader
	_scene_registry = scene_registry
	_gobo_vectorization_cache = GoboVectorizationCacheScript.new()

func rebuild(universe_offset: int) -> Dictionary:
	_bindings.clear()
	_unbound.clear()
	_fixture_nodes.clear()
	_used_universes.clear()

	if _loader == null or _scene_registry == null:
		return {
			"bound": 0,
			"unbound": 0,
			"unbound_preview": PackedStringArray(),
		}

	var result: Dictionary = _loader.build_fixture_dimmer_bindings(universe_offset)
	_bindings = result.get("bindings", [])
	_unbound = result.get("unbound", [])

	if _gobo_vectorization_cache != null:
		_gobo_vectorization_cache.enrich_bindings_with_vector_gobos(_bindings)

	for binding in _bindings:
		if binding is not Dictionary:
			continue
		var fixture_uuid: String = str(binding.get("fixture_uuid", ""))
		if fixture_uuid.is_empty():
			continue
		var fixture_node: Node = _scene_registry.get_fixture(fixture_uuid)
		if fixture_node == null:
			_unbound.append({
				"fixture_uuid": fixture_uuid,
				"reason": "fixture node not found in loaded scene",
			})
			continue
		_fixture_nodes[fixture_uuid] = fixture_node
		var universe_id: int = int(binding.get("artnet_universe_id", -1))
		if universe_id >= 0:
			_used_universes[universe_id] = true

	return _build_summary(universe_offset)

func apply_dmx(receiver, apply_fixture_callback: Callable) -> void:
	if receiver == null or not receiver.is_running():
		return
	if apply_fixture_callback.is_null():
		return

	var universe_frames: Dictionary = {}
	for universe_key in _used_universes.keys():
		var universe_id: int = int(universe_key)
		universe_frames[universe_id] = receiver.get_universe_data(universe_id)

	for binding in _bindings:
		if binding is not Dictionary:
			continue
		var fixture_uuid: String = str(binding.get("fixture_uuid", ""))
		if fixture_uuid.is_empty() or not _fixture_nodes.has(fixture_uuid):
			continue
		var universe_id: int = int(binding.get("artnet_universe_id", -1))
		var frame: PackedByteArray = universe_frames.get(universe_id, PackedByteArray())
		if frame.is_empty():
			continue

		var controls := {
			"has_dimmer": false,
			"dimmer_norm": 0.0,
			"has_pan": false,
			"pan_norm": 0.0,
			"has_tilt": false,
			"tilt_norm": 0.0,
			"has_zoom": false,
			"zoom_norm": 0.0,
			"has_cyan": false,
			"cyan_norm": 0.0,
			"has_magenta": false,
			"magenta_norm": 0.0,
			"has_yellow": false,
			"yellow_norm": 0.0,
			"has_gobo": false,
			"gobo_norm": 0.0,
			"has_gobo_index": false,
			"gobo_index_norm": 0.0,
			"has_gobo_rotation": false,
			"gobo_rotation_norm": 0.0,
		}

		_read_control(binding, frame, "dimmer_channel_index_0", "dimmer_fine_channel_index_0", "dimmer_ultra_fine_channel_index_0", controls, "has_dimmer", "dimmer_norm")
		_read_control(binding, frame, "pan_channel_index_0", "pan_fine_channel_index_0", "pan_ultra_fine_channel_index_0", controls, "has_pan", "pan_norm")
		_read_control(binding, frame, "tilt_channel_index_0", "tilt_fine_channel_index_0", "tilt_ultra_fine_channel_index_0", controls, "has_tilt", "tilt_norm")
		_read_control(binding, frame, "zoom_channel_index_0", "zoom_fine_channel_index_0", "zoom_ultra_fine_channel_index_0", controls, "has_zoom", "zoom_norm")
		_read_control(binding, frame, "cyan_channel_index_0", "cyan_fine_channel_index_0", "cyan_ultra_fine_channel_index_0", controls, "has_cyan", "cyan_norm")
		_read_control(binding, frame, "magenta_channel_index_0", "magenta_fine_channel_index_0", "magenta_ultra_fine_channel_index_0", controls, "has_magenta", "magenta_norm")
		_read_control(binding, frame, "yellow_channel_index_0", "yellow_fine_channel_index_0", "yellow_ultra_fine_channel_index_0", controls, "has_yellow", "yellow_norm")
		_read_control(binding, frame, "gobo1_channel_index_0", "gobo1_fine_channel_index_0", "gobo1_ultra_fine_channel_index_0", controls, "has_gobo", "gobo_norm")
		if not controls["has_gobo"]:
			_read_control(binding, frame, "gobo_channel_index_0", "gobo_fine_channel_index_0", "gobo_ultra_fine_channel_index_0", controls, "has_gobo", "gobo_norm")

		_read_control(binding, frame, "gobo_index_channel_index_0", "gobo_index_fine_channel_index_0", "gobo_index_ultra_fine_channel_index_0", controls, "has_gobo_index", "gobo_index_norm")
		_read_control(binding, frame, "gobo_rotation_channel_index_0", "gobo_rotation_fine_channel_index_0", "gobo_rotation_ultra_fine_channel_index_0", controls, "has_gobo_rotation", "gobo_rotation_norm")

		if controls["has_zoom"]:
			controls["has_zoom_physical_limits"] = bool(binding.get("has_zoom_physical_limits", false))
			controls["zoom_physical_min_degrees"] = float(binding.get("zoom_physical_min_degrees", -1.0))
			controls["zoom_physical_max_degrees"] = float(binding.get("zoom_physical_max_degrees", -1.0))

		if controls["has_gobo"]:
			controls["gobo_slots"] = binding.get("gobo1_slots", binding.get("gobo_slots", []))
			controls["gobo_ranges"] = binding.get("gobo1_ranges", binding.get("gobo_ranges", []))
			controls["gobo_wheel_name"] = str(binding.get("gobo1_wheel_name", binding.get("gobo_wheel_name", "")))
			controls["gobo_wheel_number"] = int(binding.get("gobo_wheel_number", 0))

		var gobo_runtime_bindings: Array = _build_runtime_gobo_bindings(binding, frame)
		if not gobo_runtime_bindings.is_empty():
			controls["has_gobo"] = true
			controls["gobo_runtime_bindings"] = gobo_runtime_bindings

		if not controls["has_dimmer"] and not controls["has_pan"] and not controls["has_tilt"] and not controls["has_zoom"] and not controls["has_cyan"] and not controls["has_magenta"] and not controls["has_yellow"] and not controls["has_gobo"] and not controls["has_gobo_index"] and not controls["has_gobo_rotation"]:
			continue
		apply_fixture_callback.call(fixture_uuid, controls)

func _read_control(binding: Dictionary,
					   frame: PackedByteArray,
					   coarse_key: String,
					   fine_key: String,
					   ultra_fine_key: String,
					   controls: Dictionary,
					   has_key: String,
					   value_key: String) -> void:
	var coarse_index: int = int(binding.get(coarse_key, -1))
	if not _is_valid_channel_index(frame, coarse_index):
		return

	var fine_index: int = int(binding.get(fine_key, -1))
	var ultra_fine_index: int = int(binding.get(ultra_fine_key, -1))
	var value: Dictionary = _read_control_value(frame, coarse_index, fine_index, ultra_fine_index)

	controls[has_key] = true
	controls[value_key] = value.get("norm", 0.0)

	var debug_prefix: String = value_key.trim_suffix("_norm")
	controls["%s_raw_value" % debug_prefix] = value.get("raw", 0)
	controls["%s_resolution_bits" % debug_prefix] = value.get("resolution_bits", 8)
	controls["%s_bytes" % debug_prefix] = value.get("bytes", PackedInt32Array())

func _read_control_value(frame: PackedByteArray,
						 coarse_index: int,
						 fine_index: int,
						 ultra_fine_index: int) -> Dictionary:
	var coarse: int = int(frame[coarse_index])

	if FORCE_COARSE_ONLY_DMX_READ:
		return {
			"raw": coarse,
			"norm": float(coarse) / DMX_8BIT_MAX_VALUE,
			"resolution_bits": 8,
			"bytes": PackedInt32Array([coarse]),
		}

	if _is_valid_channel_index(frame, fine_index) and _is_valid_channel_index(frame, ultra_fine_index):
		var fine: int = int(frame[fine_index])
		var ultra_fine: int = int(frame[ultra_fine_index])
		var raw_value_24: int = _resolve_24bit_raw_value(coarse, fine, ultra_fine)
		return {
			"raw": raw_value_24,
			"norm": float(raw_value_24) / float(DMX_24BIT_STEPS - 1),
			"resolution_bits": 24,
			"bytes": PackedInt32Array([coarse, fine, ultra_fine]),
		}

	if _is_valid_channel_index(frame, fine_index):
		var fine: int = int(frame[fine_index])
		var raw_value_16: int = _resolve_16bit_raw_value(coarse, fine)
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

func _is_valid_channel_index(frame: PackedByteArray, channel_index_0: int) -> bool:
	return channel_index_0 >= 0 and channel_index_0 < frame.size()

func _resolve_16bit_raw_value(coarse: int, fine: int) -> int:
	var safe_coarse: int = clampi(coarse, 0, int(DMX_8BIT_MAX_VALUE))
	var safe_fine: int = clampi(fine, 0, int(DMX_8BIT_MAX_VALUE))
	return (safe_coarse * DMX_8BIT_STEPS) + safe_fine

func _resolve_24bit_raw_value(coarse: int, fine: int, ultra_fine: int) -> int:
	var safe_coarse: int = clampi(coarse, 0, int(DMX_8BIT_MAX_VALUE))
	var safe_fine: int = clampi(fine, 0, int(DMX_8BIT_MAX_VALUE))
	var safe_ultra_fine: int = clampi(ultra_fine, 0, int(DMX_8BIT_MAX_VALUE))
	return (safe_coarse * DMX_16BIT_STEPS) + (safe_fine * DMX_8BIT_STEPS) + safe_ultra_fine

func get_bound_count() -> int:
	return _fixture_nodes.size()

func get_unbound_count() -> int:
	return _unbound.size()

func get_unbound_preview() -> PackedStringArray:
	var lines := PackedStringArray()
	for index in range(min(MAX_UNBOUND_PREVIEW, _unbound.size())):
		var row: Dictionary = _unbound[index]
		lines.append("%s: %s" % [str(row.get("fixture_uuid", "<unknown>")), str(row.get("reason", "unspecified"))])
	return lines

func _build_summary(universe_offset: int) -> Dictionary:
	return {
		"bound": get_bound_count(),
		"unbound": get_unbound_count(),
		"unbound_preview": get_unbound_preview(),
		"universe_offset": universe_offset,
	}

func _build_runtime_gobo_bindings(binding: Dictionary, frame: PackedByteArray) -> Array:
	var runtime_bindings: Array = []
	var raw_wheels: Array = binding.get("gobo_wheels", [])
	for item in raw_wheels:
		if item is not Dictionary:
			continue
		var coarse_index: int = int(item.get("channel_index_0", -1))
		if not _is_valid_channel_index(frame, coarse_index):
			continue
		var value: Dictionary = _read_control_value(frame, coarse_index, int(item.get("fine_channel_index_0", -1)), int(item.get("ultra_fine_channel_index_0", -1)))
		var raw_8bit: int = _resolve_raw_to_8bit(int(value.get("raw", 0)), int(value.get("resolution_bits", 8)))
		var ranges: Array = item.get("ranges", [])
		var active_range: Dictionary = _resolve_gobo_range(raw_8bit, ranges)
		# GDTF ChannelSet ranges define the active wheel function (fixed/index/spin/shake)
		# for the current DMX value on the gobo select channel.
		var range_behavior: int = int(active_range.get("behavior", GOBO_BEHAVIOR_FIXED))
		var has_index_channel: bool = _has_control_channel(item, "index_channel_index_0", "index_fine_channel_index_0", "index_ultra_fine_channel_index_0")
		var has_rotation_channel: bool = _has_control_channel(item, "rotation_channel_index_0", "rotation_fine_channel_index_0", "rotation_ultra_fine_channel_index_0")
		var has_behavior_ranges: bool = not ranges.is_empty()
		var is_rotation_behavior: bool = range_behavior == GOBO_BEHAVIOR_ROTATION or range_behavior == GOBO_BEHAVIOR_SHAKE
		var uses_range_rotation: bool = is_rotation_behavior and not has_rotation_channel
		var supports_index: bool = false
		var supports_rotation: bool = false
		if has_behavior_ranges:
			# In many fixtures, the gobo selection range can remain fixed while
			# dedicated index/rotation channels still drive the wheel. Keep channel
			# authority enabled, but prevent conflicting index+spin by honoring
			# explicit index ranges as spin-disabled segments.
			supports_index = (range_behavior == GOBO_BEHAVIOR_INDEX) or (has_index_channel and not is_rotation_behavior)
			supports_rotation = is_rotation_behavior or (has_rotation_channel and range_behavior != GOBO_BEHAVIOR_INDEX)
		else:
			supports_index = has_index_channel
			supports_rotation = has_rotation_channel
		if uses_range_rotation:
			supports_rotation = true
		var index_norm: float = -1.0
		var rotation_norm: float = -1.0
		var rotation_raw: int = raw_8bit
		var rotation_ranges: Array = item.get("rotation_ranges", [])
		var rotation_physical: float = 0.0
		if supports_index:
			index_norm = _read_optional_control_norm(frame, int(item.get("index_channel_index_0", -1)), int(item.get("index_fine_channel_index_0", -1)), int(item.get("index_ultra_fine_channel_index_0", -1)))
			if index_norm < 0.0 and range_behavior == GOBO_BEHAVIOR_INDEX:
				index_norm = _resolve_norm_from_active_range(raw_8bit, active_range)
		if supports_rotation:
			if has_rotation_channel:
				var rotation_value: Dictionary = _read_optional_control_value(frame, int(item.get("rotation_channel_index_0", -1)), int(item.get("rotation_fine_channel_index_0", -1)), int(item.get("rotation_ultra_fine_channel_index_0", -1)))
				if not rotation_value.is_empty():
					rotation_norm = clamp(float(rotation_value.get("norm", 0.0)), 0.0, 1.0)
					rotation_raw = _resolve_raw_to_8bit(int(rotation_value.get("raw", 0)), int(rotation_value.get("resolution_bits", 8)))
			if uses_range_rotation:
				rotation_norm = _resolve_norm_from_active_range(raw_8bit, active_range)
				rotation_raw = raw_8bit
			elif rotation_norm < 0.0 and is_rotation_behavior:
				rotation_norm = _resolve_norm_from_active_range(raw_8bit, active_range)
				rotation_raw = raw_8bit
			if rotation_norm >= 0.0 and not rotation_ranges.is_empty():
				rotation_physical = _resolve_rotation_physical(rotation_raw, rotation_ranges)
		runtime_bindings.append({
			"wheel_number": int(item.get("wheel_number", 0)),
			"wheel_name": str(item.get("wheel_name", "")),
			"raw_8bit": raw_8bit,
			"slot_index": int(active_range.get("slot_index", 0)),
			"behavior": range_behavior,
			"supports_index": supports_index,
			"supports_rotation": supports_rotation,
			"has_index_physical_limits": bool(item.get("has_index_physical_limits", false)),
			"index_physical_min": float(item.get("index_physical_min", 0.0)),
			"index_physical_max": float(item.get("index_physical_max", 0.0)),
			"has_rotation_physical_limits": bool(item.get("has_rotation_physical_limits", false)) or not rotation_ranges.is_empty(),
			"rotation_physical_min": float(item.get("rotation_physical_min", 0.0)),
			"rotation_physical_max": float(item.get("rotation_physical_max", 0.0)),
			"has_rotation_physical_ranges": not rotation_ranges.is_empty(),
			"has_rotation_physical_value": not rotation_ranges.is_empty() and rotation_norm >= 0.0,
			"rotation_physical": rotation_physical,
			"rotation_raw": rotation_raw,
			"rotation_raw_value": rotation_raw,
			"rotation_active_range": active_range,
			"rotation_ranges": rotation_ranges,
			"index_norm": index_norm,
			"rotation_norm": rotation_norm,
			"slots": item.get("slots", []),
			"ranges": ranges,
		})
	return runtime_bindings

func _has_control_channel(binding: Dictionary, coarse_key: String, fine_key: String, ultra_fine_key: String) -> bool:
	return int(binding.get(coarse_key, -1)) >= 0 or int(binding.get(fine_key, -1)) >= 0 or int(binding.get(ultra_fine_key, -1)) >= 0

func _resolve_norm_from_active_range(raw_8bit: int, active_range: Dictionary) -> float:
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


func _resolve_rotation_physical(dmx_val: float, ranges: Array) -> float:
	for item in ranges:
		if item is not Dictionary:
			continue
		var range_data: Dictionary = item
		var dmx_start: int = int(range_data.get("dmx_start", 0))
		var dmx_end: int = int(range_data.get("dmx_end", dmx_start))
		if dmx_end < dmx_start:
			var swap_value: int = dmx_start
			dmx_start = dmx_end
			dmx_end = swap_value
		if dmx_val < float(dmx_start) or dmx_val > float(dmx_end):
			continue
		if bool(range_data.get("is_stop_range", false)):
			return 0.0
		if dmx_end <= dmx_start:
			return float(range_data.get("physical_to", range_data.get("physical_from", 0.0)))
		var ratio: float = (dmx_val - float(dmx_start)) / float(dmx_end - dmx_start)
		return lerp(float(range_data.get("physical_from", 0.0)), float(range_data.get("physical_to", 0.0)), ratio)
	return 0.0



func _read_optional_control_value(frame: PackedByteArray, coarse_index: int, fine_index: int, ultra_fine_index: int) -> Dictionary:
	if not _is_valid_channel_index(frame, coarse_index):
		return {}
	return _read_control_value(frame, coarse_index, fine_index, ultra_fine_index)

func _read_optional_control_norm(frame: PackedByteArray, coarse_index: int, fine_index: int, ultra_fine_index: int) -> float:
	if not _is_valid_channel_index(frame, coarse_index):
		return -1.0
	var value: Dictionary = _read_control_value(frame, coarse_index, fine_index, ultra_fine_index)
	return clamp(float(value.get("norm", 0.0)), 0.0, 1.0)

func _resolve_raw_to_8bit(raw_value: int, resolution_bits: int) -> int:
	if resolution_bits >= 24:
		return clampi(raw_value >> 16, 0, 255)
	if resolution_bits >= 16:
		return clampi(raw_value >> 8, 0, 255)
	return clampi(raw_value, 0, 255)

func _resolve_gobo_range(raw_8bit: int, ranges: Array) -> Dictionary:
	return DmxGoboRangeResolverScript.resolve_active_range(raw_8bit, ranges)

func _resolve_gobo_slot_from_ranges(raw_8bit: int, ranges: Array) -> int:
	return int(_resolve_gobo_range(raw_8bit, ranges).get("slot_index", 0))
