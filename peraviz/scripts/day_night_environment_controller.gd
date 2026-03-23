@tool
class_name DayNightEnvironmentController
extends Node3D

enum EnvironmentPreset {
	DAWN,
	DAY,
	DUSK,
	NIGHT,
	BLACKOUT_NIGHT,
}

const CYCLE_KEYFRAMES := [
	{"time": 0.00, "preset": EnvironmentPreset.BLACKOUT_NIGHT},
	{"time": 0.18, "preset": EnvironmentPreset.DAWN},
	{"time": 0.40, "preset": EnvironmentPreset.DAY},
	{"time": 0.68, "preset": EnvironmentPreset.DUSK},
	{"time": 0.86, "preset": EnvironmentPreset.NIGHT},
	{"time": 1.00, "preset": EnvironmentPreset.BLACKOUT_NIGHT},
]

@export var enabled: bool = true:
	set(value):
		enabled = value
		_apply_environment_state()

@export var use_continuous_cycle: bool = false:
	set(value):
		use_continuous_cycle = value
		_apply_environment_state()

@export var current_preset: EnvironmentPreset = EnvironmentPreset.DAY:
	set(value):
		current_preset = value
		_apply_environment_state()

@export_range(0.0, 1.0, 0.001) var time_of_day: float = 0.40:
	set(value):
		time_of_day = clampf(value, 0.0, 1.0)
		if use_continuous_cycle:
			_apply_environment_state()

@export var auto_advance: bool = false
@export_range(0.0, 1.0, 0.0001) var cycle_speed: float = 0.02

@export_group("Light")
@export var day_light_intensity: float = 0.70
@export var dusk_light_intensity: float = 0.18
@export var night_light_intensity: float = 0.02
@export var moon_light_intensity: float = 0.012

@export_group("Ambient")
@export var ambient_energy_day: float = 0.08
@export var ambient_energy_night: float = 0.008

@export_group("Sky")
@export_range(0.0, 1.0, 0.01) var horizon_warmth: float = 0.60
@export_range(0.0, 2.0, 0.01) var horizon_intensity: float = 1.0

@export_group("Behavior")
@export var allow_blackout_night: bool = true
@export var force_editor_preview: bool = true
@export_node_path("WorldEnvironment") var world_environment_path: NodePath = NodePath("../WorldEnvironment")
@export_node_path("DirectionalLight3D") var directional_light_path: NodePath = NodePath("../DirectionalLight3D")

var _world_environment: WorldEnvironment
var _directional_light: DirectionalLight3D
var _runtime_environment: Environment
var _runtime_sky: Sky
var _runtime_sky_material: ProceduralSkyMaterial


func _ready() -> void:
	_resolve_nodes()
	_prepare_environment_resources()
	_apply_environment_state()


func _process(delta: float) -> void:
	if not enabled:
		return
	if not _can_run_runtime_updates():
		return
	if use_continuous_cycle and auto_advance:
		time_of_day = wrapf(time_of_day + delta * cycle_speed, 0.0, 1.0)
		_apply_environment_state()


func apply_now() -> void:
	_resolve_nodes()
	_prepare_environment_resources()
	_apply_environment_state()


func _can_run_runtime_updates() -> bool:
	if Engine.is_editor_hint():
		return force_editor_preview
	return true


func _resolve_nodes() -> void:
	_world_environment = get_node_or_null(world_environment_path) as WorldEnvironment
	_directional_light = get_node_or_null(directional_light_path) as DirectionalLight3D


func _prepare_environment_resources() -> void:
	if _world_environment == null:
		return
	if _world_environment.environment == null:
		_world_environment.environment = Environment.new()
	_runtime_environment = _world_environment.environment
	if _runtime_environment.sky == null:
		_runtime_sky = Sky.new()
		_runtime_environment.sky = _runtime_sky
	else:
		_runtime_sky = _runtime_environment.sky
	if _runtime_sky.sky_material == null or not (_runtime_sky.sky_material is ProceduralSkyMaterial):
		_runtime_sky_material = ProceduralSkyMaterial.new()
		_runtime_sky.sky_material = _runtime_sky_material
	else:
		_runtime_sky_material = _runtime_sky.sky_material as ProceduralSkyMaterial


