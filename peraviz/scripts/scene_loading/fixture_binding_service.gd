extends RefCounted
class_name FixtureBindingService

func register_fixture_registry(nodes: Array, node_index: Dictionary, scene_registry: SceneRegistry, extract_emitter_photometrics: Callable) -> void:
	if nodes.is_empty():
		return

	var parent_lookup: Dictionary = {}
	var type_lookup: Dictionary = {}
	for item in nodes:
		if item is not Dictionary:
			continue
		var node_id: String = str(item.get("node_id", ""))
		if node_id.is_empty():
			continue
		parent_lookup[node_id] = str(item.get("parent_id", ""))
		type_lookup[node_id] = str(item.get("type", ""))

	var fixture_anchors: Dictionary = {}
	for node_id in type_lookup.keys():
		if str(type_lookup.get(node_id, "")) != "fixture":
			continue
		fixture_anchors[node_id] = {
			"axis": [],
			"emitters": [],
			"lens": [],
			"geometry_nodes": [],
		}

	for item in nodes:
		if item is not Dictionary:
			continue
		var node_id: String = str(item.get("node_id", ""))
		if node_id.is_empty():
			continue

		var fixture_uuid: String = resolve_fixture_uuid(node_id, parent_lookup, type_lookup, {})
		if fixture_uuid.is_empty() or not fixture_anchors.has(fixture_uuid):
			continue
		if node_id == fixture_uuid:
			continue

		var node: Node3D = node_index.get(node_id)
		if node == null:
			continue

		var anchors: Dictionary = fixture_anchors[fixture_uuid]
		if bool(item.get("is_axis", false)):
			var axis_nodes: Array = anchors.get("axis", [])
			axis_nodes.append(node)
			anchors["axis"] = axis_nodes
		if bool(item.get("is_emitter", false)):
			var emitter_nodes: Array = anchors.get("emitters", [])
			emitter_nodes.append(node)
			anchors["emitters"] = emitter_nodes
		if str(item.get("type", "")) == "fixture_geometry":
			var geometry_nodes: Array = anchors.get("geometry_nodes", [])
			geometry_nodes.append(node)
			anchors["geometry_nodes"] = geometry_nodes
			if bool(item.get("is_lens", false)):
				var lens_nodes: Array = anchors.get("lens", [])
				lens_nodes.append(node)
				anchors["lens"] = lens_nodes
			if bool(item.get("is_emitter", false)):
				var photometric_entries: Array = anchors.get("emitter_photometrics", [])
				photometric_entries.append(extract_emitter_photometrics.call(item))
				anchors["emitter_photometrics"] = photometric_entries

	for fixture_uuid in fixture_anchors.keys():
		var fixture_node: Node3D = node_index.get(fixture_uuid)
		if fixture_node == null:
			print("[PeravizSceneRegistry] register_fixture skipped: fixture node missing uuid=", fixture_uuid)
			continue
		var anchors: Dictionary = fixture_anchors[fixture_uuid]
		scene_registry.register_fixture(fixture_uuid, fixture_node, anchors)
		attach_fixture_pick_colliders(fixture_uuid, fixture_node)

func resolve_fixture_uuid(node_id: String, parent_lookup: Dictionary, type_lookup: Dictionary, cache: Dictionary) -> String:
	if cache.has(node_id):
		return str(cache[node_id])

	var current_id: String = node_id
	while not current_id.is_empty():
		if str(type_lookup.get(current_id, "")) == "fixture":
			cache[node_id] = current_id
			return current_id
		current_id = str(parent_lookup.get(current_id, ""))

	cache[node_id] = ""
	return ""

func resolve_fixture_uuid_from_node(node: Node) -> String:
	var current: Node = node
	while current != null:
		if current.has_meta("peraviz_fixture_uuid"):
			return str(current.get_meta("peraviz_fixture_uuid", ""))
		current = current.get_parent()
	return ""

func attach_fixture_pick_colliders(fixture_uuid: String, fixture_node: Node3D) -> void:
	if fixture_node == null:
		return
	fixture_node.set_meta("peraviz_fixture_uuid", fixture_uuid)
	for child in fixture_node.get_children():
		_attach_fixture_pick_colliders_recursive(fixture_uuid, child)

func _attach_fixture_pick_colliders_recursive(fixture_uuid: String, node: Node) -> void:
	if node is MeshInstance3D:
		_attach_pick_collider_to_mesh(node as MeshInstance3D, fixture_uuid)
	if node is Node:
		node.set_meta("peraviz_fixture_uuid", fixture_uuid)
	for child in node.get_children():
		_attach_fixture_pick_colliders_recursive(fixture_uuid, child)

func _attach_pick_collider_to_mesh(mesh_instance: MeshInstance3D, fixture_uuid: String) -> void:
	if mesh_instance == null or mesh_instance.mesh == null:
		return
	var mesh_bounds: AABB = mesh_instance.get_aabb()
	if mesh_bounds.size == Vector3.ZERO:
		return

	var static_body := StaticBody3D.new()
	static_body.name = "FixturePickBody"
	static_body.set_meta("peraviz_fixture_uuid", fixture_uuid)
	mesh_instance.add_child(static_body)

	var shape := BoxShape3D.new()
	shape.size = mesh_bounds.size
	var collision := CollisionShape3D.new()
	collision.shape = shape
	collision.position = mesh_bounds.get_center()
	static_body.add_child(collision)
