extends Node
class_name AppShell

@export var side_panel_path: NodePath
@export var side_toggle_button_path: NodePath
@export var modules_container_path: NodePath

var _side_panel: Control
var _side_toggle_button: Button
var _modules_container: Control
var _is_side_panel_open: bool = true
var _active_section: StringName = &""
var _sections: Dictionary = {}

func _ready() -> void:
	_side_panel = get_node_or_null(side_panel_path) as Control
	_side_toggle_button = get_node_or_null(side_toggle_button_path) as Button
	_modules_container = get_node_or_null(modules_container_path) as Control
	if _side_toggle_button != null:
		_side_toggle_button.pressed.connect(toggle_side_panel)
	_refresh_side_panel_state()

func toggle_side_panel() -> void:
	set_side_panel_open(not _is_side_panel_open)

func set_side_panel_open(is_open: bool) -> void:
	_is_side_panel_open = is_open
	_refresh_side_panel_state()

func is_side_panel_open() -> bool:
	return _is_side_panel_open

func get_modules_container() -> Control:
	return _modules_container

func register_section(section_id: StringName, section_node: CanvasItem) -> void:
	_sections[section_id] = section_node
	if _active_section == &"":
		set_active_section(section_id)
	else:
		section_node.visible = (section_id == _active_section)

func set_active_section(section_id: StringName) -> void:
	_active_section = section_id
	for section_name in _sections.keys():
		var section_node: CanvasItem = _sections[section_name]
		if section_node != null:
			section_node.visible = (section_name == _active_section)

func get_active_section() -> StringName:
	return _active_section

func _refresh_side_panel_state() -> void:
	if _side_panel != null:
		_side_panel.visible = _is_side_panel_open
	if _side_toggle_button != null:
		_side_toggle_button.text = "☰" if not _is_side_panel_open else "×"
