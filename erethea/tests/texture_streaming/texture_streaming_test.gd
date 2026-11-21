extends Node3D

# Checks Erethea's texture streaming in a running project (see
# modules/erethea_streaming):
# - brick.png, used in 3D, is a StreamedTexture2D. It starts at a low
#   resolution and streams up to the full one, since the camera shows it one
#   texel per pixel.
# - It then looks like brick_reference.png, the same image imported without
#   streaming.
# - Once nothing shows it, it drops back to a lower resolution.
# - ui_icon.png, used only in the UI, keeps Godot's CompressedTexture2D.
#
# Prints ERETHEA_STREAMING_OK and exits with 0 if all of that holds.
# Arguments, after "--":
#   --capture=<absolute folder>  Saves the streamed and reference images there.

# Largest allowed difference between the streamed and the reference texture,
# as luminance RMSE (0 to 255).
const MAX_RMSE := 1.0
const TIMEOUT_MSEC := 120000

var failures: PackedStringArray = []


func _ready() -> void:
	_run.call_deferred()


func _run() -> void:
	var capture_dir := ""
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture="):
			capture_dir = argument.trim_prefix("--capture=")

	var material: StandardMaterial3D = $Brick.mesh.material
	var brick: Texture2D = material.albedo_texture
	var icon: Texture2D = $UI/Icon.texture

	print("brick.png: %s, ui_icon.png: %s" % [brick.get_class(), icon.get_class()])
	if icon is StreamedTexture2D:
		failures.append("ui_icon.png is only used in the UI, but it was imported as a StreamedTexture2D.")
	if not brick is StreamedTexture2D:
		failures.append("brick.png is used in 3D, but it was imported as a %s, not a StreamedTexture2D." % brick.get_class())
		_finish()
		return

	# Streaming up.
	var full_width := brick.get_width()
	var first_width := _loaded_width(brick)
	print("brick.png starts at %d of %d pixels wide." % [first_width, full_width])
	if first_width >= full_width:
		failures.append("brick.png started at its full resolution, so it didn't stream.")
	var start := Time.get_ticks_msec()
	var widths: Array[int] = [first_width]
	while _loaded_width(brick) < full_width:
		if Time.get_ticks_msec() - start > TIMEOUT_MSEC:
			failures.append("brick.png didn't reach its full resolution in %d s (it's at %d pixels wide)." % [TIMEOUT_MSEC / 1000, _loaded_width(brick)])
			_finish()
			return
		if _loaded_width(brick) != widths[-1]:
			widths.append(_loaded_width(brick))
		await get_tree().process_frame
	widths.append(full_width)
	print("brick.png streamed up to its full resolution in %d ms (widths: %s)." % [Time.get_ticks_msec() - start, widths])

	# Same look as without streaming.
	var streamed := await _capture()
	material.albedo_texture = load("res://brick_reference.png")
	var reference := await _capture()
	var rmse := _luma_rmse(streamed, reference)
	print("Streamed against reference: luma RMSE %.3f (at most %.1f)." % [rmse, MAX_RMSE])
	if rmse > MAX_RMSE:
		failures.append("With all its mipmaps, brick.png looks different from brick_reference.png (luma RMSE %.3f)." % rmse)
	if not capture_dir.is_empty():
		DirAccess.make_dir_recursive_absolute(capture_dir)
		streamed.save_png(capture_dir.path_join("streamed.png"))
		reference.save_png(capture_dir.path_join("reference.png"))

	# Streaming down once nothing shows the texture.
	material.albedo_texture = brick
	$Brick.visible = false
	start = Time.get_ticks_msec()
	while _loaded_width(brick) >= full_width:
		if Time.get_ticks_msec() - start > TIMEOUT_MSEC:
			failures.append("brick.png kept its full resolution for %d s while hidden." % [TIMEOUT_MSEC / 1000])
			break
		await get_tree().process_frame
	print("Hidden, brick.png went down to %d pixels wide in %d ms." % [_loaded_width(brick), Time.get_ticks_msec() - start])

	_finish()


# The width of the mipmap levels the GPU has for the texture right now.
func _loaded_width(texture: Texture2D) -> int:
	var rd_texture := RenderingServer.texture_get_rd_texture(texture.get_rid())
	return RenderingServer.get_rendering_device().texture_get_format(rd_texture).width


func _capture() -> Image:
	for i in 5:
		await RenderingServer.frame_post_draw
	var image := get_viewport().get_texture().get_image()
	image.convert(Image.FORMAT_RGB8)
	return image


func _luma_rmse(a: Image, b: Image) -> float:
	if a.get_size() != b.get_size():
		return INF
	var sum := 0.0
	for y in a.get_height():
		for x in a.get_width():
			var ca := a.get_pixel(x, y)
			var cb := b.get_pixel(x, y)
			var difference := (ca.get_luminance() - cb.get_luminance()) * 255.0
			sum += difference * difference
	return sqrt(sum / (a.get_width() * a.get_height()))


func _finish() -> void:
	for failure in failures:
		printerr("FAILED: " + failure)
	if failures.is_empty():
		print("ERETHEA_STREAMING_OK")
	get_tree().quit(0 if failures.is_empty() else 1)
