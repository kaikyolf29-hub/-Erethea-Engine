extends Node3D
## Erethea: automatic check of EretheaContactShadows. A 1 m cube on the
## ground, the sun behind it (so its short shadows fall towards the camera,
## where the depth buffer sees the cube) and the light's shadow map off.
## With the effect, the ground 10 cm in front of the cube (A) must be in
## shadow, while the ground beyond the effect's reach (B, still inside the
## cube's real shadow) stays lit; without it, A is lit too. Prints
## ERETHEA_CONTACT_SHADOWS_OK and quits with 0, or quits with 1.
## --out=<png> also saves the captures.

# The sun shines along this direction: from behind the cube (-Z), high.
const SUN_DIRECTION := Vector3(0.3, -1.2, 1.0)
const A := Vector3(0, 0, 0.6)
const B := Vector3(0, 0, 1.2)
const LIT := Vector3(2.5, 0, 2.5)
const SETTLE_FRAMES := 8

var frames := 0
var out := ""
var cam: Camera3D
var compositor: Compositor
var with_effect := {}


func _ready() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--out="):
			out = a.trim_prefix("--out=")
	var ground := MeshInstance3D.new()
	var plane := PlaneMesh.new()
	plane.size = Vector2(20, 20)
	ground.mesh = plane
	add_child(ground)
	var cube := MeshInstance3D.new()
	cube.mesh = BoxMesh.new()
	cube.position = Vector3(0, 0.5, 0)
	add_child(cube)
	var sun := DirectionalLight3D.new()
	sun.basis = Basis.looking_at(SUN_DIRECTION.normalized())
	sun.shadow_enabled = false
	add_child(sun)
	cam = Camera3D.new()
	cam.position = Vector3(0, 4, 5)
	add_child(cam)
	cam.look_at(Vector3(0, 0, 0.5))
	var env := WorldEnvironment.new()
	var e := Environment.new()
	e.background_mode = Environment.BG_COLOR
	e.background_color = Color(0.4, 0.6, 0.9)
	e.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	e.ambient_light_color = Color(0.25, 0.25, 0.3)
	env.environment = e
	compositor = Compositor.new()
	compositor.compositor_effects = [ClassDB.instantiate("EretheaContactShadows")]
	env.compositor = compositor
	add_child(env)


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
	return { "a": _luminance_at(img, A), "b": _luminance_at(img, B), "lit": _luminance_at(img, LIT) }


func _process(_delta: float) -> void:
	frames += 1
	if frames == SETTLE_FRAMES:
		var img := get_viewport().get_texture().get_image()
		if not out.is_empty():
			img.save_png(out.replace(".png", "_contact.png"))
		with_effect = _measure(img)
		print("CONTACT with the effect ", with_effect)
		compositor.compositor_effects = []
	elif frames == SETTLE_FRAMES * 2:
		var img := get_viewport().get_texture().get_image()
		if not out.is_empty():
			img.save_png(out.replace(".png", "_none.png"))
		var without := _measure(img)
		print("CONTACT without the effect ", without)
		var failures := PackedStringArray()
		if not with_effect.a < with_effect.lit * 0.6:
			failures.append("no contact shadow next to the cube (A)")
		if not with_effect.b > with_effect.lit * 0.85:
			failures.append("the contact shadow reaches past its length (B)")
		if not without.a > without.lit * 0.85:
			failures.append("A is in shadow without the effect")
		if failures.is_empty():
			print("ERETHEA_CONTACT_SHADOWS_OK")
			get_tree().quit(0)
		else:
			for f in failures:
				printerr("CONTACT FAILED: ", f)
			get_tree().quit(1)
