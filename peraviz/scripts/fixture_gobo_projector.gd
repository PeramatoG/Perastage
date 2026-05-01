extends RefCounted
class_name FixtureGoboProjector

const FAKE_GOBO_TEXTURE_SIZE: int = 1024
const GOBO_TEXTURE_META_KEY: String = "peraviz_gobo_texture"
const FALLBACK_GOBO_META_KEY: String = "peraviz_is_vector_fallback_gobo"
const GOBO_PLANE_META_KEY: String = "peraviz_gobo_plane"
const GOBO_SHADER_PATH: String = "res://scripts/shaders/gobo_alpha_projector.gdshader"
const GOBO_PLANE_LOCAL_Z: float = -0.043
const GOBO_PLANE_MESH_SIZE: Vector2 = Vector2(0.017, 0.017)
const GOBO_MIN_ANGLE_DEG: float = 4.0
const GOBO_MAX_ANGLE_DEG: float = 50.0
const GOBO_MIN_SCALE: float = 0.555
const GOBO_MAX_SCALE: float = 6.4
const GOBO_APERTURE_SCALE: float = 1.05
const GOBO_DEFAULT_SCALE: float = 1.0
const GOBO_DEFAULT_ROTATION_DEG: float = 0.0
const VECTOR_FALLBACK_GOBO_CACHE_KEY: String = "__vector_fallback_gobo"
const GOBO_VECTOR_POLYGONS_META_KEY: String = "peraviz_gobo_vector_polygons"
const GOBO_VECTOR_WIDTH_META_KEY: String = "peraviz_gobo_vector_width"
const GOBO_VECTOR_HEIGHT_META_KEY: String = "peraviz_gobo_vector_height"
const GOBO_WHEEL_SPIN_META_KEY: String = "peraviz_gobo_wheel_spin"
const GOBO_WHEEL_SHAKE_PHASE_META_KEY: String = "peraviz_gobo_wheel_shake_phase"
const GOBO_WHEEL_SHAKE_RANGE_META_KEY: String = "peraviz_gobo_wheel_shake_range"
const GOBO_LAST_UPDATE_MSEC_META_KEY: String = "peraviz_gobo_last_update_msec"
const GOBO_APPLIED_ROTATION_DEG_META_KEY: String = "peraviz_gobo_applied_rotation_deg"
const GOBO_APPLIED_SHAKE_TILT_DEG_META_KEY: String = "peraviz_gobo_applied_shake_tilt_deg"
const GOBO_WHEEL_MODE_META_KEY: String = "peraviz_gobo_wheel_mode"
const GOBO_APPLIED_STATE_META_KEY: String = "peraviz_gobo_applied_state"
const GOBO_APPLIED_STATE_KEY_META_KEY: String = "peraviz_gobo_applied_state_key"
const GOBO_DEBUG_OVERRIDE_STATE_META_KEY: String = "peraviz_gobo_debug_override_state"
const GOBO_INDEX_MAX_DEG: float = 360.0
const GOBO_ROTATION_DEBUG_SETTING_KEY: String = "peraviz_debug_gobo_rotation"
const GOBO_ROTATION_DEBUG_DEFAULT: bool = false
const GoboShakeLimitsScript = preload("res://scripts/gobo_shake_limits.gd")
const GOBO_DEFAULT_SHAKE_AMPLITUDE_DEG: float = GoboShakeLimitsScript.DEFAULT_DEBUG_SHAKE_AMPLITUDE_DEG
const GOBO_DEFAULT_SHAKE_FREQUENCY_HZ: float = GoboShakeLimitsScript.DEFAULT_DEBUG_SHAKE_FREQUENCY_HZ

const GOBO_DEBUG_COMPARISON_ROTATION_AND_SHAKE: int = 0
const GOBO_DEBUG_COMPARISON_ROTATION_ONLY: int = 1
const GOBO_DEBUG_COMPARISON_SHAKE_ONLY: int = 2

const GOBO_SHAKE_WAVE_TRIANGLE: int = 0
const GOBO_SHAKE_WAVE_SINE: int = 1
const GOBO_SHAKE_WAVE_SQUARE: int = 2

const GOBO_BEHAVIOR_FIXED: int = 0
const GOBO_BEHAVIOR_INDEX: int = 1
const GOBO_BEHAVIOR_ROTATION: int = 2
const GOBO_BEHAVIOR_SHAKE: int = 3

const DmxGoboRangeResolverScript = preload("res://scripts/dmx_gobo_range_resolver.gd")

var _texture_cache: Dictionary = {}
var _texture_composition_count: int = 0
var _parametric_update_count: int = 0

func clear_cache() -> void:
	_texture_cache.clear()

func get_debug_counters() -> Dictionary:
	return {
		"texture_compositions": _texture_composition_count,
		"parametric_updates": _parametric_update_count,
	}

