extends Camera3D

@export var orbit_sensitivity: float = 0.01
@export var pan_sensitivity: float = 0.01
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

enum DragMode {
	NONE,
	ORBIT,
	PAN,
}

var _left_down: bool = false
var _right_down: bool = false
var _middle_down: bool = false
var _drag_mode: DragMode = DragMode.NONE

func focus_on_aabb(bounds: AABB) -> void:
	if bounds.size.length_squared() <= 0.0:
		return

	target_position = bounds.get_center()

	var radius: float = max(bounds.size.length() * 0.5, min_distance)
	var half_fov: float = deg_to_rad(fov) * 0.5
	var fit_distance: float = radius / max(tan(half_fov), 0.01)
	target_distance = clamp(fit_distance * 1.2, min_distance, max_distance)

func _ready() -> void:
	_initialize_from_transform()
	_set_mouse_mode(false)

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		_handle_mouse_button(event)
	elif event is InputEventMouseMotion:
		_handle_mouse_motion(event)
	elif event is InputEventKey:
		_handle_key_input(event)

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
	if event.button_index == MOUSE_BUTTON_LEFT:
		_left_down = event.pressed
		_update_drag_mode(event.shift_pressed)
		_set_mouse_mode(_is_dragging())
		if event.pressed:
			get_viewport().set_input_as_handled()
		return

	if event.button_index == MOUSE_BUTTON_RIGHT:
		_right_down = event.pressed
		_update_drag_mode(event.shift_pressed)
		_set_mouse_mode(_is_dragging())
		get_viewport().set_input_as_handled()
		return

	if event.button_index == MOUSE_BUTTON_MIDDLE:
		_middle_down = event.pressed
		_update_drag_mode(event.shift_pressed)
		_set_mouse_mode(_is_dragging())
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
	_update_drag_mode(Input.is_key_pressed(KEY_SHIFT))

	if _drag_mode == DragMode.ORBIT:
		target_yaw -= event.relative.x * orbit_sensitivity
		target_pitch -= event.relative.y * orbit_sensitivity
		target_pitch = clamp(target_pitch, deg_to_rad(-89.0), deg_to_rad(89.0))
		get_viewport().set_input_as_handled()
		return

	if _drag_mode == DragMode.PAN:
		_apply_pan(-event.relative.x * pan_sensitivity, event.relative.y * pan_sensitivity)
		get_viewport().set_input_as_handled()

func _apply_zoom(steps: float) -> void:
	var adaptive_base: float = zoom_step_factor + 0.1 * clamp(target_distance / 200.0, 0.0, 1.0)
	var zoom_factor: float = pow(adaptive_base, steps)
	var new_distance: float = target_distance * zoom_factor

	if new_distance < min_distance:
		var forward: Vector3 = _forward_direction(target_yaw, target_pitch)
		var overshoot: float = min_distance - new_distance
		target_position += forward * overshoot
		target_distance = min_distance
	else:
		target_distance = clamp(new_distance, min_distance, max_distance)

func _apply_pan(delta_x: float, delta_y: float) -> void:
	var right: Vector3 = Vector3(cos(target_yaw), 0.0, -sin(target_yaw))
	target_position += right * delta_x
	target_position += Vector3.UP * delta_y

func _forward_direction(yaw: float, pitch: float) -> Vector3:
	var cos_pitch: float = cos(pitch)
	return Vector3(
		-cos_pitch * sin(yaw),
		-sin(pitch),
		-cos_pitch * cos(yaw)
	)

func _handle_key_input(event: InputEventKey) -> void:
	if not event.pressed or event.echo:
		return

	if event.keycode == KEY_LEFT:
		if event.shift_pressed:
			_apply_pan(-0.1, 0.0)
		elif event.alt_pressed:
			_apply_zoom(-1.0)
		else:
			target_yaw -= deg_to_rad(5.0)
		get_viewport().set_input_as_handled()
		return

	if event.keycode == KEY_RIGHT:
		if event.shift_pressed:
			_apply_pan(0.1, 0.0)
		elif event.alt_pressed:
			_apply_zoom(1.0)
		else:
			target_yaw += deg_to_rad(5.0)
		get_viewport().set_input_as_handled()
		return

	if event.keycode == KEY_UP:
		if event.shift_pressed:
			_apply_pan(0.0, 0.1)
		elif event.alt_pressed:
			_apply_zoom(-1.0)
		else:
			target_pitch = clamp(target_pitch + deg_to_rad(5.0), deg_to_rad(-89.0), deg_to_rad(89.0))
		get_viewport().set_input_as_handled()
		return

	if event.keycode == KEY_DOWN:
		if event.shift_pressed:
			_apply_pan(0.0, -0.1)
		elif event.alt_pressed:
			_apply_zoom(1.0)
		else:
			target_pitch = clamp(target_pitch - deg_to_rad(5.0), deg_to_rad(-89.0), deg_to_rad(89.0))
		get_viewport().set_input_as_handled()
		return

	if event.keycode == KEY_KP_1:
		target_yaw = 0.0
		target_pitch = 0.0
		get_viewport().set_input_as_handled()
		return

	if event.keycode == KEY_KP_3:
		target_yaw = deg_to_rad(90.0)
		target_pitch = 0.0
		get_viewport().set_input_as_handled()
		return

	if event.keycode == KEY_KP_7:
		target_yaw = 0.0
		target_pitch = deg_to_rad(89.0)
		get_viewport().set_input_as_handled()
		return

	if event.keycode == KEY_KP_5:
		target_position = Vector3.ZERO
		target_yaw = deg_to_rad(45.0)
		target_pitch = deg_to_rad(30.0)
		target_distance = 15.0
		get_viewport().set_input_as_handled()

func _update_drag_mode(shift_down: bool) -> void:
	if not _is_dragging():
		_drag_mode = DragMode.NONE
		return

	if shift_down or _middle_down:
		_drag_mode = DragMode.PAN
	else:
		_drag_mode = DragMode.ORBIT

func _is_dragging() -> bool:
	return _left_down or _right_down or _middle_down

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
