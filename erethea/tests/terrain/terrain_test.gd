extends Node3D

# Checks EretheaTerrain3D in a running project (see modules/erethea_terrain):
# - Builds a terrain of procedural hills, with two materials in Texture2DArrays
#   (made here, in code), a camera and a sun.
# - The data answers with the heights of the hills.
# - Rays cast down on known points find the collision at the height of the
#   terrain, exactly on the vertices and within a small tolerance between them.
# - A capture of the viewport is not a single color, and shows the terrain's
#   green material.
# - The background is pure magenta, a color that the terrain never has. Below
#   the top of the terrain, in each column of the capture, there must be no
#   magenta pixel: any would be a crack between two levels of the clipmap
#   that shows the background through the ground.
#
# Prints ERETHEA_TERRAIN_OK and exits with 0 if all of that holds, or exits
# with 1 otherwise.
# Arguments, after "--":
#   --out=<absolute png path>  Saves the capture there.

const SIZE := 256.0
const SPACING := 2.0
# Between the vertices, the drawn and collision triangles are not the
# interpolation that get_height() does. The hills are smooth, so it is little.
const TOLERANCE_BETWEEN_VERTICES := 0.25
const TOLERANCE_ON_VERTICES := 0.02
const TIMEOUT_MSEC := 120000
# A color that is not in the terrain, for finding the cracks.
const BACKGROUND := Color(1.0, 0.0, 1.0)

var failures: PackedStringArray = []
var terrain: EretheaTerrain3D
var data: EretheaTerrainData


func _ready() -> void:
	_run.call_deferred()


func _hill_height(x: float, z: float) -> float:
	return 9.0 * sin(x * 0.045) * cos(z * 0.06) + 5.0 * exp(-(x * x + z * z) / 1800.0)


func _make_layers(colors: Array[Color], stripes: bool, rough: bool) -> Texture2DArray:
	var images: Array[Image] = []
	for layer in colors.size():
		var image := Image.create_empty(64, 64, false, Image.FORMAT_RGBA8)
		for y in 64:
			for x in 64:
				var noise := 0.75 + 0.25 * sin(x * 0.9 + layer) * sin(y * 1.3)
				var color := Color(colors[layer].r * noise, colors[layer].g * noise, colors[layer].b * noise, noise)
				if stripes and (x / 8 + y / 8) % 2 == 0:
					color = color.darkened(0.25)
				if rough:
					# Normal map facing up, roughness 0.8.
					color = Color(0.5 + 0.1 * sin(x * 0.7), 0.5 + 0.1 * sin(y * 0.7), 1.0, 0.8)
				image.set_pixel(x, y, color)
		image.generate_mipmaps()
		images.append(image)
	var array := Texture2DArray.new()
	array.create_from_images(images)
	return array


func _build() -> void:
	data = EretheaTerrainData.new()
	data.create(Vector2(SIZE, SIZE), SPACING)
	var resolution := data.get_resolution()
	for z in resolution.y:
		for x in resolution.x:
			var position := data.get_vertex_position(Vector2i(x, z))
			var height := _hill_height(position.x, position.y)
			data.set_vertex_height(Vector2i(x, z), height)
			# Rock (material 1) on the high ground, grass (material 0) below.
			data.set_control(Vector2i(x, z), 0, 1, clampf((height - 3.0) / 6.0, 0.0, 1.0))
	data.notify_maps_changed()

	terrain = EretheaTerrain3D.new()
	terrain.name = "Terrain"
	terrain.data = data
	terrain.albedo_height_textures = _make_layers([Color(0.2, 0.55, 0.15), Color(0.5, 0.42, 0.38)], true, false)
	terrain.normal_roughness_textures = _make_layers([Color.WHITE, Color.WHITE], false, true)
	terrain.uv_scale = 0.2
	terrain.detiling = true
	terrain.triplanar = true
	terrain.clipmap_levels = 5
	terrain.mesh_size = 32
	add_child(terrain)

	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = BACKGROUND
	environment.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color(0.6, 0.65, 0.75)
	environment.ambient_light_energy = 0.6
	var world_environment := WorldEnvironment.new()
	world_environment.environment = environment
	add_child(world_environment)

	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-45, 40, 0)
	sun.shadow_enabled = true
	add_child(sun)

	var camera := Camera3D.new()
	camera.far = 600.0
	add_child(camera)
	camera.position = Vector3(0, 45, 130)
	camera.look_at(Vector3(0, 0, 0))
	camera.current = true


