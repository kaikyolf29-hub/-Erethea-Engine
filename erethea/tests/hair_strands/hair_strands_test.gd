extends Node3D
## Erethea hair strands test: a head (a sphere) with a few hundred strands of
## hair made of lines, as the Fornalha exports them (loose edges in the .glb),
## weighted to the only bone of a Skeleton3D. The test draws the hair, turns the
## bone by 90 degrees and draws again. It checks that hair pixels show up over
## the head, that they move with the pose, and that drawing a fraction of the
## strands draws less. Last, it lights the hair from above and from below and
## checks that the strands on top of the head are lit from above, whichever way
## the ribbons face the camera. It checks that no strand's shell faces down,
## that the back light lights the hair against the light and that wider roots
## cover more. The ribbons are opened along the tangent, so it also draws quads
## with and without skinning and checks that the skinned ones get the same
## tangent space (with both binormal signs).
##
## Run with a window (Mesa's lavapipe and xvfb are enough) and a fixed frame
## rate, giving where to save the captures after "--":
##     erethea --path erethea/tests/hair_strands --fixed-fps 30 -- --out=<folder>/hair.png
## Add --rendering-method gl_compatibility to check the Compatibility renderer.
## It saves hair_before.png, hair_after.png, hair_sparse.png, hair_lit_above.png,
## hair_lit_below.png, hair_backlit.png and hair_tangents.png next to it,
## prints ERETHEA_HAIR_OK and quits with 0, or quits with 1 on failure.

const STRAND_COUNT := 400
const POINTS_PER_STRAND := 7
const HEAD_RADIUS := 0.5
const HAIR_COLOR := Color(0.95, 0.4, 0.05)
const SEED := 4242

var rng := RandomNumberGenerator.new()
var failures: Array[String] = []
var skeleton := Skeleton3D.new()
var hair := EretheaHairStrands.new()
var head := MeshInstance3D.new()
var sun := DirectionalLight3D.new()


func _ready() -> void:
	rng.seed = SEED
	var out := _argument("out", "")
	if out.is_empty():
		out = ProjectSettings.globalize_path("res://hair.png")
	_build_scene()
	_run(out)


func _argument(name: String, default: String) -> String:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--%s=" % name):
			return argument.trim_prefix("--%s=" % name)
	return default


func _build_scene() -> void:
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color(0.02, 0.03, 0.05)
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color(0.6, 0.6, 0.7)
	environment.ambient_light_energy = 0.5
	var world := WorldEnvironment.new()
	world.environment = environment
	add_child(world)

	sun.rotation_degrees = Vector3(-35.0, 25.0, 0.0)
	sun.light_energy = 1.2
	add_child(sun)

	var camera := Camera3D.new()
	camera.position = Vector3(0.0, 0.25, 2.4)
	add_child(camera)
	camera.look_at(Vector3(0.0, 0.1, 0.0))

	# The head: a blue sphere, so it can't be mistaken for the orange hair.
	var sphere := SphereMesh.new()
	sphere.radius = HEAD_RADIUS * 0.98
	sphere.height = HEAD_RADIUS * 1.96
	var skin_material := StandardMaterial3D.new()
	skin_material.albedo_color = Color(0.25, 0.35, 0.7)
	sphere.material = skin_material
	head.mesh = sphere
	add_child(head)

	skeleton.add_bone("head")
	skeleton.set_bone_rest(0, Transform3D.IDENTITY)
	add_child(skeleton)

	hair.source_mesh = _make_strands()
	hair.width = 0.012
	hair.tip_width = 0.3
	hair.color_root = HAIR_COLOR * 0.7
	hair.color_tip = HAIR_COLOR
	skeleton.add_child(hair)
	# In this base the skeleton of a MeshInstance3D comes empty: point it at the Skeleton3D.
	hair.skeleton = NodePath("..")


