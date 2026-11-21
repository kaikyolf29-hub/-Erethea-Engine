extends SceneTree
## Runs res://principal.tscn without a window, walks the player forward and
## prints ERETHEA_WALK with what happened, as JSON.

const FRAMES := 150

var _frames := 0
var _player: CharacterBody3D
var _start := Vector3.ZERO


func _initialize() -> void:
	var scene: PackedScene = load("res://principal.tscn")
	root.add_child(scene.instantiate())


func _physics_process(_delta: float) -> bool:
	_frames += 1
	if _player == null:
		_player = get_first_node_in_group("jogador") as CharacterBody3D
		if _player:
			_start = _player.global_position
	if _player and _frames > 20:
		_player.entrada_teste = Vector2(0, -1)
	if _frames < FRAMES:
		return false
	var report := {"player": _player != null, "enemies": get_nodes_in_group("inimigos").size(), "areas": []}
	for area in root.get_node("Principal/Mundo/Areas").get_children():
		report["areas"].append({"name": String(area.name), "enemies": area.get_node("Inimigos").get_child_count()})
	if _player:
		report["walked_m"] = _player.global_position.distance_to(_start)
		report["on_floor"] = _player.is_on_floor()
		report["fell"] = _player.global_position.y < -5.0
		var anim: AnimationPlayer = _player.anim
		if anim:
			report["animation"] = String(anim.current_animation)
			report["loops"] = {}
			for animation in ["parado", "andar", "atacar"]:
				report["loops"][animation] = anim.get_animation(animation).loop_mode != Animation.LOOP_NONE
	print("ERETHEA_WALK ", JSON.stringify(report))
	return true
