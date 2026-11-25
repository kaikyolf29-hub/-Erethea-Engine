extends SceneTree
## Changes the terrain of the world the way painting in the editor does, and
## saves it, to see that the next build keeps it.


func _initialize() -> void:
	var data := load("res://World/terreno.res") as EretheaTerrainData
	data.set_vertex_height(Vector2i(80, 80), 3.0)
	data.set_control(Vector2i(80, 80), 3, 0, 0.0)
	data.notify_maps_changed()
	print("ERETHEA_TOUCHED ", ResourceSaver.save(data, "res://World/terreno.res"))
	quit()