func apply_gobo_projection(light: SpotLight3D, controls: Dictionary) -> bool:
	if light == null or not is_instance_valid(light):
		return false
	var previous_applied_state_key: String = str(light.get_meta(GOBO_APPLIED_STATE_KEY_META_KEY, ""))
	var previous_applied_state: Dictionary = light.get_meta(GOBO_APPLIED_STATE_META_KEY, {})
	if previous_applied_state is not Dictionary:
		previous_applied_state = {}
	var previous_meta_texture: Texture2D = null
	if light.has_meta(GOBO_TEXTURE_META_KEY):
		previous_meta_texture = light.get_meta(GOBO_TEXTURE_META_KEY) as Texture2D
	light.set_meta(FALLBACK_GOBO_META_KEY, false)
	var gobo_controls: Dictionary = _resolve_gobo_controls(controls)
	var runtime_bindings: Array = gobo_controls.get("gobo_runtime_bindings", [])
	var has_runtime_gobo: bool = bool(gobo_controls.get("has_gobo", false))
	if has_runtime_gobo and runtime_bindings.is_empty():
		var fallback_raw_8bit: int = _resolve_gobo_raw_8bit(gobo_controls)
		runtime_bindings = [{
			"raw_8bit": fallback_raw_8bit,
			"slot_index": _resolve_active_gobo_slot_index(gobo_controls, fallback_raw_8bit),
			"slots": gobo_controls.get("gobo_slots", []),
		}]

	var delta_sec: float = _resolve_elapsed_seconds(light, controls)
	var source_textures: Array[Texture2D] = []
	var source_texture_cache_keys: PackedStringArray = PackedStringArray()
	var wheel_slot_state: Dictionary = {}
	var wheel_mode_state: Dictionary = {}
	var global_rotation_deg: float = float(controls.get("gobo_rotation_deg", GOBO_DEFAULT_ROTATION_DEG))
	var projected_rotation_deg: float = global_rotation_deg
	var projected_shake_tilt_deg: float = 0.0
	var has_bound_wheel_rotation: bool = false

	if has_runtime_gobo:
		for wheel in runtime_bindings:
			if wheel is not Dictionary:
				continue
			var slot_index: int = int(wheel.get("slot_index", 0))
			if slot_index <= 0:
				continue
			var wheel_key: String = _resolve_wheel_cache_key(wheel)
			wheel_slot_state[wheel_key] = slot_index
			var wheel_controls := {
				"gobo_slots": wheel.get("slots", []),
			}
			var texture_entry: Dictionary = _resolve_gobo_texture_entry_for_slot(wheel_controls, slot_index)
			var gobo_texture: Texture2D = texture_entry.get("texture", null) as Texture2D
			if gobo_texture == null:
				continue
			source_texture_cache_keys.append(str(texture_entry.get("cache_key", "")))

			var wheel_motion: Dictionary = _resolve_wheel_rotation_deg(light, gobo_controls, wheel, global_rotation_deg, delta_sec)
			var wheel_rotation_deg: float = float(wheel_motion.get("rotation_deg", global_rotation_deg))
			var wheel_shake_tilt_deg: float = float(wheel_motion.get("shake_tilt_deg", 0.0))
			var behavior: int = int(wheel.get("behavior", GOBO_BEHAVIOR_FIXED))
			var supports_index: bool = bool(wheel.get("supports_index", false)) or behavior == GOBO_BEHAVIOR_INDEX
			var supports_rotation: bool = bool(wheel.get("supports_rotation", false)) or behavior == GOBO_BEHAVIOR_ROTATION or behavior == GOBO_BEHAVIOR_SHAKE
			wheel_mode_state[wheel_key] = _resolve_wheel_effect_mode(behavior, supports_index, supports_rotation)
			if _wheel_owns_rotation_control(wheel):
				projected_rotation_deg = wheel_rotation_deg
				projected_shake_tilt_deg = wheel_shake_tilt_deg
				has_bound_wheel_rotation = true
			elif not has_bound_wheel_rotation:
				projected_rotation_deg = wheel_rotation_deg
				projected_shake_tilt_deg = wheel_shake_tilt_deg
			source_textures.append(gobo_texture)

	var composed_texture_cache_key: String = _build_composed_gobo_cache_key(source_texture_cache_keys)
	var prefer_native_fog_projector: bool = bool(controls.get("prefer_native_fog_projector", true))
	var has_composed_texture: bool = not source_textures.is_empty()
	var mode: String = "fallback_vector"
	if has_runtime_gobo and has_composed_texture:
		mode = "runtime_slots"
	var topology_state: Dictionary = {
		"mode": mode,
		"prefer_native_fog_projector": prefer_native_fog_projector,
		"has_composed_texture": has_composed_texture,
		"texture_cache_key": composed_texture_cache_key,
		"wheel_slot_index_by_wheel": wheel_slot_state,
		"wheel_mode_by_wheel": wheel_mode_state,
	}
	var parametric_state: Dictionary = {
		"gobo_scale": float(controls.get("gobo_scale", GOBO_DEFAULT_SCALE)),
		"gobo_shake_tilt_deg": projected_shake_tilt_deg,
	}
	var next_applied_state: Dictionary = {
		"topology": topology_state,
		"parametric": parametric_state,
	}
	var next_applied_state_key: String = _build_applied_state_key(topology_state)
	var selection_or_composition_changed: bool = previous_applied_state_key != next_applied_state_key
	if not selection_or_composition_changed:
		_parametric_update_count += 1
		_apply_gobo_rotation_only(light, projected_rotation_deg, next_applied_state)
		return false

	# Expensive path: texture slot/composition changed.
	if source_textures.is_empty():
		_apply_vector_fallback_gobo(light, previous_meta_texture, gobo_controls)
	else:
		var projected_gobo: Texture2D = _compose_gobo_textures(source_textures, composed_texture_cache_key)
		_apply_gobo_visuals(light, projected_gobo, gobo_controls)
		light.set_meta(GOBO_TEXTURE_META_KEY, projected_gobo)
		light.set_meta(FALLBACK_GOBO_META_KEY, false)

	_apply_gobo_rotation_only(light, projected_rotation_deg, next_applied_state)
	var previous_topology_signature: String = _topology_signature_from_state(previous_applied_state)
	var next_topology_signature: String = _topology_signature_from_state(next_applied_state)
	return previous_topology_signature != next_topology_signature

func _resolve_elapsed_seconds(light: SpotLight3D, controls: Dictionary) -> float:
	if controls.has("frame_delta_sec"):
		return clamp(float(controls.get("frame_delta_sec", 0.0)), 0.0, 0.2)
	var now_msec: int = Time.get_ticks_msec()
	var delta_sec: float = 0.0
	if light.has_meta(GOBO_LAST_UPDATE_MSEC_META_KEY):
		var previous_msec: int = int(light.get_meta(GOBO_LAST_UPDATE_MSEC_META_KEY))
		if previous_msec > 0:
			delta_sec = max(float(now_msec - previous_msec) * 0.001, 0.0)
	light.set_meta(GOBO_LAST_UPDATE_MSEC_META_KEY, now_msec)
	return clamp(delta_sec, 0.0, 0.2)

