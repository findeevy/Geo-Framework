extends CharacterBody3D
class_name MovementController

@export var gravity_multiplier := 3.0
@export var speed := 10
@export var acceleration := 8
@export var deceleration := 10
@export_range(0.0, 1.0, 0.05) var air_control := 0.3
@export var jump_height: float = 2
@export var height: float = 1.8
@export var radius: float = 0.3
@export var head_offset: float = 0.25
@onready var foot_offset: float = height / 2
var direction := Vector3()
var input_axis := Vector2()
# Get the gravity from the project settings to be synced with RigidDynamicBody nodes.
@onready var gravity: float = (ProjectSettings.get_setting("physics/3d/default_gravity") 
		* gravity_multiplier)
var collision_shape: CollisionShape3D
var is_above = false

func _ready():
	set_up_collision()

func _input(event):
	if Input.is_action_pressed(&"teleport"):
		is_above=!is_above
		toggle_mode()

func _process(delta):
	Controller.process_player_position(self.global_position)

func toggle_mode():
	if is_above:
		var head = get_node("Head")
		head.position = Vector3(0, 100, 0)  
		head.rotation = Vector3(180, 0, 0)
	else:
		set_up_collision()
func set_up_collision() -> void:
	var collision = get_node("Collision")
	if collision is CollisionShape3D:
		var shape = collision.shape
		if shape is CapsuleShape3D:
			var capsule = shape as CapsuleShape3D
			capsule.height = height
			capsule.radius = radius
			collision.position = Vector3(0, height / 2, 0)
	else:
		push_error("Could not find Collision node with a capsule shape")
	var head = get_node("Head")
	head.position = Vector3(0, height - head_offset, 0)
			
# Called every physics tick. 'delta' is constant
func _physics_process(delta: float) -> void:
	if !is_above: 
		input_axis = Input.get_vector(&"move_back", &"move_forward",
				&"move_left", &"move_right")
		
		direction_input()
		
		if is_on_floor():
			if Input.is_action_just_pressed(&"jump"):
				add_jump_velocity(jump_height)
		else:
			velocity.y -= gravity * delta
		
		accelerate(delta)
		
		move_and_slide()
	
func add_jump_velocity(jump_height: float) -> void:
	velocity.y = sqrt(2 * jump_height * gravity)


func direction_input() -> void:
	direction = Vector3()
	var aim: Basis = get_global_transform().basis
	direction = aim.z * -input_axis.x + aim.x * input_axis.y


func accelerate(delta: float) -> void:
	# Using only the horizontal velocity, interpolate towards the input.
	var temp_vel := velocity
	temp_vel.y = 0
	
	var temp_accel: float
	var target: Vector3 = direction * speed
	
	if direction.dot(temp_vel) > 0:
		temp_accel = acceleration
	else:
		temp_accel = deceleration
	
	if not is_on_floor():
		temp_accel *= air_control
	
	temp_vel = temp_vel.lerp(target, temp_accel * delta)
	
	velocity.x = temp_vel.x
	velocity.z = temp_vel.z
	
func get_foot_pos() -> Vector3:
	return global_position
	
