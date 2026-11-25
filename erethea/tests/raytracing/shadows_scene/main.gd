extends Node3D
## Erethea: a cube on a plane, a pillar and the sun with its shadow map off.
## With --rt, EretheaRayTracedShadows draws the shadows (soft, 3 degrees, 16
## rays); without it, there are none. --out=<png> saves frame 8 and quits.
## Checked on lavapipe on 23/11: the shadows fall where they should, attached
## to what touches the ground, softer at the tip of the pillar.
##
## --skinned-check: an automatic check of the shadows of a mesh with a
## skeleton (an arm of two bones on the plane, only ray traced shadows). With
## the arm straight up, the shadow of its top must fall on P1; with the upper
## bone bent towards the camera, P1 must be lit and the shadow must fall on P2.
## Prints ERETHEA_RT_SKINNED_OK and quits with 0, or quits with 1.

# The sun shines along this direction (from +X, high).
const SUN_DIRECTION := Vector3(-1, -1.2, -0.3)
# The ground under the top of the straight arm (0, 2.6, 0) and under the bent
# arm (0, 1.5, 1.2), along SUN_DIRECTION, and a point that is always lit.
const P1 := Vector3(-2.6 / 1.2, 0, -0.3 * 2.6 / 1.2)
const P2 := Vector3(-1.5 / 1.2, 0, 1.2 - 0.3 * 1.5 / 1.2)
const LIT := Vector3(2.5, 0, 2.5)
# Frames to wait after a change of pose: the skeleton, the deformed vertices,
# the acceleration structure and the mask follow in the next frames.
const SETTLE_FRAMES := 10

var frames := 0
var out := ""
var skinned_check := false
var skeleton: Skeleton3D
var cam: Camera3D
var straight := {}


func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	var rt := "--rt" in args
	skinned_check = "--skinned-check" in args
	for a in args:
		if a.begins_with("--out="):
			out = a.trim_prefix("--out=")
	var ground := MeshInstance3D.new()
	var plane := PlaneMesh.new()
	plane.size = Vector2(20, 20)
	ground.mesh = plane
	add_child(ground)
	if skinned_check:
		rt = true
		skeleton = _make_arm()
		add_child(skeleton)
	else:
		var cube := MeshInstance3D.new()
		cube.mesh = BoxMesh.new()
		cube.position = Vector3(0, 0.5, 0)
		var pillar := MeshInstance3D.new()
		var pm := BoxMesh.new()
		pm.size = Vector3(0.4, 4, 0.4)
		pillar.mesh = pm
		pillar.position = Vector3(3, 2, -2)
		add_child(pillar)
		add_child(cube)
	var sun := DirectionalLight3D.new()
	# Sun from the +X side, high: the shadow falls towards -X.
	sun.basis = Basis.looking_at(SUN_DIRECTION.normalized())
	sun.shadow_enabled = false
	add_child(sun)
	cam = Camera3D.new()
	cam.position = Vector3(0, 6, 9)
	add_child(cam)
	cam.look_at(Vector3(0, 0, 0))
	var env := WorldEnvironment.new()
	var e := Environment.new()
	e.background_mode = Environment.BG_COLOR
	e.background_color = Color(0.4, 0.6, 0.9)
	e.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	e.ambient_light_color = Color(0.25, 0.25, 0.3)
	env.environment = e
	if rt:
		var comp := Compositor.new()
		var fx: CompositorEffect = ClassDB.instantiate("EretheaRayTracedShadows")
		if not skinned_check:
			fx.set("light_size", 3.0)
			fx.set("samples", 16)
		comp.compositor_effects = [fx]
		env.compositor = comp
		print("RTSCENE supported=", ClassDB.class_call_static("EretheaRayTracedShadows", "is_supported"))
	add_child(env)


# A box 3 m tall standing on the ground, with a skeleton of two bones: the
# lower half follows the "root" bone, the upper half the "upper" bone, which
# turns around (0, 1.5, 0).
func _make_arm() -> Skeleton3D:
	var sk := Skeleton3D.new()
	sk.add_bone("root")
	sk.add_bone("upper")
	sk.set_bone_parent(1, 0)
	sk.set_bone_rest(1, Transform3D(Basis.IDENTITY, Vector3(0, 1.5, 0)))
	sk.reset_bone_poses()
	var box := BoxMesh.new()
	box.size = Vector3(0.3, 3.0, 0.3)
	box.subdivide_height = 5
	var arrays := box.get_mesh_arrays()
	var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	var bones := PackedInt32Array()
	var weights := PackedFloat32Array()
	for i in vertices.size():
		vertices[i].y += 1.5
		bones.append_array([1 if vertices[i].y > 1.51 else 0, 0, 0, 0])
		weights.append_array([1.0, 0.0, 0.0, 0.0])
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_BONES] = bones
	arrays[Mesh.ARRAY_WEIGHTS] = weights
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var arm := MeshInstance3D.new()
	arm.mesh = mesh
	sk.add_child(arm)
	return sk


# The average luminance of the image around the point of the ground.
func _luminance_at(img: Image, point: Vector3) -> float:
	var p := cam.unproject_position(point)
	var total := 0.0
	var count := 0
	for y in range(int(p.y) - 2, int(p.y) + 3):
		for x in range(int(p.x) - 2, int(p.x) + 3):
			if x >= 0 and y >= 0 and x < img.get_width() and y < img.get_height():
				total += img.get_pixel(x, y).get_luminance()
				count += 1
	return total / max(count, 1)


func _measure(img: Image) -> Dictionary:
	return { "p1": _luminance_at(img, P1), "p2": _luminance_at(img, P2), "lit": _luminance_at(img, LIT) }


func _process(_d: float) -> void:
	frames += 1
	if skinned_check:
		_skinned_check_step()
		return
	if frames == 8:
		var img := get_viewport().get_texture().get_image()
		img.save_png(out)
		print("RTSCENE saved ", out)
		get_tree().quit()


func _skinned_check_step() -> void:
	if frames == SETTLE_FRAMES:
		var img := get_viewport().get_texture().get_image()
		if not out.is_empty():
			img.save_png(out.replace(".png", "_straight.png"))
		straight = _measure(img)
		print("RTSKIN straight ", straight)
		# Bend the upper bone towards the camera (+Z).
		skeleton.set_bone_pose_rotation(1, Quaternion(Vector3(1, 0, 0), PI / 2))
	elif frames == SETTLE_FRAMES * 2:
		var img := get_viewport().get_texture().get_image()
		if not out.is_empty():
			img.save_png(out.replace(".png", "_bent.png"))
		var bent := _measure(img)
		print("RTSKIN bent ", bent)
		var failures := PackedStringArray()
		# In shadow: well below the lit ground.
		if not straight.p1 < straight.lit * 0.6:
			failures.append("the straight arm casts no shadow on P1")
		if not straight.p2 > straight.lit * 0.85:
			failures.append("P2 is not lit with the arm straight")
		if not bent.p1 > bent.lit * 0.85:
			failures.append("P1 is still in shadow with the arm bent")
		if not bent.p2 < bent.lit * 0.6:
			failures.append("the bent arm casts no shadow on P2")
		if failures.is_empty():
			print("ERETHEA_RT_SKINNED_OK")
			get_tree().quit(0)
		else:
			for f in failures:
				printerr("RTSKIN FAILED: ", f)
			get_tree().quit(1)
