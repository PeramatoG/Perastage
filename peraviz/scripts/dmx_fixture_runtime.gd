extends RefCounted
class_name DmxFixtureRuntime

const MAX_UNBOUND_PREVIEW: int = 8
const DMX_8BIT_MAX_VALUE: float = 255.0
const DMX_8BIT_STEPS: int = 256
const DMX_16BIT_STEPS: int = 65536
const DMX_24BIT_STEPS: int = 16777216

var _loader = null
var _scene_registry: SceneRegistry = null
var _bindings: Array = []
var _unbound: Array = []
var _fixture_nodes: Dictionary = {}
var _used_universes: Dictionary = {}

func configure(loader, scene_registry: SceneRegistry) -> void:
	_loader = loader
	_scene_registry = scene_registry

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
		}

		_read_control(binding, frame, "dimmer_channel_index_0", "dimmer_fine_channel_index_0", "dimmer_ultra_fine_channel_index_0", controls, "has_dimmer", "dimmer_norm")
		_read_control(binding, frame, "pan_channel_index_0", "pan_fine_channel_index_0", "pan_ultra_fine_channel_index_0", controls, "has_pan", "pan_norm")
		_read_control(binding, frame, "tilt_channel_index_0", "tilt_fine_channel_index_0", "tilt_ultra_fine_channel_index_0", controls, "has_tilt", "tilt_norm")

		if not controls["has_dimmer"] and not controls["has_pan"] and not controls["has_tilt"]:
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
