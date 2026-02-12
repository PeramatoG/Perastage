extends Node

func _ready() -> void:
	var native := HelloWorld.new()
	var message := native.ping()
	print("[PeravizTest] %s" % message)
