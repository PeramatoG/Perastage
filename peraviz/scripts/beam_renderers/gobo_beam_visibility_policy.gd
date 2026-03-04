extends RefCounted
class_name GoboBeamVisibilityPolicy

const SHADOW_COOKIE_PROJECTION_MODE: int = 0
const PROJECTOR_COOKIE_PROJECTION_MODE: int = 1

const GOBO_OCCLUDER_DISTANCE_M: float = 0.043
const GOBO_PLANE_BASE_SIZE_M: float = 0.017
const GOBO_FOOTPRINT_CONE_FILL_RATIO: float = 1.0
const GOBO_COOKIE_OVERSCAN_RATIO: float = 1.12

enum BeamVisibilityMode {
	FOG_SHADOW,
	GEOMETRY_SHADER,
}

func resolve_visibility_mode(settings: Dictionary,
		prefer_native_shadow_cookie: bool,
		gobo_projection_mode: int) -> int:
	var mode_name: String = str(settings.get("gobo_beam_visibility_mode", "fog_shadow")).to_lower()
	if mode_name == "geometry_shader":
		return BeamVisibilityMode.GEOMETRY_SHADER

	if prefer_native_shadow_cookie:
		return BeamVisibilityMode.FOG_SHADOW

	return BeamVisibilityMode.FOG_SHADOW

func compute_occluder_plane_size_world(beam_angle: float, gobo_scale_ratio: float) -> float:
	var half_angle_rad: float = deg_to_rad(max(beam_angle, 0.1) * 0.5)
	var cone_radius_at_occluder: float = tan(half_angle_rad) * GOBO_OCCLUDER_DISTANCE_M
	var footprint_plane_size: float = max(cone_radius_at_occluder * 2.0, 0.001) * GOBO_FOOTPRINT_CONE_FILL_RATIO
	return max(footprint_plane_size * max(gobo_scale_ratio, 0.001) * GOBO_COOKIE_OVERSCAN_RATIO, 0.001)
