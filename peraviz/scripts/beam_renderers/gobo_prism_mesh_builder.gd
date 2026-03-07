extends RefCounted
class_name GoboPrismMeshBuilder

const VECTORIZATION_MAX_SIZE: int = 192
const VECTORIZATION_ALPHA_THRESHOLD: float = 0.5
const VECTORIZATION_EPSILON: float = 1.6
const MAX_TOTAL_VECTOR_POINTS: int = 280
const MIN_POLYGON_AREA: float = 0.00004
const FALLBACK_SEGMENTS: int = 36
const BINARY_LUMA_THRESHOLD: float = 0.5
const OUTER_BORDER_PIXELS: int = 1
const APERTURE_BORDER_RATIO: float = 0.985
const POINT_REDUCTION_EPSILON_START: float = 0.001
const POINT_REDUCTION_EPSILON_MULTIPLIER: float = 1.35
const POINT_REDUCTION_EPSILON_MAX: float = 0.25

var _mesh_cache: Dictionary = {}

func clear_cache() -> void:
	_mesh_cache.clear()

func build_beam_mesh(gobo_texture: Texture2D, near_radius: float, far_radius: float, beam_height: float, gobo_scale: float, gobo_rotation_deg: float) -> ArrayMesh:
	var shape_key: String = _shape_cache_key(gobo_texture, gobo_scale, gobo_rotation_deg)
	var geometry_key: String = "%s_%.4f_%.4f_%.4f" % [shape_key, near_radius, far_radius, beam_height]
	if _mesh_cache.has(geometry_key):
		return _mesh_cache[geometry_key] as ArrayMesh

	var polygons: Array[PackedVector2Array] = _vectorize_gobo(gobo_texture, gobo_scale, gobo_rotation_deg)
	if polygons.is_empty():
		polygons = [_build_fallback_circle()]

	var mesh: ArrayMesh = _build_extruded_mesh(polygons, max(near_radius, 0.001), max(far_radius, 0.001), max(beam_height, 0.001))
	_mesh_cache[geometry_key] = mesh
	return mesh

func _shape_cache_key(gobo_texture: Texture2D, gobo_scale: float, gobo_rotation_deg: float) -> String:
	if gobo_texture == null:
		return "__fallback_shape"
	return "__shape_%d_%.3f_%.3f" % [gobo_texture.get_rid().get_id(), gobo_scale, wrapf(gobo_rotation_deg, 0.0, 360.0)]

func _vectorize_gobo(gobo_texture: Texture2D, gobo_scale: float, gobo_rotation_deg: float) -> Array[PackedVector2Array]:
	if gobo_texture == null:
		return []
	var image: Image = gobo_texture.get_image()
	if image == null:
		return []
	if image.get_format() != Image.FORMAT_RGBA8:
		image.convert(Image.FORMAT_RGBA8)
	var longest_size: int = max(image.get_width(), image.get_height())
	if longest_size > VECTORIZATION_MAX_SIZE:
		var target_width: int = max(8, int(round(float(image.get_width()) * float(VECTORIZATION_MAX_SIZE) / float(longest_size))))
		var target_height: int = max(8, int(round(float(image.get_height()) * float(VECTORIZATION_MAX_SIZE) / float(longest_size))))
		image.resize(target_width, target_height, Image.INTERPOLATE_BILINEAR)
	_prepare_binary_mask_image(image)

	var bitmap := BitMap.new()
	bitmap.create_from_image_alpha(image, VECTORIZATION_ALPHA_THRESHOLD)
	var all_polygons: Array = bitmap.opaque_to_polygons(Rect2i(0, 0, image.get_width(), image.get_height()), VECTORIZATION_EPSILON)
	if all_polygons.is_empty():
		return []

	var normalized: Array[Dictionary] = []
	var inv_width: float = 1.0 / max(float(image.get_width()), 1.0)
	var inv_height: float = 1.0 / max(float(image.get_height()), 1.0)
	var center := Vector2(0.5, 0.5)
	var rotation_rad: float = deg_to_rad(wrapf(gobo_rotation_deg, 0.0, 360.0))
	var cos_r: float = cos(rotation_rad)
	var sin_r: float = sin(rotation_rad)
	var safe_scale: float = max(gobo_scale, 0.05)

	for polygon_variant in all_polygons:
		if polygon_variant is not PackedVector2Array:
			continue
		var polygon: PackedVector2Array = polygon_variant as PackedVector2Array
		if polygon.size() < 3:
			continue
		var transformed := PackedVector2Array()
		for point in polygon:
			var uv := Vector2(point.x * inv_width, point.y * inv_height)
			var local := (uv - center) * (2.0 / safe_scale)
			var rotated := Vector2(
				(local.x * cos_r) - (local.y * sin_r),
				(local.x * sin_r) + (local.y * cos_r)
			)
			transformed.append(Vector2(rotated.x, -rotated.y))
		var area: float = abs(_signed_polygon_area(transformed))
		if area < MIN_POLYGON_AREA:
			continue
		normalized.append({"polygon": transformed, "area": area})

	if normalized.is_empty():
		return []
	normalized.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return float(a.get("area", 0.0)) > float(b.get("area", 0.0))
	)
	var output: Array[PackedVector2Array] = []
	for item in normalized:
		output.append(item.get("polygon", PackedVector2Array()) as PackedVector2Array)
	return _reduce_polygon_point_count(output, MAX_TOTAL_VECTOR_POINTS)

