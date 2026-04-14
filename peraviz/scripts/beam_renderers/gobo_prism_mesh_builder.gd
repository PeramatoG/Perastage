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
const POINT_REDUCTION_EPSILON_MAX: float = 0.08
const GOBO_VECTOR_POLYGONS_META_KEY: String = "peraviz_gobo_vector_polygons"
const GOBO_VECTOR_WIDTH_META_KEY: String = "peraviz_gobo_vector_width"
const GOBO_VECTOR_HEIGHT_META_KEY: String = "peraviz_gobo_vector_height"
const DEBUG_GOBO_VECTORIZATION: bool = false
const CIRCULAR_STDDEV_RATIO_THRESHOLD: float = 0.045
const CIRCULAR_MAX_DEVIATION_RATIO_THRESHOLD: float = 0.14
const CIRCULAR_MIN_POINTS: int = 16

const GoboPolygonCleanupScript = preload("res://scripts/beam_renderers/gobo_polygon_cleanup.gd")

var _shape_cache: Dictionary = {}
var _mesh_cache: Dictionary = {}
var _normalized_polygon_cache: Dictionary = {}

func clear_cache() -> void:
	_shape_cache.clear()
	_mesh_cache.clear()
	_normalized_polygon_cache.clear()

func build_beam_mesh(gobo_texture: Texture2D, near_radius: float, far_radius: float, beam_height: float, gobo_scale: float, apply_edge_mask_correction: bool = true) -> ArrayMesh:
	var shape_key: String = _shape_cache_key(gobo_texture, gobo_scale, apply_edge_mask_correction)
	var geometry_key: String = "%s_%.4f_%.4f_%.4f" % [shape_key, near_radius, far_radius, beam_height]
	if _mesh_cache.has(geometry_key):
		return _mesh_cache[geometry_key] as ArrayMesh

	var polygons: Array[PackedVector2Array] = _get_or_build_shape_base(gobo_texture, gobo_scale, apply_edge_mask_correction)
	if polygons.is_empty():
		polygons = [_build_fallback_circle()]

	var mesh: ArrayMesh = _build_extruded_mesh(polygons, max(near_radius, 0.001), max(far_radius, 0.001), max(beam_height, 0.001))
	_mesh_cache[geometry_key] = mesh
	return mesh

func _get_or_build_shape_base(gobo_texture: Texture2D, gobo_scale: float, apply_edge_mask_correction: bool) -> Array[PackedVector2Array]:
	var shape_key: String = _shape_cache_key(gobo_texture, gobo_scale, apply_edge_mask_correction)
	if _shape_cache.has(shape_key):
		return (_shape_cache[shape_key] as Array).duplicate(true) as Array[PackedVector2Array]
	var polygons: Array[PackedVector2Array] = _vectorize_gobo(gobo_texture, gobo_scale, apply_edge_mask_correction)
	_shape_cache[shape_key] = polygons.duplicate(true)
	return polygons

func _shape_cache_key(gobo_texture: Texture2D, gobo_scale: float, apply_edge_mask_correction: bool) -> String:
	if gobo_texture == null:
		return "__fallback_shape_%s" % [str(apply_edge_mask_correction)]
	return "__shape_%d_%.3f_%s" % [gobo_texture.get_rid().get_id(), gobo_scale, str(apply_edge_mask_correction)]

func _vectorize_gobo(gobo_texture: Texture2D, gobo_scale: float, apply_edge_mask_correction: bool = true) -> Array[PackedVector2Array]:
	if gobo_texture == null:
		return []

	var native_polygons: Array[PackedVector2Array] = _vectorize_from_texture_metadata(gobo_texture)
	if not native_polygons.is_empty():
		var cleaned_native: Array[PackedVector2Array] = _finalize_vector_polygons(native_polygons, gobo_scale)
		if not cleaned_native.is_empty():
			if DEBUG_GOBO_VECTORIZATION:
				print("[PeravizGoboVectorization] source=metadata polygons=", cleaned_native.size(), " points=", _count_polygon_points(cleaned_native))
			return cleaned_native

	var normalized_polygons: Array[PackedVector2Array] = _get_or_build_normalized_raster_polygons(gobo_texture, apply_edge_mask_correction)
	if normalized_polygons.is_empty():
		return []
	return _finalize_vector_polygons(normalized_polygons, gobo_scale)


