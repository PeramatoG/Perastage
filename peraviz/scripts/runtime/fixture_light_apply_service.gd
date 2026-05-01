extends RefCounted
class_name FixtureLightApplyService

var _fixture_emissive_cache: Dictionary = {}
var _fixture_emitter_light_cache: Dictionary = {}
var _fixture_emitter_last_state: Dictionary = {}
var _phase_metrics: Dictionary = {
	"dmx_decode": {"calls": 0, "total_usec": 0},
	"fixture_apply": {"calls": 0, "total_usec": 0},
	"beam_update": {"calls": 0, "total_usec": 0},
	"gobo_update": {"calls": 0, "total_usec": 0},
}

func apply_dmx_controls_to_fixture(loader: Node, fixture_uuid: String, controls: Dictionary) -> void:
	var phase_start: int = Time.get_ticks_usec()
	controls["fixture_uuid"] = fixture_uuid
	if not controls.has("frame_delta_sec"):
		controls["frame_delta_sec"] = loader.get_process_delta_time()
	else:
		controls["frame_delta_sec"] = max(float(controls.get("frame_delta_sec", 0.0)), 0.0)
	var pan_tilt_controls: Dictionary = resolve_pan_tilt_controls(controls)
	if bool(pan_tilt_controls.get("has_pan", false)) or bool(pan_tilt_controls.get("has_tilt", false)):
		var has_pan: bool = bool(pan_tilt_controls.get("has_pan", false))
		var has_tilt: bool = bool(pan_tilt_controls.get("has_tilt", false))
		var pan_min: float = float(loader.pan_min_input.value)
		var pan_max: float = float(loader.pan_max_input.value)
		var tilt_min: float = float(loader.tilt_min_input.value)
		var tilt_max: float = float(loader.tilt_max_input.value)
		var pan_degrees: float = lerp(pan_min, pan_max, clamp(float(pan_tilt_controls.get("pan_norm", 0.0)), 0.0, 1.0))
		var tilt_degrees: float = lerp(tilt_min, tilt_max, clamp(float(pan_tilt_controls.get("tilt_norm", 0.0)), 0.0, 1.0))
		loader._apply_pan_tilt_components_to_fixture(fixture_uuid, has_pan, pan_degrees, has_tilt, tilt_degrees)

	if has_lighting_controls(controls):
		var dimmer_percent: float = 100.0
		var dimmer_controls: Dictionary = resolve_dimmer_controls(controls)
		if bool(dimmer_controls.get("has_dimmer", false)):
			dimmer_percent = clamp(float(dimmer_controls.get("dimmer_norm", 0.0)), 0.0, 1.0) * 100.0
		apply_dimmer_feedback_to_fixture(loader, fixture_uuid, dimmer_percent, controls)
	_track_phase("fixture_apply", phase_start)

func resolve_capability_bucket(controls: Dictionary, capability_type: String) -> Array:
	var capabilities: Dictionary = controls.get("capabilities", {})
	if capabilities is Dictionary:
		var bucket: Array = capabilities.get(capability_type, [])
		if bucket is Array:
			return bucket
	return []

func resolve_first_capability_block(controls: Dictionary, capability_type: String) -> Dictionary:
	var bucket: Array = resolve_capability_bucket(controls, capability_type)
	for item in bucket:
		if item is Dictionary:
			return item
	return {}

func resolve_pan_tilt_controls(controls: Dictionary) -> Dictionary:
	var block: Dictionary = resolve_first_capability_block(controls, "pan_tilt")
	if not block.is_empty():
		return block
	return {
		"has_pan": bool(controls.get("has_pan", false)),
		"pan_norm": float(controls.get("pan_norm", 0.0)),
		"has_tilt": bool(controls.get("has_tilt", false)),
		"tilt_norm": float(controls.get("tilt_norm", 0.0)),
	}

func resolve_dimmer_controls(controls: Dictionary) -> Dictionary:
	var block: Dictionary = resolve_first_capability_block(controls, "dimmer")
	if not block.is_empty():
		return block
	return {
		"has_dimmer": bool(controls.get("has_dimmer", false)),
		"dimmer_norm": float(controls.get("dimmer_norm", 0.0)),
		"has_zoom": bool(controls.get("has_zoom", false)),
		"zoom_norm": float(controls.get("zoom_norm", 0.0)),
		"has_zoom_physical_limits": bool(controls.get("has_zoom_physical_limits", false)),
		"zoom_physical_min_degrees": float(controls.get("zoom_physical_min_degrees", -1.0)),
		"zoom_physical_max_degrees": float(controls.get("zoom_physical_max_degrees", -1.0)),
	}

func resolve_color_wheel_controls(controls: Dictionary) -> Dictionary:
	var block: Dictionary = resolve_first_capability_block(controls, "color_wheel")
	if not block.is_empty():
		return block
	return {
		"has_cyan": bool(controls.get("has_cyan", false)),
		"cyan_norm": float(controls.get("cyan_norm", 0.0)),
		"has_magenta": bool(controls.get("has_magenta", false)),
		"magenta_norm": float(controls.get("magenta_norm", 0.0)),
		"has_yellow": bool(controls.get("has_yellow", false)),
		"yellow_norm": float(controls.get("yellow_norm", 0.0)),
	}