func _reduce_polygon_point_count(polygons: Array[PackedVector2Array], max_points: int) -> Array[PackedVector2Array]:
	if polygons.is_empty():
		return []
	if _count_polygon_points(polygons) <= max_points:
		return polygons

	var reduced: Array[PackedVector2Array] = polygons.duplicate(true)
	var epsilon: float = POINT_REDUCTION_EPSILON_START
	while _count_polygon_points(reduced) > max_points and epsilon <= POINT_REDUCTION_EPSILON_MAX:
		var iteration: Array[PackedVector2Array] = []
		for polygon in reduced:
			var simplified: PackedVector2Array = _simplify_closed_polygon(polygon, epsilon)
			if simplified.size() < 3:
				continue
			if abs(_signed_polygon_area(simplified)) < MIN_POLYGON_AREA:
				continue
			iteration.append(simplified)
		reduced = iteration
		epsilon *= POINT_REDUCTION_EPSILON_MULTIPLIER

	if reduced.is_empty():
		return []
	if _count_polygon_points(reduced) <= max_points:
		return reduced

	var sorted: Array[Dictionary] = []
	for polygon in reduced:
		sorted.append({"polygon": polygon, "area": abs(_signed_polygon_area(polygon))})
	sorted.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return float(a.get("area", 0.0)) > float(b.get("area", 0.0))
	)

	var final_polygons: Array[PackedVector2Array] = []
	var total_points: int = 0
	for item in sorted:
		var polygon: PackedVector2Array = item.get("polygon", PackedVector2Array()) as PackedVector2Array
		var polygon_points: int = polygon.size()
		if final_polygons.size() > 0 and (total_points + polygon_points) > max_points:
			continue
		final_polygons.append(polygon)
		total_points += polygon_points

	if final_polygons.is_empty():
		final_polygons.append(sorted[0].get("polygon", PackedVector2Array()) as PackedVector2Array)
	return final_polygons

func _count_polygon_points(polygons: Array[PackedVector2Array]) -> int:
	var total: int = 0
	for polygon in polygons:
		total += polygon.size()
	return total

func _simplify_closed_polygon(polygon: PackedVector2Array, epsilon: float) -> PackedVector2Array:
	if polygon.size() < 4 or epsilon <= 0.0:
		return polygon
	var simplified: PackedVector2Array = Geometry2D.simplify_polyline(polygon, epsilon)
	if simplified.size() < 3:
		return polygon
	return simplified