## A hairstyle of lines (Mesh.PRIMITIVE_LINES with indices, as the glTF importer
## makes from loose edges): strands that start on the top and back of the head
## and fall outwards, plus a ponytail on the +X side that makes the pose show.
func _make_strands() -> ArrayMesh:
	var vertices := PackedVector3Array()
	var bones := PackedInt32Array()
	var weights := PackedFloat32Array()
	var indices := PackedInt32Array()
	for strand in STRAND_COUNT:
		var ponytail := strand % 4 == 0
		var direction: Vector3
		if ponytail:
			direction = Vector3(1.0, 0.25, 0.0) + Vector3(rng.randf_range(-0.2, 0.2), rng.randf_range(-0.2, 0.2), rng.randf_range(-0.3, 0.3))
		else:
			# The upper half of the sphere.
			direction = Vector3(rng.randf_range(-1.0, 1.0), rng.randf_range(0.1, 1.0), rng.randf_range(-1.0, 0.6))
		direction = direction.normalized()
		var root := direction * HEAD_RADIUS
		var length := 0.55 if ponytail else rng.randf_range(0.2, 0.4)
		var first := vertices.size()
		for i in POINTS_PER_STRAND:
			var t := float(i) / (POINTS_PER_STRAND - 1)
			# Away from the head, then falling.
			var point := root + direction * (0.12 * t) + Vector3(0.0, -length * t * t, 0.0) + direction * (length * 0.3 * t)
			vertices.append(point)
			bones.append_array(PackedInt32Array([0, 0, 0, 0]))
			weights.append_array(PackedFloat32Array([1.0, 0.0, 0.0, 0.0]))
		for i in POINTS_PER_STRAND - 1:
			indices.append(first + i)
			indices.append(first + i + 1)
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_BONES] = bones
	arrays[Mesh.ARRAY_WEIGHTS] = weights
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_LINES, arrays)
	return mesh