func _resolve_wheel_rotation_deg(light: SpotLight3D, controls: Dictionary, wheel: Dictionary, global_rotation_deg: float, delta_sec: float) -> Dictionary:
	var wheel_key: String = _resolve_wheel_cache_key(wheel)
	var wheel_spin_state: Dictionary = light.get_meta(GOBO_WHEEL_SPIN_META_KEY, {})
	if wheel_spin_state is not Dictionary:
		wheel_spin_state = {}
	var shake_phase_state: Dictionary = light.get_meta(GOBO_WHEEL_SHAKE_PHASE_META_KEY, {})
	if shake_phase_state is not Dictionary:
		shake_phase_state = {}
	var shake_range_state: Dictionary = light.get_meta(GOBO_WHEEL_SHAKE_RANGE_META_KEY, {})
	if shake_range_state is not Dictionary:
		shake_range_state = {}

	var behavior: int = int(wheel.get("behavior", GOBO_BEHAVIOR_FIXED))
	var supports_index: bool = bool(wheel.get("supports_index", false)) or behavior == GOBO_BEHAVIOR_INDEX
	var supports_rotation: bool = bool(wheel.get("supports_rotation", false)) or behavior == GOBO_BEHAVIOR_ROTATION or behavior == GOBO_BEHAVIOR_SHAKE
	var effect_mode: int = _resolve_wheel_effect_mode(behavior, supports_index, supports_rotation)
	if _handle_wheel_mode_transition(light, wheel_key, effect_mode):
		shake_phase_state[wheel_key] = 0.0
		shake_range_state[wheel_key] = ""
		light.set_meta(GOBO_WHEEL_SHAKE_PHASE_META_KEY, shake_phase_state)
		light.set_meta(GOBO_WHEEL_SHAKE_RANGE_META_KEY, shake_range_state)

	var index_norm: float = float(wheel.get("index_norm", -1.0))
	if supports_index and index_norm < 0.0 and bool(controls.get("has_gobo_index", false)):
		index_norm = clamp(float(controls.get("gobo_index_norm", 0.0)), 0.0, 1.0)
	var base_rotation_deg: float = global_rotation_deg
	if index_norm >= 0.0:
		if bool(wheel.get("has_index_physical_limits", false)):
			var index_physical_min: float = float(wheel.get("index_physical_min", 0.0))
			var index_physical_max: float = float(wheel.get("index_physical_max", 0.0))
			base_rotation_deg = lerp(index_physical_min, index_physical_max, clamp(index_norm, 0.0, 1.0))
		else:
			base_rotation_deg = lerp(0.0, GOBO_INDEX_MAX_DEG, clamp(index_norm, 0.0, 1.0))

	var spin_angle_deg: float = float(wheel_spin_state.get(wheel_key, 0.0))
	var has_native_speed: bool = bool(wheel.get("has_rotation_physical_value", false))
	var native_speed_deg_per_sec: float = float(wheel.get("rotation_speed_deg_per_sec", wheel.get("rotation_physical", 0.0)))
	var native_is_stop: bool = bool(wheel.get("is_stop", false))
	var has_active_rotation_command: bool = supports_rotation and has_native_speed and not native_is_stop and absf(native_speed_deg_per_sec) > 0.0001
	var shake_offset_deg: float = 0.0
	var debug_override: Dictionary = _resolve_debug_rotation_override(controls)
	var previous_debug_state: Dictionary = light.get_meta(GOBO_DEBUG_OVERRIDE_STATE_META_KEY, {})
	if previous_debug_state is not Dictionary:
		previous_debug_state = {}
	var was_debug_override_enabled: bool = bool(previous_debug_state.get(wheel_key, false))
	var is_debug_override_enabled: bool = bool(debug_override.get("enabled", false))
	if was_debug_override_enabled and not is_debug_override_enabled:
		shake_phase_state[wheel_key] = 0.0
		shake_range_state[wheel_key] = ""
		light.set_meta(GOBO_WHEEL_SHAKE_PHASE_META_KEY, shake_phase_state)
		light.set_meta(GOBO_WHEEL_SHAKE_RANGE_META_KEY, shake_range_state)
	previous_debug_state[wheel_key] = is_debug_override_enabled
	light.set_meta(GOBO_DEBUG_OVERRIDE_STATE_META_KEY, previous_debug_state)
	var has_runtime_shake: bool = bool(wheel.get("supports_shake", false)) and bool(wheel.get("shake_active", false))
	var should_apply_shake_effect: bool = effect_mode == GOBO_BEHAVIOR_SHAKE or has_runtime_shake or bool(debug_override.get("shake_enabled", false))
	var applied_shake_amplitude_deg: float = 0.0
	var applied_shake_frequency_hz: float = 0.0
	var shake_source: String = "none"

	if index_norm >= 0.0 and not has_active_rotation_command:
		wheel_spin_state[wheel_key] = 0.0
		light.set_meta(GOBO_WHEEL_SPIN_META_KEY, wheel_spin_state)
		if not should_apply_shake_effect:
			shake_phase_state[wheel_key] = 0.0
			shake_range_state[wheel_key] = ""
			light.set_meta(GOBO_WHEEL_SHAKE_PHASE_META_KEY, shake_phase_state)
			light.set_meta(GOBO_WHEEL_SHAKE_RANGE_META_KEY, shake_range_state)
			return {"rotation_deg": base_rotation_deg, "shake_tilt_deg": shake_offset_deg}

	if supports_rotation and delta_sec > 0.0:
		if not has_native_speed:
			wheel_spin_state[wheel_key] = 0.0
			light.set_meta(GOBO_WHEEL_SPIN_META_KEY, wheel_spin_state)
			if not should_apply_shake_effect:
				shake_phase_state[wheel_key] = 0.0
				shake_range_state[wheel_key] = ""
				light.set_meta(GOBO_WHEEL_SHAKE_PHASE_META_KEY, shake_phase_state)
				light.set_meta(GOBO_WHEEL_SHAKE_RANGE_META_KEY, shake_range_state)
				return {"rotation_deg": base_rotation_deg, "shake_tilt_deg": shake_offset_deg}
		var speed_deg_per_sec: float = native_speed_deg_per_sec
		if debug_override.get("enabled", false):
			if not bool(debug_override.get("rotation_enabled", true)):
				speed_deg_per_sec = 0.0
			elif bool(debug_override.get("force_rotation_speed", false)):
				speed_deg_per_sec = float(debug_override.get("rotation_speed_deg_per_sec", native_speed_deg_per_sec))
		var is_stop: bool = bool(wheel.get("is_stop", false))
		if is_stop:
			spin_angle_deg = 0.0
		else:
			spin_angle_deg = wrapf(spin_angle_deg + (speed_deg_per_sec * delta_sec), -360000.0, 360000.0)
		wheel_spin_state[wheel_key] = spin_angle_deg
		light.set_meta(GOBO_WHEEL_SPIN_META_KEY, wheel_spin_state)

	if not supports_rotation:
		wheel_spin_state[wheel_key] = 0.0
		light.set_meta(GOBO_WHEEL_SPIN_META_KEY, wheel_spin_state)
		if not should_apply_shake_effect:
			shake_phase_state[wheel_key] = 0.0
			shake_range_state[wheel_key] = ""
			light.set_meta(GOBO_WHEEL_SHAKE_PHASE_META_KEY, shake_phase_state)
			light.set_meta(GOBO_WHEEL_SHAKE_RANGE_META_KEY, shake_range_state)
			return {"rotation_deg": base_rotation_deg, "shake_tilt_deg": shake_offset_deg}

	base_rotation_deg += spin_angle_deg
	if should_apply_shake_effect:
		var current_shake_range_signature: String = _resolve_shake_range_signature(wheel)
		var previous_shake_range_signature: String = str(shake_range_state.get(wheel_key, ""))
		var shake_phase: float = float(shake_phase_state.get(wheel_key, 0.0))
		if current_shake_range_signature != previous_shake_range_signature:
			shake_phase = 0.0
		var shake_amplitude_deg: float = GoboShakeLimitsScript.clamp_amplitude_deg(float(wheel.get("shake_amplitude_deg", controls.get("gobo_shake_amplitude_deg", GOBO_DEFAULT_SHAKE_AMPLITUDE_DEG))))
		var shake_frequency_hz: float = _resolve_shake_frequency_hz(native_speed_deg_per_sec, wheel, controls)
		var shake_waveform: int = GOBO_SHAKE_WAVE_TRIANGLE
		if bool(debug_override.get("enabled", false)):
			if not bool(debug_override.get("shake_enabled", true)):
				shake_amplitude_deg = 0.0
				shake_frequency_hz = 0.0
			else:
				shake_amplitude_deg = GoboShakeLimitsScript.clamp_amplitude_deg(float(debug_override.get("shake_amplitude_deg", shake_amplitude_deg)))
				shake_frequency_hz = GoboShakeLimitsScript.clamp_frequency_hz(float(debug_override.get("shake_frequency_hz", shake_frequency_hz)))
				shake_waveform = int(debug_override.get("shake_waveform", GOBO_SHAKE_WAVE_TRIANGLE))
		applied_shake_amplitude_deg = shake_amplitude_deg
		applied_shake_frequency_hz = shake_frequency_hz
		shake_source = "debug" if bool(debug_override.get("shake_enabled", false)) else ("select" if has_runtime_shake else "rotation")
		if shake_amplitude_deg > 0.0001 and shake_frequency_hz > 0.0001:
			shake_phase = wrapf(shake_phase + (shake_frequency_hz * max(delta_sec, 0.0)), 0.0, 1.0)
			shake_offset_deg = shake_amplitude_deg * _shake_wave_centered_zero(shake_phase, shake_waveform)
		else:
			shake_phase = 0.0
			shake_offset_deg = 0.0
		shake_phase_state[wheel_key] = shake_phase
		shake_range_state[wheel_key] = current_shake_range_signature
		light.set_meta(GOBO_WHEEL_SHAKE_PHASE_META_KEY, shake_phase_state)
		light.set_meta(GOBO_WHEEL_SHAKE_RANGE_META_KEY, shake_range_state)
	else:
		shake_phase_state[wheel_key] = 0.0
		shake_range_state[wheel_key] = ""
		light.set_meta(GOBO_WHEEL_SHAKE_PHASE_META_KEY, shake_phase_state)
		light.set_meta(GOBO_WHEEL_SHAKE_RANGE_META_KEY, shake_range_state)

	_log_rotation_debug_if_enabled(
		wheel,
		wheel_key,
		behavior,
		delta_sec,
		bool(debug_override.get("enabled", false)),
		shake_source,
		applied_shake_amplitude_deg,
		applied_shake_frequency_hz
	)
	return {"rotation_deg": base_rotation_deg, "shake_tilt_deg": shake_offset_deg}

