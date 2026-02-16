extends RefCounted
class_name SceneRegistry

var _owner_root: Node
var _fixture_entries: Dictionary = {}

func configure(owner_root: Node) -> void:
	_owner_root = owner_root
	_debug_log("configure owner=%s" % [_describe_node(owner_root)])

func register_fixture(uuid: String, root_node: Node, anchors: Dictionary = {}) -> void:
	if uuid.is_empty():
		_debug_log("register_fixture skipped: empty uuid")
		return
	if root_node == null:
		_debug_log("register_fixture skipped: null root uuid=%s" % uuid)
		return

	var conflict: bool = _fixture_entries.has(uuid)
	if conflict:
		_debug_log("register_fixture conflict: replacing existing entry uuid=%s" % uuid)

	var normalized_anchors: Dictionary = _normalize_anchor_dictionary(anchors)
	var entry: Dictionary = {
		"path": root_node.get_path(),
		"weak": weakref(root_node),
		"anchors": normalized_anchors,
	}
	_fixture_entries[uuid] = entry

	if not root_node.tree_exited.is_connected(_on_fixture_tree_exited.bind(uuid)):
		root_node.tree_exited.connect(_on_fixture_tree_exited.bind(uuid), CONNECT_ONE_SHOT)

	_debug_log("register_fixture uuid=%s anchors=%d total=%d" % [uuid, normalized_anchors.size(), _fixture_entries.size()])

func unregister_fixture(uuid: String, reason: String = "") -> void:
	if not _fixture_entries.erase(uuid):
		return
	_debug_log("unregister_fixture uuid=%s reason=%s total=%d" % [uuid, reason, _fixture_entries.size()])

func clear(reason: String = "") -> void:
	if _fixture_entries.is_empty():
		return
	var previous_total: int = _fixture_entries.size()
	_fixture_entries.clear()
	_debug_log("clear reason=%s removed=%d total=0" % [reason, previous_total])

func get_fixture(uuid: String) -> Node:
	var entry: Dictionary = _fixture_entries.get(uuid, {})
	if entry.is_empty():
		return null

	var weak_ref: WeakRef = entry.get("weak")
	if weak_ref != null:
		var weak_node: Variant = weak_ref.get_ref()
		if weak_node is Node and is_instance_valid(weak_node):
			return weak_node

	var node_path: NodePath = entry.get("path", NodePath(""))
	if _owner_root == null or node_path.is_empty():
		return null
	return _owner_root.get_node_or_null(node_path)

func has_fixture(uuid: String) -> bool:
	return get_fixture(uuid) != null

func list_fixture_uuids() -> PackedStringArray:
	var uuids := PackedStringArray()
	for uuid in _fixture_entries.keys():
		var fixture_uuid: String = str(uuid)
		if fixture_uuid.is_empty():
			continue
		if get_fixture(fixture_uuid) == null:
			continue
		uuids.append(fixture_uuid)
	uuids.sort()
	return uuids

func get_anchor(uuid: String, anchor_name_or_id: Variant) -> Variant:
	var entry: Dictionary = _fixture_entries.get(uuid, {})
	if entry.is_empty():
		return null

	var anchors: Dictionary = entry.get("anchors", {})
	if anchors.is_empty():
		return null

	var key: String = str(anchor_name_or_id)
	if anchors.has(key):
		return _resolve_anchor_value(anchors.get(key))
	return null

func _normalize_anchor_dictionary(anchors: Dictionary) -> Dictionary:
	var normalized: Dictionary = {}
	for key in anchors.keys():
		var anchor_name: String = str(key)
		normalized[anchor_name] = _normalize_anchor_value(anchors.get(key))
	return normalized

func _normalize_anchor_value(anchor_value: Variant) -> Variant:
	if anchor_value is Node:
		var node: Node = anchor_value
		return node.get_path()
	if anchor_value is Array:
		var normalized_list: Array = []
		for entry in anchor_value:
			normalized_list.append(_normalize_anchor_value(entry))
		return normalized_list
	return anchor_value

func _resolve_anchor_value(anchor_value: Variant) -> Variant:
	if anchor_value is NodePath:
		if _owner_root == null:
			return null
		return _owner_root.get_node_or_null(anchor_value)
	if anchor_value is Array:
		var resolved_nodes: Array = []
		for entry in anchor_value:
			resolved_nodes.append(_resolve_anchor_value(entry))
		return resolved_nodes
	return anchor_value

func _on_fixture_tree_exited(uuid: String) -> void:
	unregister_fixture(uuid, "tree_exited")

func _debug_log(message: String) -> void:
	print("[PeravizSceneRegistry] %s" % message)

func _describe_node(node: Node) -> String:
	if node == null:
		return "<null>"
	return "%s@%s" % [node.name, node.get_path()]
