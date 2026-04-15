extends RefCounted
class_name DebugOverlayService

func ensure_debug_gizmo_root(owner: Node3D) -> Node3D:
	var existing: Variant = owner.get("_debug_gizmos_root")
	if existing != null and is_instance_valid(existing):
		return existing
	var root := Node3D.new()
	root.name = "DebugGizmos"
	root.top_level = true
	owner.add_child(root)
	owner.set("_debug_gizmos_root", root)
	return root

func clear_debug_gizmos(owner: Node3D) -> void:
	var root: Node3D = ensure_debug_gizmo_root(owner)
	for child in root.get_children():
		child.queue_free()

func rebuild_debug_gizmos(owner: Node3D, debug_enabled: bool, proxies_root: Node3D, node_index: Dictionary) -> void:
	clear_debug_gizmos(owner)
	if not debug_enabled:
		return

	_add_debug_gizmo_for_target(owner, proxies_root, "scene_root", Color(1.0, 0.25, 1.0), 0.75)
	for node in node_index.values():
		if node is not Node3D:
			continue
		var node3d: Node3D = node
		var metadata_type: String = str(node3d.get_meta("peraviz_type", ""))
		var is_axis: bool = bool(node3d.get_meta("peraviz_is_axis", false))
		var is_emitter: bool = bool(node3d.get_meta("peraviz_is_emitter", false))

		if metadata_type in ["fixture", "truss", "support", "scene_object"]:
			_add_debug_gizmo_for_target(owner, node3d, "mvr_instance_root", Color(1.0, 0.9, 0.2), 0.55)
		if is_axis:
			_add_debug_gizmo_for_target(owner, node3d, "gdtf_axis", Color(0.0, 1.0, 1.0), 0.35)
		if is_emitter:
			_add_debug_gizmo_for_target(owner, node3d, "emitter", Color(1.0, 0.55, 0.15), 0.30)

func print_debug_legend(debug_enabled: bool) -> void:
	if not debug_enabled:
		print("[PeravizCoordDebugLegend] disabled (press C to enable)")
		return
	print("[PeravizCoordDebugLegend] X=Red Y=Green Z=Blue scene_root_origin=Magenta mvr_instance_root_origin=Yellow gdtf_axis_origin=Cyan emitter_origin=Orange beam_expected_local=-Y")

func _safe_target_global_position(target: Node3D) -> Vector3:
	if target == null:
		return Vector3.ZERO
	var origin: Vector3 = target.global_position
	if not is_finite(origin.x) or not is_finite(origin.y) or not is_finite(origin.z):
		print("[PeravizCoordDebug] event=invalid_target_origin_fallback node=", target.name, " origin=", origin)
		return Vector3.ZERO
	return origin

func _add_debug_gizmo_for_target(owner: Node3D, target: Node3D, kind: String, origin_color: Color, length: float) -> void:
	if target == null:
		return
	var root: Node3D = ensure_debug_gizmo_root(owner)
	var holder := Node3D.new()
	holder.name = "Gizmo_%s" % kind
	root.add_child(holder)
	holder.global_transform = Transform3D(target.global_basis, _safe_target_global_position(target))
	holder.add_child(_create_axes_gizmo_node(origin_color, length))
	if kind == "emitter":
		holder.add_child(_create_emitter_direction_gizmo(length))

func _create_emitter_direction_gizmo(length: float) -> MeshInstance3D:
	var immediate := ImmediateMesh.new()
	var beam_material := ORMMaterial3D.new()
	beam_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	beam_material.albedo_color = Color(1.0, 0.55, 0.15)
	beam_material.emission_enabled = true
	beam_material.emission = Color(1.0, 0.55, 0.15)
	beam_material.emission_energy_multiplier = 2.0
	immediate.surface_begin(Mesh.PRIMITIVE_LINES, beam_material)
	immediate.surface_add_vertex(Vector3.ZERO)
	immediate.surface_add_vertex(Vector3.DOWN * (length * 1.8))
	immediate.surface_end()

	var beam := MeshInstance3D.new()
	beam.mesh = immediate
	beam.material_override = beam_material
	return beam

func _create_axes_gizmo_node(origin_color: Color, length: float) -> Node3D:
	var immediate := ImmediateMesh.new()
	var line_material := ORMMaterial3D.new()
	line_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	line_material.vertex_color_use_as_albedo = true
	immediate.surface_begin(Mesh.PRIMITIVE_LINES, line_material)
	immediate.surface_set_color(Color(1, 0, 0))
	immediate.surface_add_vertex(Vector3.ZERO)
	immediate.surface_add_vertex(Vector3.RIGHT * length)
	immediate.surface_set_color(Color(0, 1, 0))
	immediate.surface_add_vertex(Vector3.ZERO)
	immediate.surface_add_vertex(Vector3.UP * length)
	immediate.surface_set_color(Color(0, 0.4, 1))
	immediate.surface_add_vertex(Vector3.ZERO)
	immediate.surface_add_vertex(Vector3.BACK * length)
	immediate.surface_end()

	var axes_instance := MeshInstance3D.new()
	axes_instance.mesh = immediate
	axes_instance.material_override = line_material

	var marker := SphereMesh.new()
	marker.radius = max(length * 0.08, 0.01)
	marker.height = marker.radius * 2.0
	var marker_material := StandardMaterial3D.new()
	marker_material.albedo_color = origin_color
	marker_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	var marker_instance := MeshInstance3D.new()
	marker_instance.mesh = marker
	marker_instance.material_override = marker_material

	var container := Node3D.new()
	container.add_child(axes_instance)
	container.add_child(marker_instance)
	return container
