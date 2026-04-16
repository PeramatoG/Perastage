extends RefCounted
class_name UiController

var _load_button: Button
var _visual_settings_button: Button
var _picker: FileDialog
var _open_visual_settings_callback: Callable

func configure(load_button: Button,
		visual_settings_button: Button,
		picker: FileDialog,
		on_file_selected_callback: Callable,
		open_visual_settings_callback: Callable) -> void:
	_load_button = load_button
	_visual_settings_button = visual_settings_button
	_picker = picker
	_open_visual_settings_callback = open_visual_settings_callback

	if _load_button != null and not _load_button.pressed.is_connected(_on_load_pressed):
		_load_button.pressed.connect(_on_load_pressed)
	if _picker != null:
		_picker.access = FileDialog.ACCESS_FILESYSTEM
		if not _picker.file_selected.is_connected(on_file_selected_callback):
			_picker.file_selected.connect(on_file_selected_callback)
	if _visual_settings_button != null and not _visual_settings_button.pressed.is_connected(_on_visual_settings_pressed):
		_visual_settings_button.pressed.connect(_on_visual_settings_pressed)

func _on_load_pressed() -> void:
	if _picker == null:
		return
	_picker.popup_centered_ratio(0.7)

func _on_visual_settings_pressed() -> void:
	if not _open_visual_settings_callback.is_valid():
		return
	_open_visual_settings_callback.call()
