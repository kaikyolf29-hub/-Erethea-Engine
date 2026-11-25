extends Node3D
## Erethea: measures a dense model (dense.glb, made by test_dense_geometry.py)
## the way the renderer draws it: how many mesh instances it became, and how
## many primitives a frame draws and how long it takes, from near the ground
## and from far away. Prints one line "DENSE {...}" and quits.

const FRAMES := 20

var model: Node3D
var camera: Camera3D
var results := {}


func _ready() -> void:
	model = (load("res://dense.glb") as PackedScene).instantiate()
	add_child(model)
	camera = Camera3D.new()
	camera.far = 4000.0
	add_child(camera)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-50, 30, 0)
	add_child(sun)
	_measure.call_deferred()


func _count_instances(node: Node) -> int:
	var count := 1 if node is MeshInstance3D else 0
	for child in node.get_children():
		count += _count_instances(child)
	return count


func _view(position: Vector3, target: Vector3) -> Dictionary:
	var environment := WorldEnvironment.new()
	if get_node_or_null("Sky") == null:
		# A bright red background, so that a crack between clusters shows.
		var e := Environment.new()
		e.background_mode = Environment.BG_COLOR
		e.background_color = Color(1, 0, 0)
		environment.environment = e
		environment.name = "Sky"
		add_child(environment)
	else:
		environment.free()
	camera.position = position
	camera.look_at(target)
	for i in 4:
		await RenderingServer.frame_post_draw
	var primitives := 0
	var begin := Time.get_ticks_usec()
	for i in FRAMES:
		await RenderingServer.frame_post_draw
		primitives = max(primitives, get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_PRIMITIVES_IN_FRAME))
	var result := { "primitives": primitives, "ms": (Time.get_ticks_usec() - begin) / 1000.0 / FRAMES }
	# A capture next to the project, to look for cracks between the clusters.
	get_viewport().get_texture().get_image().save_png(ProjectSettings.globalize_path("res://view_%d.png" % int(position.y)))
	return result


func _measure() -> void:
	results["instances"] = _count_instances(model)
	# The model is a 1000 m wide landscape around the origin. Near: standing at
	# one edge, looking across it. Far: from high above, seeing all of it.
	results["near"] = await _view(Vector3(-480, 40, -480), Vector3(0, 0, 0))
	results["far"] = await _view(Vector3(0, 1800, 1800), Vector3(0, 0, 0))
	print("DENSE ", JSON.stringify(results))
	get_tree().quit()
