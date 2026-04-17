extends RefCounted
class_name UiVisibilityPolicy

const MODULE_USER: StringName = &"User"
const MODULE_ADVANCED: StringName = &"Advanced"
const MODULE_DEBUG: StringName = &"Debug"

const SETTINGS_FILE_PATH: String = "user://ui_visibility_policy.cfg"
const SETTINGS_SECTION: String = "ui_visibility"
const SETTING_FORCE_DEBUG: String = "force_debug_controls"

static var _advanced_controls_enabled: bool = false
static var _cached_settings: Dictionary = {}

static func is_module_visible(module_name: StringName) -> bool:
	match module_name:
		MODULE_USER:
			return true
		MODULE_ADVANCED:
			return is_advanced_controls_enabled()
		MODULE_DEBUG:
			return OS.is_debug_build() or is_debug_controls_forced()
		_:
			return false

static func is_advanced_controls_enabled() -> bool:
	return _advanced_controls_enabled

static func set_advanced_controls_enabled(enabled: bool) -> void:
	_advanced_controls_enabled = enabled

static func is_debug_controls_forced() -> bool:
	return bool(_get_setting(SETTING_FORCE_DEBUG, false))

static func set_debug_controls_forced(enabled: bool) -> void:
	_set_setting(SETTING_FORCE_DEBUG, enabled)

static func _get_setting(key: String, default_value: Variant) -> Variant:
	_ensure_settings_loaded()
	return _cached_settings.get(key, default_value)

static func _set_setting(key: String, value: Variant) -> void:
	_ensure_settings_loaded()
	_cached_settings[key] = value
	_save_settings()

static func _ensure_settings_loaded() -> void:
	if not _cached_settings.is_empty():
		return
	var config := ConfigFile.new()
	if config.load(SETTINGS_FILE_PATH) != OK:
		_cached_settings = {}
		return
	_cached_settings[SETTING_FORCE_DEBUG] = bool(config.get_value(SETTINGS_SECTION, SETTING_FORCE_DEBUG, false))

static func _save_settings() -> void:
	var config := ConfigFile.new()
	for key in _cached_settings.keys():
		config.set_value(SETTINGS_SECTION, key, _cached_settings[key])
	var save_result: int = config.save(SETTINGS_FILE_PATH)
	if save_result != OK:
		push_warning("Failed to save UI visibility settings to %s (error=%d)" % [SETTINGS_FILE_PATH, save_result])
