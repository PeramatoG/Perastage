extends RefCounted
class_name VolumetricBeamShapeProvider

func shape_mode() -> String:
	return "base"

func apply_shape(beam: MeshInstance3D, light: SpotLight3D, params: Dictionary) -> Dictionary:
	return {
		"gobo_projection_radius": 0.1,
		"beam_rotation_deg": 0.0,
		"mirror_x": true,
	}

func clear_cache() -> void:
	pass
