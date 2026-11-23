extends Control

## A game with its own title bar: the window test drags nothing here, it only
## looks at the window's frame and clicks the close button.

const BAR_HEIGHT := 32
const BUTTON_WIDTH := 46

var _bar: ColorRect


func _ready() -> void:
	var background := ColorRect.new()
	background.color = Color(0.08, 0.1, 0.14)
	background.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(background)

	_bar = ColorRect.new()
	_bar.color = Color(0.12, 0.15, 0.2)
	_bar.set_anchors_preset(Control.PRESET_TOP_WIDE)
	_bar.offset_bottom = BAR_HEIGHT
	_bar.gui_input.connect(_on_bar_gui_input)
	add_child(_bar)

	var close := Button.new()
	close.text = "X"
	close.name = "Close"
	close.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	close.offset_left = -BUTTON_WIDTH
	close.offset_right = 0
	close.offset_top = 0
	close.offset_bottom = BAR_HEIGHT
	close.pressed.connect(func() -> void: get_tree().quit(0))
	add_child(close)

	DisplayServer.window_set_color(_bar.color)
	print("ERETHEA_GAME_READY extend=%s" % DisplayServer.window_get_flag(DisplayServer.WINDOW_FLAG_EXTEND_TO_TITLE))


func _on_bar_gui_input(event: InputEvent) -> void:
	var mouse_event := event as InputEventMouseButton
	if mouse_event and mouse_event.pressed and mouse_event.button_index == MOUSE_BUTTON_LEFT:
		DisplayServer.window_start_drag()
