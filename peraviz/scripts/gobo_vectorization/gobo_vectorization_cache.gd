extends RefCounted
class_name GoboVectorizationCache

const VECTOR_POLYGONS_KEY: String = "vector_polygons"
const VECTOR_WIDTH_KEY: String = "vector_width"
const VECTOR_HEIGHT_KEY: String = "vector_height"
const VECTORIZATION_MAX_SIZE: int = 512
const VECTORIZATION_LUMA_ALPHA_THRESHOLD: float = 0.5

var _native_vectorizer: Object = null
var _cache_by_image_path: Dictionary = {}

func _init() -> void:
	if ClassDB.class_exists("PeravizGoboVectorizer"):
		_native_vectorizer = ClassDB.instantiate("PeravizGoboVectorizer")

func is_available() -> bool:
	return _native_vectorizer != null

func clear() -> void:
	_cache_by_image_path.clear()

func enrich_bindings_with_vector_gobos(bindings: Array) -> void:
	if not is_available():
		return
	for index in range(bindings.size()):
		var binding: Variant = bindings[index]
		if binding is not Dictionary:
			continue
		var updated_binding: Dictionary = (binding as Dictionary).duplicate(true)
		updated_binding["gobo_slots"] = _enrich_slots(updated_binding.get("gobo_slots", []))
		updated_binding["gobo1_slots"] = _enrich_slots(updated_binding.get("gobo1_slots", []))
		updated_binding["gobo_wheels"] = _enrich_wheels(updated_binding.get("gobo_wheels", []))
		bindings[index] = updated_binding

func _enrich_wheels(raw_wheels: Array) -> Array:
	var enriched: Array = []
	for wheel_variant in raw_wheels:
		if wheel_variant is not Dictionary:
			enriched.append(wheel_variant)
			continue
		var wheel: Dictionary = (wheel_variant as Dictionary).duplicate(true)
		wheel["slots"] = _enrich_slots(wheel.get("slots", []))
		enriched.append(wheel)
	return enriched

func _enrich_slots(raw_slots: Array) -> Array:
	var enriched: Array = []
	for slot_variant in raw_slots:
		if slot_variant is not Dictionary:
			enriched.append(slot_variant)
			continue
		var slot: Dictionary = (slot_variant as Dictionary).duplicate(true)
		var image_path: String = str(slot.get("image_path", ""))
		if image_path.is_empty():
			enriched.append(slot)
			continue
		var cached: Dictionary = _get_or_create_vector_entry(image_path)
		if bool(cached.get("ok", false)):
			slot[VECTOR_POLYGONS_KEY] = cached.get("polygons", [])
			slot[VECTOR_WIDTH_KEY] = int(cached.get("width", 0))
			slot[VECTOR_HEIGHT_KEY] = int(cached.get("height", 0))
		enriched.append(slot)
	return enriched

func _get_or_create_vector_entry(image_path: String) -> Dictionary:
	if _cache_by_image_path.has(image_path):
		return _cache_by_image_path[image_path] as Dictionary
	var result: Dictionary = _native_vectorizer.call("vectorize_image", image_path, VECTORIZATION_MAX_SIZE, VECTORIZATION_LUMA_ALPHA_THRESHOLD, true)
	_cache_by_image_path[image_path] = result
	return result
