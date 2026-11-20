extends SceneTree
## Compares two sets of captures of the reference scene, for example stock
## Godot and Erethea, and writes side-by-side images and a Markdown summary.
##
##     erethea --headless --path erethea/tests/reference_scene --script res://compare_captures.gd -- \
##         --baseline=<folder> --candidate=<folder> --out=<folder> [--max-rmse=<value>]
##
## Each image in <out> shows the baseline on the left, the candidate in the
## middle and their difference, amplified, on the right. The error is measured
## on luminance, from 0 to 255. With --max-rmse, the exit code is 1 when a
## capture differs more than that.

const DIFFERENCE_GAIN := 8.0


func _initialize() -> void:
	var arguments := {}
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--") and argument.contains("="):
			var parts := argument.trim_prefix("--").split("=", true, 1)
			arguments[parts[0]] = parts[1]
	for required in ["baseline", "candidate", "out"]:
		if not arguments.has(required):
			printerr("Missing --%s=<folder>." % required)
			quit(2)
			return
	quit(_compare(arguments["baseline"], arguments["candidate"], arguments["out"], float(arguments.get("max-rmse", "-1"))))


func _compare(baseline_folder: String, candidate_folder: String, out_folder: String, max_rmse: float) -> int:
	DirAccess.make_dir_recursive_absolute(out_folder)
	var baseline_times := _read_frame_times(baseline_folder)
	var candidate_times := _read_frame_times(candidate_folder)
	var names := Array(DirAccess.get_files_at(baseline_folder)).filter(func(file: String) -> bool: return file.get_extension() == "png")
	names.sort()
	if names.is_empty():
		printerr("No captures in %s." % baseline_folder)
		return 1

	var failed := false
	var lines: Array[String] = [
		"| Captura | RMSE | PSNR (dB) | Base: ms por quadro | Comparada: ms por quadro |",
		"| --- | ---: | ---: | ---: | ---: |",
	]
	for file: String in names:
		var name := file.get_basename()
		var baseline := Image.load_from_file(baseline_folder.path_join(file))
		var candidate_path := candidate_folder.path_join(file)
		if not FileAccess.file_exists(candidate_path):
			printerr("%s is missing from %s." % [file, candidate_folder])
			failed = true
			continue
		var candidate := Image.load_from_file(candidate_path)
		baseline.convert(Image.FORMAT_RGB8)
		candidate.convert(Image.FORMAT_RGB8)
		if baseline.get_size() != candidate.get_size():
			printerr("%s has size %s in the baseline and %s in the candidate." % [file, baseline.get_size(), candidate.get_size()])
			failed = true
			continue

		var metrics := baseline.compute_image_metrics(candidate, true)
		var rmse: float = metrics["root_mean_squared"]
		var psnr: float = metrics["peak_snr"]
		var too_different := max_rmse >= 0.0 and rmse > max_rmse
		failed = failed or too_different
		_side_by_side(baseline, candidate).save_png(out_folder.path_join(file))
		lines.append("| %s%s | %.3f | %s | %s | %s |" % [
			name,
			" ❌" if too_different else "",
			rmse,
			"igual" if is_inf(psnr) or psnr >= 99.0 else "%.1f" % psnr,
			baseline_times.get(name, "?"),
			candidate_times.get(name, "?"),
		])
		print("%s: RMSE %.3f, PSNR %.1f dB%s" % [name, rmse, psnr, " (above %.3f)" % max_rmse if too_different else ""])

	var summary := FileAccess.open(out_folder.path_join("summary.md"), FileAccess.WRITE)
	summary.store_string("\n".join(lines) + "\n")
	summary.close()
	return 1 if failed else 0


func _read_frame_times(folder: String) -> Dictionary:
	var times := {}
	var text := FileAccess.get_file_as_string(folder.path_join("results.json"))
	var results = JSON.parse_string(text)
	if results is Dictionary:
		for capture: Dictionary in results.get("captures", []):
			times[capture["name"]] = "%.1f" % capture["frame_ms"]
	return times


func _side_by_side(baseline: Image, candidate: Image) -> Image:
	var size := baseline.get_size() / 2
	var left := baseline.duplicate() as Image
	var middle := candidate.duplicate() as Image
	left.resize(size.x, size.y, Image.INTERPOLATE_BILINEAR)
	middle.resize(size.x, size.y, Image.INTERPOLATE_BILINEAR)

	var difference := Image.create_empty(size.x, size.y, false, Image.FORMAT_RGB8)
	for y in size.y:
		for x in size.x:
			var a := left.get_pixel(x, y)
			var b := middle.get_pixel(x, y)
			var error := minf(1.0, (absf(a.r - b.r) + absf(a.g - b.g) + absf(a.b - b.b)) * DIFFERENCE_GAIN / 3.0)
			difference.set_pixel(x, y, Color(error, error * 0.3, 0.0))

	var result := Image.create_empty(size.x * 3, size.y, false, Image.FORMAT_RGB8)
	var rect := Rect2i(Vector2i.ZERO, size)
	result.blit_rect(left, rect, Vector2i.ZERO)
	result.blit_rect(middle, rect, Vector2i(size.x, 0))
	result.blit_rect(difference, rect, Vector2i(size.x * 2, 0))
	return result
