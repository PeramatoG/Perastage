extends RefCounted
class_name GoboBeamMaterialCache

var _base_material: ShaderMaterial
var _material_cache: Dictionary = {}

func configure_base_material(base_material: ShaderMaterial) -> void:
	_base_material = base_material

func clear() -> void:
	_material_cache.clear()

func resolve_material(base_material: ShaderMaterial, gobo_tex: Texture2D, settings: Dictionary, enable_flag: bool) -> ShaderMaterial:
	if base_material == null:
		return null
	if _base_material == null or _base_material != base_material:
		configure_base_material(base_material)

	if not enable_flag or gobo_tex == null:
		_apply_gobo_uniforms(base_material, settings, false)
		base_material.set_shader_parameter("gobo_texture", null)
		return base_material

	var rid_key: int = int(gobo_tex.get_rid().get_id())
	if rid_key <= 0:
		_apply_gobo_uniforms(base_material, settings, false)
		base_material.set_shader_parameter("gobo_texture", null)
		return base_material

	if not _material_cache.has(rid_key):
		var cached_material: ShaderMaterial = base_material.duplicate() as ShaderMaterial
		cached_material.set_shader_parameter("gobo_texture", gobo_tex)
		_material_cache[rid_key] = cached_material

	var resolved_material: ShaderMaterial = _material_cache[rid_key] as ShaderMaterial
	if resolved_material == null:
		_apply_gobo_uniforms(base_material, settings, false)
		base_material.set_shader_parameter("gobo_texture", null)
		return base_material

	_apply_gobo_uniforms(resolved_material, settings, true)
	return resolved_material

func apply_settings_to_all_cached_materials(settings: Dictionary) -> void:
	if _base_material != null:
		_apply_gobo_uniforms(_base_material, settings, false)
	for material in _material_cache.values():
		if material is ShaderMaterial:
			_apply_gobo_uniforms(material, settings, true)

func _apply_gobo_uniforms(material: ShaderMaterial, settings: Dictionary, enabled: bool) -> void:
	if material == null:
		return
	material.set_shader_parameter("gobo_enabled", enabled)
	material.set_shader_parameter("gobo_strength", clamp(float(settings.get("beam_gobo_strength", 1.0)), 0.0, 1.0))
	material.set_shader_parameter("gobo_mip_bias", clamp(float(settings.get("beam_gobo_softness", 0.0)), 0.0, 6.0))
	material.set_shader_parameter("gobo_gamma", clamp(float(settings.get("beam_gobo_gamma", 1.0)), 0.25, 4.0))
	material.set_shader_parameter("gobo_rotation_rad", clamp(float(settings.get("beam_gobo_rotation", 0.0)), -6.283, 6.283))
