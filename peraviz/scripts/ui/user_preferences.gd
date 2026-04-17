extends RefCounted
class_name UserPreferences

const SETTINGS_FILE_PATH: String = "user://user_preferences.cfg"
const SETTINGS_SECTION: String = "ui"

const KEY_SIDEBAR_OPEN: String = "sidebar_open"
const KEY_ADVANCED_MODE: String = "advanced_mode"
const KEY_LAST_SETTINGS_TAB: String = "last_settings_tab"
const KEY_MODULE_USER_VISIBLE: String = "module_user_visible"
const KEY_MODULE_ADVANCED_VISIBLE: String = "module_advanced_visible"
const KEY_MODULE_DEBUG_VISIBLE: String = "module_debug_visible"

const DEFAULT_SIDEBAR_OPEN: bool = false
const DEFAULT_ADVANCED_MODE: bool = false
const DEFAULT_LAST_SETTINGS_TAB: int = 0
const DEFAULT_MODULE_VISIBILITY := {
	"User": true,
	"Advanced": false,
	"Debug": false,
}

var sidebar_open: bool = DEFAULT_SIDEBAR_OPEN
var advanced_mode: bool = DEFAULT_ADVANCED_MODE
var last_settings_tab: int = DEFAULT_LAST_SETTINGS_TAB
var module_visibility: Dictionary = DEFAULT_MODULE_VISIBILITY.duplicate(true)

func load_from_disk() -> void:
	var config := ConfigFile.new()
	if config.load(SETTINGS_FILE_PATH) != OK:
		_apply_defaults()
		return

	sidebar_open = bool(config.get_value(SETTINGS_SECTION, KEY_SIDEBAR_OPEN, DEFAULT_SIDEBAR_OPEN))
	advanced_mode = bool(config.get_value(SETTINGS_SECTION, KEY_ADVANCED_MODE, DEFAULT_ADVANCED_MODE))
	last_settings_tab = max(0, int(config.get_value(SETTINGS_SECTION, KEY_LAST_SETTINGS_TAB, DEFAULT_LAST_SETTINGS_TAB)))

	module_visibility = DEFAULT_MODULE_VISIBILITY.duplicate(true)
	module_visibility["User"] = bool(config.get_value(SETTINGS_SECTION, KEY_MODULE_USER_VISIBLE, DEFAULT_MODULE_VISIBILITY["User"]))
	module_visibility["Advanced"] = bool(config.get_value(SETTINGS_SECTION, KEY_MODULE_ADVANCED_VISIBLE, DEFAULT_MODULE_VISIBILITY["Advanced"]))
	module_visibility["Debug"] = bool(config.get_value(SETTINGS_SECTION, KEY_MODULE_DEBUG_VISIBLE, DEFAULT_MODULE_VISIBILITY["Debug"]))

func save_to_disk() -> void:
	var config := ConfigFile.new()
	config.set_value(SETTINGS_SECTION, KEY_SIDEBAR_OPEN, sidebar_open)
	config.set_value(SETTINGS_SECTION, KEY_ADVANCED_MODE, advanced_mode)
	config.set_value(SETTINGS_SECTION, KEY_LAST_SETTINGS_TAB, max(0, last_settings_tab))
	config.set_value(SETTINGS_SECTION, KEY_MODULE_USER_VISIBLE, bool(module_visibility.get("User", DEFAULT_MODULE_VISIBILITY["User"])))
	config.set_value(SETTINGS_SECTION, KEY_MODULE_ADVANCED_VISIBLE, bool(module_visibility.get("Advanced", DEFAULT_MODULE_VISIBILITY["Advanced"])))
	config.set_value(SETTINGS_SECTION, KEY_MODULE_DEBUG_VISIBLE, bool(module_visibility.get("Debug", DEFAULT_MODULE_VISIBILITY["Debug"])))

	var save_result: int = config.save(SETTINGS_FILE_PATH)
	if save_result != OK:
		push_warning("Failed to save user preferences to %s (error=%d)" % [SETTINGS_FILE_PATH, save_result])

func get_module_visible(module_name: StringName, default_value: bool) -> bool:
	return bool(module_visibility.get(str(module_name), default_value))

func set_module_visible(module_name: StringName, visible: bool) -> void:
	module_visibility[str(module_name)] = visible

func _apply_defaults() -> void:
	sidebar_open = DEFAULT_SIDEBAR_OPEN
	advanced_mode = DEFAULT_ADVANCED_MODE
	last_settings_tab = DEFAULT_LAST_SETTINGS_TAB
	module_visibility = DEFAULT_MODULE_VISIBILITY.duplicate(true)
