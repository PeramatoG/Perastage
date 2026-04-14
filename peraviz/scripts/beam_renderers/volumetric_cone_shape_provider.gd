extends VolumetricBeamShapeProvider
class_name VolumetricConeShapeProvider

const EMITTER_CONE_MAX_BASE_RADIUS_M: float = 10.0

func shape_mode() -> String:
	return "cone"

func apply_shape(beam: MeshInstance3D, _light: SpotLight3D, params: Dictionary) -> Dictionary:
	var beam_range: float = max(float(params.get("beam_range", 0.1)), 0.01)
	var beam_angle: float = max(float(params.get("beam_angle", 1.0)), 0.1)
	var lens_radius: float = max(float(params.get("lens_radius", 0.03)), 0.005)
	var half_angle_deg: float = beam_angle * 0.5
	var tan_half_angle: float = tan(deg_to_rad(half_angle_deg))
	var radius: float = tan_half_angle * beam_range
	var bottom_radius: float = clamp(radius, 0.03, EMITTER_CONE_MAX_BASE_RADIUS_M)
	var cone_mesh: CylinderMesh = beam.mesh as CylinderMesh
	if cone_mesh == null:
		cone_mesh = CylinderMesh.new()
		cone_mesh.radial_segments = 96
		cone_mesh.rings = 24
		cone_mesh.cap_top = false
		cone_mesh.cap_bottom = false
		beam.mesh = cone_mesh
	cone_mesh.top_radius = max(lens_radius, 0.003)
	cone_mesh.bottom_radius = bottom_radius
	cone_mesh.height = beam_range
	var lens_offset_m: float = max(float(params.get("lens_offset_m", params.get("near_offset", 0.0))), 0.0)
	var lens_shift_x: float = float(params.get("lens_shift_x", 0.0))
	var lens_shift_y: float = float(params.get("lens_shift_y", 0.0))
	beam.position = Vector3(lens_shift_x, lens_shift_y, -(beam_range * 0.5 + lens_offset_m))
	beam.scale = Vector3.ONE
	var gobo_rotation_deg: float = float(params.get("gobo_rotation_deg", 0.0))
	var alignment_rotation_deg: float = float(params.get("beam_gobo_alignment_rotation_deg", 0.0))
	var beam_rotation_deg: float = wrapf(gobo_rotation_deg + alignment_rotation_deg + 180.0, 0.0, 360.0)
	var mirror_x: bool = bool(params.get("beam_gobo_mirror_x", true))
	var mirror_z: bool = bool(params.get("beam_gobo_mirror_z", true))
	_apply_beam_axis_rotation(beam, beam_rotation_deg)
	return {
		"gobo_projection_radius": max(bottom_radius, 0.001),
		"beam_rotation_deg": beam_rotation_deg,
		"mirror_x": mirror_x,
		"mirror_z": mirror_z,
	}

func _apply_beam_axis_rotation(node: Node3D, beam_rotation_deg: float) -> void:
	if node == null:
		return
	node.rotation = Vector3(deg_to_rad(90.0), 0.0, 0.0)
	node.rotate_object_local(Vector3.UP, deg_to_rad(-beam_rotation_deg))
