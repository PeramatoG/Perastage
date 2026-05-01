extends RefCounted
class_name DmxFixtureRuntime

const MAX_UNBOUND_PREVIEW: int = 8
const FORCE_COARSE_ONLY_DMX_READ: bool = false

const GoboVectorizationCacheScript = preload("res://scripts/gobo_vectorization/gobo_vectorization_cache.gd")
const ControlReaderScript = preload("res://scripts/dmx_capability_control_reader.gd")
const CapabilityNormalizerScript = preload("res://scripts/dmx_capability_normalizer.gd")
const PanTiltCapabilityHandlerScript = preload("res://scripts/dmx_capabilities/pan_tilt_capability_handler.gd")
const DimmerCapabilityHandlerScript = preload("res://scripts/dmx_capabilities/dimmer_capability_handler.gd")
const ColorWheelCapabilityHandlerScript = preload("res://scripts/dmx_capabilities/color_wheel_capability_handler.gd")
const GoboCapabilityHandlerScript = preload("res://scripts/dmx_capabilities/gobo_capability_handler.gd")
const PrismCapabilityHandlerScript = preload("res://scripts/dmx_capabilities/prism_capability_handler.gd")
const StrobeCapabilityHandlerScript = preload("res://scripts/dmx_capabilities/strobe_capability_handler.gd")

var _loader = null
var _scene_registry: SceneRegistry = null
var _bindings: Array = []
var _unbound: Array = []
var _fixture_nodes: Dictionary = {}
var _fixture_channel_offsets: Dictionary = {}
var _fixture_snapshot_cache: Dictionary = {}
var _used_universes: Dictionary = {}
var _gobo_vectorization_cache: GoboVectorizationCache = null
var _debug_force_full_apply: bool = false

func configure(loader, scene_registry: SceneRegistry) -> void:
	_loader = loader
	_scene_registry = scene_registry
	_gobo_vectorization_cache = GoboVectorizationCacheScript.new()

func rebuild(universe_offset: int) -> Dictionary:
	_bindings.clear()
	_unbound.clear()
	_fixture_nodes.clear()
	_fixture_channel_offsets.clear()
	_fixture_snapshot_cache.clear()
	_used_universes.clear()

	if _loader == null or _scene_registry == null:
		return {
			"bound": 0,
			"unbound": 0,
			"unbound_preview": PackedStringArray(),
		}

	var result: Dictionary = _loader.build_fixture_dmx_bindings(universe_offset)
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
		_fixture_channel_offsets[fixture_uuid] = _collect_used_channel_offsets(binding)
		var universe_id: int = int(binding.get("artnet_universe_id", -1))
		if universe_id >= 0:
			_used_universes[universe_id] = true

	return _build_summary(universe_offset)

func set_debug_force_full_apply(enabled: bool) -> void:
	_debug_force_full_apply = enabled

func get_bound_fixture_ids() -> PackedStringArray:
	var fixture_ids := PackedStringArray()
	for fixture_uuid in _fixture_nodes.keys():
		fixture_ids.append(str(fixture_uuid))
	return fixture_ids

func apply_dmx(receiver, apply_fixture_callback: Callable) -> Dictionary:
	if receiver == null or not receiver.is_running():
		return {"updated": 0, "skipped": 0}
	if apply_fixture_callback.is_null():
		return {"updated": 0, "skipped": 0}

	var universe_frames: Dictionary = {}
	for universe_key in _used_universes.keys():
		var universe_id: int = int(universe_key)
		universe_frames[universe_id] = receiver.get_universe_data(universe_id)

	var updated: int = 0
	var skipped: int = 0
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
		var snapshot: PackedByteArray = _extract_snapshot_for_fixture(fixture_uuid, frame)
		if snapshot.is_empty():
			var controls_without_cache: Dictionary = _build_controls(binding, frame)
			if not _has_any_capability(controls_without_cache):
				continue
			apply_fixture_callback.call(fixture_uuid, controls_without_cache)
			updated += 1
			continue
		var snapshot_hash: int = _compute_snapshot_hash(snapshot)
		var previous_state: Dictionary = _fixture_snapshot_cache.get(fixture_uuid, {})
		if not _debug_force_full_apply and _snapshot_is_unchanged(previous_state, snapshot_hash, snapshot):
			skipped += 1
			continue

		var controls: Dictionary = _build_controls(binding, frame)
		if not _has_any_capability(controls):
			continue
		_fixture_snapshot_cache[fixture_uuid] = {
			"hash": snapshot_hash,
			"snapshot": snapshot,
		}
		apply_fixture_callback.call(fixture_uuid, controls)
		updated += 1
	return {"updated": updated, "skipped": skipped}