func _resolve_debug_rotation_override(controls: Dictionary) -> Dictionary:
	var debug_override_enabled: bool = bool(controls.get("gobo_debug_override_enabled", false))
	if not OS.is_debug_build():
		return _disabled_debug_rotation_override(debug_override_enabled)
	if not debug_override_enabled:
		return _disabled_debug_rotation_override(debug_override_enabled)
	var comparison_mode: int = int(controls.get("gobo_debug_comparison_mode", GOBO_DEBUG_COMPARISON_ROTATION_AND_SHAKE))
	var rotation_enabled: bool = comparison_mode != GOBO_DEBUG_COMPARISON_SHAKE_ONLY
	var shake_enabled: bool = comparison_mode != GOBO_DEBUG_COMPARISON_ROTATION_ONLY and bool(controls.get("gobo_debug_shake_enabled", false))
	return {
		"enabled": true,
		"debug_enabled": true,
		"rotation_enabled": rotation_enabled,
		"shake_enabled": shake_enabled,
		"shake_amplitude_deg": GoboShakeLimitsScript.clamp_amplitude_deg(float(controls.get("gobo_debug_shake_amplitude_deg", GOBO_DEFAULT_SHAKE_AMPLITUDE_DEG))),
		"shake_frequency_hz": GoboShakeLimitsScript.clamp_frequency_hz(float(controls.get("gobo_debug_shake_frequency_hz", GOBO_DEFAULT_SHAKE_FREQUENCY_HZ))),
		"shake_waveform": int(clamp(int(controls.get("gobo_debug_shake_waveform", GOBO_SHAKE_WAVE_TRIANGLE)), GOBO_SHAKE_WAVE_TRIANGLE, GOBO_SHAKE_WAVE_SQUARE)),
	}

