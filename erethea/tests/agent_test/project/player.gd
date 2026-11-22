extends CharacterBody3D
## Walks forward with "move_forward" and jumps with "jump". For the test, the
## user argument --error makes jumping print an error, and --quit-at=<seconds>
## quits the game by itself.

const SPEED := 3.0
const JUMP_SPEED := 4.5

var fail_on_jump := false
var quit_at := 0.0
var time := 0.0


func _ready() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument == "--error":
			fail_on_jump = true
		elif argument.begins_with("--quit-at="):
			quit_at = float(argument.trim_prefix("--quit-at="))


func _physics_process(delta: float) -> void:
	time += delta
	if quit_at > 0.0 and time >= quit_at:
		quit_at = 0.0
		get_tree().quit()
	if not is_on_floor():
		velocity += get_gravity() * delta
	velocity.x = 0.0
	velocity.z = -Input.get_action_strength("move_forward") * SPEED
	if Input.is_action_just_pressed("jump") and is_on_floor():
		velocity.y = JUMP_SPEED
		if fail_on_jump:
			push_error("The player jumped into an error.")
	move_and_slide()