func _vectorize_from_texture_metadata(gobo_texture: Texture2D) -> Array[PackedVector2Array]:
	if gobo_texture == null or not gobo_texture.has_meta(GOBO_VECTOR_POLYGONS_META_KEY):
		return []
	var raw_polygons: Array = gobo_texture.get_meta(GOBO_VECTOR_POLYGONS_META_KEY, [])
	var source_width: int = int(gobo_texture.get_meta(GOBO_VECTOR_WIDTH_META_KEY, 0))
	var source_height: int = int(gobo_texture.get_meta(GOBO_VECTOR_HEIGHT_META_KEY, 0))
	if source_width <= 0 or source_height <= 0 or raw_polygons.is_empty():
		return []

	var inv_width: float = 1.0 / max(float(source_width), 1.0)
	var inv_height: float = 1.0 / max(float(source_height), 1.0)
	var normalized: Array[PackedVector2Array] = []
	for polygon_variant in raw_polygons:
		if polygon_variant is not PackedVector2Array:
			continue
		var polygon: PackedVector2Array = polygon_variant as PackedVector2Array
		if polygon.size() < 3:
			continue
		var normalized_polygon: PackedVector2Array = _normalize_polygon_to_local_space(polygon, inv_width, inv_height)
		if normalized_polygon.size() >= 3:
			normalized.append(normalized_polygon)
	return normalized

func _get_or_build_normalized_raster_polygons(gobo_texture: Texture2D, apply_edge_mask_correction: bool) -> Array[PackedVector2Array]:
	var normalization_key: String = _normalized_polygon_cache_key(gobo_texture, apply_edge_mask_correction)
	if _normalized_polygon_cache.has(normalization_key):
		return (_normalized_polygon_cache[normalization_key] as Array).duplicate(true) as Array[PackedVector2Array]
	var normalized_polygons: Array[PackedVector2Array] = _vectorize_texture_to_normalized_polygons(gobo_texture, apply_edge_mask_correction)
	_normalized_polygon_cache[normalization_key] = normalized_polygons.duplicate(true)
	return normalized_polygons

func _normalized_polygon_cache_key(gobo_texture: Texture2D, apply_edge_mask_correction: bool) -> String:
	if gobo_texture == null:
		return "__normalized_fallback_%s_%.3f" % [str(apply_edge_mask_correction), VECTORIZATION_ALPHA_THRESHOLD]
	return "__normalized_%d_%s_%.3f" % [gobo_texture.get_rid().get_id(), str(apply_edge_mask_correction), VECTORIZATION_ALPHA_THRESHOLD]

func _vectorize_texture_to_normalized_polygons(gobo_texture: Texture2D, apply_edge_mask_correction: bool) -> Array[PackedVector2Array]:
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
	if apply_edge_mask_correction:
		_prepare_binary_mask_image(image)
	if DEBUG_GOBO_VECTORIZATION:
		print("[PeravizGoboVectorization] source=raster texture=", gobo_texture.get_rid().get_id())

	var bitmap := BitMap.new()
	bitmap.create_from_image_alpha(image, VECTORIZATION_ALPHA_THRESHOLD)
	var all_polygons: Array = bitmap.opaque_to_polygons(Rect2i(0, 0, image.get_width(), image.get_height()), VECTORIZATION_EPSILON)
	if all_polygons.is_empty():
		return []
	var inv_width: float = 1.0 / max(float(image.get_width()), 1.0)
	var inv_height: float = 1.0 / max(float(image.get_height()), 1.0)
	var normalized: Array[PackedVector2Array] = []
	for polygon_variant in all_polygons:
		if polygon_variant is not PackedVector2Array:
			continue
		var polygon: PackedVector2Array = polygon_variant as PackedVector2Array
		if polygon.size() < 3:
			continue
		var normalized_polygon: PackedVector2Array = _normalize_polygon_to_local_space(polygon, inv_width, inv_height)
		if normalized_polygon.size() >= 3:
			normalized.append(normalized_polygon)
	return normalized

