extends RefCounted
class_name VolumetricBeamShapeProvider

func shape_mode() -> String:
	return "base"

func apply_shape(_beam: MeshInstance3D, _light: SpotLight3D, _params: Dictionary) -> Dictionary:
	return {
		"gobo_projection_radius": 0.1,
		"beam_rotation_deg": 0.0,
		"mirror_x": true,
		"mirror_z": false,
	}

func clear_cache() -> void:
	pass
