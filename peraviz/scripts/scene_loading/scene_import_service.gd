extends RefCounted
class_name SceneImportService

func clear_scene(scene_registry: SceneRegistry,
		proxies_root: Node3D,
		node_index: Dictionary,
		asset_cache: PeravizRuntimeAssetCache,
		fixture_emissive_cache: Dictionary,
		fixture_emitter_light_cache: Dictionary,
		fixture_emitter_photometrics: Dictionary,
		fixture_gobo_projector: FixtureGoboProjector,
		clear_selected_fixture: Callable,
		clear_debug_gizmos: Callable,
		set_has_loaded_bounds: Callable) -> void:
	scene_registry.clear("scene_reload")
	clear_selected_fixture.call("scene_clear")
	for child in proxies_root.get_children():
		child.queue_free()
	node_index.clear()
	asset_cache.clear()
	fixture_emissive_cache.clear()
	fixture_emitter_light_cache.clear()
	fixture_emitter_photometrics.clear()
	if fixture_gobo_projector != null:
		fixture_gobo_projector.clear_cache()
	set_has_loaded_bounds.call(false)
	clear_debug_gizmos.call()

func import_scene(path: String,
		loader: PeravizLoader,
		node_factory: NodeFactory,
		fixture_binding_service: FixtureBindingService,
		asset_cache: PeravizRuntimeAssetCache,
		proxies_root: Node3D,
		node_index: Dictionary,
		refresh_dmx_fixture_bindings: Callable,
		populate_fixture_list: Callable,
		sync_selection_state: Callable,
		rebuild_debug_gizmos: Callable,
		focus_loaded_scene: Callable,
		update_debug_legend: Callable,
		refresh_emitter_light_scalars: Callable,
		extract_emitter_photometrics: Callable,
		rebuild_loaded_bounds: Callable,
		set_has_loaded_bounds: Callable,
		debug_coords_enabled: bool,
		debug_asset_cache_enabled: bool,
		scene_registry: SceneRegistry) -> Dictionary:
	if path.is_empty():
		return {
			"ok": false,
			"error": "empty file path",
			"node_count": 0,
		}
	if not FileAccess.file_exists(path):
		return {
			"ok": false,
			"error": "file does not exist",
			"node_count": 0,
		}
	var native_path: String = ProjectSettings.globalize_path(path)
	var peraviz_debug_baseline: bool = bool(ProjectSettings.get_setting("peraviz_debug_baseline", false))
	var nodes: Array = loader.load_mvr(native_path, peraviz_debug_baseline, debug_coords_enabled)
	if nodes.is_empty():
		return {
			"ok": false,
			"error": "MVR returned no nodes",
			"node_count": 0,
		}
	print("[Peraviz] Loaded render nodes: ", nodes.size(), " baseline_debug=", peraviz_debug_baseline, " coords_debug=", debug_coords_enabled)
	set_has_loaded_bounds.call(false)

	node_factory.build_node_tree(nodes, proxies_root, node_index, rebuild_loaded_bounds, loader, asset_cache)
	fixture_binding_service.register_fixture_registry(nodes, node_index, scene_registry, extract_emitter_photometrics)
	refresh_dmx_fixture_bindings.call()
	populate_fixture_list.call()
	sync_selection_state.call("scene_reload")
	rebuild_debug_gizmos.call()
	focus_loaded_scene.call()
	if debug_asset_cache_enabled:
		_print_asset_cache_summary(asset_cache)
	update_debug_legend.call()
	refresh_emitter_light_scalars.call()
	return {
		"ok": true,
		"error": "",
		"node_count": nodes.size(),
	}

func _print_asset_cache_summary(asset_cache: PeravizRuntimeAssetCache) -> void:
	var cache_summary: Dictionary = asset_cache.debug_summary()
	var hit_by_kind: Dictionary = cache_summary.get("hits_by_kind", {})
	var miss_by_kind: Dictionary = cache_summary.get("misses_by_kind", {})
	print("[PeravizAssetCache] summary hits=", cache_summary.get("hits", 0),
		" misses=", cache_summary.get("misses", 0),
		" unique=", cache_summary.get("unique_resources", 0),
		" mesh(hit/miss)=", hit_by_kind.get("mesh", 0), "/", miss_by_kind.get("mesh", 0),
		" scene(hit/miss)=", hit_by_kind.get("scene", 0), "/", miss_by_kind.get("scene", 0),
		" material(hit/miss)=", hit_by_kind.get("material", 0), "/", miss_by_kind.get("material", 0))