func _run() -> void:
	var out_path := ""
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--out="):
			out_path = argument.trim_prefix("--out=")

	_build()

	# The data's own heights.
	for point in [Vector2(0, 0), Vector2(40, -30), Vector2(-100, 64), Vector2(126, 126)]:
		var vertex_point := Vector2(roundf(point.x / SPACING) * SPACING, roundf(point.y / SPACING) * SPACING)
		var expected := _hill_height(vertex_point.x, vertex_point.y)
		var got := data.get_height(vertex_point.x, vertex_point.y)
		if absf(got - expected) > TOLERANCE_ON_VERTICES:
			failures.append("data.get_height(%s) is %.3f, the hill is at %.3f." % [vertex_point, got, expected])
	if not is_nan(data.get_height(SIZE, 0.0)):
		failures.append("data.get_height() outside of the terrain is not NaN.")
	if data.get_normal(0.0, 0.0).y <= 0.0:
		failures.append("data.get_normal() doesn't point up.")
	if terrain.get_level_centers().size() != terrain.clipmap_levels:
		failures.append("The clipmap has %d levels, not %d." % [terrain.get_level_centers().size(), terrain.clipmap_levels])

	# A few frames for the terrain to be drawn and the physics to have the shape.
	for frame in 8:
		await get_tree().physics_frame
	for frame in 8:
		await get_tree().process_frame

	var space := get_world_3d().direct_space_state
	var points: Array[Vector2] = []
	for i in 12:
		# On the vertices (even coordinates) and between them.
		points.append(Vector2(-110.0 + i * 19.0, 100.0 - i * 17.0))
		points.append(Vector2(-110.0 + i * 19.0 + 1.0, 100.0 - i * 17.0 + 1.0))
	var hits := 0
	var worst := 0.0
	for point in points:
		var query := PhysicsRayQueryParameters3D.create(Vector3(point.x, 100.0, point.y), Vector3(point.x, -100.0, point.y))
		var hit := space.intersect_ray(query)
		if hit.is_empty():
			failures.append("The ray at %s didn't find the terrain." % point)
			continue
		hits += 1
		var expected := data.get_height(point.x, point.y)
		var error := absf(hit.position.y - expected)
		var on_vertex := is_zero_approx(fposmod(point.x, SPACING)) and is_zero_approx(fposmod(point.y, SPACING))
		worst = maxf(worst, error)
		if error > (TOLERANCE_ON_VERTICES if on_vertex else TOLERANCE_BETWEEN_VERTICES):
			failures.append("The ray at %s hit y = %.3f, the terrain is at %.3f." % [point, hit.position.y, expected])
	print("%d of %d rays hit the terrain, the largest difference is %.3f m." % [hits, points.size(), worst])

	# Disabling the collision lets the rays through.
	terrain.collision_enabled = false
	await get_tree().physics_frame
	await get_tree().physics_frame
	var through := space.intersect_ray(PhysicsRayQueryParameters3D.create(Vector3(0, 100, 0), Vector3(0, -100, 0)))
	if not through.is_empty():
		failures.append("The ray still hit the terrain with collision_enabled off.")
	terrain.collision_enabled = true

	# The drawn terrain.
	for frame in 4:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	var image := get_viewport().get_texture().get_image()
	if image == null or image.is_empty():
		failures.append("The viewport gave no image.")
	else:
		_check_image(image, out_path)

	_finish()


func _check_image(image: Image, out_path: String) -> void:
	if not out_path.is_empty():
		DirAccess.make_dir_recursive_absolute(out_path.get_base_dir())
		image.save_png(out_path)
	var width := image.get_width()
	var height := image.get_height()
	var first := image.get_pixel(0, 0)
	var different := 0
	var green := 0
	var total := 0
	var luma_sum := 0.0
	var luma_square_sum := 0.0
	for y in range(0, height, 2):
		for x in range(0, width, 2):
			var color := image.get_pixel(x, y)
			total += 1
			if not color.is_equal_approx(first):
				different += 1
			# The grass, in sun or shade, is greener than anything else in the scene.
			if color.g > color.r + 0.06 and color.g > color.b + 0.06:
				green += 1
			var luma := color.get_luminance()
			luma_sum += luma
			luma_square_sum += luma * luma
	var mean := luma_sum / total
	var deviation := sqrt(maxf(luma_square_sum / total - mean * mean, 0.0)) * 255.0
	print("Capture %dx%d: %.1f%% of the pixels differ from the first, %.1f%% are green, luminance deviation %.1f of 255." % [width, height, 100.0 * different / total, 100.0 * green / total, deviation])
	if different < total * 0.2 or deviation < 3.0:
		failures.append("The capture is almost a single color.")
	if green < total * 0.03:
		failures.append("The capture has no green: the terrain's grass material isn't drawn.")
	_check_cracks(image)


# In each column, counts the background pixels below the first pixel of the
# terrain, which is the silhouette of the ground. There are none without cracks.
func _check_cracks(image: Image) -> void:
	var cracks := 0
	var columns_with_cracks := 0
	var first_crack := Vector2i(-1, -1)
	var covered_columns := 0
	for x in image.get_width():
		var top := -1
		for y in image.get_height():
			if not _is_background(image.get_pixel(x, y)):
				top = y
				break
		if top < 0:
			continue
		covered_columns += 1
		var column_cracks := 0
		for y in range(top, image.get_height()):
			if _is_background(image.get_pixel(x, y)):
				column_cracks += 1
				if first_crack.x < 0:
					first_crack = Vector2i(x, y)
		cracks += column_cracks
		if column_cracks > 0:
			columns_with_cracks += 1
	print("Cracks: %d background pixels below the terrain, in %d of %d columns%s." % [cracks, columns_with_cracks, covered_columns, "" if cracks == 0 else " (first at %s)" % first_crack])
	if covered_columns < image.get_width() / 2:
		failures.append("The terrain covers only %d of %d columns of the capture." % [covered_columns, image.get_width()])
	if cracks > 0:
		failures.append("%d background pixels show through the terrain, in %d columns (first at %s): there are cracks in the clipmap." % [cracks, columns_with_cracks, first_crack])


func _is_background(color: Color) -> bool:
	return color.r > 0.9 and color.g < 0.1 and color.b > 0.9


func _finish() -> void:
	if failures.is_empty():
		print("ERETHEA_TERRAIN_OK")
		get_tree().quit(0)
	else:
		for failure in failures:
			printerr("FAIL: ", failure)
		get_tree().quit(1)


func _process(_delta: float) -> void:
	if Time.get_ticks_msec() > TIMEOUT_MSEC:
		printerr("FAIL: the test didn't finish in %d s." % (TIMEOUT_MSEC / 1000))
		get_tree().quit(1)
