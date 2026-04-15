extends RefCounted
class_name GoboPolygonCleanup

const NEAR_POINT_EPSILON: float = 0.01
const COLLINEAR_EPSILON: float = 0.00005
const SHORT_EDGE_EPSILON: float = 0.001

static func sanitize_polygons(polygons: Array[PackedVector2Array], min_area: float) -> Array[PackedVector2Array]:
	var output: Array[PackedVector2Array] = []
	for polygon in polygons:
		var sanitized: PackedVector2Array = _sanitize_single_polygon(polygon, min_area)
		if sanitized.size() >= 3:
			output.append(sanitized)
	return output

static func _sanitize_single_polygon(polygon: PackedVector2Array, min_area: float) -> PackedVector2Array:
	if polygon.size() < 3:
		return PackedVector2Array()

	var deduped: PackedVector2Array = _remove_near_duplicate_points(polygon)
	if deduped.size() < 3:
		return PackedVector2Array()

	var joined: PackedVector2Array = _collapse_short_edges(deduped)
	if joined.size() < 3:
		return PackedVector2Array()

	var simplified: PackedVector2Array = _remove_collinear_points(joined)
	if simplified.size() < 3:
		return PackedVector2Array()

	if _has_edge_crossings(simplified):
		simplified = _reorder_points_by_angle(simplified)

	if simplified.size() < 3:
		return PackedVector2Array()

	var area: float = _signed_polygon_area(simplified)
	if abs(area) < min_area:
		return PackedVector2Array()

	if area < 0.0:
		simplified.reverse()

	return simplified

static func _remove_near_duplicate_points(polygon: PackedVector2Array) -> PackedVector2Array:
	var result := PackedVector2Array()
	for point in polygon:
		if result.is_empty() or point.distance_to(result[result.size() - 1]) > NEAR_POINT_EPSILON:
			result.append(point)
	if result.size() > 2 and result[0].distance_to(result[result.size() - 1]) <= NEAR_POINT_EPSILON:
		result.remove_at(result.size() - 1)
	return result

static func _remove_collinear_points(polygon: PackedVector2Array) -> PackedVector2Array:
	if polygon.size() < 4:
		return polygon
	var cleaned := PackedVector2Array()
	var count: int = polygon.size()
	for i in range(count):
		var prev: Vector2 = polygon[(i - 1 + count) % count]
		var current: Vector2 = polygon[i]
		var next: Vector2 = polygon[(i + 1) % count]
		var cross: float = abs((current - prev).cross(next - current))
		if cross > COLLINEAR_EPSILON:
			cleaned.append(current)
	return cleaned

static func _collapse_short_edges(polygon: PackedVector2Array) -> PackedVector2Array:
	if polygon.size() < 4:
		return polygon

	var collapsed: PackedVector2Array = PackedVector2Array(polygon)
	var changed: bool = true
	while changed and collapsed.size() >= 3:
		changed = false
		var count: int = collapsed.size()
		for i in range(count):
			var next_index: int = (i + 1) % count
			if collapsed[i].distance_to(collapsed[next_index]) > SHORT_EDGE_EPSILON:
				continue
			if next_index == 0:
				collapsed.remove_at(i)
			else:
				collapsed.remove_at(next_index)
			changed = true
			break
	return _remove_near_duplicate_points(collapsed)

static func _has_edge_crossings(polygon: PackedVector2Array) -> bool:
	var count: int = polygon.size()
	if count < 4:
		return false
	for i in range(count):
		var a0: Vector2 = polygon[i]
		var a1: Vector2 = polygon[(i + 1) % count]
		for j in range(i + 1, count):
			if abs(i - j) <= 1:
				continue
			if i == 0 and j == count - 1:
				continue
			var b0: Vector2 = polygon[j]
			var b1: Vector2 = polygon[(j + 1) % count]
			if Geometry2D.segment_intersects_segment(a0, a1, b0, b1) != null:
				return true
	return false

static func _reorder_points_by_angle(polygon: PackedVector2Array) -> PackedVector2Array:
	var centroid := Vector2.ZERO
	for point in polygon:
		centroid += point
	centroid /= float(max(polygon.size(), 1))

	var items: Array[Dictionary] = []
	for point in polygon:
		items.append({
			"point": point,
			"angle": atan2(point.y - centroid.y, point.x - centroid.x),
		})
	items.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return float(a.get("angle", 0.0)) < float(b.get("angle", 0.0))
	)

	var ordered := PackedVector2Array()
	for item in items:
		ordered.append(item.get("point", Vector2.ZERO) as Vector2)
	return _remove_near_duplicate_points(ordered)

static func _signed_polygon_area(polygon: PackedVector2Array) -> float:
	var count: int = polygon.size()
	if count < 3:
		return 0.0
	var area: float = 0.0
	for i in range(count):
		var current: Vector2 = polygon[i]
		var next: Vector2 = polygon[(i + 1) % count]
		area += (current.x * next.y) - (next.x * current.y)
	return 0.5 * area