func _prepare_binary_mask_image(image: Image) -> void:
	var width: int = image.get_width()
	var height: int = image.get_height()
	if width <= 0 or height <= 0:
		return
	var center := Vector2((float(width) - 1.0) * 0.5, (float(height) - 1.0) * 0.5)
	var aperture_radius: float = (min(float(width), float(height)) * 0.5) * APERTURE_BORDER_RATIO
	for y in range(height):
		for x in range(width):
			if x < OUTER_BORDER_PIXELS or y < OUTER_BORDER_PIXELS or x >= (width - OUTER_BORDER_PIXELS) or y >= (height - OUTER_BORDER_PIXELS):
				image.set_pixel(x, y, Color(0.0, 0.0, 0.0, 0.0))
				continue
			var pos := Vector2(float(x), float(y))
			if pos.distance_to(center) >= aperture_radius:
				image.set_pixel(x, y, Color(0.0, 0.0, 0.0, 0.0))
				continue
			var sample: Color = image.get_pixel(x, y)
			var luma: float = (sample.r * 0.299) + (sample.g * 0.587) + (sample.b * 0.114)
			var binary: float = 1.0 if (luma * sample.a) >= BINARY_LUMA_THRESHOLD else 0.0
			image.set_pixel(x, y, Color(binary, binary, binary, binary))

func _signed_polygon_area(polygon: PackedVector2Array) -> float:
	var count: int = polygon.size()
	if count < 3:
		return 0.0
	var area: float = 0.0
	for i in range(count):
		var current: Vector2 = polygon[i]
		var next: Vector2 = polygon[(i + 1) % count]
		area += (current.x * next.y) - (next.x * current.y)
	return 0.5 * area

func _build_fallback_circle() -> PackedVector2Array:
	var polygon := PackedVector2Array()
	for i in range(FALLBACK_SEGMENTS):
		var angle: float = TAU * float(i) / float(FALLBACK_SEGMENTS)
		polygon.append(Vector2(cos(angle), sin(angle)))
	return polygon

func _build_extruded_mesh(polygons: Array[PackedVector2Array], near_radius: float, far_radius: float, beam_height: float) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var half_height: float = beam_height * 0.5
	for polygon in polygons:
		if polygon.size() < 3:
			continue
		_add_caps(st, polygon, near_radius, far_radius, half_height)
		_add_sides(st, polygon, near_radius, far_radius, half_height)
	st.generate_normals()
	return st.commit()

func _add_caps(st: SurfaceTool, polygon: PackedVector2Array, near_radius: float, far_radius: float, half_height: float) -> void:
	var indices: PackedInt32Array = Geometry2D.triangulate_polygon(polygon)
	if indices.is_empty():
		return
	for i in range(0, indices.size(), 3):
		var ia: int = indices[i]
		var ib: int = indices[i + 1]
		var ic: int = indices[i + 2]
		var a: Vector2 = polygon[ia]
		var b: Vector2 = polygon[ib]
		var c: Vector2 = polygon[ic]
		st.add_vertex(Vector3(a.x * near_radius, half_height, a.y * near_radius))
		st.add_vertex(Vector3(b.x * near_radius, half_height, b.y * near_radius))
		st.add_vertex(Vector3(c.x * near_radius, half_height, c.y * near_radius))
		st.add_vertex(Vector3(c.x * far_radius, -half_height, c.y * far_radius))
		st.add_vertex(Vector3(b.x * far_radius, -half_height, b.y * far_radius))
		st.add_vertex(Vector3(a.x * far_radius, -half_height, a.y * far_radius))

func _add_sides(st: SurfaceTool, polygon: PackedVector2Array, near_radius: float, far_radius: float, half_height: float) -> void:
	var point_count: int = polygon.size()
	for i in range(point_count):
		var current: Vector2 = polygon[i]
		var next: Vector2 = polygon[(i + 1) % point_count]
		var near_current := Vector3(current.x * near_radius, half_height, current.y * near_radius)
		var near_next := Vector3(next.x * near_radius, half_height, next.y * near_radius)
		var far_current := Vector3(current.x * far_radius, -half_height, current.y * far_radius)
		var far_next := Vector3(next.x * far_radius, -half_height, next.y * far_radius)
		st.add_vertex(near_current)
		st.add_vertex(near_next)
		st.add_vertex(far_next)
		st.add_vertex(near_current)
		st.add_vertex(far_next)
		st.add_vertex(far_current)