func _run(out: String) -> void:
	var folder := out.get_base_dir()
	var stem := out.get_file().get_basename()
	DirAccess.make_dir_recursive_absolute(folder)

	await _frames(5)
	var before := await _capture()
	before.save_png(folder.path_join(stem + "_before.png"))

	skeleton.set_bone_pose_rotation(0, Quaternion(Vector3.UP, deg_to_rad(90.0)))
	await _frames(5)
	var after := await _capture()
	after.save_png(folder.path_join(stem + "_after.png"))

	skeleton.set_bone_pose_rotation(0, Quaternion.IDENTITY)
	hair.strand_fraction = 0.25
	await _frames(5)
	var sparse := await _capture()
	sparse.save_png(folder.path_join(stem + "_sparse.png"))

	_check(hair.get_source_strand_count() == STRAND_COUNT, "the source has %d strands, got %d" % [STRAND_COUNT, hair.get_source_strand_count()])
	_check(hair.get_strand_count() == STRAND_COUNT / 4, "strand_fraction 0.25 draws %d strands, got %d" % [STRAND_COUNT / 4, hair.get_strand_count()])

	# Lit from straight above and straight below, without the head, so only
	# the hair changes. The strands grow from the top half of the head, so
	# their shell normals point up: from above they must take the light, and
	# from below only the wrapped and back light reach them. Ribbons that face
	# away from the camera got their normal flipped by the double-sided
	# lighting, which lit the top of the head from below (Fornalha, 27/11).
	hair.strand_fraction = 1.0
	head.visible = false
	sun.rotation_degrees = Vector3(-90.0, 0.0, 0.0)
	await _frames(5)
	var lit_above := await _capture()
	lit_above.save_png(folder.path_join(stem + "_lit_above.png"))
	sun.rotation_degrees = Vector3(90.0, 0.0, 0.0)
	await _frames(5)
	var lit_below := await _capture()
	lit_below.save_png(folder.path_join(stem + "_lit_below.png"))
	var brightness_above := _top_brightness(lit_above)
	var brightness_below := _top_brightness(lit_below)
	print("hair brightness on top of the head: lit from above=%.4f lit from below=%.4f" % [brightness_above, brightness_below])
	_check(brightness_above > brightness_below * 1.25, "the top of the hair is lit from above (%.4f, from below %.4f)" % [brightness_above, brightness_below])

	# Hair below the middle of the hairstyle hangs: its shell normal faces out,
	# never down, where it took the ground's dark ambient light and none of the
	# sun's (Fornalha, 28/11: the lower half of the hair was black from behind).
	var lowest_normal_y := 1.0
	for surface in hair.mesh.get_surface_count():
		for normal in hair.mesh.surface_get_arrays(surface)[Mesh.ARRAY_NORMAL]:
			lowest_normal_y = minf(lowest_normal_y, normal.y)
	_check(lowest_normal_y > -0.001, "no strand's shell faces down (lowest normal y %.3f)" % lowest_normal_y)

	# Against the light (the sun shines from behind the hair at the camera),
	# the back light property lights the hair. It did nothing, as Godot drops
	# BACKLIGHT in shaders with their own light().
	sun.rotation_degrees = Vector3(0.0, 180.0, 0.0)
	hair.backlight = 0.0
	await _frames(5)
	var dark_backlit := _image_brightness(await _capture())
	hair.backlight = 1.0
	await _frames(5)
	var backlit_image := await _capture()
	backlit_image.save_png(folder.path_join(stem + "_backlit.png"))
	var backlit := _image_brightness(backlit_image)
	hair.backlight = 0.4
	print("hair against the light: backlight 0=%.1f backlight 1=%.1f" % [dark_backlit, backlit])
	_check(backlit > dark_backlit * 1.15, "the back light lights the hair against the light (%.1f, without it %.1f)" % [backlit, dark_backlit])

	# Wider roots cover more: a game has far fewer strands than a scalp, and
	# what is under them showed through at the crown (Fornalha, 28/11).
	sun.rotation_degrees = Vector3(-35.0, 25.0, 0.0)
	hair.root_width = 1.0
	await _frames(5)
	var thin_roots: int = _hair_stats(await _capture()).count
	hair.root_width = 3.0
	await _frames(5)
	var wide_roots: int = _hair_stats(await _capture()).count
	hair.root_width = 2.0
	print("hair pixels: root_width 1=%d root_width 3=%d" % [thin_roots, wide_roots])
	_check(wide_roots > thin_roots * 1.1, "wider roots cover more (%d pixels, %d with thin roots)" % [wide_roots, thin_roots])

	await _check_tangents(folder.path_join(stem + "_tangents.png"))

	var stats_before := _hair_stats(before)
	var stats_after := _hair_stats(after)
	var stats_sparse := _hair_stats(sparse)
	print("hair pixels: before=%d after=%d sparse=%d" % [stats_before.count, stats_after.count, stats_sparse.count])
	print("hair centroid x: before=%.1f after=%.1f" % [stats_before.centroid.x, stats_after.centroid.x])

	_check(stats_before.count > 1500, "hair pixels are drawn (%d)" % stats_before.count)
	_check(stats_before.on_head > 800, "hair pixels over the head (%d)" % stats_before.on_head)
	_check(stats_after.count > 1500, "hair pixels after the turn (%d)" % stats_after.count)
	# The ponytail leaves the right side of the picture and goes behind the head.
	_check(stats_before.centroid.x - stats_after.centroid.x > 15.0, "the hair moves with the bone (centroid x %.1f -> %.1f)" % [stats_before.centroid.x, stats_after.centroid.x])
	_check(stats_sparse.count > 100 and stats_sparse.count < stats_before.count * 0.7, "fewer strands draw fewer pixels (%d of %d)" % [stats_sparse.count, stats_before.count])

	if failures.is_empty():
		print("ERETHEA_HAIR_OK")
		get_tree().quit(0)
	else:
		for failure in failures:
			printerr("FAIL: ", failure)
		get_tree().quit(1)


func _frames(count: int) -> void:
	for i in count:
		await get_tree().process_frame


func _capture() -> Image:
	await RenderingServer.frame_post_draw
	return get_viewport().get_texture().get_image()


## Hair pixels are orange: red well above blue, and above green.
func _is_hair(color: Color) -> bool:
	return color.r - color.b > 0.25 and color.r > color.g * 1.2


func _hair_stats(image: Image) -> Dictionary:
	var count := 0
	var on_head := 0
	var sum := Vector2.ZERO
	var size := image.get_size()
	var center := Vector2(size) * 0.5
	for y in size.y:
		for x in size.x:
			if _is_hair(image.get_pixel(x, y)):
				count += 1
				sum += Vector2(x, y)
				if Vector2(x, y).distance_to(center) < size.x * 0.25:
					on_head += 1
	return {"count": count, "on_head": on_head, "centroid": sum / maxf(count, 1)}


