shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_opaque;

instance uniform sampler2D gobo_texture;
instance uniform float gobo_cutoff : hint_range(0.0, 1.0) = 0.5;
instance uniform vec2 gobo_size = vec2(0.1, 0.1);
instance uniform float gobo_thickness : hint_range(0.0005, 0.05) = 0.003;

varying vec3 local_pos;

void vertex() {
	local_pos = VERTEX;
}

void fragment() {
	ALBEDO = vec3(0.0);
	if (abs(local_pos.z) > (gobo_thickness * 0.35)) {
		return;
	}

	vec2 uv = (local_pos.xy / max(gobo_size, vec2(0.001))) + vec2(0.5);
	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
		return;
	}
	vec4 tex = texture(gobo_texture, uv);
	float luma = dot(tex.rgb, vec3(0.299, 0.587, 0.114));
	if (luma >= gobo_cutoff) {
		discard;
	}
}
