extends Camera3D

@export var orbit_sensitivity: float = 0.01
@export var pan_sensitivity: float = 0.002
@export var zoom_step_factor: float = 1.1
@export var min_distance: float = 0.5
@export var max_distance: float = 500.0
@export var smoothing_speed: float = 12.0

var target_position: Vector3 = Vector3.ZERO
var target_yaw: float = 0.0
var target_pitch: float = -0.3
var target_distance: float = 8.0

var current_yaw: float = 0.0
var current_pitch: float = -0.3
var current_distance: float = 8.0

var _orbiting: bool = false
var _panning: bool = false

func _ready() -> void:
	_initialize_from_transform()
	_set_mouse_mode(false)

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		_handle_mouse_button(event)
	elif event is InputEventMouseMotion:
		_handle_mouse_motion(event)

func _process(delta: float) -> void:
	var alpha: float = clamp(delta * smoothing_speed, 0.0, 1.0)
	current_yaw = lerp_angle(current_yaw, target_yaw, alpha)
	current_pitch = lerp(current_pitch, target_pitch, alpha)
	current_distance = lerp(current_distance, target_distance, alpha)
	_update_camera_transform()

func _initialize_from_transform() -> void:
	var offset: Vector3 = global_position - target_position
	var distance: float = offset.length()
	if distance < min_distance:
		distance = max(min_distance, 0.001)
		offset = Vector3(0.0, 0.0, distance)

	target_distance = clamp(distance, min_distance, max_distance)
	current_distance = target_distance

	target_yaw = atan2(offset.x, offset.z)
	current_yaw = target_yaw

	target_pitch = asin(clamp(offset.y / distance, -1.0, 1.0))
	target_pitch = clamp(target_pitch, deg_to_rad(-89.0), deg_to_rad(89.0))
	current_pitch = target_pitch

func _handle_mouse_button(event: InputEventMouseButton) -> void:
	if event.button_index == MOUSE_BUTTON_RIGHT:
		_orbiting = event.pressed
		_set_mouse_mode(_orbiting or _panning)
		get_viewport().set_input_as_handled()
		return

	if event.button_index == MOUSE_BUTTON_MIDDLE:
		_panning = event.pressed
		_set_mouse_mode(_orbiting or _panning)
		get_viewport().set_input_as_handled()
		return

	if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
		_apply_zoom(-1.0)
		get_viewport().set_input_as_handled()
		return

	if event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
		_apply_zoom(1.0)
		get_viewport().set_input_as_handled()

func _handle_mouse_motion(event: InputEventMouseMotion) -> void:
	if _orbiting:
		target_yaw -= event.relative.x * orbit_sensitivity
		target_pitch -= event.relative.y * orbit_sensitivity
		target_pitch = clamp(target_pitch, deg_to_rad(-89.0), deg_to_rad(89.0))
		get_viewport().set_input_as_handled()
		return

	if _panning:
		var distance_scale: float = max(current_distance, min_distance)
		var movement: Vector3 = (-global_transform.basis.x * event.relative.x + global_transform.basis.y * event.relative.y)
		target_position += movement * (pan_sensitivity * distance_scale)
		get_viewport().set_input_as_handled()

func _apply_zoom(steps: float) -> void:
	var adaptive_base: float = zoom_step_factor + 0.1 * clamp(target_distance / 200.0, 0.0, 1.0)
	var zoom_factor: float = pow(adaptive_base, steps)
	target_distance = clamp(target_distance * zoom_factor, min_distance, max_distance)

func _update_camera_transform() -> void:
	var cos_pitch: float = cos(current_pitch)
	var sin_pitch: float = sin(current_pitch)
	var sin_yaw: float = sin(current_yaw)
	var cos_yaw: float = cos(current_yaw)

	var orbit_offset := Vector3(
		current_distance * cos_pitch * sin_yaw,
		current_distance * sin_pitch,
		current_distance * cos_pitch * cos_yaw
	)

	global_position = target_position + orbit_offset
	look_at(target_position, Vector3.UP)

func _set_mouse_mode(active: bool) -> void:
	if active:
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
	else:
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