func _apply_environment_state() -> void:
	if _world_environment == null or _runtime_environment == null or _runtime_sky_material == null:
		return

	if not enabled:
		_apply_disabled_fallback()
		return

	var target_state: Dictionary = _compute_target_state()
	_apply_environment_dict(target_state)


func _apply_disabled_fallback() -> void:
	_runtime_environment.background_mode = Environment.BG_COLOR
	_runtime_environment.background_color = Color(0.1294, 0.1373, 0.1569, 1.0)
	_runtime_environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	_runtime_environment.ambient_light_color = Color(1.0, 1.0, 1.0, 1.0)
	_runtime_environment.ambient_light_energy = ambient_energy_day
	if _environment_has_property(_runtime_environment, "ambient_light_sky_contribution"):
		_runtime_environment.ambient_light_sky_contribution = 0.0
	if _directional_light != null:
		_directional_light.visible = false
		_directional_light.light_energy = 0.0


func _compute_target_state() -> Dictionary:
	if use_continuous_cycle:
		return _state_from_time(time_of_day)
	return _state_for_preset(current_preset)


func _state_from_time(normalized_time: float) -> Dictionary:
	var t := clampf(normalized_time, 0.0, 1.0)
	for index in range(CYCLE_KEYFRAMES.size() - 1):
		var current: Dictionary = CYCLE_KEYFRAMES[index]
		var next: Dictionary = CYCLE_KEYFRAMES[index + 1]
		var start_time: float = float(current["time"])
		var end_time: float = float(next["time"])
		if t >= start_time and t <= end_time:
			var span: float = max(end_time - start_time, 0.0001)
			var local_t: float = clampf((t - start_time) / span, 0.0, 1.0)
			return _lerp_states(_state_for_preset(int(current["preset"])), _state_for_preset(int(next["preset"])), local_t)
	return _state_for_preset(EnvironmentPreset.BLACKOUT_NIGHT)


func _state_for_preset(preset: EnvironmentPreset) -> Dictionary:
	match preset:
		EnvironmentPreset.DAWN:
			return {
				"sky_top_color": Color(0.12, 0.18, 0.30, 1.0),
				"sky_horizon_color": _warm_horizon_color(0.52, 0.40),
				"ground_bottom_color": Color(0.03, 0.04, 0.05, 1.0),
				"sky_energy": 0.38,
				"ambient_energy": lerpf(ambient_energy_night, ambient_energy_day, 0.35),
				"directional_energy": dusk_light_intensity,
				"directional_color": Color(1.00, 0.83, 0.69, 1.0),
				"sun_elevation_deg": 8.0,
				"moon_mode": false,
			}
		EnvironmentPreset.DAY:
			return {
				"sky_top_color": Color(0.30, 0.52, 0.83, 1.0),
				"sky_horizon_color": Color(0.70, 0.82, 0.98, 1.0),
				"ground_bottom_color": Color(0.09, 0.10, 0.13, 1.0),
				"sky_energy": 1.0,
				"ambient_energy": ambient_energy_day,
				"directional_energy": day_light_intensity,
				"directional_color": Color(1.0, 0.98, 0.95, 1.0),
				"sun_elevation_deg": 52.0,
				"moon_mode": false,
			}
		EnvironmentPreset.DUSK:
			return {
				"sky_top_color": Color(0.12, 0.16, 0.32, 1.0),
				"sky_horizon_color": _warm_horizon_color(0.60, 0.62),
				"ground_bottom_color": Color(0.03, 0.03, 0.05, 1.0),
				"sky_energy": 0.32,
				"ambient_energy": lerpf(ambient_energy_day, ambient_energy_night, 0.55),
				"directional_energy": dusk_light_intensity,
				"directional_color": Color(1.0, 0.76, 0.62, 1.0),
				"sun_elevation_deg": 4.0,
				"moon_mode": false,
			}
		EnvironmentPreset.NIGHT:
			return {
				"sky_top_color": Color(0.01, 0.02, 0.05, 1.0),
				"sky_horizon_color": Color(0.03, 0.04, 0.06, 1.0),
				"ground_bottom_color": Color(0.0, 0.0, 0.0, 1.0),
				"sky_energy": 0.06,
				"ambient_energy": ambient_energy_night,
				"directional_energy": moon_light_intensity,
				"directional_color": Color(0.70, 0.76, 0.95, 1.0),
				"sun_elevation_deg": -42.0,
				"moon_mode": true,
			}
		_:
			var blackout_enabled: bool = allow_blackout_night
			return {
				"sky_top_color": Color.BLACK,
				"sky_horizon_color": Color.BLACK,
				"ground_bottom_color": Color.BLACK,
				"sky_energy": 0.0 if blackout_enabled else 0.03,
				"ambient_energy": 0.0 if blackout_enabled else ambient_energy_night * 0.5,
				"directional_energy": 0.0 if blackout_enabled else night_light_intensity,
				"directional_color": Color(0.55, 0.60, 0.82, 1.0),
				"sun_elevation_deg": -68.0,
				"moon_mode": true,
			}