func _disabled_debug_rotation_override(debug_enabled: bool) -> Dictionary:
	return {
		"enabled": false,
		"debug_enabled": debug_enabled,
		"rotation_enabled": true,
		"shake_enabled": false,
		"shake_amplitude_deg": 0.0,
		"shake_frequency_hz": 0.0,
		"shake_waveform": GOBO_SHAKE_WAVE_TRIANGLE,
	}

func _log_rotation_debug_if_enabled(wheel: Dictionary, wheel_key: String, behavior: int, delta_sec: float, debug_enabled: bool, shake_source: String, final_shake_amplitude_deg: float, final_shake_frequency_hz: float) -> void:
	if not bool(ProjectSettings.get_setting(GOBO_ROTATION_DEBUG_SETTING_KEY, GOBO_ROTATION_DEBUG_DEFAULT)):
		return
	print("[PeravizGoboRotation] wheel_key=%s wheel_number=%d wheel_name=%s behavior=%d source=%s raw_coarse=%d raw_fine=%d raw_8bit=%d mode_window=%d-%d matched_range=%d-%d matched_physical=%.4f->%.4f stop=%s dir=%d speed_dps=%.4f dt=%.4f debug_enabled=%s shake_source=%s final_shake_amp=%.4f final_shake_freq=%.4f" % [
		wheel_key,
		int(wheel.get("wheel_number", 0)),
		str(wheel.get("wheel_name", "")),
		behavior,
		str(wheel.get("rotation_source_channel", "")),
		int(wheel.get("rotation_raw_coarse", -1)),
		int(wheel.get("rotation_raw_fine", -1)),
		int(wheel.get("rotation_raw_8bit", -1)),
		int(wheel.get("rotation_mode_window_from", -1)),
		int(wheel.get("rotation_mode_window_to", -1)),
		int(wheel.get("rotation_matched_range_start", -1)),
		int(wheel.get("rotation_matched_range_end", -1)),
		float(wheel.get("rotation_matched_physical_from", 0.0)),
		float(wheel.get("rotation_matched_physical_to", 0.0)),
		str(bool(wheel.get("rotation_is_stop_range", false))),
		int(wheel.get("rotation_direction_sign", 0)),
		float(wheel.get("rotation_speed_deg_per_sec", wheel.get("rotation_physical", 0.0))),
		delta_sec,
		str(debug_enabled),
		shake_source,
		final_shake_amplitude_deg,
		final_shake_frequency_hz,
	])


func _wheel_owns_rotation_control(wheel: Dictionary) -> bool:
	if wheel.is_empty():
		return false
	var behavior: int = int(wheel.get("behavior", GOBO_BEHAVIOR_FIXED))
	if behavior == GOBO_BEHAVIOR_INDEX or behavior == GOBO_BEHAVIOR_ROTATION or behavior == GOBO_BEHAVIOR_SHAKE:
		return true
	var index_norm: float = float(wheel.get("index_norm", -1.0))
	if index_norm >= 0.0:
		return true
	return false

func _resolve_wheel_effect_mode(behavior: int, supports_index: bool, supports_rotation: bool) -> int:
	if behavior == GOBO_BEHAVIOR_SHAKE:
		return GOBO_BEHAVIOR_SHAKE
	if behavior == GOBO_BEHAVIOR_ROTATION:
		return GOBO_BEHAVIOR_ROTATION
	if behavior == GOBO_BEHAVIOR_INDEX:
		return GOBO_BEHAVIOR_INDEX
	if supports_rotation:
		return GOBO_BEHAVIOR_ROTATION
	if supports_index:
		return GOBO_BEHAVIOR_INDEX
	return GOBO_BEHAVIOR_FIXED

func _handle_wheel_mode_transition(light: SpotLight3D, wheel_key: String, effect_mode: int) -> bool:
	var wheel_mode_state: Dictionary = light.get_meta(GOBO_WHEEL_MODE_META_KEY, {})
	if wheel_mode_state is not Dictionary:
		wheel_mode_state = {}
	var previous_mode: int = int(wheel_mode_state.get(wheel_key, GOBO_BEHAVIOR_FIXED))
	if previous_mode == effect_mode:
		return false
	wheel_mode_state[wheel_key] = effect_mode
	light.set_meta(GOBO_WHEEL_MODE_META_KEY, wheel_mode_state)
	if effect_mode == GOBO_BEHAVIOR_INDEX:
		var wheel_spin_state: Dictionary = light.get_meta(GOBO_WHEEL_SPIN_META_KEY, {})
		if wheel_spin_state is not Dictionary:
			wheel_spin_state = {}
		wheel_spin_state[wheel_key] = 0.0
		light.set_meta(GOBO_WHEEL_SPIN_META_KEY, wheel_spin_state)
	return true

func _triangle_wave_centered_zero(phase: float) -> float:
	var wrapped_phase: float = wrapf(phase, 0.0, 1.0)
	return 1.0 - (4.0 * absf(wrapped_phase - 0.5))

func _shake_wave_centered_zero(phase: float, waveform: int) -> float:
	match waveform:
		GOBO_SHAKE_WAVE_SINE:
			return sin(TAU * wrapf(phase, 0.0, 1.0))
		GOBO_SHAKE_WAVE_SQUARE:
			return -1.0 if wrapf(phase, 0.0, 1.0) < 0.5 else 1.0
		_:
			return _triangle_wave_centered_zero(phase)

func _resolve_shake_frequency_hz(native_speed_deg_per_sec: float, wheel: Dictionary, controls: Dictionary) -> float:
	var explicit_frequency_hz: float = float(wheel.get("shake_frequency_hz", controls.get("gobo_shake_frequency_hz", -1.0)))
	if explicit_frequency_hz >= 0.0:
		return explicit_frequency_hz
	var abs_speed_deg_per_sec: float = absf(native_speed_deg_per_sec)
	if abs_speed_deg_per_sec <= 0.0001:
		return 0.0
	return abs_speed_deg_per_sec / GOBO_INDEX_MAX_DEG

