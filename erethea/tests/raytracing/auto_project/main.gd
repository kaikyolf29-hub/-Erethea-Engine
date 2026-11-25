extends Node3D
## Erethea: this project sets rendering/erethea/ray_tracing/sun_shadows to
## Enabled. On a graphics card with ray tracing (lavapipe has it), the running
## project must get EretheaRayTracedShadows in the compositor of its
## WorldEnvironment (created, as it has none) and in a camera's own compositor,
## also for a camera added later, while a camera without a compositor keeps
## using the environment's. Prints ERETHEA_RT_AUTO_OK and quits with 0, or
## quits with 1.

var frames := 0
var environment: WorldEnvironment
var own_compositor_camera: Camera3D
var plain_camera: Camera3D
var late_camera: Camera3D


func _ready() -> void:
	environment = WorldEnvironment.new()
	environment.environment = Environment.new()
	add_child(environment)
	own_compositor_camera = Camera3D.new()
	own_compositor_camera.compositor = Compositor.new()
	add_child(own_compositor_camera)
	plain_camera = Camera3D.new()
	add_child(plain_camera)
	plain_camera.make_current()
	var sun := DirectionalLight3D.new()
	add_child(sun)
	var ground := MeshInstance3D.new()
	ground.mesh = PlaneMesh.new()
	add_child(ground)


func _count_effects(compositor: Compositor) -> int:
	if compositor == null:
		return -1
	var count := 0
	for effect in compositor.compositor_effects:
		if effect != null and effect.get_class() == "EretheaRayTracedShadows":
			count += 1
	return count


func _process(_delta: float) -> void:
	frames += 1
	if frames == 3:
		late_camera = Camera3D.new()
		late_camera.compositor = Compositor.new()
		add_child(late_camera)
	elif frames == 6:
		var supported: bool = ClassDB.class_call_static("EretheaRayTracedShadows", "is_supported")
		var counts := {
			"environment": _count_effects(environment.compositor),
			"own_compositor_camera": _count_effects(own_compositor_camera.compositor),
			"plain_camera": _count_effects(plain_camera.compositor),
			"late_camera": _count_effects(late_camera.compositor),
		}
		print("RTAUTO supported=", supported, " ", counts)
		var expected := { "environment": 1, "own_compositor_camera": 1, "plain_camera": -1, "late_camera": 1 }
		if not supported:
			printerr("RTAUTO FAILED: no ray tracing on this graphics card.")
			get_tree().quit(1)
		elif counts != expected:
			printerr("RTAUTO FAILED: expected ", expected)
			get_tree().quit(1)
		else:
			print("ERETHEA_RT_AUTO_OK")
			get_tree().quit(0)
