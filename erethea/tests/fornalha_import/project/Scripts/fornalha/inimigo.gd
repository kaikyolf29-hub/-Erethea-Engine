extends CharacterBody3D
## Stands in for the enemy script that the Fornalha writes.

var vida := 100


func _ready() -> void:
	add_to_group("inimigos")


func _physics_process(delta: float) -> void:
	if not is_on_floor():
		velocity.y -= 9.8 * delta
	move_and_slide()