func _resolve_shake_range_signature(wheel: Dictionary) -> String:
	return "%d:%d:%d:%d:%d:%s" % [
		int(wheel.get("rotation_mode_window_from", -1)),
		int(wheel.get("rotation_mode_window_to", -1)),
		int(wheel.get("rotation_matched_range_start", -1)),
		int(wheel.get("rotation_matched_range_end", -1)),
		int(wheel.get("rotation_raw_8bit", -1)),
		str(bool(wheel.get("rotation_is_stop_range", false))),
	]

func _resolve_wheel_cache_key(wheel: Dictionary) -> String:
	var wheel_number: int = int(wheel.get("wheel_number", 0))
	if wheel_number > 0:
		return "wheel_%d" % wheel_number
	var wheel_name: String = str(wheel.get("wheel_name", ""))
	if not wheel_name.is_empty():
		return "name_%s" % wheel_name.to_lower()
	return "default"

func _apply_gobo_visuals(light: SpotLight3D, gobo_texture: Texture2D, controls: Dictionary = {}) -> void:
	if light == null or not is_instance_valid(light):
		return
	_set_light_projector_texture(light, gobo_texture)
	if gobo_texture == null:
		_remove_gobo_plane(light)
		return
	var prefer_native_fog_projector: bool = bool(controls.get("prefer_native_fog_projector", true))
	if prefer_native_fog_projector:
		_remove_gobo_plane(light)
		return
	var gobo_plane: MeshInstance3D = _ensure_gobo_plane(light)
	if gobo_plane == null:
		return
	if gobo_plane.material_override is ShaderMaterial:
		var gobo_material: ShaderMaterial = gobo_plane.material_override as ShaderMaterial
		gobo_material.set_shader_parameter("gobo_texture", gobo_texture)
	_update_gobo_plane_scale(light, gobo_plane)


func _apply_gobo_rotation_to_light(light: SpotLight3D, gobo_rotation_deg: float) -> void:
	if light == null or not is_instance_valid(light):
		return
	var updated_rotation: Vector3 = light.rotation_degrees
	updated_rotation.z = wrapf(gobo_rotation_deg, -180.0, 180.0)
	light.rotation_degrees = updated_rotation

func _apply_gobo_shake_tilt_to_light(light: SpotLight3D, shake_tilt_deg: float) -> void:
	if light == null or not is_instance_valid(light):
		return
	var previous_shake_tilt_deg: float = float(light.get_meta(GOBO_APPLIED_SHAKE_TILT_DEG_META_KEY, 0.0))
	var updated_rotation: Vector3 = light.rotation_degrees
	updated_rotation.x = updated_rotation.x - previous_shake_tilt_deg + shake_tilt_deg
	light.rotation_degrees = updated_rotation
	light.set_meta(GOBO_APPLIED_SHAKE_TILT_DEG_META_KEY, shake_tilt_deg)

func _set_light_projector_texture(light: SpotLight3D, texture: Texture2D) -> void:
	if light == null or not is_instance_valid(light):
		return
	# Godot custom branches may expose either `projector` or `light_projector`.
	if _has_property(light, "projector"):
		light.set("projector", texture)
	if _has_property(light, "light_projector"):
		light.set("light_projector", texture)

func _has_property(object: Object, property_name: String) -> bool:
	if object == null:
		return false
	for property_info in object.get_property_list():
		if str(property_info.get("name", "")) == property_name:
			return true
	return false

func _clear_gobo_visuals(light: SpotLight3D) -> void:
	if light == null or not is_instance_valid(light):
		return
	_set_light_projector_texture(light, null)
	_remove_gobo_plane(light)
	_apply_gobo_rotation_to_light(light, GOBO_DEFAULT_ROTATION_DEG)
	_apply_gobo_shake_tilt_to_light(light, 0.0)
	light.set_meta(GOBO_APPLIED_ROTATION_DEG_META_KEY, GOBO_DEFAULT_ROTATION_DEG)
	light.remove_meta(GOBO_APPLIED_STATE_META_KEY)
	light.remove_meta(GOBO_APPLIED_STATE_KEY_META_KEY)
	light.remove_meta(GOBO_WHEEL_SPIN_META_KEY)
	light.remove_meta(GOBO_WHEEL_MODE_META_KEY)
	light.remove_meta(GOBO_WHEEL_SHAKE_PHASE_META_KEY)
	light.remove_meta(GOBO_WHEEL_SHAKE_RANGE_META_KEY)
	light.remove_meta(GOBO_DEBUG_OVERRIDE_STATE_META_KEY)
	light.remove_meta(GOBO_APPLIED_SHAKE_TILT_DEG_META_KEY)

func _apply_gobo_rotation_only(light: SpotLight3D, projected_rotation_deg: float, applied_state: Dictionary) -> void:
	_apply_gobo_rotation_to_light(light, projected_rotation_deg)
	var parametric_state: Dictionary = applied_state.get("parametric", {})
	_apply_gobo_shake_tilt_to_light(light, float(parametric_state.get("gobo_shake_tilt_deg", 0.0)))
	light.set_meta(GOBO_APPLIED_ROTATION_DEG_META_KEY, projected_rotation_deg)
	light.set_meta(GOBO_APPLIED_STATE_META_KEY, applied_state)
	var topology_state: Dictionary = applied_state.get("topology", {})
	light.set_meta(GOBO_APPLIED_STATE_KEY_META_KEY, _build_applied_state_key(topology_state))

func _topology_signature_from_state(applied_state: Dictionary) -> String:
	if applied_state.is_empty():
		return ""
	var topology_state: Dictionary = applied_state.get("topology", {})
	if topology_state.is_empty():
		return ""
	return "%s|%s|%s" % [
		str(topology_state.get("mode", "")),
		str(bool(topology_state.get("prefer_native_fog_projector", true))),
		str(bool(topology_state.get("has_composed_texture", false))),
	]

func _build_applied_state_key(topology_state: Dictionary) -> String:
	var texture_key: String = str(topology_state.get("texture_cache_key", ""))
	var wheel_slot_key: String = JSON.stringify(topology_state.get("wheel_slot_index_by_wheel", {}))
	var wheel_mode_key: String = JSON.stringify(topology_state.get("wheel_mode_by_wheel", {}))
	return "%s|%s|%s|%s|%s" % [
		str(topology_state.get("mode", "")),
		str(bool(topology_state.get("prefer_native_fog_projector", true))),
		str(bool(topology_state.get("has_composed_texture", false))),
		texture_key,
		wheel_slot_key + "|" + wheel_mode_key,
	]

