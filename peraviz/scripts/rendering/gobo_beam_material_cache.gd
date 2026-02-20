extends RefCounted

class_name GoboBeamMaterialCache

var _materials_by_rid: Dictionary = {}

func clear() -> void:
	_materials_by_rid.clear()

func get_material(base_material: ShaderMaterial, gobo_tex: Texture2D, settings: Dictionary, enable_flag: bool) -> ShaderMaterial:
	if base_material == null:
		return null

	_apply_settings(base_material, settings)
	if not enable_flag or gobo_tex == null:
		base_material.set_shader_parameter("gobo_enabled", false)
		base_material.set_shader_parameter("gobo_texture", null)
		return base_material

	var key: int = int(gobo_tex.get_rid().get_id())
	if key <= 0:
		base_material.set_shader_parameter("gobo_enabled", false)
		base_material.set_shader_parameter("gobo_texture", null)
		return base_material

	if not _materials_by_rid.has(key):
		var cached_material: ShaderMaterial = base_material.duplicate() as ShaderMaterial
		if cached_material == null:
			return base_material
		cached_material.set_shader_parameter("gobo_texture", gobo_tex)
		cached_material.set_shader_parameter("gobo_enabled", true)
		_apply_settings(cached_material, settings)
		_materials_by_rid[key] = cached_material

	var material: ShaderMaterial = _materials_by_rid.get(key, base_material)
	if material != null:
		material.set_shader_parameter("gobo_enabled", true)
		material.set_shader_parameter("gobo_texture", gobo_tex)
		_apply_settings(material, settings)
	return material

func apply_settings_to_all_cached_materials(settings: Dictionary) -> void:
	for material in _materials_by_rid.values():
		if material is ShaderMaterial:
			_apply_settings(material, settings)

func _apply_settings(material: ShaderMaterial, settings: Dictionary) -> void:
	if material == null:
		return
	material.set_shader_parameter("gobo_strength", clamp(float(settings.get("beam_gobo_strength", 1.0)), 0.0, 1.0))
	material.set_shader_parameter("gobo_mip_bias", clamp(float(settings.get("beam_gobo_softness", 0.0)), 0.0, 6.0))
	material.set_shader_parameter("gobo_gamma", clamp(float(settings.get("beam_gobo_gamma", 1.0)), 0.25, 4.0))
	material.set_shader_parameter("gobo_rotation_rad", clamp(float(settings.get("beam_gobo_rotation", 0.0)), -TAU, TAU))
