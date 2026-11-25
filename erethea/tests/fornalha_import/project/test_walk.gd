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
	if _frames == 25:
		# What the Sons autoload does: ask the group for a situation.
		call_group("erethea_musica", "tocar", "vila")
		call_group("erethea_musica", "tocar", "combate")
		call_group("erethea_musica", "tocar", "nao_existe")
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
	var music := get_first_node_in_group("erethea_musica") as AudioStreamPlayer
	if music:
		var playback := music.get_stream_playback() as AudioStreamPlaybackInteractive
		var clips := music.stream as AudioStreamInteractive
		report["music"] = {
			"playing": music.playing,
			"clips": clips.clip_count,
			"current": String(clips.get_clip_name(playback.get_current_clip_index())) if playback else "",
		}
	# With the terrain made from the blockout (terrain_from_blockout), the ground
	# of the blockout is hidden and has no collision, and the terrain holds the player.
	var terrain := root.get_node_or_null("Principal/Mundo/Terreno")
	if terrain:
		var ground := root.get_node("Principal/Mundo/Blockout/chao") as Node3D
		var ground_layers := []
		for child in ground.get_children():
			if child is CollisionObject3D:
				ground_layers.append(child.collision_layer)
		var house_layers := []
		for child in root.get_node("Principal/Mundo/Blockout/vila_casa").get_children():
			if child is CollisionObject3D:
				house_layers.append(child.collision_layer)
		var data = terrain.data
		var resolution: Vector2i = data.get_resolution()
		report["terrain"] = {
			"class": terrain.get_class(),
			"resolution": [resolution.x, resolution.y],
			"ground_visible": ground.visible,
			"ground_layers": ground_layers,
			"house_layers": house_layers,
			"height_under_player": data.get_height(_player.global_position.x - terrain.global_position.x, _player.global_position.z - terrain.global_position.z) if _player else NAN,
			"materials": terrain.albedo_height_textures != null,
		}
	print("ERETHEA_WALK ", JSON.stringify(report))
	return true
