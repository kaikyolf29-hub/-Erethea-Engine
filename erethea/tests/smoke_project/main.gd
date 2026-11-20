extends Node3D

## Smoke test: spin the box for a few frames, print a marker the CI looks for, then quit.

var frames := 0


func _process(delta: float) -> void:
	$Box.rotate_y(delta)
	frames += 1
	if frames == 10:
		print("ERETHEA_SMOKE_OK version=%s os=%s" % [Engine.get_version_info().string, OS.get_name()])
		get_tree().quit(0)
