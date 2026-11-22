extends CharacterBody3D
## Stands in for the player script that the Fornalha writes: walks where
## entrada_teste points and plays the walk cycle.

var entrada_teste := Vector2.ZERO
var vida := 100
var anim: AnimationPlayer

@onready var modelo: Node3D = $Modelo
@onready var braco: SpringArm3D = $BracoCamera


func _ready() -> void:
	add_to_group("jogador")
	var players := modelo.find_children("*", "AnimationPlayer", true, false)
	if not players.is_empty():
		anim = players[0]
		anim.play("parado")


func _physics_process(delta: float) -> void:
	var direcao := Vector3(entrada_teste.x, 0.0, entrada_teste.y)
	velocity.x = direcao.x * 4.0
	velocity.z = direcao.z * 4.0
	if not is_on_floor():
		velocity.y -= 9.8 * delta
	move_and_slide()
	if anim:
		var nome := "andar" if direcao.length() > 0.1 else "parado"
		if anim.current_animation != nome:
			anim.play(nome)
