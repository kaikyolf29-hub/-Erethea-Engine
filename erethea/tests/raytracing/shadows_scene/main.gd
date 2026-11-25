extends Node3D
## Erethea: a cube on a plane, a pillar and the sun with its shadow map off.
## With --rt, EretheaRayTracedShadows draws the shadows (soft, 3 degrees, 16
## rays); without it, there are none. --out=<png> saves frame 8 and quits.
## Checked on lavapipe on 23/11: the shadows fall where they should, attached
## to what touches the ground, softer at the tip of the pillar.

var frames := 0
var out := ""

func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	var rt := "--rt" in args
	for a in args:
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
	var pillar := MeshInstance3D.new()
	var pm := BoxMesh.new()
	pm.size = Vector3(0.4, 4, 0.4)
	pillar.mesh = pm
	pillar.position = Vector3(3, 2, -2)
	add_child(pillar)
	add_child(cube)
	var sun := DirectionalLight3D.new()
	# Sun from the +X side, high: the shadow falls towards -X.
	sun.basis = Basis.looking_at(Vector3(-1, -1.2, -0.3).normalized())
	sun.shadow_enabled = false
	add_child(sun)
	var cam := Camera3D.new()
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
		fx.set("light_size", 3.0)
		fx.set("samples", 16)
		comp.compositor_effects = [fx]
		env.compositor = comp
		print("RTSCENE supported=", ClassDB.class_call_static("EretheaRayTracedShadows", "is_supported"))
	add_child(env)

func _process(_d: float) -> void:
	frames += 1
	if frames == 8:
		var img := get_viewport().get_texture().get_image()
		img.save_png(out)
		print("RTSCENE saved ", out)
		get_tree().quit()
