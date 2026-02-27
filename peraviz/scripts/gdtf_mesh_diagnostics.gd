extends SceneTree

const LoaderScript = preload("res://scripts/mesh_winding_utils.gd")

func _initialize() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	var mesh_paths: PackedStringArray = PackedStringArray()
	var enable_heuristic: bool = false
	for arg in args:
		if arg.begins_with("--mesh="):
			mesh_paths.append(arg.trim_prefix("--mesh="))
		elif arg == "--heuristic":
			enable_heuristic = true

	if mesh_paths.is_empty():
		print("[PeravizMeshDiag] usage: godot --headless --path peraviz -s res://scripts/gdtf_mesh_diagnostics.gd -- --mesh=/path/base.3ds [--mesh=/path/head.3ds] [--heuristic]")
		quit(2)
		return

	var loader := PeravizLoader.new()
	for mesh_path in mesh_paths:
		_run_mesh_check(loader, mesh_path, enable_heuristic)

	quit()

func _run_mesh_check(loader: PeravizLoader, mesh_path: String, enable_heuristic: bool) -> void:
	var extension: String = mesh_path.get_extension().to_lower()
	if extension == "3ds":
		var mesh_data: Dictionary = loader.load_3ds_mesh_data(mesh_path)
		if not bool(mesh_data.get("ok", false)):
			print("[PeravizMeshDiag] mesh=", mesh_path, " ok=false error=", str(mesh_data.get("error", "unknown")))
			return
		var vertices: PackedVector3Array = mesh_data.get("vertices", PackedVector3Array())
		var normals: PackedVector3Array = mesh_data.get("normals", PackedVector3Array())
		var indices: PackedInt32Array = mesh_data.get("indices", PackedInt32Array())
		var det_fix: Dictionary = LoaderScript.apply_transform_winding_fix_if_needed(vertices, normals, indices, Basis.IDENTITY, mesh_path)
		var heuristic_fix: Dictionary = LoaderScript.apply_inside_out_heuristic_if_enabled(vertices, normals, indices, enable_heuristic, mesh_path)
		print("[PeravizMeshDiag] mesh=", mesh_path,
			" format=3ds",
			" vertices=", vertices.size(),
			" triangles=", indices.size() / 3,
			" determinant_fix=", bool(det_fix.get("applied", false)),
			" heuristic_fix=", bool(heuristic_fix.get("applied", false)),
			" heuristic_ratio=", float(heuristic_fix.get("outward_ratio", 1.0)))
		return

	if extension == "glb" or extension == "gltf":
		var gltf := GLTFDocument.new()
		var state := GLTFState.new()
		var err: int = gltf.append_from_file(mesh_path, state)
		print("[PeravizMeshDiag] mesh=", mesh_path,
			" format=", extension,
			" loaded=", err == OK,
			" determinant_fix=false heuristic_fix=false")
		return

	print("[PeravizMeshDiag] mesh=", mesh_path, " unsupported_format=", extension)
