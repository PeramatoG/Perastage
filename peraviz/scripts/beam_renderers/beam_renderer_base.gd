extends RefCounted

class_name BeamRendererBase

func configure(_view_camera: Camera3D, _settings: Dictionary) -> void:
	pass

func ensure_beam(_light: SpotLight3D) -> void:
	pass

func update_beam(_light: SpotLight3D, _params: Dictionary) -> void:
	pass

func cleanup_beam(_light: SpotLight3D) -> void:
	pass