## Four quads in a row: without skinning and skinned, with a binormal sign of
## +1 and then -1. The shader paints the binormal, which comes from the normal,
## the tangent and the sign, so a skinned quad must look like its unskinned
## twin. The Compatibility renderer skinned with a constant tangent (Fornalha,
## 27/11), which also opened the hair ribbons the wrong way.
func _check_tangents(path: String) -> void:
	hair.visible = false
	var shader := Shader.new()
	shader.code = "shader_type spatial;\nrender_mode unshaded, cull_disabled;\nvoid fragment() {\n\tALBEDO = BINORMAL * 0.5 + 0.5;\n}\n"
	var material := ShaderMaterial.new()
	material.shader = shader
	var quads: Array[MeshInstance3D] = []
	for i in 4:
		var quad := MeshInstance3D.new()
		quad.mesh = _make_quad(1.0 if i < 2 else -1.0)
		quad.material_override = material
		quad.position = Vector3(-0.75 + 0.5 * i, 0.1, 0.0)
		if i % 2 == 1:
			skeleton.add_child(quad)
			quad.skeleton = NodePath("..")
		else:
			add_child(quad)
		quads.append(quad)
	await _frames(5)
	var image := await _capture()
	image.save_png(path)
	var size := image.get_size()
	var colors: Array[Color] = []
	for quad in quads:
		var at := get_viewport().get_camera_3d().unproject_position(quad.global_position)
		colors.append(image.get_pixelv(Vector2i(at.round()).clamp(Vector2i.ZERO, size - Vector2i.ONE)))
	print("binormal colors: %s" % [colors])
	for sign_index in 2:
		var plain := colors[2 * sign_index]
		var skinned := colors[2 * sign_index + 1]
		var difference := maxf(absf(plain.r - skinned.r), maxf(absf(plain.g - skinned.g), absf(plain.b - skinned.b)))
		_check(difference < 0.02, "the skinned quad has the same tangent space (sign %d: %s and %s)" % [1 if sign_index == 0 else -1, plain, skinned])
	for quad in quads:
		quad.queue_free()
	hair.visible = true


## A 0.4 m quad facing the camera, weighted to the only bone, with a tangent
## along +X turned a little (so it isn't an axis) and the given binormal sign.
func _make_quad(binormal_sign: float) -> ArrayMesh:
	var tangent := Vector3(1.0, 0.3, 0.0).normalized()
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([Vector3(-0.2, -0.2, 0.0), Vector3(0.2, -0.2, 0.0), Vector3(0.2, 0.2, 0.0), Vector3(-0.2, 0.2, 0.0)])
	arrays[Mesh.ARRAY_NORMAL] = PackedVector3Array([Vector3.BACK, Vector3.BACK, Vector3.BACK, Vector3.BACK])
	var tangents := PackedFloat32Array()
	var bones := PackedInt32Array()
	var weights := PackedFloat32Array()
	for i in 4:
		tangents.append_array(PackedFloat32Array([tangent.x, tangent.y, tangent.z, binormal_sign]))
		bones.append_array(PackedInt32Array([0, 0, 0, 0]))
		weights.append_array(PackedFloat32Array([1.0, 0.0, 0.0, 0.0]))
	arrays[Mesh.ARRAY_TANGENT] = tangents
	arrays[Mesh.ARRAY_BONES] = bones
	arrays[Mesh.ARRAY_WEIGHTS] = weights
	arrays[Mesh.ARRAY_INDEX] = PackedInt32Array([0, 1, 2, 0, 2, 3])
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh


## How much brighter than the background the picture is, summed over it: with
## the head hidden, that is the hair's light.
func _image_brightness(image: Image) -> float:
	var size := image.get_size()
	var background := image.get_pixel(0, 0).get_luminance()
	var total := 0.0
	for y in size.y:
		for x in size.x:
			total += maxf(image.get_pixel(x, y).get_luminance() - background, 0.0)
	return total


## Mean brightness of the band of the picture where the top of the hair is
## (the background is the same dark color in both pictures).
func _top_brightness(image: Image) -> float:
	var size := image.get_size()
	var total := 0.0
	var count := 0
	for y in range(int(size.y * 0.36), int(size.y * 0.48)):
		for x in size.x:
			total += image.get_pixel(x, y).get_luminance()
			count += 1
	return total / maxf(count, 1)


func _check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)
