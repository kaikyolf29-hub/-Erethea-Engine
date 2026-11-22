@tool
extends EditorPlugin
## Drives the assistant panel for test_assistant.py. The test writes
## command_<n>.json in res://.driver; this plugin runs it, waits for the panel
## to finish, and writes done_<n>.json with what the panel shows.

var folder := ""
var next := 1
var waiting := false


func _enter_tree() -> void:
	folder = ProjectSettings.globalize_path("res://.driver")
	DirAccess.make_dir_recursive_absolute(folder)


func _find(node_name: String) -> Node:
	return get_tree().root.find_child(node_name, true, false)


func _write(file_name: String, data: Dictionary) -> void:
	var temporary := folder.path_join(file_name + ".tmp")
	var file := FileAccess.open(temporary, FileAccess.WRITE)
	file.store_string(JSON.stringify(data))
	file.close()
	DirAccess.rename_absolute(temporary, folder.path_join(file_name))


func _report() -> void:
	var conversation := _find("AssistantConversation") as RichTextLabel
	var undo := _find("AssistantUndo") as Button
	_write("done_%d.json" % next, {
		"conversation": conversation.get_parsed_text(),
		"undo_disabled": undo.disabled,
	})
	next += 1


func _process(_delta: float) -> void:
	var send := _find("AssistantSend") as Button
	if send == null:
		return
	if not FileAccess.file_exists(folder.path_join("ready.json")):
		_write("ready.json", {"ready": true})
	if waiting:
		if send.text == "Send":
			waiting = false
			_report()
		return

	var path := folder.path_join("command_%d.json" % next)
	if not FileAccess.file_exists(path):
		return
	var command: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(path))
	if command.has("settings"):
		var settings := EditorInterface.get_editor_settings()
		for setting: String in command["settings"]:
			settings.set_setting(setting, command["settings"][setting])
		_report()
	elif command.has("ask"):
		var input := _find("AssistantInput") as TextEdit
		input.text = command["ask"]
		send.pressed.emit()
		waiting = true
	elif command.has("undo"):
		(_find("AssistantUndo") as Button).pressed.emit()
		_report()
	elif command.has("new"):
		(_find("AssistantNewConversation") as Button).pressed.emit()
		_report()