func _normalize_polygon_to_local_space(polygon: PackedVector2Array, inv_width: float, inv_height: float) -> PackedVector2Array:
	var center := Vector2(0.5, 0.5)
	var normalized_polygon := PackedVector2Array()
	for point in polygon:
		var uv := Vector2(point.x * inv_width, point.y * inv_height)
		var local := (uv - center) * 2.0
		normalized_polygon.append(Vector2(local.x, -local.y))
	return normalized_polygon

func _finalize_vector_polygons(normalized_polygons: Array[PackedVector2Array], gobo_scale: float) -> Array[PackedVector2Array]:
	if normalized_polygons.is_empty():
		return []
	var safe_scale: float = max(gobo_scale, 0.05)
	var scaled_polygons: Array[PackedVector2Array] = []
	for polygon in normalized_polygons:
		var scaled_polygon := PackedVector2Array()
		for point in polygon:
			scaled_polygon.append(point / safe_scale)
		scaled_polygons.append(scaled_polygon)
	var cleaned_output: Array[PackedVector2Array] = GoboPolygonCleanupScript.sanitize_polygons(scaled_polygons, MIN_POLYGON_AREA)
	if cleaned_output.is_empty():
		return []
	var regularized_output: Array[PackedVector2Array] = _regularize_near_circular_polygons(cleaned_output)
	var reduced_output: Array[PackedVector2Array] = _reduce_polygon_point_count(regularized_output, MAX_TOTAL_VECTOR_POINTS)
	return _sort_polygons_by_area_descending(reduced_output)

func _sort_polygons_by_area_descending(polygons: Array[PackedVector2Array]) -> Array[PackedVector2Array]:
	if polygons.is_empty():
		return []
	var sorted: Array[Dictionary] = []
	for polygon in polygons:
		var area: float = abs(_signed_polygon_area(polygon))
		if area < MIN_POLYGON_AREA:
			continue
		sorted.append({"polygon": polygon, "area": area})
	if sorted.is_empty():
		return []
	sorted.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return float(a.get("area", 0.0)) > float(b.get("area", 0.0))
	)
	var output: Array[PackedVector2Array] = []
	for item in sorted:
		output.append(item.get("polygon", PackedVector2Array()) as PackedVector2Array)
	return output

func _regularize_near_circular_polygons(polygons: Array[PackedVector2Array]) -> Array[PackedVector2Array]:
	var output: Array[PackedVector2Array] = []
	for polygon in polygons:
		if _is_near_circular_polygon(polygon):
			output.append(_fit_polygon_to_circle(polygon))
		else:
			output.append(polygon)
	return output

func _is_near_circular_polygon(polygon: PackedVector2Array) -> bool:
	if polygon.size() < CIRCULAR_MIN_POINTS:
		return false
	var centroid := Vector2.ZERO
	for point in polygon:
		centroid += point
	centroid /= float(polygon.size())

	var radii: PackedFloat32Array = PackedFloat32Array()
	radii.resize(polygon.size())
	var mean_radius: float = 0.0
	for i in range(polygon.size()):
		var radius: float = polygon[i].distance_to(centroid)
		radii[i] = radius
		mean_radius += radius
	mean_radius /= float(polygon.size())
	if mean_radius <= 0.0001:
		return false

	var variance: float = 0.0
	var max_deviation: float = 0.0
	for radius in radii:
		var deviation: float = abs(radius - mean_radius)
		variance += deviation * deviation
		max_deviation = max(max_deviation, deviation)
	variance /= float(polygon.size())
	var stddev: float = sqrt(max(variance, 0.0))
	var stddev_ratio: float = stddev / mean_radius
	var max_deviation_ratio: float = max_deviation / mean_radius
	return stddev_ratio <= CIRCULAR_STDDEV_RATIO_THRESHOLD and max_deviation_ratio <= CIRCULAR_MAX_DEVIATION_RATIO_THRESHOLD

