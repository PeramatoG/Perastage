extends Node

func _ready() -> void:
	var hello_world = HelloWorld.new()
	var message = hello_world.ping()
	print("[PeravizTest] " + str(message))
