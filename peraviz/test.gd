extends Node

func _ready() -> void:
	var hello_world = HelloWorld.new()
	var message = hello_world.ping()
	print("[PeravizTest] " + str(message))

	var ping_label := get_node_or_null("HUD/PingLabel") as Label
	if ping_label:
		ping_label.text = str(message)
	else:
		push_warning("[PeravizTest] HUD/PingLabel is missing; cannot display ping result")
