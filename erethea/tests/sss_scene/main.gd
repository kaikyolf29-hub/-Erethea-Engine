extends Node3D
## Erethea: check of rendering/erethea/subsurface_scattering/follow_surface.
## Two skin surfaces with subsurface scattering, 1 m apart in depth: a small
## one in front, lit by a light that only reaches it, and a large one behind,
## in the dark. Godot's filter lets the light of the front skin bleed onto
## the skin behind it, next to the edge; following the surface (this
## project sets 5 cm), it doesn't.
##   --expect=clean (default): no bleeding, as this project is set up.
##   --expect=bleed: with follow_surface at 0 (an override.cfg), it bleeds.
## Prints ERETHEA_SSS_OK and quits with 0, or quits with 1. --out=<png> saves
## the capture.

const SETTLE_FRAMES := 8
const NEAR_HALF_SIZE := 0.5

var frames := 0
var out := ""
var expect := "clean"
var cam: Camera3D


func _skin() -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = Color(0.9, 0.8, 0.75)
	m.subsurf_scatter_enabled = true
	m.subsurf_scatter_strength = 1.0
	m.subsurf_scatter_skin_mode = true
	return m


func _ready() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--out="):
			out = a.trim_prefix("--out=")
		elif a.begins_with("--expect="):
			expect = a.trim_prefix("--expect=")
	var far := MeshInstance3D.new()
	var far_quad := QuadMesh.new()
	far_quad.size = Vector2(6, 4)
	far.mesh = far_quad
	far.material_override = _skin()
	far.position = Vector3(0, 0, -1)
	far.layers = 1
	add_child(far)
	var near := MeshInstance3D.new()
	var near_quad := QuadMesh.new()
	near_quad.size = Vector2(NEAR_HALF_SIZE * 2, NEAR_HALF_SIZE * 2)
	near.mesh = near_quad
	near.material_override = _skin()
	near.layers = 2
	add_child(near)
	# Only the front skin gets this light.
	var light := OmniLight3D.new()
	light.position = Vector3(0, 0, 0.8)
	light.omni_range = 1.5
	light.light_energy = 6.0
	light.light_cull_mask = 2
	add_child(light)
	cam = Camera3D.new()
	cam.position = Vector3(0, 0, 3)
	add_child(cam)
	var env := WorldEnvironment.new()
	var e := Environment.new()
	e.background_mode = Environment.BG_COLOR
	e.background_color = Color(0, 0, 0)
	e.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	e.ambient_light_color = Color(0.15, 0.15, 0.15)
	env.environment = e
	add_child(env)


func _luminance_at(img: Image, point: Vector3) -> float:
	var p := cam.unproject_position(point)
	var total := 0.0
	var count := 0
	for y in range(int(p.y) - 1, int(p.y) + 2):
		for x in range(int(p.x) - 1, int(p.x) + 2):
			total += img.get_pixel(x, y).get_luminance()
			count += 1
	return total / count


func _process(_delta: float) -> void:
	frames += 1
	if frames != SETTLE_FRAMES:
		return
	var img := get_viewport().get_texture().get_image()
	if not out.is_empty():
		img.save_png(out)
	# On the skin behind, just right of the front skin's edge as seen from the
	# camera (a few pixels away), and far from it.
	var edge_screen := cam.unproject_position(Vector3(NEAR_HALF_SIZE, 0, 0))
	var next_to_edge := cam.project_position(edge_screen + Vector2(4, 0), 4.0)
	var values := {
		"front": _luminance_at(img, Vector3(0, 0, 0)),
		"next_to_edge": _luminance_at(img, next_to_edge),
		"far_from_edge": _luminance_at(img, Vector3(2.2, 0, -1)),
	}
	var bleed: float = values.next_to_edge - values.far_from_edge
	print("SSS ", values, " bleed=", bleed, " follow_surface=", ProjectSettings.get_setting("rendering/erethea/subsurface_scattering/follow_surface"))
	var ok := false
	if values.front < values.far_from_edge + 0.2:
		printerr("SSS FAILED: the front skin isn't lit.")
	elif expect == "bleed":
		ok = bleed > 0.03
		if not ok:
			printerr("SSS FAILED: expected the light to bleed onto the skin behind.")
	else:
		ok = bleed < 0.01
		if not ok:
			printerr("SSS FAILED: the light bleeds onto the skin behind.")
	if ok:
		print("ERETHEA_SSS_OK")
	get_tree().quit(0 if ok else 1)
