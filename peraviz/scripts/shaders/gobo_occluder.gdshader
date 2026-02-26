shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_opaque;

instance uniform sampler2D gobo_texture;
instance uniform float gobo_cutoff : hint_range(0.0, 1.0) = 0.5;

void fragment() {
	vec4 tex = texture(gobo_texture, UV);
	float luma = dot(tex.rgb, vec3(0.299, 0.587, 0.114));
	float open_mask = step(gobo_cutoff, luma);
	ALBEDO = vec3(0.0);
	ALPHA = 1.0 - open_mask;
	ALPHA_SCISSOR_THRESHOLD = 0.5;
}