func _ensure_gobo_plane(light: SpotLight3D) -> MeshInstance3D:
	if light.has_meta(GOBO_PLANE_META_KEY):
		var existing_plane: MeshInstance3D = light.get_meta(GOBO_PLANE_META_KEY) as MeshInstance3D
		if existing_plane != null and is_instance_valid(existing_plane):
			return existing_plane

	var gobo_plane := MeshInstance3D.new()
	gobo_plane.name = "PeravizGoboPlane"
	gobo_plane.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_DOUBLE_SIDED
	var quad := QuadMesh.new()
	quad.size = GOBO_PLANE_MESH_SIZE
	gobo_plane.mesh = quad
	var gobo_material := ShaderMaterial.new()
	gobo_material.shader = load(GOBO_SHADER_PATH)
	gobo_plane.material_override = gobo_material
	gobo_plane.position = Vector3(0.0, 0.0, GOBO_PLANE_LOCAL_Z)
	light.add_child(gobo_plane)
	light.set_meta(GOBO_PLANE_META_KEY, gobo_plane)
	return gobo_plane

func _remove_gobo_plane(light: SpotLight3D) -> void:
	if not light.has_meta(GOBO_PLANE_META_KEY):
		return
	var gobo_plane: MeshInstance3D = light.get_meta(GOBO_PLANE_META_KEY) as MeshInstance3D
	if gobo_plane != null and is_instance_valid(gobo_plane):
		gobo_plane.queue_free()
	light.remove_meta(GOBO_PLANE_META_KEY)

func _update_gobo_plane_scale(light: SpotLight3D, gobo_plane: MeshInstance3D) -> void:
	if light == null or gobo_plane == null:
		return
	var half_spot_angle_deg: float = clamp(light.spot_angle, GOBO_MIN_ANGLE_DEG * 0.5, GOBO_MAX_ANGLE_DEG * 0.5)
	var local_distance: float = abs(GOBO_PLANE_LOCAL_Z)
	var target_plane_size: float = 2.0 * local_distance * tan(deg_to_rad(half_spot_angle_deg)) * GOBO_APERTURE_SCALE
	var base_size: float = max(GOBO_PLANE_MESH_SIZE.x, 0.0001)
	var physical_scale: float = target_plane_size / base_size
	var clamped_scale: float = clamp(physical_scale, GOBO_MIN_SCALE, GOBO_MAX_SCALE)
	gobo_plane.scale = Vector3.ONE * clamped_scale

func _resolve_gobo_raw_8bit(controls: Dictionary) -> int:
	var gobo_raw: int = int(round(clamp(float(controls.get("gobo_norm", 0.0)), 0.0, 1.0) * 255.0))
	if controls.has("gobo_raw_value") and controls.has("gobo_resolution_bits"):
		var raw_value: int = int(controls.get("gobo_raw_value", gobo_raw))
		var resolution_bits: int = int(controls.get("gobo_resolution_bits", 8))
		if resolution_bits > 8:
			var shift_bits: int = max(0, resolution_bits - 8)
			gobo_raw = raw_value >> shift_bits
		else:
			gobo_raw = raw_value
	return clampi(gobo_raw, 0, 255)

func _resolve_gobo_controls(controls: Dictionary) -> Dictionary:
	var merged_controls: Dictionary = controls.duplicate(true)
	var capabilities: Dictionary = controls.get("capabilities", {})
	if capabilities is Dictionary:
		var gobo_blocks: Array = capabilities.get("gobo", [])
		var first_gobo_block: Dictionary = {}
		var aggregated_runtime_bindings: Array = []
		var aggregated_slots: Array = []
		var has_any_gobo: bool = false
		for item in gobo_blocks:
			if item is Dictionary:
				var block: Dictionary = item.duplicate(true)
				if first_gobo_block.is_empty():
					first_gobo_block = block
				if bool(block.get("has_gobo", false)):
					has_any_gobo = true
				for runtime_binding in block.get("gobo_runtime_bindings", []):
					aggregated_runtime_bindings.append(runtime_binding)
				for slot_item in block.get("gobo_slots", []):
					aggregated_slots.append(slot_item)
		if not first_gobo_block.is_empty():
			merged_controls.merge(first_gobo_block, true)
		merged_controls["has_gobo"] = has_any_gobo or not aggregated_runtime_bindings.is_empty()
		merged_controls["gobo_runtime_bindings"] = aggregated_runtime_bindings
		merged_controls["gobo_slots"] = aggregated_slots
	if not merged_controls.has("gobo_runtime_bindings") or merged_controls.get("gobo_runtime_bindings", []) == null:
		merged_controls["gobo_runtime_bindings"] = []
	if not merged_controls.has("gobo_slots") or merged_controls.get("gobo_slots", []) == null:
		merged_controls["gobo_slots"] = []
	return merged_controls

func _resolve_active_gobo_slot_index(controls: Dictionary, gobo_raw_8bit: int) -> int:
	var gobo_ranges: Array = controls.get("gobo_ranges", [])
	if gobo_ranges.is_empty():
		return -1
	return int(DmxGoboRangeResolverScript.resolve_active_range(gobo_raw_8bit, gobo_ranges).get("slot_index", -1))

func _resolve_gobo_texture_entry_for_slot(controls: Dictionary, slot_index: int) -> Dictionary:
	var gobo_slots: Array = controls.get("gobo_slots", [])
	for item in gobo_slots:
		if item is not Dictionary:
			continue
		if int(item.get("slot_index", -1)) != slot_index:
			continue
		var image_path: String = str(item.get("image_path", ""))
		if image_path.is_empty():
			return {}
		if _texture_cache.has(image_path):
			return {
				"texture": _texture_cache[image_path] as Texture2D,
				"cache_key": image_path,
			}
		var image := Image.new()
		var load_error: Error = image.load(image_path)
		if load_error != OK:
			return {}
		if image.get_format() != Image.FORMAT_RGBA8:
			image.convert(Image.FORMAT_RGBA8)
		var texture: ImageTexture = ImageTexture.create_from_image(image)
		_apply_vector_metadata_from_slot(texture, item)
		_texture_cache[image_path] = texture
		return {
			"texture": texture,
			"cache_key": image_path,
		}
	return {}



