extends Node3D

@export_file("*.mvr") var robin_megapointe_mvr: String
@export_file("*.mvr") var mac_axiom_hybrid_mvr: String
@export var fixture_spacing_m: float = 6.0
@export var viewer_scene: PackedScene = preload("res://test.tscn")

# Manual repro harness for winding/culling validation.
# It spawns two independent viewers side by side so both fixtures can be audited quickly:
# - Robin MegaPointe (GLB path, expected baseline)
# - MAC Axiom Hybrid (3DS path, used for inside-out regression checks)
func _ready() -> void:
	_spawn_viewer(robin_megapointe_mvr, Vector3(-fixture_spacing_m * 0.5, 0.0, 0.0), "RobinMegaPointe")
	_spawn_viewer(mac_axiom_hybrid_mvr, Vector3(fixture_spacing_m * 0.5, 0.0, 0.0), "MacAxiomHybrid")

func _spawn_viewer(mvr_path: String, offset: Vector3, debug_name: String) -> void:
	if viewer_scene == null:
		push_warning("[PeravizRepro] Missing viewer scene; cannot spawn " + debug_name)
		return

	if mvr_path.is_empty():
		push_warning("[PeravizRepro] Missing MVR path for " + debug_name)
		return

	var viewer_root := viewer_scene.instantiate()
	if viewer_root is not Node3D:
		push_warning("[PeravizRepro] Instanced viewer root is not Node3D for " + debug_name)
		return

	var viewer_node: Node3D = viewer_root
	viewer_node.name = "Viewer_" + debug_name
	add_child(viewer_node)
	viewer_node.position = offset

	if viewer_node.has_method("_on_file_selected"):
		viewer_node.call_deferred("_on_file_selected", mvr_path)
	else:
		push_warning("[PeravizRepro] Viewer script does not expose _on_file_selected for " + debug_name)
