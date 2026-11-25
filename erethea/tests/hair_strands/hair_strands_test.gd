extends Node3D
## Erethea hair strands test: a head (a sphere) with a few hundred strands of
## hair made of lines, as the Fornalha exports them (loose edges in the .glb),
## weighted to the only bone of a Skeleton3D. The test draws the hair, turns the
## bone by 90 degrees and draws again. It checks that hair pixels show up over
## the head, that they move with the pose, and that drawing a fraction of the
## strands draws less.
##
## Run with a window (Mesa's lavapipe and xvfb are enough) and a fixed frame
## rate, giving where to save the captures after "--":
##     erethea --path erethea/tests/hair_strands --fixed-fps 30 -- --out=<folder>/hair.png
## It saves hair_before.png, hair_after.png and hair_sparse.png next to it,
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

	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-35.0, 25.0, 0.0)
	sun.light_energy = 1.2
	add_child(sun)

	var camera := Camera3D.new()
	camera.position = Vector3(0.0, 0.25, 2.4)
	add_child(camera)
	camera.look_at(Vector3(0.0, 0.1, 0.0))

	# The head: a blue sphere, so it can't be mistaken for the orange hair.
	var head := MeshInstance3D.new()
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


func _check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)