func _snapshot_is_unchanged(previous_state: Dictionary, snapshot_hash: int, snapshot: PackedByteArray) -> bool:
	if previous_state.is_empty():
		return false
	if int(previous_state.get("hash", -1)) != snapshot_hash:
		return false
	var previous_snapshot: PackedByteArray = previous_state.get("snapshot", PackedByteArray())
	return previous_snapshot == snapshot

func _extract_snapshot_for_fixture(fixture_uuid: String, frame: PackedByteArray) -> PackedByteArray:
	var offsets: PackedInt32Array = _fixture_channel_offsets.get(fixture_uuid, PackedInt32Array())
	var snapshot := PackedByteArray()
	for offset in offsets:
		if offset < 0 or offset >= frame.size():
			snapshot.append(0)
		else:
			snapshot.append(frame[offset])
	return snapshot

func _compute_snapshot_hash(snapshot: PackedByteArray) -> int:
	var hash_value: int = 2166136261
	for value in snapshot:
		hash_value = int((hash_value ^ int(value)) * 16777619)
	return hash_value

func _collect_used_channel_offsets(binding: Dictionary) -> PackedInt32Array:
	var offsets := PackedInt32Array()
	var used_offsets := {}
	var channel_bindings: Array = binding.get("channel_bindings", [])
	for channel_binding in channel_bindings:
		if channel_binding is not Dictionary:
			continue
		var offset: int = int(channel_binding.get("dmx_offset", -1))
		var fine_offset: int = int(channel_binding.get("dmx_fine_offset", -1))
		if offset >= 0:
			used_offsets[offset] = true
		if fine_offset >= 0:
			used_offsets[fine_offset] = true
	var sorted_offsets: Array = used_offsets.keys()
	sorted_offsets.sort()
	for offset in sorted_offsets:
		offsets.append(int(offset))
	return offsets

func _build_controls(binding: Dictionary, frame: PackedByteArray) -> Dictionary:
	var capabilities := {
		"pan_tilt": [],
		"dimmer": [],
		"color_wheel": [],
		"gobo": [],
		"prism": [],
		"strobe": [],
	}
	_append_capabilities(capabilities, "pan_tilt", PanTiltCapabilityHandlerScript.collect(binding, frame, ControlReaderScript, FORCE_COARSE_ONLY_DMX_READ))
	_append_capabilities(capabilities, "dimmer", DimmerCapabilityHandlerScript.collect(binding, frame, ControlReaderScript, FORCE_COARSE_ONLY_DMX_READ))
	_append_capabilities(capabilities, "color_wheel", ColorWheelCapabilityHandlerScript.collect(binding, frame, ControlReaderScript, FORCE_COARSE_ONLY_DMX_READ))
	_append_capabilities(capabilities, "gobo", GoboCapabilityHandlerScript.collect(binding, frame, ControlReaderScript, CapabilityNormalizerScript, FORCE_COARSE_ONLY_DMX_READ))
	_append_capabilities(capabilities, "prism", PrismCapabilityHandlerScript.collect(binding, frame, ControlReaderScript, FORCE_COARSE_ONLY_DMX_READ))
	_append_capabilities(capabilities, "strobe", StrobeCapabilityHandlerScript.collect(binding, frame, ControlReaderScript, FORCE_COARSE_ONLY_DMX_READ))
	return {
		"capabilities": capabilities,
	}

func _append_capabilities(capabilities: Dictionary, capability_type: String, blocks: Array) -> void:
	if blocks.is_empty():
		return
	if not capabilities.has(capability_type):
		capabilities[capability_type] = []
	var bucket: Array = capabilities.get(capability_type, [])
	for block in blocks:
		if block is Dictionary:
			bucket.append(block)
	capabilities[capability_type] = bucket

func _has_any_capability(controls: Dictionary) -> bool:
	var capabilities: Dictionary = controls.get("capabilities", {})
	for key in capabilities.keys():
		var items: Array = capabilities.get(key, [])
		if not items.is_empty():
			return true
	return false

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
