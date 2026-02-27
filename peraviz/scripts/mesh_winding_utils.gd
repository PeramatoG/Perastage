extends RefCounted
class_name PeravizMeshWindingUtils

const TRIANGLE_SAMPLE_LIMIT: int = 1024

static func compute_outward_ratio(vertices: PackedVector3Array, indices: PackedInt32Array, sample_limit: int = TRIANGLE_SAMPLE_LIMIT) -> Dictionary:
	if vertices.is_empty() or indices.size() < 3:
		return {
			"outward_ratio": 1.0,
			"sampled_triangles": 0,
		}

	var center: Vector3 = _compute_center(vertices)
	var sampled: int = 0
	var outward: int = 0
	for tri_start in range(0, indices.size() - 2, 3):
		if sampled >= sample_limit:
			break
		var i0: int = indices[tri_start]
		var i1: int = indices[tri_start + 1]
		var i2: int = indices[tri_start + 2]
		if i0 < 0 or i1 < 0 or i2 < 0:
			continue
		if i0 >= vertices.size() or i1 >= vertices.size() or i2 >= vertices.size():
			continue
		var v0: Vector3 = vertices[i0]
		var v1: Vector3 = vertices[i1]
		var v2: Vector3 = vertices[i2]
		var triangle_normal: Vector3 = (v1 - v0).cross(v2 - v0)
		if triangle_normal.length_squared() <= 1e-12:
			continue
		var tri_center: Vector3 = (v0 + v1 + v2) / 3.0
		if triangle_normal.dot(tri_center - center) > 0.0:
			outward += 1
		sampled += 1

	if sampled == 0:
		return {
			"outward_ratio": 1.0,
			"sampled_triangles": 0,
		}

	return {
		"outward_ratio": float(outward) / float(sampled),
		"sampled_triangles": sampled,
	}

static func apply_transform_winding_fix_if_needed(vertices: PackedVector3Array, normals: PackedVector3Array, indices: PackedInt32Array, transform_basis: Basis, log_context: String = "") -> Dictionary:
	var determinant: float = transform_basis.determinant()
	if determinant >= 0.0:
		return {
			"applied": false,
			"reason": "none",
			"determinant": determinant,
		}
	apply_winding_fix(indices, normals)
	print("[PeravizMeshWinding] event=negative_determinant_fix context=", log_context, " determinant=", determinant)
	return {
		"applied": true,
		"reason": "negative_determinant",
		"determinant": determinant,
	}

static func apply_inside_out_heuristic_if_enabled(vertices: PackedVector3Array, normals: PackedVector3Array, indices: PackedInt32Array, enabled: bool, log_context: String = "") -> Dictionary:
	if not enabled:
		return {
			"applied": false,
			"reason": "disabled",
			"outward_ratio": 1.0,
			"sampled_triangles": 0,
		}
	if vertices.is_empty() or indices.size() < 3:
		return {
			"applied": false,
			"reason": "insufficient_geometry",
			"outward_ratio": 1.0,
			"sampled_triangles": 0,
		}

	var metrics: Dictionary = compute_outward_ratio(vertices, indices, TRIANGLE_SAMPLE_LIMIT)
	var sampled: int = int(metrics.get("sampled_triangles", 0))
	if sampled == 0:
		return {
			"applied": false,
			"reason": "no_valid_samples",
			"outward_ratio": 1.0,
			"sampled_triangles": 0,
		}

	var outward_ratio: float = float(metrics.get("outward_ratio", 1.0))
	if outward_ratio >= 0.35:
		return {
			"applied": false,
			"reason": "ratio_ok",
			"outward_ratio": outward_ratio,
			"sampled_triangles": sampled,
		}

	apply_winding_fix(indices, normals)
	print("[PeravizMeshWinding] event=heuristic_inside_out_fix context=", log_context, " outward_ratio=", outward_ratio, " sampled_triangles=", sampled)
	return {
		"applied": true,
		"reason": "heuristic_inside_out",
		"outward_ratio": outward_ratio,
		"sampled_triangles": sampled,
	}

static func apply_winding_fix(indices: PackedInt32Array, normals: PackedVector3Array) -> void:
	for tri_start in range(0, indices.size() - 2, 3):
		var i1: int = indices[tri_start + 1]
		indices[tri_start + 1] = indices[tri_start + 2]
		indices[tri_start + 2] = i1
	for normal_index in range(normals.size()):
		normals[normal_index] = -normals[normal_index]

static func _compute_center(vertices: PackedVector3Array) -> Vector3:
	var accumulator := Vector3.ZERO
	for vertex in vertices:
		accumulator += vertex
	return accumulator / float(max(vertices.size(), 1))