func resolve_gobo_controls(controls: Dictionary) -> Dictionary:
	var capabilities: Dictionary = controls.get("capabilities", {})
	if capabilities is Dictionary:
		var gobo_blocks: Array = capabilities.get("gobo", [])
		if not gobo_blocks.is_empty():
			var aggregated: Dictionary = {}
			var aggregated_bindings: Array = []
			var aggregated_slots: Array = []
			var has_any_gobo: bool = false
			for item in gobo_blocks:
				if item is not Dictionary:
					continue
				var block: Dictionary = item as Dictionary
				if aggregated.is_empty():
					aggregated = block.duplicate(true)
				if bool(block.get("has_gobo", false)):
					has_any_gobo = true
				for runtime_binding in block.get("gobo_runtime_bindings", []):
					aggregated_bindings.append(runtime_binding)
				for slot_item in block.get("gobo_slots", []):
					aggregated_slots.append(slot_item)
			if not aggregated.is_empty():
				aggregated["has_gobo"] = has_any_gobo or not aggregated_bindings.is_empty()
				aggregated["gobo_runtime_bindings"] = aggregated_bindings
				aggregated["gobo_slots"] = aggregated_slots
				return aggregated
	return {
		"has_gobo": bool(controls.get("has_gobo", false)),
		"gobo_norm": float(controls.get("gobo_norm", 0.0)),
		"has_gobo_index": bool(controls.get("has_gobo_index", false)),
		"gobo_index_norm": float(controls.get("gobo_index_norm", 0.0)),
		"has_gobo_rotation": bool(controls.get("has_gobo_rotation", false)),
		"gobo_rotation_norm": float(controls.get("gobo_rotation_norm", 0.0)),
		"gobo_slots": controls.get("gobo_slots", []),
		"gobo_ranges": controls.get("gobo_ranges", []),
		"gobo_runtime_bindings": controls.get("gobo_runtime_bindings", []),
	}

func has_lighting_controls(controls: Dictionary) -> bool:
	var dimmer_controls: Dictionary = resolve_dimmer_controls(controls)
	if bool(dimmer_controls.get("has_dimmer", false)) or bool(dimmer_controls.get("has_zoom", false)):
		return true
	var color_controls: Dictionary = resolve_color_wheel_controls(controls)
	if bool(color_controls.get("has_cyan", false)) or bool(color_controls.get("has_magenta", false)) or bool(color_controls.get("has_yellow", false)):
		return true
	var gobo_controls: Dictionary = resolve_gobo_controls(controls)
	return bool(gobo_controls.get("has_gobo", false)) or bool(gobo_controls.get("has_gobo_index", false)) or bool(gobo_controls.get("has_gobo_rotation", false))

func apply_dimmer_feedback_to_fixture(loader: Node, fixture_uuid: String, dimmer: float, controls: Dictionary = {}) -> void:
	var geometry_nodes: Array = loader._to_node3d_array(loader._scene_registry.get_anchor(fixture_uuid, "geometry_nodes"))
	var emitter_nodes: Array = loader._to_node3d_array(loader._scene_registry.get_anchor(fixture_uuid, "emitters"))
	if geometry_nodes.is_empty() and emitter_nodes.is_empty():
		return
	var dimmer_percent: float = clamp(dimmer, 0.0, 100.0)
	var normalized_dimmer: float = dimmer_percent / 100.0
	var emitter_photometrics: Array = loader._get_fixture_emitter_photometrics(fixture_uuid)
	var beam_color: Color = loader._resolve_fixture_beam_color(emitter_photometrics, controls)
	var emissive_materials: Array = loader._collect_fixture_emissive_materials(fixture_uuid, geometry_nodes)
	var energy_multiplier: float = lerp(0.0, 4.0, normalized_dimmer)
	for material in emissive_materials:
		if material is BaseMaterial3D:
			material.emission_enabled = true
			material.emission = beam_color
			material.emission_energy_multiplier = energy_multiplier
	var emitter_lights: Array = loader._collect_fixture_emitter_lights(fixture_uuid, emitter_nodes)
	for index in range(emitter_lights.size()):
		var light: SpotLight3D = emitter_lights[index]
		if light == null or not is_instance_valid(light):
			continue
		var photometric: Dictionary = loader.DEFAULT_EMITTER_PHOTOMETRICS.duplicate(true)
		if index < emitter_photometrics.size() and emitter_photometrics[index] is Dictionary:
			photometric.merge(emitter_photometrics[index], true)
		loader._apply_emitter_light_state(light, photometric, normalized_dimmer, controls)

func record_beam_update() -> void:
	_track_phase("beam_update", Time.get_ticks_usec())

func record_gobo_update() -> void:
	_track_phase("gobo_update", Time.get_ticks_usec())

func get_phase_metrics() -> Dictionary:
	return _phase_metrics.duplicate(true)

func _track_phase(phase: String, phase_start_usec: int) -> void:
	var elapsed: int = max(Time.get_ticks_usec() - phase_start_usec, 0)
	if not _phase_metrics.has(phase):
		_phase_metrics[phase] = {"calls": 0, "total_usec": 0}
	var bucket: Dictionary = _phase_metrics.get(phase, {})
	bucket["calls"] = int(bucket.get("calls", 0)) + 1
	bucket["total_usec"] = int(bucket.get("total_usec", 0)) + elapsed
	_phase_metrics[phase] = bucket

func record_dmx_decode_elapsed(elapsed_usec: int) -> void:
	var start: int = Time.get_ticks_usec() - max(elapsed_usec, 0)
	_track_phase("dmx_decode", start)