func _lerp_states(start: Dictionary, target: Dictionary, t: float) -> Dictionary:
	var result := {}
	for key in [
		"sky_top_color",
		"sky_horizon_color",
		"ground_bottom_color",
		"sky_energy",
		"ambient_energy",
		"directional_energy",
		"directional_color",
		"sun_elevation_deg",
	]:
		if start[key] is Color:
			result[key] = (start[key] as Color).lerp(target[key] as Color, t)
		else:
			result[key] = lerpf(float(start[key]), float(target[key]), t)
	result["moon_mode"] = bool(start["moon_mode"]) if t < 0.5 else bool(target["moon_mode"])
	return result


func _apply_environment_dict(state: Dictionary) -> void:
	_runtime_environment.background_mode = Environment.BG_SKY
	_runtime_environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	_runtime_environment.ambient_light_color = Color(1.0, 1.0, 1.0, 1.0)
	_runtime_environment.ambient_light_energy = max(float(state["ambient_energy"]), 0.0)
	if _environment_has_property(_runtime_environment, "ambient_light_sky_contribution"):
		_runtime_environment.ambient_light_sky_contribution = 0.0

	_runtime_sky_material.sky_top_color = state["sky_top_color"]
	_runtime_sky_material.sky_horizon_color = state["sky_horizon_color"]
	_runtime_sky_material.ground_bottom_color = state["ground_bottom_color"]
	_runtime_sky_material.ground_horizon_color = Color(0.02, 0.02, 0.02, 1.0)
	_runtime_sky_material.sky_curve = 0.18
	_runtime_sky_material.ground_curve = 0.20
	_runtime_sky_material.sun_curve = 0.015
	_runtime_sky_material.use_debanding = true
	_runtime_sky_material.energy_multiplier = max(float(state["sky_energy"]), 0.0)

	if _directional_light != null:
		var directional_energy: float = max(float(state["directional_energy"]), 0.0)
		_directional_light.visible = directional_energy > 0.0005
		_directional_light.light_energy = directional_energy
		_directional_light.light_color = state["directional_color"]
		_directional_light.rotation_degrees = Vector3(-float(state["sun_elevation_deg"]), -45.0, 0.0)


func _warm_horizon_color(base_intensity: float, dusk_factor: float) -> Color:
	var warm := Color(1.0, 0.56, 0.31, 1.0)
	var cool := Color(0.40, 0.52, 0.78, 1.0)
	var blend: Color = cool.lerp(warm, clampf(horizon_warmth + dusk_factor * 0.2, 0.0, 1.0))
	return Color(
		clampf(blend.r * base_intensity * horizon_intensity, 0.0, 1.0),
		clampf(blend.g * base_intensity * horizon_intensity, 0.0, 1.0),
		clampf(blend.b * base_intensity * horizon_intensity, 0.0, 1.0),
		1.0
	)


func _environment_has_property(environment: Environment, property_name: String) -> bool:
	if environment == null:
		return false
	for entry in environment.get_property_list():
		if str(entry.get("name", "")) == property_name:
			return true
	return false