func _apply_vector_fallback_gobo(light: SpotLight3D, previous_meta_texture: Texture2D, controls: Dictionary) -> bool:
	var fallback_texture: Texture2D = _resolve_vector_fallback_gobo_texture()
	if fallback_texture == null:
		_clear_gobo_visuals(light)
		light.set_meta(GOBO_TEXTURE_META_KEY, null)
		return previous_meta_texture != null
	var fallback_rotation_deg: float = float(controls.get("gobo_rotation_deg", GOBO_DEFAULT_ROTATION_DEG))
	_apply_gobo_visuals(light, fallback_texture, controls)
	_apply_gobo_rotation_to_light(light, fallback_rotation_deg)
	_apply_gobo_shake_tilt_to_light(light, 0.0)
	light.set_meta(GOBO_APPLIED_ROTATION_DEG_META_KEY, fallback_rotation_deg)
	light.set_meta(GOBO_TEXTURE_META_KEY, fallback_texture)
	return fallback_texture != previous_meta_texture

func _resolve_vector_fallback_gobo_texture() -> Texture2D:
	if _texture_cache.has(VECTOR_FALLBACK_GOBO_CACHE_KEY):
		return _texture_cache[VECTOR_FALLBACK_GOBO_CACHE_KEY] as Texture2D
	var image := Image.create(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE, false, Image.FORMAT_RGBA8)
	image.fill(Color(0.0, 0.0, 0.0, 1.0))
	var center: Vector2 = Vector2(float(FAKE_GOBO_TEXTURE_SIZE - 1), float(FAKE_GOBO_TEXTURE_SIZE - 1)) * 0.5
	var outer_radius: float = float(FAKE_GOBO_TEXTURE_SIZE) * 0.48
	for y in range(FAKE_GOBO_TEXTURE_SIZE):
		for x in range(FAKE_GOBO_TEXTURE_SIZE):
			var distance_to_center: float = Vector2(float(x), float(y)).distance_to(center)
			if distance_to_center <= outer_radius:
				image.set_pixel(x, y, Color(1.0, 1.0, 1.0, 1.0))
	var texture: ImageTexture = ImageTexture.create_from_image(image)
	_texture_cache[VECTOR_FALLBACK_GOBO_CACHE_KEY] = texture
	return texture

func _build_composed_gobo_cache_key(texture_cache_keys: PackedStringArray) -> String:
	if texture_cache_keys.is_empty():
		return ""
	return "__composed_gobo_" + "_".join(texture_cache_keys)

func _compose_gobo_textures(textures: Array[Texture2D], composed_cache_key: String = "") -> Texture2D:
	if textures.is_empty():
		return null
	if textures.size() == 1:
		return textures[0]

	var cache_key: String = composed_cache_key
	if cache_key.is_empty():
		var key_parts: PackedStringArray = PackedStringArray()
		for texture in textures:
			if texture != null:
				key_parts.append(str(texture.get_rid().get_id()))
		cache_key = "__composed_gobo_" + "_".join(key_parts)
	if _texture_cache.has(cache_key):
		return _texture_cache[cache_key] as Texture2D

	var composed: Image = Image.create(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE, false, Image.FORMAT_RGBA8)
	composed.fill(Color(1.0, 1.0, 1.0, 1.0))
	for texture in textures:
		if texture == null:
			continue
		var image: Image = texture.get_image()
		if image == null:
			continue
		if image.get_width() != FAKE_GOBO_TEXTURE_SIZE or image.get_height() != FAKE_GOBO_TEXTURE_SIZE:
			image.resize(FAKE_GOBO_TEXTURE_SIZE, FAKE_GOBO_TEXTURE_SIZE, Image.INTERPOLATE_LANCZOS)
		for y in range(FAKE_GOBO_TEXTURE_SIZE):
			for x in range(FAKE_GOBO_TEXTURE_SIZE):
				var dst: Color = composed.get_pixel(x, y)
				var src: Color = image.get_pixel(x, y)
				var src_luma: float = (src.r + src.g + src.b) / 3.0
				var src_mask: float = src_luma * src.a
				var out_luma: float = dst.r * src_mask
				composed.set_pixel(x, y, Color(out_luma, out_luma, out_luma, out_luma))

	var out_texture: ImageTexture = ImageTexture.create_from_image(composed)
	_texture_composition_count += 1
	if textures.size() == 1:
		_copy_vector_metadata(out_texture, textures[0])
	_texture_cache[cache_key] = out_texture
	return out_texture



func _apply_vector_metadata_from_slot(texture: Texture2D, slot: Dictionary) -> void:
	if texture == null:
		return
	if slot.has("vector_polygons"):
		texture.set_meta(GOBO_VECTOR_POLYGONS_META_KEY, slot.get("vector_polygons", []))
	if slot.has("vector_width"):
		texture.set_meta(GOBO_VECTOR_WIDTH_META_KEY, int(slot.get("vector_width", 0)))
	if slot.has("vector_height"):
		texture.set_meta(GOBO_VECTOR_HEIGHT_META_KEY, int(slot.get("vector_height", 0)))

func _copy_vector_metadata(target_texture: Texture2D, source_texture: Texture2D) -> void:
	if target_texture == null or source_texture == null:
		return
	if source_texture.has_meta(GOBO_VECTOR_POLYGONS_META_KEY):
		target_texture.set_meta(GOBO_VECTOR_POLYGONS_META_KEY, source_texture.get_meta(GOBO_VECTOR_POLYGONS_META_KEY))
	if source_texture.has_meta(GOBO_VECTOR_WIDTH_META_KEY):
		target_texture.set_meta(GOBO_VECTOR_WIDTH_META_KEY, source_texture.get_meta(GOBO_VECTOR_WIDTH_META_KEY))
	if source_texture.has_meta(GOBO_VECTOR_HEIGHT_META_KEY):
		target_texture.set_meta(GOBO_VECTOR_HEIGHT_META_KEY, source_texture.get_meta(GOBO_VECTOR_HEIGHT_META_KEY))
