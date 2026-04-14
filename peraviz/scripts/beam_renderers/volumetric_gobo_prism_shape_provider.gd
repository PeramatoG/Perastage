extends VolumetricBeamShapeProvider
class_name VolumetricGoboPrismShapeProvider

const EMITTER_CONE_MAX_BASE_RADIUS_M: float = 10.0
const GOBO_TEXTURE_META_KEY: String = "peraviz_gobo_texture"
const DEFAULT_MIRROR_BEAM_SHAPE_X: bool = true
const DEFAULT_MIRROR_BEAM_SHAPE_Z: bool = true

var _mesh_builder: GoboPrismMeshBuilder = GoboPrismMeshBuilder.new()

func shape_mode() -> String:
	return "gobo_prism"

func apply_shape(beam: MeshInstance3D, light: SpotLight3D, params: Dictionary) -> Dictionary:
	var beam_range: float = max(float(params.get("beam_range", 0.1)), 0.01)
	var beam_angle: float = max(float(params.get("beam_angle", 1.0)), 0.1)
	var lens_radius: float = max(float(params.get("lens_radius", 0.03)), 0.005)
	var beam_half_angle_deg: float = beam_angle * 0.5
	var radius: float = tan(deg_to_rad(beam_half_angle_deg)) * beam_range
	var bottom_radius: float = clamp(radius, 0.03, EMITTER_CONE_MAX_BASE_RADIUS_M)
	var gobo_texture: Texture2D = null
	if light.has_meta(GOBO_TEXTURE_META_KEY):
		gobo_texture = light.get_meta(GOBO_TEXTURE_META_KEY) as Texture2D
	var gobo_scale: float = max(float(params.get("gobo_scale", 1.0)), 0.05)
	var alignment_rotation_deg: float = float(params.get("beam_gobo_alignment_rotation_deg", 0.0))
	# The light node already carries gobo wheel/index rotation via FixtureGoboProjector.
	# Keep only local alignment on the prism node to avoid double-rotating versus the footprint.
	var beam_rotation_deg: float = wrapf(alignment_rotation_deg + 180.0, 0.0, 360.0)
	var mirror_x: bool = bool(params.get("beam_gobo_mirror_x", DEFAULT_MIRROR_BEAM_SHAPE_X))
	var mirror_z: bool = bool(params.get("beam_gobo_mirror_z", DEFAULT_MIRROR_BEAM_SHAPE_Z))
	var prism_mesh: ArrayMesh = _mesh_builder.build_beam_mesh(gobo_texture, lens_radius, bottom_radius, beam_range, gobo_scale)
	if prism_mesh != null:
		beam.mesh = prism_mesh
	var lens_offset_m: float = max(float(params.get("lens_offset_m", params.get("near_offset", 0.0))), 0.0)
	var lens_shift_x: float = float(params.get("lens_shift_x", 0.0))
	var lens_shift_y: float = float(params.get("lens_shift_y", 0.0))
	beam.position = Vector3(lens_shift_x, lens_shift_y, -(beam_range * 0.5 + lens_offset_m))
	beam.scale = Vector3(-1.0 if mirror_x else 1.0, 1.0, -1.0 if mirror_z else 1.0)
	_apply_beam_axis_rotation(beam, beam_rotation_deg)
	return {
		"gobo_projection_radius": max(bottom_radius, 0.001),
		"beam_rotation_deg": beam_rotation_deg,
		"mirror_x": mirror_x,
		"mirror_z": mirror_z,
	}

func clear_cache() -> void:
	_mesh_builder.clear_cache()

func _apply_beam_axis_rotation(node: Node3D, beam_rotation_deg: float) -> void:
	if node == null:
		return
	node.rotation = Vector3(deg_to_rad(90.0), 0.0, 0.0)
	node.rotate_object_local(Vector3.UP, deg_to_rad(-beam_rotation_deg))
