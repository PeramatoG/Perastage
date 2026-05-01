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
var _used_universes: Dictionary = {}
var _gobo_vectorization_cache: GoboVectorizationCache = null
var _binding_runtime_cache: Array = []

func configure(loader, scene_registry: SceneRegistry) -> void:
	_loader = loader
	_scene_registry = scene_registry
	_gobo_vectorization_cache = GoboVectorizationCacheScript.new()

func rebuild(universe_offset: int) -> Dictionary:
	_bindings.clear()
	_unbound.clear()
	_fixture_nodes.clear()
	_used_universes.clear()
	_binding_runtime_cache.clear()

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
		var universe_id: int = int(binding.get("artnet_universe_id", -1))
		if universe_id >= 0:
			_used_universes[universe_id] = true
		_binding_runtime_cache.append(_build_binding_runtime_cache(binding))

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

	for binding_index in range(_bindings.size()):
		var binding: Dictionary = _bindings[binding_index]
		if binding is not Dictionary:
			continue
		var binding_cache: Dictionary = _binding_runtime_cache[binding_index] if binding_index < _binding_runtime_cache.size() else {}
		var fixture_uuid: String = str(binding.get("fixture_uuid", ""))
		if fixture_uuid.is_empty() or not _fixture_nodes.has(fixture_uuid):
			continue
		var universe_id: int = int(binding_cache.get("universe_id", int(binding.get("artnet_universe_id", -1))))
		var frame: PackedByteArray = universe_frames.get(universe_id, PackedByteArray())
		if frame.is_empty():
			continue

		var controls: Dictionary = _build_controls(binding, frame, binding_cache)
		if not _has_any_capability(controls):
			continue
		apply_fixture_callback.call(fixture_uuid, controls)

func _build_controls(binding: Dictionary, frame: PackedByteArray, binding_cache: Dictionary = {}) -> Dictionary:
	var capabilities: Dictionary = binding_cache.get("capability_template", {}).duplicate(true)
	if capabilities.is_empty():
		capabilities = _build_capability_template()
	_append_capabilities(capabilities, "pan_tilt", PanTiltCapabilityHandlerScript.collect(binding, frame, ControlReaderScript, FORCE_COARSE_ONLY_DMX_READ))
	_append_capabilities(capabilities, "dimmer", DimmerCapabilityHandlerScript.collect(binding, frame, ControlReaderScript, FORCE_COARSE_ONLY_DMX_READ))
	_append_capabilities(capabilities, "color_wheel", ColorWheelCapabilityHandlerScript.collect(binding, frame, ControlReaderScript, FORCE_COARSE_ONLY_DMX_READ))
	_append_capabilities(capabilities, "gobo", GoboCapabilityHandlerScript.collect(binding, frame, ControlReaderScript, CapabilityNormalizerScript, FORCE_COARSE_ONLY_DMX_READ))
	_append_capabilities(capabilities, "prism", PrismCapabilityHandlerScript.collect(binding, frame, ControlReaderScript, FORCE_COARSE_ONLY_DMX_READ))
	_append_capabilities(capabilities, "strobe", StrobeCapabilityHandlerScript.collect(binding, frame, ControlReaderScript, FORCE_COARSE_ONLY_DMX_READ))
	return {
		"capabilities": capabilities,
	}

func _build_binding_runtime_cache(binding: Dictionary) -> Dictionary:
	return {
		"universe_id": int(binding.get("artnet_universe_id", -1)),
		"capability_template": _build_capability_template(),
	}

func _build_capability_template() -> Dictionary:
	return {
		"pan_tilt": [],
		"dimmer": [],
		"color_wheel": [],
		"gobo": [],
		"prism": [],
		"strobe": [],
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