func _fit_polygon_to_circle(polygon: PackedVector2Array) -> PackedVector2Array:
	if polygon.size() < 3:
		return polygon
	var centroid := Vector2.ZERO
	for point in polygon:
		centroid += point
	centroid /= float(polygon.size())

	var mean_radius: float = 0.0
	for point in polygon:
		mean_radius += point.distance_to(centroid)
	mean_radius /= float(polygon.size())
	if mean_radius <= 0.0001:
		return polygon

	var fitted := PackedVector2Array()
	for point in polygon:
		var direction: Vector2 = point - centroid
		var length: float = direction.length()
		if length <= 0.000001:
			fitted.append(point)
			continue
		fitted.append(centroid + ((direction / length) * mean_radius))
	return fitted

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
			var simplified_area: float = abs(_signed_polygon_area(simplified))
			if simplified_area < MIN_POLYGON_AREA:
				iteration.append(polygon)
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
	var final_cleaned: Array[PackedVector2Array] = GoboPolygonCleanupScript.sanitize_polygons(final_polygons, MIN_POLYGON_AREA)
	return final_cleaned if not final_cleaned.is_empty() else final_polygons

func _count_polygon_points(polygons: Array[PackedVector2Array]) -> int:
	var total: int = 0
	for polygon in polygons:
		total += polygon.size()
	return total

func _simplify_closed_polygon(polygon: PackedVector2Array, epsilon: float) -> PackedVector2Array:
	if polygon.size() < 4 or epsilon <= 0.0:
		return polygon

	var closed_path := PackedVector2Array(polygon)
	closed_path.append(polygon[0])
	var kept_indices: PackedInt32Array = _rdp_keep_indices(closed_path, epsilon)
	if kept_indices.size() < 4:
		return polygon

	var simplified_path := PackedVector2Array()
	for index in kept_indices:
		simplified_path.append(closed_path[index])
	if simplified_path[0].distance_to(simplified_path[simplified_path.size() - 1]) <= 0.0001:
		simplified_path.remove_at(simplified_path.size() - 1)
	if simplified_path.size() < 3:
		return polygon
	return simplified_path

func _rdp_keep_indices(path: PackedVector2Array, epsilon: float) -> PackedInt32Array:
	var count: int = path.size()
	if count < 2:
		return PackedInt32Array()
	var keep: Array[bool] = []
	keep.resize(count)
	for i in range(count):
		keep[i] = false
	keep[0] = true
	keep[count - 1] = true
	_rdp_mark(path, 0, count - 1, epsilon, keep)

	var indices := PackedInt32Array()
	for i in range(count):
		if keep[i]:
			indices.append(i)
	return indices

func _rdp_mark(path: PackedVector2Array, start_index: int, end_index: int, epsilon: float, keep: Array[bool]) -> void:
	if (end_index - start_index) <= 1:
		return
	var segment_start: Vector2 = path[start_index]
	var segment_end: Vector2 = path[end_index]
	var max_distance: float = -1.0
	var split_index: int = -1
	for i in range(start_index + 1, end_index):
		var distance: float = _distance_point_to_segment(path[i], segment_start, segment_end)
		if distance > max_distance:
			max_distance = distance
			split_index = i
	if split_index == -1:
		return
	if max_distance > epsilon:
		keep[split_index] = true
		_rdp_mark(path, start_index, split_index, epsilon, keep)
		_rdp_mark(path, split_index, end_index, epsilon, keep)

func _distance_point_to_segment(point: Vector2, segment_start: Vector2, segment_end: Vector2) -> float:
	var segment: Vector2 = segment_end - segment_start
	var segment_length_squared: float = segment.length_squared()
	if segment_length_squared <= 0.0000001:
		return point.distance_to(segment_start)
	var t: float = clamp((point - segment_start).dot(segment) / segment_length_squared, 0.0, 1.0)
	var projection: Vector2 = segment_start + (segment * t)
	return point.distance_to(projection)


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
	st.set_smooth_group(-1)
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
	st.set_smooth_group(0)
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
	st.set_smooth_group(-1)
