extends RefCounted
class_name DmxFixtureRuntime

const MAX_UNBOUND_PREVIEW: int = 8

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

func apply_dmx(receiver, apply_dimmer_callback: Callable) -> void:
	if receiver == null or not receiver.is_running():
		return
	if apply_dimmer_callback.is_null():
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
		var channel_index: int = int(binding.get("dmx_channel_index_0", -1))
		if channel_index < 0 or channel_index >= 512:
			continue
		var frame: PackedByteArray = universe_frames.get(universe_id, PackedByteArray())
		if channel_index >= frame.size():
			continue
		var normalized_value: float = float(frame[channel_index]) / 255.0
		apply_dimmer_callback.call(fixture_uuid, normalized_value * 100.0)

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
